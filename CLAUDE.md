# vortch — project notes

## Build environment
- **UI toolkit**: **wxWidgets** (3.3.x; vcpkg installs 3.3.3) is the chosen cross-platform GUI toolkit
  for the front end (`vortch-ui`). One shared UI codebase builds native controls
  on Windows/macOS/Linux; per-OS integration (autostart, badge) is via
  conditional compilation. License: wxWindows Licence (LGPL + static-linking
  exception). See `docs/design.md`.
- **Native Windows builds**: existing **MSVC** installation is the primary
  Windows toolchain. (Note: `cl.exe` is typically only on PATH inside a
  Developer Command Prompt / after `vcvars`, not a bare shell.)
- **Linux target / cross-builds**: **Docker Desktop for Windows** is installed
  and is the intended way to build/test with gcc/clang for eventual Linux
  support (containerized native-Linux builds, not a Windows cross-toolchain).
- **macOS target**: NOT covered by Docker. Cross-compiling to real macOS needs
  an Apple SDK (e.g. osxcross) or an actual Mac / CI runner — treat as a
  separate, later concern; do not assume it's buildable locally.

## Conventions
- **Text**: UTF-8 everywhere. Every `std::string` is UTF-8; do NOT use
  `char8_t`/`std::u8string`. Convert only at OS boundaries via `vortch/text.hpp`
  (Win32 wide APIs / filesystem paths on Windows) and `wxString::FromUTF8`.
- **Windows floor**: Windows 7 (`_WIN32_WINNT=0x0601`). XP is out of scope.
- **DB migrations (pre-release)**: do NOT write migrations yet — schema changes
  are free; wipe-and-reinstall the SQLite DB to adopt them. Real migrations only
  after the first public release.

## Line endings (LF, enforced)
All tracked text is **LF** in both the object store and the working tree.
`.gitattributes` owns the policy — do NOT rely on `core.autocrlf`.

- **Every generator/writer must emit `\n`.** (Python text mode defaults to
  `newline=None`, turning `\n` into `\r\n` on Windows — pass `newline="\n"`.)
- On this NTFS/MSYS setup a mass rewrite can leave **phantom ` M` entries** in
  `git status`. Trust the diff: `git diff --numstat` / `git diff --stat HEAD`
  show real content changes; if they report nothing, nothing changed.

## Python environment
- Python 3.12 virtual environment lives in a **sibling** directory `../venv`
  (i.e. `kooky006/venv`, alongside this repo — deliberately OUTSIDE the git repo).
- The venv is already on PATH. Invoke the interpreter as plain `python`
  (never a full/explicit path, and do not re-create or re-activate it).
