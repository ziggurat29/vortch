#pragma once

#include <string>

namespace vortch {

// Per-OS integration seam (implemented in platform_{win,mac,linux}).
// Autostart: launch vortch (no per-instance args) so it recreates the user's
// instances on login.
bool installAutostart(const std::string& command);  // full login command line
bool uninstallAutostart();

// Apply per-OS "desktop gadget" styling to a native top-level window handle (the
// value from wxWindow::GetHandle()). On Linux this sets a GTK window type hint so
// the WM/compositor does not draw a drop-shadow around the (shaped) gadget — an
// oversized shadow leaks past the shaped-down icon. No-op where not needed.
void styleGadgetWindow(void* nativeHandle);

} // namespace vortch
