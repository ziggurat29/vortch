#include "platform.hpp"

#include <gtk/gtk.h>

namespace vortch {

// TODO: write ~/.config/autostart/vortch.desktop
bool installAutostart(const std::string& /*command*/) { return false; }
bool uninstallAutostart() { return false; }

// The gadget is a shaped, borderless, always-on-top window. Marco's compositor
// draws a drop-shadow around a normal top-level's full geometry, which leaks past
// the shaped-down icon as "fog." A DOCK type hint marks it as a panel/dock, which
// the compositor does not shadow (UTILITY still gets a shadow). Must be set before
// the window is mapped (called at construction, before Show()).
void styleGadgetWindow(void* nativeHandle) {
  GtkWidget* w = static_cast<GtkWidget*>(nativeHandle);
  if (!w || !GTK_IS_WINDOW(w)) return;
  gtk_window_set_type_hint(GTK_WINDOW(w), GDK_WINDOW_TYPE_HINT_DOCK);
}

} // namespace vortch
