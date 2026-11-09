// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include "adapters/live_preview.h"
#include "adapters/win_capture.h"
#include "desktop/capture_visibility.h"
#include "desktop/live_output_window.h"
#include "compat/qt_widgets_compat.h"
#include "desktop/scene_editor.h"
#include "desktop/scene_layer_item.h"
#include "core/scene_geometry.h"
#include <QGraphicsView>
#include <QJsonDocument>
#include <QApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QGuiApplication>
#include <QScreen>
#include <QThread>
#include <QPainter>
#include <QWidget>
#include <QTimer>
#include <QtMath>
#include <atomic>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void settle(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
class CadencePattern final : public QWidget {
public:
    using QWidget::QWidget;
    unsigned frame{};
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor::fromRgb(++frame & 0xffffff));
    }
};

static int cadenceProbe(int durationMs,bool sceneMode=false)
{
    if(const auto status=interactiveDesktopStatus();status){std::cout<<"NATIVE_HOLD "<<status.code<<'\n';return 2;}
    WinCaptureBackend backend;
    std::set<int> rates;
    for(auto* screen:QGuiApplication::screens()){
        const auto target=qRound(screen->refreshRate());
        if(target<=1 || (sceneMode && target!=60) || !rates.insert(target).second)continue;
        const auto dpi=screen->devicePixelRatio();
        CadencePattern pattern(nullptr,Qt::Window|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
        pattern.setAttribute(Qt::WA_ShowWithoutActivating);
        pattern.setGeometry(QRect(screen->geometry().topLeft()+QPoint(24,24),QSize(qCeil(1920/dpi),qCeil(1080/dpi))));
        LiveOutputWindow viewer("pattern","Synthetic cadence");
        viewer.findChild<QWidget*>("liveOutputPreview")->setMinimumSize(120,80);
        viewer.resize(280,145);
        const auto edge=screen->availableGeometry().bottomRight();
        viewer.move(edge-QPoint(viewer.width()+8,viewer.height()+8));
        pattern.show();viewer.show();settle(150);
        viewer.move(edge-QPoint(viewer.width()+24,viewer.height()+24));settle(100);
        RECT rect{};check(GetWindowRect(reinterpret_cast<HWND>(pattern.winId()),&rect),"physical cadence fixture");
        check(rect.right-rect.left>=1920 && rect.bottom-rect.top>=1080,"1080p cadence fixture");
        RECT output{};check(GetWindowRect(reinterpret_cast<HWND>(viewer.winId()),&output),"physical viewer fixture");
        check(!core::intersect({rect.left,rect.top,1920,1080},{output.left,output.top,output.right-output.left,output.bottom-output.top}),
              "native preview sits outside the synthetic source");
        check(viewer.screenRate()==target,"viewer follows its actual display");
        core::CaptureProfile profile;profile.id="cadence";profile.name="Synthetic";profile.calibratedDisplays=backend.displays();
        profile.regions={{"pattern","Synthetic 1080p",{rect.left,rect.top,1920,1080},true,1}};
        std::unique_ptr<CadencePattern> second;
        std::vector<core::SceneTemplate> scenes;
        if(sceneMode){
            for(auto* other:QGuiApplication::screens()){
                if(other==screen || other->geometry().width()*other->devicePixelRatio()<1980)continue;
                const auto scale=other->devicePixelRatio();
                second=std::make_unique<CadencePattern>(nullptr,Qt::Window|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
                second->setAttribute(Qt::WA_ShowWithoutActivating);
                second->setGeometry(QRect(other->geometry().topLeft()+QPoint(24,24),QSize(qCeil(1920/scale),qCeil(1080/scale))));
                second->show();settle(150);
                RECT bounds{};check(GetWindowRect(reinterpret_cast<HWND>(second->winId()),&bounds),"second 1080p source");
                check(bounds.right-bounds.left>=1920 && bounds.bottom-bounds.top>=1080,"second physical source dimensions");
                profile.regions.push_back({"second","Second synthetic 1080p",{bounds.left,bounds.top,1920,1080},true,2});
                scenes.push_back(core::presetScene(profile,"cadence-scene","Synthetic dual 1080p",1600,900,core::LayoutPreset::Row));
                break;
            }
            check(bool(second),"two physical displays for dual-source scene");
        }
        QTimer source;source.setTimerType(Qt::PreciseTimer);
        QObject::connect(&source,&QTimer::timeout,&pattern,[&]{pattern.update(0,0,128,128);if(second)second->update(0,0,128,128);});
        source.start(std::max(1,1000/target));
        LivePreviewService service;
        service.setFps(target,true);service.configure(profile,{},scenes);service.setActive(true);
        settle(1000);
        unsigned frames=0,last=0;double samplingMs=0;bool compatibility=false;std::string reason;
        service.changed=[&]{
            const auto& sample=service.snapshot();
            compatibility=compatibility || sample.compatibilityCapture;
            if(sample.compatibilityCapture)reason=sample.compatibilityReason;
            const auto it=sample.sources.find("pattern");
            if(it==sample.sources.end())return;
            const auto id=it->second.image.pixel(80,80)&0xffffff;
            if(id>last){last=id;++frames;samplingMs+=sample.elapsedMs;}
            QImage image=it->second.image;
            auto stamp=sample.sourceTimes.at("pattern");
            if(sceneMode){
                const auto composed=sample.composites.find("cadence-scene");
                if(composed==sample.composites.end() || !composed->second.complete)return;
                image=composed->second.image;stamp=std::max(stamp,sample.sourceTimes.at("second"));
            }
            viewer.displayFrame(std::move(image),sample.state,stamp,target,
                                QString::fromStdString(sample.compatibilityReason));
        };
        QElapsedTimer clock;clock.start();settle(durationMs);
        const auto actual=1000.0*frames/clock.elapsed();
        const auto painted=1000.0*viewer.paintedFrames()/clock.elapsed();
        service.changed={};service.setActive(false);
        std::cout<<"NATIVE_CADENCE mode="<<(sceneMode?"scene":"source")<<" screen="<<screen->name().toStdString()<<" target_hz="<<target
                 <<" source_frames="<<pattern.frame<<" fresh_frames="<<frames
                 <<" elapsed_ms="<<clock.elapsed()<<" acquisition_fps="<<actual
                 <<" paint_fps="<<painted
                 <<" avg_sampling_ms="<<(frames?samplingMs/frames:0)
                 <<" compatibility="<<compatibility<<" reason="<<reason<<'\n';
        check(frames>0,"native changing pixels observed");
    }
    return rates.empty()?2:0;
}
struct FakeBackend:core::ICaptureBackend {
    int acquired{};std::vector<core::Display> inventory{{"red",{0,0,16,8},0},{"green",{16,0,16,8},0}};
    std::vector<core::Display> displays()override{return inventory;}
    core::DisplayFrame acquire(const core::Display& d)override{
        ++acquired;auto bytes=std::make_shared<std::vector<std::uint8_t>>(16*8*4,255);
        for(int i=0;i<16*8;++i){(*bytes)[i*4]=0;(*bytes)[i*4+1]=d.id=="green"?255:0;(*bytes)[i*4+2]=d.id=="red"?255:0;}
        return {d,{16,8,bytes},"2026-11-01T15:00:00.000Z",1,1000001,{}};
    }
};
int main(int argc,char** argv)
{
    compat::configureHighDpi();QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    try{
        if(app.arguments().contains("--probe-refresh"))return cadenceProbe(app.arguments().contains("--soak")?600000:5000);
        if(app.arguments().contains("--probe-scene"))return cadenceProbe(app.arguments().contains("--soak")?600000:5000,true);
        FakeBackend backend;core::CaptureProfile profile;profile.id="live";profile.name="Live";profile.calibratedDisplays=backend.inventory;
        profile.regions={{"cross","跨屏",{12,0,8,8},true,1},{"small","来源",{0,0,4,4},true,2}};
        auto snapshot=sampleLiveSources(profile,backend);check(backend.acquired==2 && snapshot.sources.size()==2,"one frame per needed display shared by sources");
        const auto image=snapshot.sources.at("cross").image;check(image.size()==QSize(8,8) && image.pixelColor(1,1)==QColor(Qt::red) && image.pixelColor(6,1)==QColor(Qt::green),"physical crop across two screens");
        check(snapshot.sources.at("cross").path.isEmpty() && snapshot.sources.at("cross").artifactId.empty(),"preview has no saved evidence identity");
        {
            const auto model=core::presetScene(profile,"scene","实时场景",160,100,core::LayoutPreset::Grid);SceneEditor editor(model,profile,{});auto* view=editor.findChild<QGraphicsView*>();check(view,"scene view");
            for(auto* item:view->scene()->items())if(dynamic_cast<LayerItem*>(item)){item->setSelected(true);break;}
            const auto before=QJsonDocument(sceneJson(editor.sceneTemplate())).toJson();const auto selected=view->scene()->selectedItems().size();editor.setPreviewSources(snapshot.sources);
            check(QJsonDocument(sceneJson(editor.sceneTemplate())).toJson()==before && view->scene()->selectedItems().size()==selected,"live frame update preserves scene geometry and selection");
            QString error;check(!editor.exportDerived("unused-live-output.png",error),"unrecorded live frames cannot masquerade as saved source evidence");editor.setPreviewSources({});check(QJsonDocument(sceneJson(editor.sceneTemplate())).toJson()==before,"frame loss does not change saved scene model");
        }
        snapshot=sampleLiveSources(profile,backend,{{12,0,8,8}});check(snapshot.sources.size()==1 && snapshot.issues.count("cross"),"unexcluded overlap only holds affected source");
        backend.inventory[0].rect.x=-1;snapshot=sampleLiveSources(profile,backend);check(snapshot.sources.empty() && !snapshot.issues.empty(),"changed topology never pretends current pixels");
        std::atomic<int> calls=0;
        LivePreviewService service(nullptr,[&](const auto& p,const auto&){++calls;QThread::msleep(100);LiveSnapshot s;s.profile=p;s.state="test";QImage frame(4,4,QImage::Format_RGB32);frame.fill(Qt::red);s.sources[p.id]={frame,{},{},{}};return s;});
        service.configure(profile);service.setActive(true);auto changed=profile;changed.id="new-generation";service.configure(changed);settle(200);check(service.snapshot().sources.empty(),"late old-generation frame discarded");settle(500);check(service.snapshot().sources.count("new-generation"),"new-generation frame delivered");service.setActive(false);const auto count=calls.load();settle(200);check(service.snapshot().sources.empty() && calls==count,"paused service releases cache and stops sampling");
        std::cout<<"LIVE_COMPONENT_PASS exact crop/shared displays/overlap/topology/generation/pause; no evidence or report path\n";
        if(app.arguments().contains("--native")){
            const auto status=interactiveDesktopStatus();if(status){std::cout<<"NATIVE_HOLD "<<status.code<<'\n';return 2;}
            QWidget pattern(nullptr,Qt::Window|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint),overlay(nullptr,Qt::Window|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
            const auto origin=QGuiApplication::primaryScreen()->availableGeometry().topLeft()+QPoint(80,80);
            for(auto* widget:{&pattern,&overlay}){widget->setAttribute(Qt::WA_ShowWithoutActivating);widget->setGeometry(QRect(origin,QSize(160,100)));}
            pattern.setStyleSheet("background:#ff0000");overlay.setStyleSheet("background:#0000ff");pattern.show();overlay.show();settle(150);DwmFlush();
            setOwnCaptureExclusionEnabled(true);check(excludeOwnCaptureWindow(overlay),"explicit opt-in native exclusion API supported");settle(150);DwmFlush();RECT rect{};check(GetWindowRect(reinterpret_cast<HWND>(overlay.winId()),&rect),"own overlay native bounds");
            WinCaptureBackend native;core::CaptureProfile p;p.id="native-live";p.name="Synthetic";p.calibratedDisplays=native.displays();p.regions={{"r","known red under blue",{rect.left+20,rect.top+20,16,16},true,1}};
            const auto actual=sampleLiveSources(p,native);check(actual.sources.size()==1 && actual.sources.at("r").image.pixelColor(4,4)==QColor(Qt::red),"actual GDI excluded window reveals underlying pixels, not black");
            std::cout<<"LIVE_NATIVE_EXCLUSION_PASS actual GDI exact red under excluded blue; span_ms="<<actual.displaySpanMs<<" elapsed_ms="<<actual.elapsedMs<<'\n';
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
