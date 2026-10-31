// SPDX-License-Identifier: Apache-2.0
#include "context.h"
#include <set>
namespace aitsnap::core {
ExecutionKey executionKey(const Context& c)
{
    return {c.sourceSystem, c.sequenceRunId, c.stepExecutionId, c.commandInvocationId, c.attempt};
}
Context correlate(const ExecutionEvent& event)
{
    auto c = event.context; c.sourceSystem = event.sourceSystem; c.eventId = event.eventId;
    c.correlationStatus = "UNLINKED";
    if (c.sequenceRunId.empty()) return c;
    const bool sequence = event.eventType.rfind("SEQUENCE_", 0) == 0;
    const bool step = event.eventType.rfind("STEP_", 0) == 0;
    const bool command = event.eventType.rfind("COMMAND_", 0) == 0;
    const bool versioned = !event.sequenceDefinitionId.empty() && !event.sequenceRevision.empty();
    const bool complete = versioned && (sequence || (step && !c.stepExecutionId.empty()) ||
        (command && !c.stepExecutionId.empty() && !c.commandInvocationId.empty() && c.attempt > 0 && !event.commandDefinitionId.empty()));
    c.correlationStatus = complete ? "EXACT" : "AMBIGUOUS";
    return c;
}
EventResult EventRegistry::ingest(const ExecutionEvent& input)
{
    static const std::set<std::string> types{"SEQUENCE_STARTED", "SEQUENCE_COMPLETED", "SEQUENCE_FAILED", "SEQUENCE_ABORTED", "SEQUENCE_PAUSED", "SEQUENCE_RESUMED",
        "STEP_STARTED", "STEP_COMPLETED", "COMMAND_DISPATCHED", "COMMAND_ACKNOWLEDGED", "COMMAND_FAILED", "TELEMETRY_CONDITION"};
    auto event = input; event.context = correlate(event);
    if (event.sourceSystem.empty() || event.eventId.empty() || event.sourceTimeUtc.empty() || event.sourceSequenceNo < 0 || !types.count(event.eventType))
        return {EventDisposition::Rejected, event.context, {"EVENT_INVALID", "Source identity, supported event type, time and sequence number required"}};
    const EventKey key{event.sourceSystem, event.eventId};
    const auto previous = events_.find(key);
    if (previous != events_.end()) {
        // Receive UTC belongs to the first receipt, not semantic source identity.
        event.receivedAtUtc = previous->second.receivedAtUtc;
        if (event == previous->second) return {EventDisposition::Duplicate, previous->second.context, {}};
        return {EventDisposition::Conflict, previous->second.context, {"EVENT_CONFLICT", "Same source event id has different immutable content"}};
    }
    if (events_.size() >= capacity_) return {EventDisposition::Rejected, event.context, {"EVENT_CAPACITY", "Event capacity reached; no silent eviction"}};
    events_.emplace(key, event);
    return {EventDisposition::Accepted, event.context, {}};
}
} // namespace aitsnap::core
