#include <doctest/doctest.h>
#include "vortch/launch.hpp"

using namespace vortch;

TEST_CASE("launch id round trip") {
  const auto args = buildLaunchArgs("gid-123");
  REQUIRE(args.size() == 2);
  CHECK(args[0] == std::string(kIdFlag));
  CHECK(args[1] == "gid-123");

  auto got = parseVortchId(args);
  REQUIRE(got.has_value());
  CHECK(*got == "gid-123");

  std::vector<std::string> none = { "foo", "bar" };
  CHECK_FALSE(parseVortchId(none).has_value());
}
