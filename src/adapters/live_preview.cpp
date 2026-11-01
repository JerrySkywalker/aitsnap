// SPDX-License-Identifier: Apache-2.0
#include "live_preview.h"
#include "json_store.h"
#include "win_capture.h"
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
LiveSnapshot sampleLiveSources(const core::CaptureProfile& input,core::ICaptureBackend& backend,const std::vector<core::Rect>& blocked)
{
    QElapsedTimer clock;clock.start();LiveSnapshot result;result.profile=input;
    core::CaptureRequest request;request.profile=input;
    std::int64_t pixels=0;
    for(auto& region:request.profile.regions)if(region.enabled){
        const auto size=std::int64_t(region.rect.width)*region.rect.height;
        if(!region.rect.valid() || pixels+size>32000000){region.enabled=false;result.issues[region.id]=uiText("实时预览像素预算已满，请减少启用区域。");continue;}
        if(std::any_of(blocked.begin(),blocked.end(),[&](const auto& rect){return core::intersect(region.rect,rect).has_value();})){
            region.enabled=false;result.issues[region.id]=uiText("预览窗与来源重叠，请移出截图区域。");
        }
        if(region.enabled)pixels+=size;
    }
    try{
        const auto displays=backend.displays();std::int64_t displayPixels=0;
        for(const auto& d:displays)if(std::any_of(request.profile.regions.begin(),request.profile.regions.end(),[&](const auto& r){return r.enabled && core::intersect(r.rect,d.rect).has_value();}))displayPixels+=std::int64_t(d.rect.width)*d.rect.height;
        if(displayPixels>64000000)throw std::runtime_error("Live desktop frame budget exceeded");
        // Reuse exact crop/coverage/topology logic, without a request ID or evidence lifecycle.
        core::CaptureService capture(backend);const auto sample=capture.capture(request);
        for(const auto& source:sample.captures){if(source.error)result.issues[source.region.id]=QString::fromStdString(source.error.code+": "+source.error.message);else if(source.image.valid())result.sources[source.region.id]={toQImage(source.image),{}, {},{}};}
        qint64 first=0,last=0;
        for(const auto& frame:sample.displayFrames)if(!frame.error){if(!first || frame.acquisitionStartNs<first)first=frame.acquisitionStartNs;last=std::max(last,qint64(frame.acquisitionEndNs));result.acquiredAtUtc=QString::fromStdString(frame.acquiredAtUtc);}
        if(first)result.displaySpanMs=double(last-first)/1000000.0;
    }catch(const std::exception& error){for(const auto& r:input.regions)if(r.enabled)result.issues[r.id]=QString::fromUtf8(error.what());}
    result.state=result.sources.empty()?uiText("实时预览不可用"):result.issues.empty()?uiText("实时"):uiText("部分来源不可用");result.elapsedMs=double(clock.nsecsElapsed())/1000000.0;return result;
}
LivePreviewService::LivePreviewService(QObject* parent,Sampler sampler):QObject(parent),sampler_(std::move(sampler))
{
    if(!sampler_)sampler_=[](const auto& profile,const auto& blocked){const std::lock_guard<std::mutex> lock(desktopCaptureMutex());WinCaptureBackend backend;return sampleLiveSources(profile,backend,blocked);};
    pool_.setMaxThreadCount(1);timer_.setInterval(500);connect(&timer_,&QTimer::timeout,this,[this]{requestFrame();});
}
LivePreviewService::~LivePreviewService(){changed={};active_=false;++generation_;timer_.stop();pool_.waitForDone();}
void LivePreviewService::setFps(int fps){timer_.setInterval(1000/std::clamp(fps,1,5));}
void LivePreviewService::configure(core::CaptureProfile profile,std::vector<core::Rect> blocked)
{
    QJsonArray rects;for(const auto& r:blocked)rects.append(QJsonArray{r.x,r.y,r.width,r.height});
    const auto key=QJsonDocument(QJsonObject{{"profile",profileJson(profile)},{"blocked",rects}}).toJson(QJsonDocument::Compact);
    if(key==key_)return;key_=key;profile_=std::move(profile);blocked_=std::move(blocked);++generation_;snapshot_={};age_.invalidate();snapshot_.state=uiText("未采集");if(active_)requestFrame();
}
void LivePreviewService::setActive(bool active)
{
    if(active_==active)return;active_=active;++generation_;
    if(active_){timer_.start();requestFrame();}else{timer_.stop();snapshot_={};snapshot_.state=uiText("预览已暂停");if(changed)changed();}
}
void LivePreviewService::requestFrame()
{
    if(!active_)return;
    if(age_.isValid() && age_.elapsed()>2500 && !snapshot_.sources.empty()){snapshot_.sources.clear();snapshot_.state=uiText("预览已过期");if(changed)changed();}
    if(inFlight_)return;inFlight_=true;const auto generation=generation_;const auto profile=profile_;const auto blocked=blocked_;const auto sampler=sampler_;QPointer<LivePreviewService> self(this);
    pool_.start(QRunnable::create([self,generation,profile,blocked,sampler]{
        auto result=sampler(profile,blocked);
        if(self)QMetaObject::invokeMethod(self,[self,generation,result=std::move(result)]()mutable{if(!self)return;self->inFlight_=false;if(self->active_ && self->generation_==generation){self->snapshot_=std::move(result);self->age_.start();if(self->changed)self->changed();}},Qt::QueuedConnection);
    }));
}
}
