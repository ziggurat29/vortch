#include <doctest/doctest.h>
#include "vortch/status_machine.hpp"

using namespace vortch;
using Clock = StatusMachine::Clock;

TEST_CASE("done pulse is monostable") {
  DefaultAggregator agg;
  StatusMachineConfig cfg; cfg.donePulseMs = 1000;
  StatusMachine sm(agg, cfg);
  const auto t0 = Clock::now();

  JobReport done; done.kind = StatusKind::Done;
  CHECK(sm.update({ done }, t0).kind == StatusKind::Done);
  // jobs gone, but within the pulse window it still reads done
  CHECK(sm.update({}, t0 + std::chrono::milliseconds(500)).kind == StatusKind::Done);
  // after the window it decays to quiescent
  CHECK(sm.update({}, t0 + std::chrono::milliseconds(1500)).kind == StatusKind::Quiescent);
}

TEST_CASE("error is sticky until acknowledged") {
  DefaultAggregator agg;
  StatusMachineConfig cfg; cfg.errorAutoClear = false;
  StatusMachine sm(agg, cfg);
  const auto t = Clock::now();

  JobReport err; err.kind = StatusKind::Error;
  CHECK(sm.update({ err }, t).kind == StatusKind::Error);
  CHECK(sm.errorLatched());
  // jobs gone, error held
  CHECK(sm.update({}, t + std::chrono::seconds(5)).kind == StatusKind::Error);
  sm.acknowledge();
  CHECK(sm.update({}, t + std::chrono::seconds(6)).kind == StatusKind::Quiescent);
}

TEST_CASE("error auto-clear when configured") {
  DefaultAggregator agg;
  StatusMachineConfig cfg; cfg.errorAutoClear = true;
  StatusMachine sm(agg, cfg);
  const auto t = Clock::now();

  JobReport err; err.kind = StatusKind::Error;
  CHECK(sm.update({ err }, t).kind == StatusKind::Error);
  CHECK_FALSE(sm.errorLatched());
  CHECK(sm.update({}, t + std::chrono::seconds(1)).kind == StatusKind::Quiescent);
}
