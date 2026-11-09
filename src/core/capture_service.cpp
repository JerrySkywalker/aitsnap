// SPDX-License-Identifier: Apache-2.0
#include "capture_service.h"
#include <algorithm>
#include <cstring>
#include <exception>

namespace aitsnap::core {
AcquiredDesktop CaptureService::acquireFrames(const CaptureRequest& request)
{
    AcquiredDesktop acquisition;
    auto& batch = acquisition.batch;
    batch.batchId = request.requestId; batch.request = request;
    const auto profileError = validateProfile(request.profile);
    if (profileError) {
        batch.outputs.push_back({"capture", "", "FAILED", profileError});
        return acquisition;
    }
    const auto monitors = backend_.displays();
    acquisition.topologyChanged = !request.profile.calibratedDisplays.empty() &&
        !sameTopology(monitors, request.profile.calibratedDisplays);
    if (!acquisition.topologyChanged) {
        for (const auto& monitor : monitors) {
            if (std::none_of(request.profile.regions.begin(), request.profile.regions.end(),
                [&](const auto& r) { return r.enabled && intersect(r.rect, monitor.rect).has_value(); })) continue;
            try { batch.displayFrames.push_back(backend_.acquire(monitor)); }
            catch (const std::exception& e) { batch.displayFrames.push_back({monitor, {}, "", 0, 0, {"ACQUIRE", e.what()}}); }
        }
    }
    return acquisition;
}

CaptureBatch CaptureService::cropFrames(AcquiredDesktop acquisition)
{
    auto& batch = acquisition.batch;
    if (!batch.outputs.empty()) return std::move(batch);
    for (const auto& region : batch.request.profile.regions) {
        if (!region.enabled) continue;
        SourceCapture source; source.region = region;
        if (acquisition.topologyChanged) {
            source.error = {"TOPOLOGY_CHANGED", "Display arrangement changed; recalibrate the profile"};
            batch.captures.push_back(std::move(source)); continue;
        }
        const auto size = std::size_t(region.rect.width) * std::size_t(region.rect.height) * 4;
        auto pixels = std::make_shared<std::vector<std::uint8_t>>(size, 0);
        // Row intervals prove coverage even with overlapping/mirrored displays.
        std::vector<std::vector<std::pair<int, int>>> coverage(std::size_t(region.rect.height));
        for (const auto& frame : batch.displayFrames) {
            const auto overlap = intersect(region.rect, frame.display.rect);
            if (!overlap) continue;
            source.displayIds.push_back(frame.display.id);
            if (frame.error || !frame.image.valid() || frame.image.width != frame.display.rect.width || frame.image.height != frame.display.rect.height) {
                source.error = frame.error ? frame.error : Error{"FRAME", "Invalid acquired display frame"}; continue;
            }
            const auto r = *overlap;
            for (int y = 0; y < r.height; ++y) {
                const int dy = r.y - region.rect.y + y, dx = r.x - region.rect.x;
                const auto from = (std::size_t(r.y - frame.display.rect.y + y) * frame.image.width + std::size_t(r.x - frame.display.rect.x)) * 4;
                const auto to = (std::size_t(dy) * region.rect.width + dx) * 4;
                std::memcpy(pixels->data() + to, frame.image.bgra->data() + from, std::size_t(r.width) * 4);
                coverage[std::size_t(dy)].push_back({dx, dx + r.width});
            }
        }
        for (auto& row : coverage) {
            std::sort(row.begin(), row.end());
            int end = 0;
            for (auto interval : row) { if (interval.first > end) break; end = std::max(end, interval.second); }
            if (end != region.rect.width) { if (!source.error) source.error = {"UNCOVERED_REGION", "Region contains unavailable desktop pixels"}; break; }
        }
        if (!source.error) source.image = {region.rect.width, region.rect.height, pixels};
        batch.captures.push_back(std::move(source));
    }
    return std::move(batch);
}

CaptureBatch CaptureService::capture(const CaptureRequest& request)
{
    return cropFrames(acquireFrames(request));
}
} // namespace aitsnap::core
