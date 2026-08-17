#pragma once

#include <string>
#include <vector>
#include <optional>

namespace vortch {

// The launch-identity contract: a dispatched processor is told only WHICH
// vortch launched it (its GUID); it then looks up parameters via vortch-core.
// The command line never carries secrets -- only the identifier.
inline constexpr const char* kIdFlag = "--vortch-id";

inline std::vector<std::string> buildLaunchArgs(const std::string& id) {
  return { kIdFlag, id };
}

inline std::optional<std::string> parseVortchId(const std::vector<std::string>& args) {
  for (std::size_t i = 0; i + 1 < args.size(); ++i)
    if (args[i] == kIdFlag) return args[i + 1];
  return std::nullopt;
}

inline std::optional<std::string> parseVortchId(int argc, const char* const argv[]) {
  std::vector<std::string> a;
  a.reserve(argc);
  for (int i = 0; i < argc; ++i) a.emplace_back(argv[i]);
  return parseVortchId(a);
}

} // namespace vortch
