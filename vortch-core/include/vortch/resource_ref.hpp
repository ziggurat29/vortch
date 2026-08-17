#pragma once

#include <string>
#include <stdexcept>

namespace vortch {

// A small URL-esque scheme for icon/overlay resources:
//   vortch:builtin/<name>    bundled asset
//   vortch:embedded/<id>     per-instance image bytes embedded in config
//   file://<path>            external image
enum class ResourceScheme { Builtin, Embedded, File };

struct ResourceRef {
  ResourceScheme scheme = ResourceScheme::Builtin;
  std::string    value;  // builtin/embedded: name or id; file: path remainder
};

inline bool operator==(const ResourceRef& a, const ResourceRef& b) {
  return a.scheme == b.scheme && a.value == b.value;
}

inline ResourceRef parseResourceRef(const std::string& s) {
  static const std::string kBuiltin  = "vortch:builtin/";
  static const std::string kEmbedded = "vortch:embedded/";
  static const std::string kFile     = "file://";
  if (s.rfind(kBuiltin, 0) == 0)  return { ResourceScheme::Builtin,  s.substr(kBuiltin.size()) };
  if (s.rfind(kEmbedded, 0) == 0) return { ResourceScheme::Embedded, s.substr(kEmbedded.size()) };
  if (s.rfind(kFile, 0) == 0)     return { ResourceScheme::File,     s.substr(kFile.size()) };
  throw std::invalid_argument("unrecognized resource ref: " + s);
}

inline std::string toString(const ResourceRef& r) {
  switch (r.scheme) {
    case ResourceScheme::Builtin:  return "vortch:builtin/"  + r.value;
    case ResourceScheme::Embedded: return "vortch:embedded/" + r.value;
    case ResourceScheme::File:     return "file://"          + r.value;
  }
  return {};
}

} // namespace vortch
