#include "platform.hpp"

namespace vortch {

// TODO: register a Login Item via SMAppService
bool installAutostart(const std::string& /*command*/) { return false; }
bool uninstallAutostart() { return false; }

void styleGadgetWindow(void* /*nativeHandle*/) {}

} // namespace vortch
