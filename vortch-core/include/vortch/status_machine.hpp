#pragma once

#include "aggregator.hpp"
#include <chrono>
#include <optional>

namespace vortch {

struct StatusMachineConfig {
  int  donePulseMs   = 1500;   // how long the monostable 'done' pulse shows
  bool errorAutoClear = false; // false -> error is sticky until acknowledge()
};

// The temporal overlay seam: wraps a pure aggregator and adds time-based
// behavior the reducer can't express from active jobs alone -- the monostable
// 'done' pulse and error stickiness. Time is injected (testable headless).
class StatusMachine {
public:
  using Clock     = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  StatusMachine(const IStatusAggregator& agg, StatusMachineConfig cfg = {})
    : agg_(agg), cfg_(cfg) {}

  InstanceStatus update(const std::vector<JobReport>& active, TimePoint now) {
    InstanceStatus base = agg_.reduce(active);

    // Error has highest precedence.
    if (base.kind == StatusKind::Error) {
      if (!cfg_.errorAutoClear) errorLatched_ = true;
      current_ = base;
      return current_;
    }
    if (errorLatched_ && !cfg_.errorAutoClear) {
      current_.kind = StatusKind::Error;  // hold until acknowledged
      return current_;
    }

    // Monostable 'done' pulse.
    if (base.kind == StatusKind::Done) {
      doneUntil_ = now + std::chrono::milliseconds(cfg_.donePulseMs);
      current_   = base;
      return current_;
    }
    if (doneUntil_ && now < *doneUntil_) {
      current_.kind = StatusKind::Done;   // pulse still active
      return current_;
    }
    doneUntil_.reset();

    current_ = base;
    return current_;
  }

  void acknowledge() { errorLatched_ = false; }

  const InstanceStatus& current() const { return current_; }
  bool errorLatched() const { return errorLatched_; }

private:
  const IStatusAggregator& agg_;
  StatusMachineConfig      cfg_;
  InstanceStatus           current_{};
  std::optional<TimePoint> doneUntil_{};
  bool                     errorLatched_ = false;
};

} // namespace vortch
