# Building vortch

## Prerequisites
- **Windows**: Visual Studio 2022 (MSVC + the bundled CMake/Ninja). Build from a
  shell that has run `vcvars64.bat`, or use the `scripts/` helpers.
- **vcpkg**: cloned + bootstrapped; set `VCPKG_ROOT` to its path.
- Dependencies (fetched by vcpkg via `vcpkg.json`): nlohmann-json, doctest;
  `history` feature -> sqlite3; `ui` feature -> wxWidgets.

## Presets
- `core`        - portable `vortch-core` + unit tests. No UI, no sqlite. Fast.
- `core-sqlite` - core + the SQLite history store.
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

## Notes
- The wxWidgets (`full`) build can take many minutes the first time; run it
  detached so it is not interrupted.
- All build output goes under `build/` (git-ignored).
