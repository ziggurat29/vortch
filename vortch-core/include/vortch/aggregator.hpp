#pragma once

#include "status.hpp"
#include <vector>

namespace vortch {

// The pure reducer seam: active job reports -> one displayed status.
// Stateless. Swap implementations for more sophisticated synthesis later.
struct IStatusAggregator {
  virtual ~IStatusAggregator() = default;
  virtual InstanceStatus reduce(const std::vector<JobReport>& active) const = 0;
};

// v1 rule: none -> quiescent; exactly one -> passthrough (normalized);
//          many -> coarse Processing with a count.
struct DefaultAggregator : IStatusAggregator {
  InstanceStatus reduce(const std::vector<JobReport>& active) const override {
    InstanceStatus s;
    if (active.empty()) {
      s.kind = StatusKind::Quiescent;
      return s;
    }
    if (active.size() == 1) {
      const auto& j = active.front();
      s.kind      = j.kind;
      s.progress  = j.progress;
      s.shortText = j.text;
      s.count     = 1;
      return s;
    }
    s.kind  = StatusKind::Processing;
    s.count = static_cast<int>(active.size());
    return s;
  }
};

} // namespace vortch
