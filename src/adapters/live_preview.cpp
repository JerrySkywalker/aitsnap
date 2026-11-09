// SPDX-License-Identifier: Apache-2.0
#include "live_preview.h"
#include "json_store.h"
#include "win_capture.h"
#include "dxgi_preview.h"
#include "desktop/appearance.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRunnable>
#include <algorithm>
#include <exception>
namespace aitsnap {
static QImage previewImage(const core::Image& image)
{
    using Pixels=std::shared_ptr<const std::vector<std::uint8_t>>;
    auto* retained=new Pixels(image.bgra);
    return QImage(image.bgra->data(),image.width,image.height,image.width*4,QImage::Format_ARGB32,
                  [](void* data){delete static_cast<Pixels*>(data);},retained);
}

LiveSnapshot sampleLiveSources(const core::CaptureProfile& input,core::ICaptureBackend& backend,const std::vector<core::Rect>& blocked)
{
    QElapsedTimer clock;clock.start();LiveSnapshot result;result.profile=input;
    core::CaptureRequest request;request.profile=input;
    std::int64_t pixels=0;
    for(auto& region:request.profile.regions)if(region.enabled){
        const auto size=std::int64_t(region.rect.width)*region.rect.height;
        if(!region.rect.valid() || pixels+size>32000000){region.enabled=false;result.issues[region.id]=uiText("实时预览像素预算已满，请减少启用区域。");continue;}
        if(std::any_of(blocked.begin(),blocked.end(),[&](const auto& rect){return core::intersect(region.rect,rect).has_value();})){
            region.enabled=false;result.issues[region.id]=uiText("AITSnap 窗口与来源重叠，请移开窗口后预览；正式截图仍会隐藏自身。");
        }
        if(region.enabled)pixels+=size;
    }
    try{
        const auto displays=backend.displays();std::int64_t displayPixels=0;
        for(const auto& d:displays)if(std::any_of(request.profile.regions.begin(),request.profile.regions.end(),[&](const auto& r){return r.enabled && core::intersect(r.rect,d.rect).has_value();}))displayPixels+=std::int64_t(d.rect.width)*d.rect.height;
        if(displayPixels>64000000)throw std::runtime_error("Live desktop frame budget exceeded");
        // Reuse exact crop/coverage/topology logic, without a request ID or evidence lifecycle.
        core::CaptureService capture(backend);const auto sample=capture.capture(request);
        for(const auto& source:sample.captures){if(source.error)result.issues[source.region.id]=QString::fromStdString(source.error.code+": "+source.error.message);else if(source.image.valid())result.sources[source.region.id]={previewImage(source.image),{}, {},{}};}
        for (const auto& source : sample.captures) if (source.image.valid()) {
            auto& newest = result.sourceTimes[source.region.id];
            for (const auto& frame : sample.displayFrames)
                if (!frame.error && core::intersect(source.region.rect, frame.display.rect))
                    newest = std::max(newest, frame.acquisitionEndNs);
        }
        qint64 first=0,last=0;
        for(const auto& frame:sample.displayFrames)if(!frame.error){if(!first || frame.acquisitionStartNs<first)first=frame.acquisitionStartNs;last=std::max(last,qint64(frame.acquisitionEndNs));result.acquiredAtUtc=QString::fromStdString(frame.acquiredAtUtc);result.frameTimes.push_back(frame.acquisitionEndNs);}
        if(first)result.displaySpanMs=double(last-first)/1000000.0;
    }catch(const std::exception& error){for(const auto& r:input.regions)if(r.enabled)result.issues[r.id]=QString::fromUtf8(error.what());}
    result.state=result.sources.empty()?uiText("实时预览不可用"):result.issues.empty()?uiText("实时"):uiText("部分来源不可用");result.elapsedMs=double(clock.nsecsElapsed())/1000000.0;return result;
}
LivePreviewService::LivePreviewService(QObject* parent,Sampler sampler):QObject(parent),sampler_(std::move(sampler))
{
    setObjectName("livePreviewService");
    if(!sampler_) {
        modern_=std::make_unique<DxgiPreview>();
        sampler_=[](const auto& profile,const auto& blocked){const std::lock_guard<std::mutex> lock(desktopCaptureMutex());WinCaptureBackend backend;return sampleLiveSources(profile,backend,blocked);};
    }
    pool_.setMaxThreadCount(1);timer_.setTimerType(Qt::PreciseTimer);timer_.setInterval(500);
    connect(&timer_,&QTimer::timeout,this,[this]{requestFrame();});
}
LivePreviewService::~LivePreviewService(){changed={};active_=false;++generation_;timer_.stop();releasePreviewBackend();pool_.waitForDone();}
void LivePreviewService::releasePreviewBackend()
{
    if(auto* backend=modern_.get())pool_.start(QRunnable::create([backend]{backend->reset();}));
}
void LivePreviewService::setFps(int fps, bool followScreen)
{
    const auto interval = std::max(1, followScreen ? std::min(3,500/std::max(1,fps)) : 1000/std::max(1,fps));
    if (timer_.interval() != interval) timer_.setInterval(interval);
    if (followScreen_ != followScreen) {
        if(!followScreen)releasePreviewBackend();
        followScreen_ = followScreen; ++generation_;
    }
}
void LivePreviewService::configure(core::CaptureProfile profile,std::vector<core::Rect> blocked,std::vector<core::SceneTemplate> scenes)
{
    QJsonArray rects;for(const auto& r:blocked)rects.append(QJsonArray{r.x,r.y,r.width,r.height});
    QJsonArray layout;for(const auto& scene:scenes)layout.append(sceneJson(scene));
    const auto key=QJsonDocument(QJsonObject{{"profile",profileJson(profile)},{"blocked",rects},{"scenes",layout}}).toJson(QJsonDocument::Compact);
    if(key==key_)return;key_=key;profile_=std::move(profile);blocked_=std::move(blocked);scenes_=std::move(scenes);++generation_;snapshot_={};age_.invalidate();snapshot_.state=uiText("未采集");if(active_)requestFrame();
}
void LivePreviewService::setActive(bool active)
{
    if(active_==active)return;active_=active;++generation_;
    if(active_){timer_.start();requestFrame();}else{timer_.stop();releasePreviewBackend();snapshot_={};snapshot_.state=uiText("预览已暂停");if(changed)changed();}
}
void LivePreviewService::requestFrame()
{
    if(!active_)return;
    if(age_.isValid() && age_.elapsed()>2500 && !snapshot_.sources.empty()){snapshot_.sources.clear();snapshot_.composites.clear();snapshot_.state=uiText("预览已过期");if(changed)changed();}
    if(inFlight_)return;inFlight_=true;const auto generation=generation_;const auto profile=profile_;const auto blocked=blocked_;const auto sampler=sampler_;QPointer<LivePreviewService> self(this);
    const auto previous = snapshot_.frameTimes;
    const auto previousIssues = snapshot_.issues;
    const auto scenes = scenes_;
    auto* modern = followScreen_ ? modern_.get() : nullptr;
    pool_.start(QRunnable::create([self,generation,profile,blocked,sampler,modern,previous,previousIssues,scenes]{
        if(modern)modern->retainOutputs(profile.regions,blocked);
        auto result=modern ? sampleLiveSources(profile,*modern,blocked) : sampler(profile,blocked);
        result.compatibilityCapture=modern && modern->usesCompatibilityCapture();
        if(result.compatibilityCapture)result.compatibilityReason=modern->compatibilityReason();
        const bool repeated = modern && result.frameTimes == previous && result.issues == previousIssues;
        if (!repeated) for (const auto& scene : scenes)
            result.composites.emplace(scene.id, renderScenePreview(scene, result.sources, {1600,1000}));
        if(self)QMetaObject::invokeMethod(self,[self,generation,repeated,result=std::move(result)]()mutable{
            if(!self)return;
            self->inFlight_=false;
            if(!self->active_ || self->generation_!=generation)return;
            self->age_.start();
            if(repeated && self->snapshot_.state != uiText("未采集") && self->snapshot_.state != uiText("预览已过期")) return;
            result.serial=++self->serial_;
            self->snapshot_=std::move(result);
            if(self->changed)self->changed();
        },Qt::QueuedConnection);
    }));
}
}
