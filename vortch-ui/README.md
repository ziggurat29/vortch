# vortch-ui (stub)

Single **wxWidgets** cross-platform front end (Tier C: borderless, always-on-top,
desktop-icon-style window). Links `vortch-core`. Replaces the earlier
per-OS `vortch-win` / `vortch-mac` / `vortch-linux` split — wxWidgets wraps real
native controls behind one API, so the UI is one shared codebase.

Planned responsibilities (per `docs/design.md`):
- Borderless, always-on-top `wxFrame` styled as a desktop icon.
- `wxDropTarget` — multi-format drag-drop, one implementation for all OSes;
  double-click opens the "data intake" panel.
- `wxMenu` context menu.
- Icon + overlay-badge painting via `wxGraphicsContext` / `wxDC` (badge baked
  into the bitmap; do NOT rely on the saturated system icon-overlay slots).
- Self-install / autostart and other per-OS bits via conditional compilation:
  - `platform_win.cpp`  — Run key / Startup shortcut, `%APPDATA%` config
  - `platform_mac.mm`   — Login Items (`SMAppService`), `~/Library/Application Support`
  - `platform_linux.cpp`— `~/.config/autostart/*.desktop`, XDG config dir

Toolkit: **wxWidgets 3.2.x** (wxWindows Licence — LGPL + static-linking exception).

> Status: directory reserved; no code yet. Build system + code deferred pending
> the item-2 discussion (dependency acquisition, process model, JSON lib).
