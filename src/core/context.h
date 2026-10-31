// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "domain.h"
#include <map>
#include <tuple>
namespace aitsnap::core {
enum class EventDisposition { Accepted, Duplicate, Conflict, Rejected };
struct EventResult { EventDisposition disposition; Context context; Error error; };
using EventKey = std::pair<std::string, std::string>;
using ExecutionKey = std::tuple<std::string, std::string, std::string, std::string, int>;
ExecutionKey executionKey(const Context&);
Context correlate(const ExecutionEvent&);
// Immutable source-event records, in receive order only via explicit source/UTC fields.
// No global "latest command", no inferred physical command outcome and no transmitter.
class EventRegistry {
public:
    explicit EventRegistry(std::size_t capacity = 10000) : capacity_(capacity) {}
    EventResult ingest(const ExecutionEvent&);
    const std::map<EventKey, ExecutionEvent>& events() const { return events_; }
private:
    std::size_t capacity_;
    std::map<EventKey, ExecutionEvent> events_;
};
} // namespace aitsnap::core
