#include <doctest/doctest.h>
#include "vortch/aggregator.hpp"

using namespace vortch;

TEST_CASE("default aggregator v1 rule") {
  DefaultAggregator agg;

  SUBCASE("none -> quiescent") {
    CHECK(agg.reduce({}).kind == StatusKind::Quiescent);
  }
  SUBCASE("one -> passthrough") {
    JobReport j; j.kind = StatusKind::Processing; j.progress = 0.5; j.text = "x";
    auto s = agg.reduce({ j });
    CHECK(s.kind == StatusKind::Processing);
    CHECK(s.count == 1);
    REQUIRE(s.progress.has_value());
    CHECK(s.progress.value() == doctest::Approx(0.5));
    CHECK(s.shortText == "x");
  }
  SUBCASE("many -> coarse processing with count") {
    JobReport a, b; a.kind = StatusKind::Done; b.kind = StatusKind::Processing;
    auto s = agg.reduce({ a, b });
    CHECK(s.kind == StatusKind::Processing);
    CHECK(s.count == 2);
    CHECK_FALSE(s.progress.has_value());
  }
}
