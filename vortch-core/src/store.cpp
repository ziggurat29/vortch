#include "vortch/store.hpp"
#include "vortch/text.hpp"

#include <sqlite3.h>
#include <chrono>
#include <stdexcept>

namespace vortch {

std::int64_t nowUnix() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

namespace {

const char* const kSchemaSql = R"SQL(
CREATE TABLE IF NOT EXISTS objects (
  id       TEXT PRIMARY KEY,
  kind     TEXT NOT NULL,
  name     TEXT,
  facets   TEXT NOT NULL DEFAULT '{}',
  body     TEXT NOT NULL DEFAULT '{}',
  created  INTEGER NOT NULL,
  modified INTEGER NOT NULL
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS idx_objects_kind ON objects(kind);

CREATE TABLE IF NOT EXISTS logs (
  id       INTEGER PRIMARY KEY AUTOINCREMENT,
  ts       INTEGER NOT NULL,
  level    TEXT,
  machine  TEXT,
  user     TEXT,
  instance TEXT,
  body     TEXT
);
CREATE INDEX IF NOT EXISTS idx_logs_ts       ON logs(ts);
CREATE INDEX IF NOT EXISTS idx_logs_instance ON logs(instance);

CREATE TABLE IF NOT EXISTS meta  ( key TEXT PRIMARY KEY, value TEXT ) WITHOUT ROWID;
CREATE TABLE IF NOT EXISTS local ( key TEXT PRIMARY KEY, value TEXT ) WITHOUT ROWID;
)SQL";

void execSql(sqlite3* db, const char* sql) {
  char* err = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : "exec failed";
    sqlite3_free(err);
    throw std::runtime_error("sqlite exec: " + msg);
  }
}

// RAII prepared statement + small bind/column helpers.
struct Stmt {
  sqlite3*      db = nullptr;
  sqlite3_stmt* s  = nullptr;

  Stmt(sqlite3* d, const char* sql) : db(d) {
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK)
      throw std::runtime_error(std::string("sqlite prepare: ") + sqlite3_errmsg(db));
  }
  ~Stmt() { if (s) sqlite3_finalize(s); }
  Stmt(const Stmt&) = delete;
  Stmt& operator=(const Stmt&) = delete;

  void bindText(int i, const std::string& v) {
    sqlite3_bind_text(s, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
  }
  void bindInt64(int i, std::int64_t v) { sqlite3_bind_int64(s, i, v); }
  void bindText(const char* name, const std::string& v) {
    bindText(sqlite3_bind_parameter_index(s, name), v);
  }
  void bindInt64(const char* name, std::int64_t v) {
    bindInt64(sqlite3_bind_parameter_index(s, name), v);
  }

  bool step() {
    const int r = sqlite3_step(s);
    if (r == SQLITE_ROW)  return true;
    if (r == SQLITE_DONE) return false;
    throw std::runtime_error(std::string("sqlite step: ") + sqlite3_errmsg(db));
  }
  std::string  colText(int c)  {
    auto p = reinterpret_cast<const char*>(sqlite3_column_text(s, c));
    return p ? std::string(p) : std::string();
  }
  std::int64_t colInt64(int c) { return sqlite3_column_int64(s, c); }
};

nlohmann::json parseOrEmpty(const std::string& s) {
  return s.empty() ? nlohmann::json::object() : nlohmann::json::parse(s);
}

StoredObject readObject(Stmt& q) {
  StoredObject o;
  o.id       = q.colText(0);
  o.kind     = q.colText(1);
  o.name     = q.colText(2);
  o.facets   = parseOrEmpty(q.colText(3));
  o.body     = parseOrEmpty(q.colText(4));
  o.created  = q.colInt64(5);
  o.modified = q.colInt64(6);
  return o;
}

const char* const kObjCols =
    "id,kind,name,facets,body,created,modified";

} // namespace

Store Store::open(const std::filesystem::path& dbPath) {
  sqlite3* db = nullptr;
  const std::string path = pathToUtf8(dbPath);  // sqlite wants UTF-8 filename
  if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
    std::string msg = db ? sqlite3_errmsg(db) : "open failed";
    if (db) sqlite3_close(db);
    throw std::runtime_error("sqlite open: " + msg);
  }
  sqlite3_busy_timeout(db, 3000);

  std::int32_t appId = 0;
  { Stmt q(db, "PRAGMA application_id;"); if (q.step()) appId = static_cast<std::int32_t>(q.colInt64(0)); }

  if (appId == 0) {
    // fresh / uninitialized file: stamp the invariants (derive from the
    // constants so the decimal can never diverge from kApplicationId).
    execSql(db, ("PRAGMA application_id = " + std::to_string(kApplicationId) + ";").c_str());
    execSql(db, ("PRAGMA user_version = "   + std::to_string(kSchemaVersion) + ";").c_str());
  } else if (appId != kApplicationId) {
    sqlite3_close(db);
    throw std::runtime_error("not a vortch database (application_id mismatch)");
  }

  execSql(db, kSchemaSql);  // idempotent
  return Store(db);
}

Store::Store(Store&& o) noexcept : db_(o.db_) { o.db_ = nullptr; }
Store& Store::operator=(Store&& o) noexcept {
  if (this != &o) {
    if (db_) sqlite3_close(db_);
    db_ = o.db_;
    o.db_ = nullptr;
  }
  return *this;
}
Store::~Store() { if (db_) sqlite3_close(db_); }

void Store::putObject(const StoredObject& o) {
  Stmt s(db_,
    "INSERT INTO objects(id,kind,name,facets,body,created,modified) "
    "VALUES(:id,:kind,:name,:facets,:body,:created,:modified) "
    "ON CONFLICT(id) DO UPDATE SET "
    "kind=:kind, name=:name, facets=:facets, body=:body, modified=:modified;");
  s.bindText(":id", o.id);
  s.bindText(":kind", o.kind);
  s.bindText(":name", o.name);
  s.bindText(":facets", o.facets.dump());
  s.bindText(":body", o.body.dump());
  s.bindInt64(":created", o.created);
  s.bindInt64(":modified", o.modified);
  s.step();
}

std::optional<StoredObject> Store::getObject(const std::string& id) {
  Stmt s(db_, (std::string("SELECT ") + kObjCols + " FROM objects WHERE id=?;").c_str());
  s.bindText(1, id);
  if (!s.step()) return std::nullopt;
  return readObject(s);
}

bool Store::removeObject(const std::string& id) {
  Stmt s(db_, "DELETE FROM objects WHERE id=?;");
  s.bindText(1, id);
  s.step();
  return sqlite3_changes(db_) > 0;
}

std::vector<StoredObject> Store::listByKind(const std::string& kind) {
  Stmt s(db_, (std::string("SELECT ") + kObjCols + " FROM objects WHERE kind=?;").c_str());
  s.bindText(1, kind);
  std::vector<StoredObject> out;
  while (s.step()) out.push_back(readObject(s));
  return out;
}

std::vector<StoredObject> Store::queryByFacet(const std::string& kind,
                                              const std::string& category,
                                              const std::string& value) {
  const std::string path = "$." + category;
  Stmt s(db_, (std::string("SELECT ") + kObjCols + " FROM objects WHERE kind=:kind AND ("
    "  json_extract(facets, :path) IS NULL"
    "  OR json_array_length(facets, :path) = 0"
    "  OR EXISTS (SELECT 1 FROM json_each(facets, :path) WHERE value = :val)"
    ");").c_str());
  s.bindText(":kind", kind);
  s.bindText(":path", path);
  s.bindText(":val", value);
  std::vector<StoredObject> out;
  while (s.step()) out.push_back(readObject(s));
  return out;
}

void Store::setMeta(const std::string& key, const std::string& value) {
  Stmt s(db_, "INSERT INTO meta(key,value) VALUES(?,?) "
              "ON CONFLICT(key) DO UPDATE SET value=excluded.value;");
  s.bindText(1, key);
  s.bindText(2, value);
  s.step();
}

std::optional<std::string> Store::getMeta(const std::string& key) {
  Stmt s(db_, "SELECT value FROM meta WHERE key=?;");
  s.bindText(1, key);
  if (!s.step()) return std::nullopt;
  return s.colText(0);
}

std::int64_t Store::appendLog(const LogEntry& e) {
  Stmt s(db_, "INSERT INTO logs(ts,level,machine,user,instance,body) "
              "VALUES(?,?,?,?,?,?);");
  s.bindInt64(1, e.ts ? e.ts : nowUnix());
  s.bindText(2, e.level);
  s.bindText(3, e.machine);
  s.bindText(4, e.user);
  s.bindText(5, e.instance);
  s.bindText(6, e.body.dump());
  s.step();
  return sqlite3_last_insert_rowid(db_);
}

std::vector<LogEntry> Store::recentLogs(int limit) {
  Stmt s(db_, "SELECT id,ts,level,machine,user,instance,body FROM logs "
              "ORDER BY id DESC LIMIT ?;");
  s.bindInt64(1, limit);
  std::vector<LogEntry> out;
  while (s.step()) {
    LogEntry e;
    e.id       = s.colInt64(0);
    e.ts       = s.colInt64(1);
    e.level    = s.colText(2);
    e.machine  = s.colText(3);
    e.user     = s.colText(4);
    e.instance = s.colText(5);
    e.body     = parseOrEmpty(s.colText(6));
    out.push_back(std::move(e));
  }
  return out;
}

} // namespace vortch
