// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/capture_service.h"

namespace aitsnap {
class DxgiPreview final : public core::ICaptureBackend {
public:
    DxgiPreview();
    ~DxgiPreview() override;
    std::vector<core::Display> displays() override;
    core::DisplayFrame acquire(const core::Display&) override;
    core::DisplayFrame acquireRegion(const core::Display&, const core::Rect&) override;
    bool usesCompatibilityCapture() const;
    std::string compatibilityReason() const;
    void reset();
    void retainOutputs(const std::vector<core::Region>&,const std::vector<core::Rect>& blocked);
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
