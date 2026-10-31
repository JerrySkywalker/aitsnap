// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "geometry.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace aitsnap::core {
inline constexpr int schemaVersion = 1;
struct Error { std::string code, message; explicit operator bool() const { return !code.empty(); } };
struct Region { std::string id, name; Rect rect; bool enabled{true}; };
struct Display { std::string id; Rect rect; int rotation{}; };
struct CaptureProfile {
    std::string id, name;
    int revision{1};
    std::vector<Region> regions;
    std::vector<Display> calibratedDisplays;
    std::vector<std::string> sceneIds;
    std::string outputMode{"originals"}, clipboardSelection;
};
struct Context {
    std::string sourceSystem, eventId, sequenceRunId, stepExecutionId, commandInvocationId;
    int attempt{};
    std::string correlationStatus{"UNLINKED"};
    std::string testId, deviceId, stationId, operatorId;
    friend bool operator==(const Context&, const Context&) = default;
};
struct CaptureRequest {
    std::string requestId, issuedAtUtc;
    CaptureProfile profile;
    Context context;
    std::vector<std::pair<std::string, int>> sceneRevisions;
};
// Owned immutable packed BGRA8, top-to-bottom, alpha 255. No Qt/platform type.
struct Image {
    int width{}, height{};
    std::shared_ptr<const std::vector<std::uint8_t>> bgra;
    bool valid() const;
};
struct DisplayFrame {
    Display display;
    Image image;
    std::string acquiredAtUtc;
    std::int64_t acquisitionStartNs{}, acquisitionEndNs{};
    Error error;
};
struct SourceCapture { Region region; Image image; std::vector<std::string> displayIds; Error error; };
struct OutputResult { std::string kind, path, status{"NOT_REQUESTED"}; Error error; std::string regionId, sha256, sceneId; int sceneRevision{}; std::string relativePath; };
struct CaptureBatch {
    std::string batchId;
    CaptureRequest request;
    std::vector<DisplayFrame> displayFrames;
    std::vector<SourceCapture> captures;
    std::vector<OutputResult> outputs;
};
struct Destination { double x{}, y{}, width{1}, height{1}, rotationDeg{}; };
struct Layer {
    std::string id, sourceRegionId;
    int zIndex{};
    bool visible{true}, locked{};
    Rect crop; // nonpositive width/height means entire source
    Destination destination;
    std::string missingSourcePolicy{"fail"};
    std::string groupId, sourceArtifactId;
    bool preserveAspect{true};
};
struct SceneTemplate {
    std::string id, name;
    int revision{1}, width{1600}, height{900};
    std::string background{"#20252C"};
    std::vector<Layer> layers;
    std::string revisionCreatedAtUtc, extensionsJson;
};
struct ExecutionEvent {
    std::string sourceSystem, eventId, eventType, sourceTimeUtc, receivedAtUtc;
    std::string sequenceDefinitionId, sequenceRevision, stepId, commandDefinitionId;
    std::int64_t sourceSequenceNo{};
    Context context;
    std::vector<std::string> stepPath;
    std::string commandStatus, contentDigest;
    friend bool operator==(const ExecutionEvent&, const ExecutionEvent&) = default;
};
enum class ReportState { Queued, Running, Committed, Retryable, Failed, Uncertain };
struct ReportItem { std::string artifactPath, caption; };
struct ReportJob {
    std::string id, batchId, documentPath;
    ReportState state{ReportState::Queued};
    int attemptCount{};
    std::vector<ReportItem> items;
    Error error;
};
std::string toString(ReportState state);
bool canTransition(ReportState from, ReportState to);
Error validateVersion(int version);
Error validateProfile(const CaptureProfile& profile);
Error validateScene(const SceneTemplate& scene);
std::string safeSegment(const std::string& value);
bool sameTopology(std::vector<Display> a, std::vector<Display> b);
} // namespace aitsnap::core
