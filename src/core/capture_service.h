// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "domain.h"
namespace aitsnap::core {
struct ICaptureBackend {
    virtual ~ICaptureBackend() = default;
    virtual std::vector<Display> displays() = 0;
    virtual DisplayFrame acquire(const Display&) = 0;
};
struct AcquiredDesktop {
    CaptureBatch batch;
    bool topologyChanged{};
};
// One logical batch. Calling host supplies queue/thread policy and frozen request.
class CaptureService {
public:
    explicit CaptureService(ICaptureBackend& backend) : backend_(backend) {}
    AcquiredDesktop acquireFrames(const CaptureRequest& request);
    static CaptureBatch cropFrames(AcquiredDesktop acquisition);
    CaptureBatch capture(const CaptureRequest& request);
private:
    ICaptureBackend& backend_;
};
} // namespace aitsnap::core
