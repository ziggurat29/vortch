#pragma once

namespace vortch {

// Global (per-install) UI/status settings; the "settings" object in the config.
struct Settings {
  bool collapsed      = false;  // reserved: collapse-to-tray mode
  bool autostart      = true;
  int  donePulseMs    = 1500;
  bool errorAutoClear = false;
};

} // namespace vortch
