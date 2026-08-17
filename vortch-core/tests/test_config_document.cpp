#include <doctest/doctest.h>
#include "vortch/config_document.hpp"

using namespace vortch;

TEST_CASE("config document JSON round trip") {
  ConfigDocument d;
  d.settings.autostart   = false;
  d.settings.donePulseMs = 800;

  InstanceConfig c;
  c.id          = "abc";
  c.label       = "My Vortch";
  c.icon        = { ResourceScheme::Builtin, "icon" };
  c.position    = { 100, 200 };
  c.monitor     = "mon-1";
  c.size        = { 72, 72 };
  c.visualMode  = VisualMode::Expanded;
  c.params      = { {"foo", 1}, {"bar", "baz"} };
  d.instances.push_back(c);

  const std::string s = toJsonString(d);
  ConfigDocument d2 = fromJsonString(s);

  CHECK(d2.schemaVersion == 1);
  CHECK(d2.settings.autostart == false);
  CHECK(d2.settings.donePulseMs == 800);

  REQUIRE(d2.instances.size() == 1);
  const auto& c2 = d2.instances[0];
  CHECK(c2.id == "abc");
  CHECK(c2.label == "My Vortch");
  CHECK(c2.icon == ResourceRef{ ResourceScheme::Builtin, "icon" });
  CHECK(c2.position.x == 100);
  CHECK(c2.position.y == 200);
  CHECK(c2.size.w == 72);
  CHECK(c2.visualMode == VisualMode::Expanded);
  CHECK(c2.params.at("foo").get<int>() == 1);
  CHECK(c2.params.at("bar").get<std::string>() == "baz");
}
