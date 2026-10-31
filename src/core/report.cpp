// SPDX-License-Identifier: Apache-2.0
#include "domain.h"
namespace aitsnap::core {
std::string toString(ReportState s)
{
    switch (s) {
    case ReportState::Queued: return "QUEUED";
    case ReportState::Running: return "RUNNING";
    case ReportState::Committed: return "COMMITTED";
    case ReportState::Retryable: return "RETRYABLE";
    case ReportState::Failed: return "FAILED";
    case ReportState::Uncertain: return "UNCERTAIN";
    }
    return "FAILED";
}
bool canTransition(ReportState from, ReportState to)
{
    if (from == ReportState::Queued || from == ReportState::Retryable) return to == ReportState::Running;
    if (from == ReportState::Running) return to == ReportState::Committed || to == ReportState::Retryable || to == ReportState::Failed || to == ReportState::Uncertain;
    // Uncertain requires inspecting the exact document's marker before either action.
    return from == ReportState::Uncertain && (to == ReportState::Committed || to == ReportState::Retryable);
}
} // namespace aitsnap::core
