#pragma once

#include <string>

namespace vortch {

// Per-OS integration seam (implemented in platform_{win,mac,linux}).
// Autostart: launch vortch (no per-instance args) so it recreates the user's
// instances on login.
bool installAutostart(const std::string& appPath);
bool uninstallAutostart();

} // namespace vortch
