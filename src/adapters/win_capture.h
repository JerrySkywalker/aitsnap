// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/capture_service.h"
namespace aitsnap {
class WinCaptureBackend final : public core::ICaptureBackend {
public:
    std::vector<core::Display> displays() override;
    core::DisplayFrame acquire(const core::Display&) override;
};
} // namespace aitsnap
