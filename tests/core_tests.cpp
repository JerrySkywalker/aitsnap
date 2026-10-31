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
        backend.topology[1].rect.x = 10;
        check(service.capture(req).captures[0].error.code == "TOPOLOGY_CHANGED", "recalibration required");
        req.profile.calibratedDisplays.clear();
        check(service.capture(req).captures[0].error.code == "UNCOVERED_REGION", "gap rejected");
        req.profile.regions.push_back(req.profile.regions.front());
        check(bool(validateProfile(req.profile)), "duplicate region rejected");
        check(bool(validateVersion(2)) && !validateVersion(1), "migration guard");
        check(safeSegment("../x:y") == ".._x_y" && safeSegment("CON.txt") == "_CON.txt" && safeSegment("..") == "unnamed", "safe segments");
        check(canTransition(ReportState::Running, ReportState::Uncertain) &&
              !canTransition(ReportState::Committed, ReportState::Running), "report transition guards");
        SceneTemplate scene; scene.id = "s"; scene.layers.push_back({"l", "span"});
        check(!validateScene(scene), "scene valid");
        scene.layers[0].destination.width = 0;
        check(bool(validateScene(scene)), "invalid transform rejected");
        check((*image.bgra)[0] == 10, "retained immutable source");
        check(draggedRect({}, {50, 40}, {-50, -40}, RegionDrag::Create) == Rect{-50, -40, 100, 80}, "reverse physical selection across origins");
        check(draggedRect({-100, -100, 200, 200}, {-50, -50}, {50, 50}, RegionDrag::Move) == Rect{0, 0, 200, 200}, "physical drag delta");
        check(draggedRect({-10, -10, 20, 20}, {10, 10}, {30, 50}, RegionDrag::BottomRight) == Rect{-10, -10, 40, 60}, "resize handle physical dimensions");
        check(draggedRect({0, 0, 20, 20}, {0, 0}, {30, 40}, RegionDrag::TopLeft) == Rect{20, 20, 10, 20}, "resize through opposite corner");
        std::cout << "CORE_PASS physical spanning pixels, topology/gap, snapshots, ids, versions, names, scenes, report transitions\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
