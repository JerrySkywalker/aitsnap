// SPDX-License-Identifier: Apache-2.0
#include "domain.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace aitsnap::core {
bool Image::valid() const
{
    return width > 0 && height > 0 && bgra &&
        std::uint64_t(width) * std::uint64_t(height) * 4 == bgra->size();
}
Error validateVersion(int version)
{
    return version == schemaVersion ? Error{} : Error{"SCHEMA_VERSION", "Unsupported schema version; no automatic migration"};
}
Error validateProfile(const CaptureProfile& p)
{
    if (p.id.empty() || p.revision < 1 || p.regions.empty()) return {"PROFILE", "Profile id, revision and regions required"};
    std::set<std::string> ids;
    bool enabled = false;
    for (const auto& r : p.regions) {
        if (r.id.empty() || !ids.insert(r.id).second || !r.rect.valid()) return {"REGION", "Unique region ids and positive physical sizes required"};
        if (r.rect.width > 32768 || r.rect.height > 32768 ||
            std::int64_t(r.rect.width) * r.rect.height > 100000000) return {"REGION_SIZE", "Region exceeds preview memory limit"};
        enabled = enabled || r.enabled;
    }
    return enabled ? Error{} : Error{"NO_REGIONS", "Enable at least one region"};
}
Error validateScene(const SceneTemplate& s)
{
    if (s.id.empty() || s.revision < 1 || s.width <= 0 || s.height <= 0 ||
        std::int64_t(s.width) * s.height > 100000000) return {"SCENE", "Invalid scene identity or canvas size"};
    std::set<std::string> ids;
    for (const auto& l : s.layers) {
        const auto& d = l.destination;
        if (l.id.empty() || !ids.insert(l.id).second || l.sourceRegionId.empty() ||
            !std::isfinite(d.x) || !std::isfinite(d.y) || !std::isfinite(d.width) ||
            !std::isfinite(d.height) || !std::isfinite(d.rotationDeg) || d.width <= 0 || d.height <= 0 ||
            (l.missingSourcePolicy != "fail" && l.missingSourcePolicy != "skip" && l.missingSourcePolicy != "placeholder"))
            return {"LAYER", "Invalid layer identity, transform or missing-source policy"};
    }
    return {};
}
std::string safeSegment(const std::string& value)
{
    std::string result;
    for (unsigned char c : value) {
        if (c < 32 || std::string("<>:\"/\\|?*").find(char(c)) != std::string::npos) result += '_';
        else result += char(c);
    }
    while (!result.empty() && (result.back() == '.' || result.back() == ' ')) result.pop_back();
    if (result.empty() || result == "." || result == "..") result = "unnamed";
    auto stem = result.substr(0, result.find('.'));
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return char(std::toupper(c)); });
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" ||
        (stem.size() == 4 && (stem.substr(0, 3) == "COM" || stem.substr(0, 3) == "LPT") && stem[3] >= '1' && stem[3] <= '9'))
        result = "_" + result;
    return result.substr(0, 100);
}
bool sameTopology(std::vector<Display> a, std::vector<Display> b)
{
    auto sort = [](auto& list) { std::sort(list.begin(), list.end(), [](const auto& x, const auto& y) { return x.id < y.id; }); };
    sort(a); sort(b);
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].id != b[i].id || a[i].rect != b[i].rect || a[i].rotation != b[i].rotation) return false;
    return true;
}
} // namespace aitsnap::core
