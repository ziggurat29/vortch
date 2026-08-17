#include <doctest/doctest.h>
#include "vortch/resource_ref.hpp"

using namespace vortch;

TEST_CASE("resource ref parse + round trip") {
  CHECK(toString(parseResourceRef("vortch:builtin/processing")) == "vortch:builtin/processing");

  auto e = parseResourceRef("vortch:embedded/abc123");
  CHECK(e.scheme == ResourceScheme::Embedded);
  CHECK(e.value == "abc123");

  auto f = parseResourceRef("file:///C:/x/y.svg");
  CHECK(f.scheme == ResourceScheme::File);
  CHECK(f.value == "/C:/x/y.svg");
  CHECK(toString(f) == "file:///C:/x/y.svg");

  CHECK_THROWS_AS(parseResourceRef("nope"), std::invalid_argument);
}
