#pragma once

// The single SQLite store (config + logs + meta + local), per the design doc.
// Portable: opened next to the exe or at --data-dir. Header carries the header
// pragmas as invariants; schema is CREATE-IF-NOT-EXISTS (no migrations pre-1.0).

#include "identity.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

struct sqlite3;  // fwd — keep sqlite out of the public header

namespace vortch {

// Unix time (seconds).
std::int64_t nowUnix();

// A row in the polymorphic `objects` table.
struct StoredObject {
  std::string    id;                                  // GUID (replication-safe)
  std::string    kind;                                // discriminator ('config',…)
  std::string    name;                                // presentation label
  nlohmann::json facets = nlohmann::json::object();   // extrinsic selection props
  nlohmann::json body   = nlohmann::json::object();   // kind-specific data
  std::int64_t   created  = 0;
  std::int64_t   modified = 0;
};

// A row in the specialized `logs` table (machine-local; int rowid is fine).
struct LogEntry {
  std::int64_t   id = 0;      // rowid (assigned by store)
  std::int64_t   ts = 0;      // unix seconds (0 -> now on append)
  std::string    level;
  std::string    machine;
  std::string    user;
  std::string    instance;    // vortex id
  nlohmann::json body = nlohmann::json::object();
};

inline StoredObject newStoredObject(std::string kind, std::string name) {
  StoredObject o;
  o.id       = newUuid();
  o.kind     = std::move(kind);
  o.name     = std::move(name);
  o.created  = o.modified = nowUnix();
  return o;
}

class Store {
public:
  static constexpr std::int32_t kApplicationId = 0x564F5254;  // "VORT"
  static constexpr std::int32_t kSchemaVersion = 1;

  // Open (create file + schema if needed). Throws std::runtime_error on failure
  // or if an existing file is not a vortch DB (application_id mismatch).
  static Store open(const std::filesystem::path& dbPath);

  Store(Store&&) noexcept;
  Store& operator=(Store&&) noexcept;
  ~Store();
  Store(const Store&) = delete;
  Store& operator=(const Store&) = delete;

  // objects
  void putObject(const StoredObject& obj);            // insert or replace by id
  std::optional<StoredObject> getObject(const std::string& id);
  bool removeObject(const std::string& id);
  std::vector<StoredObject> listByKind(const std::string& kind);
  // objects of `kind` where facets.<category> is absent/empty (match-all) OR
  // contains `value` (restriction-style facet).
  std::vector<StoredObject> queryByFacet(const std::string& kind,
                                         const std::string& category,
                                         const std::string& value);

  // meta (extensible key/value)
  void setMeta(const std::string& key, const std::string& value);
  std::optional<std::string> getMeta(const std::string& key);

  // logs
  std::int64_t appendLog(const LogEntry& e);          // returns new rowid
  std::vector<LogEntry> recentLogs(int limit = 100);

private:
  explicit Store(sqlite3* db) : db_(db) {}
  sqlite3* db_ = nullptr;
};

} // namespace vortch
