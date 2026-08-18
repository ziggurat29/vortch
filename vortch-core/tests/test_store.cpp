#include <doctest/doctest.h>
#include "vortch/store.hpp"

#include <sqlite3.h>
#include <filesystem>
#include <set>

using namespace vortch;

namespace {
std::filesystem::path tempDb() {
  return std::filesystem::temp_directory_path() / ("vortch_test_" + newUuid() + ".db");
}
struct TempDb {
  std::filesystem::path path = tempDb();
  ~TempDb() { std::error_code ec; std::filesystem::remove(path, ec); }
};
} // namespace

TEST_CASE("object round-trip + persistence across reopen") {
  TempDb t;
  std::string id;
  {
    Store s = Store::open(t.path);
    StoredObject o = newStoredObject("config", "My Settings");
    o.facets = nlohmann::json::object();
    o.facets["machines"] = nlohmann::json::array({"PC1"});
    o.body["foo"] = 1;
    o.body["bar"] = "baz";
    s.putObject(o);
    id = o.id;

    auto got = s.getObject(id);
    REQUIRE(got.has_value());
    CHECK(got->kind == "config");
    CHECK(got->name == "My Settings");
    CHECK(got->body.at("foo").get<int>() == 1);
    CHECK(got->facets.at("machines")[0].get<std::string>() == "PC1");
  }
  {
    Store s = Store::open(t.path);              // reopen -> data persisted
    auto all = s.listByKind("config");
    REQUIRE(all.size() == 1);
    CHECK(all[0].id == id);
  }
}

TEST_CASE("facet query: absent/empty = match-all, else contains") {
  TempDb t;
  Store s = Store::open(t.path);

  StoredObject a = newStoredObject("vortex", "A");
  a.facets["machines"] = nlohmann::json::array({"PC1"});
  s.putObject(a);

  StoredObject b = newStoredObject("vortex", "B");
  b.facets["machines"] = nlohmann::json::array({"PC2"});
  s.putObject(b);

  StoredObject c = newStoredObject("vortex", "C");   // empty list -> match all
  c.facets["machines"] = nlohmann::json::array();
  s.putObject(c);

  StoredObject d = newStoredObject("vortex", "D");   // absent key -> match all
  s.putObject(d);

  std::set<std::string> names;
  for (auto& o : s.queryByFacet("vortex", "machines", "PC1")) names.insert(o.name);

  CHECK(names.count("A") == 1);
  CHECK(names.count("C") == 1);
  CHECK(names.count("D") == 1);
  CHECK(names.count("B") == 0);
}

TEST_CASE("meta + logs") {
  TempDb t;
  Store s = Store::open(t.path);

  s.setMeta("welcomed", "true");
  CHECK(s.getMeta("welcomed").value() == "true");
  CHECK_FALSE(s.getMeta("nope").has_value());

  LogEntry e;
  e.level = "info"; e.machine = "PC1"; e.instance = "v1";
  e.body["msg"] = "hi";
  s.appendLog(e);

  auto logs = s.recentLogs(10);
  REQUIRE(logs.size() == 1);
  CHECK(logs[0].instance == "v1");
  CHECK(logs[0].body.at("msg").get<std::string>() == "hi");
  CHECK(logs[0].ts > 0);  // stamped on append
}

TEST_CASE("rejects a non-vortch database") {
  TempDb t;
  {
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(t.path.string().c_str(), &db) == SQLITE_OK);
    sqlite3_exec(db, "PRAGMA application_id = 12345; CREATE TABLE x(a);",
                 nullptr, nullptr, nullptr);
    sqlite3_close(db);
  }
  CHECK_THROWS_AS(Store::open(t.path), std::runtime_error);
}
