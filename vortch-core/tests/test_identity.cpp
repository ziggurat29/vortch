#include <doctest/doctest.h>
#include "vortch/identity.hpp"

using namespace vortch;

TEST_CASE("uuid format, version, uniqueness") {
  const auto a = newUuid();
  const auto b = newUuid();
  CHECK(looksLikeUuid(a));
  CHECK(looksLikeUuid(b));
  CHECK(a != b);
  CHECK(a[14] == '4');  // version-4 nibble
  CHECK_FALSE(looksLikeUuid("not-a-uuid"));
  CHECK_FALSE(looksLikeUuid(""));
}
