#pragma once

#include "settings.hpp"
#include "instance.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdexcept>

namespace vortch {

// The single self-contained config "package" (roamable JSON).
struct ConfigDocument {
  int                         schemaVersion = 1;
  Settings                    settings;
  std::vector<InstanceConfig> instances;
};

// ---- JSON (de)serialization (ADL for nlohmann) ----

inline void to_json(nlohmann::json& j, const Point& p) { j = { {"x", p.x}, {"y", p.y} }; }
inline void from_json(const nlohmann::json& j, Point& p) { p.x = j.value("x", 0); p.y = j.value("y", 0); }

inline void to_json(nlohmann::json& j, const Size& s) { j = { {"w", s.w}, {"h", s.h} }; }
inline void from_json(const nlohmann::json& j, Size& s) { s.w = j.value("w", 64); s.h = j.value("h", 64); }

inline void to_json(nlohmann::json& j, const Settings& s) {
  j = { {"collapsed", s.collapsed}, {"autostart", s.autostart},
        {"donePulseMs", s.donePulseMs}, {"errorAutoClear", s.errorAutoClear} };
}
inline void from_json(const nlohmann::json& j, Settings& s) {
  s.collapsed      = j.value("collapsed", false);
  s.autostart      = j.value("autostart", true);
  s.donePulseMs    = j.value("donePulseMs", 1500);
  s.errorAutoClear = j.value("errorAutoClear", false);
}

inline void to_json(nlohmann::json& j, const InstanceConfig& c) {
  j = nlohmann::json::object();
  j["id"]         = c.id;
  j["label"]      = c.label;
  j["icon"]       = toString(c.icon);
  if (!c.iconHint.empty()) j["iconHint"] = c.iconHint;
  j["position"]   = c.position;
  j["monitor"]    = c.monitor;
  j["size"]       = c.size;
  j["visualMode"] = toString(c.visualMode);
  j["state"]      = toString(c.defaultState);
  j["params"]     = c.params;
}
inline void from_json(const nlohmann::json& j, InstanceConfig& c) {
  c.id       = j.value("id", std::string{});
  c.label    = j.value("label", std::string{});
  c.icon     = parseResourceRef(j.value("icon", std::string("vortch:builtin/icon")));
  c.iconHint = j.value("iconHint", std::string{});
  if (j.contains("position")) c.position = j.at("position").get<Point>();
  c.monitor  = j.value("monitor", std::string{});
  if (j.contains("size")) c.size = j.at("size").get<Size>();
  c.visualMode = visualModeFromString(j.value("visualMode", std::string("collapsed")));
  auto st = statusFromString(j.value("state", std::string("quiescent")));
  c.defaultState = st ? *st : StatusKind::Quiescent;
  c.params = j.contains("params") ? j.at("params") : nlohmann::json::object();
}

inline void to_json(nlohmann::json& j, const ConfigDocument& d) {
  j = { {"schemaVersion", d.schemaVersion}, {"settings", d.settings}, {"instances", d.instances} };
}
inline void from_json(const nlohmann::json& j, ConfigDocument& d) {
  d.schemaVersion = j.value("schemaVersion", 1);
  d.settings = j.contains("settings") ? j.at("settings").get<Settings>() : Settings{};
  d.instances.clear();
  if (j.contains("instances"))
    for (const auto& e : j.at("instances")) d.instances.push_back(e.get<InstanceConfig>());
}

inline std::string toJsonString(const ConfigDocument& d, int indent = 2) {
  nlohmann::json j = d;
  return j.dump(indent);
}
inline ConfigDocument fromJsonString(const std::string& s) {
  return nlohmann::json::parse(s).get<ConfigDocument>();
}

// Atomic save: write to a temp file then rename over the target.
inline void save(const ConfigDocument& d, const std::filesystem::path& path) {
  std::filesystem::path tmp = path;
  tmp += ".tmp";
  {
    std::ofstream os(tmp, std::ios::binary | std::ios::trunc);
    if (!os) throw std::runtime_error("cannot open for write: " + tmp.string());
    os << toJsonString(d);
  }
  std::filesystem::rename(tmp, path);
}
inline ConfigDocument load(const std::filesystem::path& path) {
  std::ifstream is(path, std::ios::binary);
  if (!is) throw std::runtime_error("cannot open for read: " + path.string());
  std::stringstream ss;
  ss << is.rdbuf();
  return fromJsonString(ss.str());
}

} // namespace vortch
