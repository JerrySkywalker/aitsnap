// SPDX-License-Identifier: Apache-2.0
#include "capture_service.h"
#include "region_edit.h"
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
    acquisition.displays = backend_.displays();
    const auto& monitors = acquisition.displays;
    acquisition.topologyChanged = !request.profile.calibratedDisplays.empty() &&
        !sameTopology(monitors, request.profile.calibratedDisplays);
    acquisition.reservationChanged = request.reservedDisplays && !sameTopology(monitors, *request.reservedDisplays);
    if (!acquisition.topologyChanged && !acquisition.reservationChanged) {
        for (const auto& region : request.profile.regions)
            if (region.enabled && !coveredByDisplays(region.rect, monitors)) acquisition.uncoveredRegions.insert(region.id);
        for (const auto& monitor : monitors) {
            Rect area;
            for (const auto& region : request.profile.regions) {
                if (!region.enabled || acquisition.uncoveredRegions.count(region.id)) continue;
                const auto overlap = intersect(region.rect, monitor.rect);
                if (!overlap) continue;
                if (!area.valid()) { area = *overlap; continue; }
                const int left = std::min(area.x, overlap->x), top = std::min(area.y, overlap->y);
                const auto right = std::max(std::int64_t(area.x) + area.width, std::int64_t(overlap->x) + overlap->width);
                const auto bottom = std::max(std::int64_t(area.y) + area.height, std::int64_t(overlap->y) + overlap->height);
                area = {left, top, int(right - left), int(bottom - top)};
            }
            if (!area.valid()) continue;
            try { batch.displayFrames.push_back(backend_.acquireRegion(monitor, area)); }
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
        if (acquisition.reservationChanged) {
            source.error = {"TOPOLOGY_CHANGED", "Display arrangement changed before frame acquisition; retry capture"};
            batch.captures.push_back(std::move(source)); continue;
        }
        if (acquisition.uncoveredRegions.count(region.id)) {
            source.error = {"UNCOVERED_REGION", "Region extends beyond active display pixels"};
            for (const auto& display : acquisition.displays)
                if (intersect(region.rect, display.rect)) source.displayIds.push_back(display.id);
            batch.captures.push_back(std::move(source));
            continue;
        }
        const DisplayFrame* single=nullptr;
        for(const auto& frame:batch.displayFrames)if(intersect(region.rect,frame.display.rect)){
            if(single){single=nullptr;break;}
            single=&frame;
        }
        if(single && single->display.rect==region.rect && !single->error && single->image.valid() &&
           single->image.width==region.rect.width && single->image.height==region.rect.height){
            source.image=single->image;source.displayIds.push_back(single->display.id);
            batch.captures.push_back(std::move(source));continue;
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
