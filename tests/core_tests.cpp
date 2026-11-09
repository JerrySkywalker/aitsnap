// SPDX-License-Identifier: Apache-2.0
#include "core/capture_service.h"
#include "core/region_edit.h"
#include <iostream>
#include <stdexcept>
using namespace aitsnap::core;
void check(bool ok, const char* msg) { if (!ok) throw std::runtime_error(msg); }
struct FakeBackend final : ICaptureBackend {
    int acquired{};
    std::vector<Display> topology{{"left", {-100, 0, 100, 80}, 0}, {"right", {0, 0, 100, 80}, 0}};
    std::vector<Display> displays() override { return topology; }
    DisplayFrame acquire(const Display& d) override {
        ++acquired;
        auto bytes = std::make_shared<std::vector<std::uint8_t>>(std::size_t(d.rect.width * d.rect.height * 4), 255);
        for (std::size_t i = 0; i < bytes->size(); i += 4) (*bytes)[i] = d.id == "left" ? 10 : 20;
        return {d, {d.rect.width, d.rect.height, bytes}, "synthetic", 100, 200, {}};
    }
};
int main()
{
    try {
        FakeBackend backend; CaptureService service(backend);
        CaptureRequest req; req.requestId = "test"; req.profile.id = "p";
        req.profile.regions = {{"span", "span", {-20, 0, 40, 10}, true}, {"overlap", "overlap", {0, 0, 10, 10}, true}};
        req.profile.calibratedDisplays = backend.topology;
        const auto batch = service.capture(req);
        check(backend.acquired == 2 && batch.captures.size() == 2, "once per relevant display");
        auto image = batch.captures[0].image;
        check(image.valid() && image.width == 40 && image.height == 10, "spanning image dimensions");
        check((*image.bgra)[0] == 10 && (*image.bgra)[20 * 4] == 20, "exact spanning pixels");
        auto acquired = service.acquireFrames(req);
        check(acquired.batch.displayFrames.size() == 2 && acquired.batch.captures.empty(), "frames captured before any crop");
        const auto acquiredCalls = backend.acquired;
        backend.topology[1].rect.x = 10;
        const auto cropped = CaptureService::cropFrames(std::move(acquired));
        check(backend.acquired == acquiredCalls && cropped.captures[0].image.valid() &&
              *cropped.captures[0].image.bgra == *image.bgra, "deferred crop uses only the already acquired frames");
        check(service.capture(req).captures[0].error.code == "TOPOLOGY_CHANGED", "recalibration required");
        req.reservedDisplays = req.profile.calibratedDisplays;
        req.profile.calibratedDisplays.clear();
        const auto changed = service.capture(req);
        check(changed.captures[0].error.code == "TOPOLOGY_CHANGED" && backend.acquired == acquiredCalls,
              "unbound profile rejects topology that changed after memory reservation");
        req.reservedDisplays.reset();
        const auto beforeGap = backend.acquired;
        const auto withGap = service.capture(req);
        check(withGap.captures[0].error.code == "UNCOVERED_REGION" && backend.acquired == beforeGap, "gap rejected before acquisition");
        req.profile.regions[1].rect.x = 10;
        const auto partial = service.capture(req);
        check(partial.captures[0].error.code == "UNCOVERED_REGION" && partial.captures[1].image.valid() &&
              backend.acquired == beforeGap + 1, "invalid source does not prevent a covered source from being acquired");
        req.profile.regions.push_back(req.profile.regions.front());
        check(bool(validateProfile(req.profile)), "duplicate region rejected");
        auto draft = req.profile; draft.regions.clear();
        check(!validateProfileDefinition(draft) && validateProfile(draft).code == "NO_REGIONS", "empty persisted draft is not capture-ready");
        draft.regions = {{"disabled", "Disabled", {0, 0, 16, 16}, false, 1}};
        check(!validateProfileDefinition(draft) && validateProfile(draft).code == "NO_REGIONS", "disabled sources can be saved but cannot capture");
        draft.regions.front().rect.width = 0;
        check(bool(validateProfileDefinition(draft)), "disabled draft still enforces source geometry");
        check(bool(validateVersion(2)) && !validateVersion(1), "migration guard");
        check(safeSegment("../x:y") == ".._x_y" && safeSegment("CON.txt") == "_CON.txt" && safeSegment("..") == "unnamed", "safe segments");
        std::string longName; for (int i = 0; i < 40; ++i) longName += "\xe4\xb8\xad";
        const auto shortened = safeSegment(longName);
        check(shortened.size() == 99 && shortened == longName.substr(0, 99), "long Chinese names truncate only at UTF8 character boundaries");
        check(canTransition(ReportState::Running, ReportState::Uncertain) &&
              !canTransition(ReportState::Committed, ReportState::Running), "report transition guards");
        SceneTemplate scene; scene.id = "s"; scene.name = "Synthetic scene"; scene.layers.push_back({"l", "span"});
        check(!validateScene(scene), "scene valid");
        scene.layers[0].destination.width = 0;
        check(bool(validateScene(scene)), "invalid transform rejected");
        check((*image.bgra)[0] == 10, "retained immutable source");
        check(draggedRect({}, {50, 40}, {-50, -40}, RegionDrag::Create) == Rect{-50, -40, 100, 80}, "reverse physical selection across origins");
        check(draggedRect({-100, -100, 200, 200}, {-50, -50}, {50, 50}, RegionDrag::Move) == Rect{0, 0, 200, 200}, "physical drag delta");
        check(draggedRect({-10, -10, 20, 20}, {10, 10}, {30, 50}, RegionDrag::BottomRight) == Rect{-10, -10, 40, 60}, "resize handle physical dimensions");
        check(draggedRect({0, 0, 20, 20}, {0, 0}, {30, 40}, RegionDrag::TopLeft) == Rect{20, 20, 10, 20}, "resize through opposite corner");
        const std::vector<Display> aligned{{"left", {-100, 0, 100, 80}}, {"right", {0, 0, 100, 80}}};
        check(coveredByDisplays({-20, 0, 40, 10}, aligned), "adjacent displays cover the shared seam");
        check(coveredByDisplays({-1, 0, 1, 80}, aligned), "half-open region ends exactly on the left display edge");
        check(!coveredByDisplays({80, 20, 30, 10}, aligned), "outer edge of desktop is uncovered");
        auto offset = aligned;
        offset[1].rect.y = 15;
        check(!coveredByDisplays({-20, 5, 40, 25}, offset), "corners in desktop bounds do not imply coverage");
        check(coveredByDisplays({-20, 15, 40, 25}, offset), "offset displays cover an aligned subset");
        auto gap = aligned;
        ++gap[1].rect.x;
        check(!coveredByDisplays({-20, 0, 40, 10}, gap), "one-pixel gap fails");
        const std::vector<Display> rows{{"top", {0, -80, 100, 80}}, {"bottom", {0, 0, 100, 80}}};
        check(coveredByDisplays({0, -20, 100, 40}, rows), "vertically adjacent displays cover the shared seam");
        std::cout << "CORE_PASS physical spanning pixels, topology/gap, snapshots, ids, versions, names, scenes, report transitions\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
