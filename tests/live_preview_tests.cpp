// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include "adapters/live_preview.h"
#include "adapters/win_capture.h"
#include "desktop/capture_visibility.h"
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
#include <QWidget>
#include <QTimer>
#include <atomic>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void settle(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
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
