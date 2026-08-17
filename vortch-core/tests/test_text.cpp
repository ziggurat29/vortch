#include <doctest/doctest.h>
#include "vortch/text.hpp"

using namespace vortch;

TEST_CASE("utf8 path round trip (non-ASCII)") {
  // "cafe" + e-acute, underscore, grinning-face emoji, ".json"
  const std::string s = u8"café_\U0001F600.json";

  const auto p = utf8ToPath(s);
  CHECK(pathToUtf8(p) == s);

#ifdef _WIN32
  const auto w = utf8ToWide(s);
  CHECK(wideToUtf8(w) == s);
  CHECK_FALSE(w.empty());
#endif
}
