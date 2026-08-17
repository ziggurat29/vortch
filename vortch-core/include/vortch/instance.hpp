#pragma once

#include "resource_ref.hpp"
#include "geometry.hpp"
#include "status.hpp"
#include <string>
#include <nlohmann/json.hpp>

namespace vortch {

enum class VisualMode { Collapsed, Expanded };

inline std::string toString(VisualMode m) {
  return m == VisualMode::Expanded ? "expanded" : "collapsed";
}
inline VisualMode visualModeFromString(const std::string& s) {
  return s == "expanded" ? VisualMode::Expanded : VisualMode::Collapsed;
}

// Persisted ("quiescent") per-instance config. Runtime/live status is separate
// (see InstanceStatus / StatusMachine) and is NOT stored here.
struct InstanceConfig {
  std::string    id;                                  // stable GUID
  std::string    label;                               // user-facing name
  ResourceRef    icon { ResourceScheme::Builtin, "icon" };
  std::string    iconHint;                            // optional source path
  Point          position;
  std::string    monitor;                             // stable monitor id
  Size           size { 64, 64 };
  VisualMode     visualMode   = VisualMode::Collapsed;
  StatusKind     defaultState = StatusKind::Quiescent;
  nlohmann::json params       = nlohmann::json::object();  // free-form, opaque
};

} // namespace vortch
