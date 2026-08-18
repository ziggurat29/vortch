# Building vortch

## Prerequisites
- **Windows**: Visual Studio 2022 (MSVC + the bundled CMake/Ninja). Build from a
  shell that has run `vcvars64.bat`, or use the `scripts/` helpers.
- **vcpkg**: cloned + bootstrapped; set `VCPKG_ROOT` to its path.
- Dependencies (fetched by vcpkg via `vcpkg.json`): nlohmann-json, doctest;
  `history` feature -> sqlite3; `ui` feature -> wxWidgets.

## Presets
- `core`        - portable `vortch-core` (incl. the SQLite Store) + unit tests. No UI.
- `full`        - everything incl. the wxWidgets `vortch-ui`. First build is
  slow (wxWidgets compiles from source).
- `full-static` - like `full` but `x64-windows-static` triplet + static CRT
  (Release). Produces a single self-contained `vortch.exe` (0 DLLs; imports only
  Windows system DLLs). Rebuilds all deps static (slow first time).

## Configure / build / test (from a vcvars shell, VCPKG_ROOT set)
```
cmake --preset core
cmake --build --preset core
ctest --preset core --output-on-failure   # or: ctest --test-dir build/core
```
Swap `core` for `core-sqlite` or `full` as needed.

## Verified
Built + tested on Windows with VS2022 Community (MSVC 19.44), CMake 3.31, Ninja
1.12, vcpkg at `C:\Users\lemleyd\vcpkg`. `core` tests pass; `full` builds
`vortch.exe` (wxWidgets 3.3.3) which launches and renders `assets/icon.svg`.
`scripts/build.bat` hardcodes the local VS/vcpkg paths — adjust per machine.
`full-static` verified: ~4.6 MB single `vortch.exe`, no DLLs, system-only imports.

## Linux (Ubuntu MATE 22.04)
Install toolchain + the system dev libs vcpkg's wxWidgets needs, then build:
```
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config git \
  curl zip unzip tar autoconf automake libtool \
  libgtk-3-dev libgl1-mesa-dev libglu1-mesa-dev \
  libx11-dev libxrandr-dev libxi-dev libxxf86vm-dev

git clone https://github.com/microsoft/vcpkg "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_ROOT="$HOME/vcpkg"

bash scripts/build.sh core     # fast: core + tests
bash scripts/build.sh full     # slow first time: builds wxWidgets (GTK)
```
No `vcvars` needed on Linux; `scripts/build.sh` drives cmake/ctest directly.

**Current Linux status (spot-check expectations).** The portable core + Store are
fully cross-platform. In the UI, these are **Windows-only today** (no-ops/stubs
elsewhere, guarded by `#ifdef __WXMSW__`): the z-order toggle (topmost / below-apps),
the non-activating window (`WS_EX_NOACTIVATE`), the context-menu foreground handling,
and autostart (`--enable/--disable-autostart` return false; `platform_linux.cpp` is
a stub). So on Linux expect: widgets render, and drag-drop + move + peek work, but
the z-mode menu items do nothing yet and autostart isn't wired. The **system tray**
(`wxTaskBarIcon`) depends on the desktop's notification area — MATE's should work,
but if it doesn't appear that's a known DE-dependent wx limitation, not a vortch bug.
Build `core` first (fast — proves the Store compiles + tests pass under gcc), then
`full`. `VORTCH_ASSETS_DIR` is baked to the repo's `assets/` at build time, so the
dev build runs from anywhere.

## Notes
- The wxWidgets (`full`) build can take many minutes the first time; run it
  detached so it is not interrupted.
- All build output goes under `build/` (git-ignored).
