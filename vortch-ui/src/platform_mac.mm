#include "platform.hpp"

namespace vortch {

// TODO: register a Login Item via SMAppService
bool installAutostart(const std::string& /*appPath*/) { return false; }
bool uninstallAutostart() { return false; }

} // namespace vortch
