// SPDX-License-Identifier: Apache-2.0
#include "core/context.h"
#include <iostream>
#include <stdexcept>
using namespace aitsnap::core;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static ExecutionEvent event(std::string id, std::string run, int attempt, std::int64_t number)
{
    ExecutionEvent e; e.sourceSystem = "SYNTHETIC"; e.eventId = std::move(id); e.eventType = "COMMAND_ACKNOWLEDGED";
    e.sourceTimeUtc = "2026-10-31T08:00:00Z"; e.receivedAtUtc = "2026-10-31T08:00:01Z"; e.sourceSequenceNo = number;
    e.sequenceDefinitionId = "synthetic-sequence"; e.sequenceRevision = "v1"; e.commandDefinitionId = "synthetic-read";
    e.context.sequenceRunId = std::move(run); e.context.stepExecutionId = "nested/loop-2/step-4";
    e.context.commandInvocationId = "invocation"; e.context.attempt = attempt; e.stepPath = {"nested", "loop-2", "step-4"};
    return e;
}
int main()
{
    try {
        EventRegistry registry(10);
        auto a = event("e-a", "run-a", 1, 11), b = event("e-b", "run-b", 1, 12), retry = event("e-retry", "run-a", 2, 10);
        const auto ra = registry.ingest(a), rb = registry.ingest(b), rr = registry.ingest(retry);
        check(ra.disposition == EventDisposition::Accepted && ra.context.correlationStatus == "EXACT", "exact initial event");
        check(executionKey(ra.context) != executionKey(rb.context) && executionKey(ra.context) != executionKey(rr.context), "two runs and retry isolated");
        check(rr.disposition == EventDisposition::Accepted, "out-of-order retained");
        a.receivedAtUtc = "2026-10-31T08:00:05Z";
        check(registry.ingest(a).disposition == EventDisposition::Duplicate && registry.events().size() == 3, "duplicate ignores transport receipt time");
        a.commandStatus = "changed"; check(registry.ingest(a).disposition == EventDisposition::Conflict, "changed duplicate rejected");
        auto ambiguous = event("e-incomplete", "run-a", 1, 13); ambiguous.context.stepExecutionId.clear(); ambiguous.context.correlationStatus = "EXACT";
        check(registry.ingest(ambiguous).context.correlationStatus == "AMBIGUOUS", "cannot trust claimed exact correlation");
        auto unlinked = event("e-unlinked", "", 1, 14); check(registry.ingest(unlinked).context.correlationStatus == "UNLINKED", "missing run unlinked");
        auto control = event("control", "run-a", 1, 15); control.eventType = "EXECUTE_COMMAND";
        check(registry.ingest(control).disposition == EventDisposition::Rejected, "command execution intent absent");
        EventRegistry bounded(1); check(bounded.ingest(b).disposition == EventDisposition::Accepted && bounded.ingest(retry).error.code == "EVENT_CAPACITY" && bounded.ingest(b).disposition == EventDisposition::Duplicate, "bounded without silent loss");
        std::cout << "CONTEXT_PASS two-runs retry nested-step out-of-order duplicate conflict ambiguous unlinked capacity no-transmitter\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
