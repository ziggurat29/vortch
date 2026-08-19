#include "platform.hpp"

#include <gtk/gtk.h>

namespace vortch {

// TODO: write ~/.config/autostart/vortch.desktop
bool installAutostart(const std::string& /*command*/) { return false; }
bool uninstallAutostart() { return false; }

// Desktop-gadget window styling; the X11 analog of the Win32 style bits.
//   * DOCK type hint — the gadget is a shaped, borderless, always-on-top window;
//     Marco's compositor draws a drop-shadow around a normal top-level's full
//     geometry, which leaks past the shaped-down icon as "fog." DOCK marks it as a
//     panel/dock, which the compositor does not shadow (UTILITY still gets one).
//   * accept-focus / focus-on-map FALSE — the X11 analog of WS_EX_NOACTIVATE:
//     interacting with the gadget (or its context menu) must not steal input focus
//     from the window the user was working in. The gadget needs no keyboard focus
//     itself (move-mode reads global key state; drags/menus use pointer grabs).
// All of these must be set before the window is mapped (called before Show()).
void styleGadgetWindow(void* nativeHandle) {
  GtkWidget* w = static_cast<GtkWidget*>(nativeHandle);
  if (!w || !GTK_IS_WINDOW(w)) return;
  GtkWindow* win = GTK_WINDOW(w);
  gtk_window_set_type_hint(win, GDK_WINDOW_TYPE_HINT_DOCK);
  gtk_window_set_accept_focus(win, FALSE);
  gtk_window_set_focus_on_map(win, FALSE);
}

} // namespace vortch
