// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "scene_render.h"
#include "core/capture_service.h"
#include <QObject>
#include <QThreadPool>
#include <QTimer>
#include <QElapsedTimer>
#include <functional>
namespace aitsnap {
struct LiveSnapshot {
    core::CaptureProfile profile;
    SceneSources sources;
    std::map<std::string,QString> issues;
    QString acquiredAtUtc,state;
    double elapsedMs{},displaySpanMs{};
};
LiveSnapshot sampleLiveSources(const core::CaptureProfile&,core::ICaptureBackend&,const std::vector<core::Rect>& blocked={});
class LivePreviewService final:public QObject {
public:
    using Sampler=std::function<LiveSnapshot(const core::CaptureProfile&,const std::vector<core::Rect>&)>;
    explicit LivePreviewService(QObject* parent=nullptr,Sampler sampler={});
    ~LivePreviewService() override;
    void configure(core::CaptureProfile,std::vector<core::Rect> blocked={});
    void setActive(bool);void setFps(int);
    const LiveSnapshot& snapshot() const{return snapshot_;}
    bool active() const{return active_;}
    std::function<void()> changed;
private:
    QTimer timer_;QThreadPool pool_;core::CaptureProfile profile_;
    Sampler sampler_;QElapsedTimer age_;
    std::vector<core::Rect> blocked_;QByteArray key_;LiveSnapshot snapshot_;
    quint64 generation_{};bool active_{},inFlight_{};
    void requestFrame();
};
}
