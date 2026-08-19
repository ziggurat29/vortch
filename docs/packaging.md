# Packaging & distribution

How vortch is packaged for end users on each OS, and why. The guiding goal is a
**portable "just works" artifact** — ideally a single self-contained binary the
user can run without installing dependencies or building from source. That goal
is fully reachable on Windows, largely reachable on Linux, and reachable on
macOS with a signing/notarization pipeline.

This doc is about *distribution*. For how to *build*, see `BUILD.md`.

## What links static vs. dynamic

vortch's own deps (wxWidgets, sqlite3, nlohmann-json) come from vcpkg. On the
default per-OS triplets, vcpkg builds these **static**, so they are embedded in
the binary — there are no vcpkg `.dll`/`.so`/`.dylib` files to ship.

The variable across platforms is the **GUI toolkit backend** and the **system
libraries** underneath it:

| Platform | GUI backend | Embedded (static) | Remaining dynamic deps |
|---|---|---|---|
| Windows | native Win32 | wx, sqlite, +static CRT (`full-static`) | Windows system DLLs only |
| Linux   | wxGTK (GTK3) | wx **and the entire GTK stack** (see below) | ubiquitous system libs only |
| macOS   | wxOSX (Cocoa) | wx, sqlite | system frameworks only |

## Linux

### Static-linking reality (better than it first looks)

vcpkg's `x64-linux` triplet builds **everything** static — including the whole
GTK stack. A `full` build's `vortch` binary statically embeds GTK, glib/gobject/
gio, cairo, pango, gdk-pixbuf, fontconfig, freetype, and harfbuzz. Verified via
`ldd`: the binary's only dynamic dependencies are **ubiquitous system
libraries** — glibc, libstdc++, the X11/xcb family, dbus, xkbcommon, uuid. No
wx, sqlite, or GTK `.so` needs to ship.

So the "single portable binary" story is largely intact on Linux. The battle is
*not* library linkage.

### The real caveats

1. **`dlopen`'d GTK runtime modules.** A statically-linked GTK can still try to
   load, at runtime, modules that live outside the binary: gdk-pixbuf image
   loaders, input-method (IM) modules, print backends, theme engines. These are
   the actual gap between "static linkage" and "truly portable," and must be
   tested on a *clean* machine (not a dev box that has GTK installed).
   - Favorable for vortch: it renders `assets/icon.svg` via wxWidgets' built-in
     nanosvg, **not** gdk-pixbuf loaders — so the most common `dlopen` trap may
     not be on our path. Verify, don't assume.
2. **glibc baseline.** A binary linked against a newer glibc will not start on a
   system with an older one. The fix is not more builds — it's building **once
   on the oldest glibc you intend to support** (build old, run everywhere
   newer). Use an old-baseline container (e.g. Ubuntu 20.04 / glibc 2.31, or a
   manylinux-style image).
3. **Build type & size.** The dev `full` preset is **Debug** and unstripped
   (~172 MB). A **Release + `strip`** build is the portable artifact:
   - Debug, unstripped: ~172 MB (all deps' debug symbols fused in — not a
     shipping artifact)
   - Release, unstripped: ~34 MB
   - Release, stripped: **~29 MB** _(measured on this VM: x86-64, GTK3)_

   Configure a release build with `-DCMAKE_BUILD_TYPE=Release` (see BUILD.md);
   `strip` the resulting binary.

### Packaging formats & how many builds

The count is small once the format and baseline are chosen well:

| Format | Builds needed | Reach | Notes |
|---|---|---|---|
| **AppImage** (recommended) | 1 per arch | very broad | Closest to the Windows single-file feel: one downloadable file, no install. Build on an old-glibc base; the wrapper also carries any `dlopen`'d GTK modules so they're guaranteed present. |
| **Flatpak** | 1 | broad + store | Ships a GTK runtime, sandboxed; arch handled by the build service. Good for discoverability. |
| Native `.deb`/`.rpm`/… | 6–10+ | best integration | Ubuntu LTSs × Fedora × openSUSE × arch… — high maintenance. Defer / community-maintained. |

**Architectures:** x86-64 is ~95%+ of desktop Linux; arm64 is the only other
one worth considering. Start with **one AppImage (x86-64)**; add arm64 and/or a
Flatpak later if demand warrants.

**Recommendation:** Release + stripped, built on an old-glibc base, wrapped as an
**AppImage**. That is the Linux analog of the Windows single-exe.

## macOS

The walled garden removes the fragmentation problems: **no GTK, no glibc, no
distro matrix.** wxWidgets uses the native **Cocoa** backend, so the GTK
`dlopen`-module issue simply does not exist; link the always-present system
frameworks and bundle any vcpkg deps into the `.app`.

- **Architectures:** build **one universal binary** (arm64 + x86-64 fused) — a
  single `.app` covers both Apple Silicon and Intel. No separate downloads.
- **OS floor:** set a deployment target (e.g. macOS 11) against one SDK — same
  "build old, run newer" idea, cleaner than glibc.
- **The real cost is signing + notarization,** not build count. To launch
  without Gatekeeper warnings you need an Apple Developer ID cert and Apple's
  notarization step, then ship a `.dmg`. This also needs an actual Mac or CI
  runner (see CLAUDE.md — macOS is not locally buildable on the current setup).

### Local build/test box vs. release pipeline

Separate two roles: *does it even build/run on macOS* (cheap, local) from
*produce the shippable release artifact* (universal + notarized; needs modern
tooling).

- **Local Cocoa smoke-test box** — an **old Intel Mac is enough** for this role,
  and it's the piece Docker/Linux cannot cover. Example: a 2011 Mac mini
  (`Macmini5,x`, Sandy Bridge, x86-64). It compiles the wxWidgets **Cocoa** path
  and runs the app, so it shakes out macOS-specific build issues fast without
  cloud latency. Ceilings: **Intel/x86-64 only** (no native arm64, no universal
  binary); stock OS caps at **macOS 10.13 High Sierra** → ~**Xcode 10** (fine for
  C++17/CMake/vcpkg/wx 3.3 to compile); and **notarization is out of reach on
  stock High Sierra** because Apple's `notarytool` needs macOS 11+. An x86-64
  `.app` from it still *runs everywhere* — natively on Intel, via **Rosetta 2**
  on Apple Silicon. **OpenCore Legacy Patcher** can put Monterey/Ventura on such
  a machine (unofficial) to unlock modern Xcode + `notarytool`, at the cost of
  support and speed. An SSD + RAM bump makes from-source wx builds tolerable.
- **Release pipeline** — for the **universal (arm64+x86-64), signed, notarized**
  `.app`, use an **Apple Silicon Mac** or a **cloud/CI macOS runner** (GitHub
  Actions' macOS runners are the cheap path; free tier for public repos, modern
  enough for universal builds + notarization).

**Result:** essentially **one artifact** — a signed, notarized universal `.app`
in a `.dmg`. Complexity is the pipeline, not a matrix. A legacy Intel Mac covers
the *build/test* half locally; the *release* half is a modern-Mac/CI concern.

## Windows

The `full-static` preset (`x64-windows-static` triplet + static CRT, Release)
already produces the target: a single self-contained `vortch.exe` — 0 DLLs,
imports only Windows system DLLs (~4.6 MB, verified). This is the reference for
what "portable single binary" means, and the model the other platforms aim at.

## Recommended artifact matrix (summary)

| OS | Artifact(s) | Count | Main effort |
|---|---|---|---|
| Windows | self-contained `vortch.exe` (`full-static`) | 1 | already done |
| Linux | AppImage x86-64 (+ arm64 / Flatpak optional) | 1–2 | old-glibc build env; test `dlopen` modules |
| macOS | universal `.app` in signed/notarized `.dmg` | 1 | signing/notarization pipeline; Mac/CI |

The engineering is front-loaded into **build environments** (an old-glibc Linux
container; a macOS signing pipeline) — not into a large number of binaries.

## Appendix: AppImage recipe (Linux) — sketch

A starting recipe for the recommended Linux artifact. Treat as a sketch to
harden, not a finished CI script.

### 0. Assets are embedded (already done)

Assets (`assets/*.svg`) are **compiled into the binary as byte arrays** at build
time — see `vortch-ui/cmake/embed_assets.cmake` and the generated
`embedded_assets.{hpp,cpp}`; `main.cpp` loads them from memory via
`vortch::embedded_asset()` + `wxBitmapBundle::FromSVG(data, len, …)`. There is no
external asset file and no baked absolute path, so the binary is self-contained
and relocatable on every platform. The embedding is format-agnostic (raw bytes),
so raster assets later need no change to the mechanism.

Consequence for packaging: **the AppDir does not need to carry an `assets/`
directory**, and AppRun needs no asset-path environment variable. (This replaces
the earlier baked-`VORTCH_ASSETS_DIR` scheme, which was not relocatable.)

### 1. Build (Release, old-glibc base)

Build in an **old-glibc container** (e.g. Ubuntu 20.04 / glibc 2.31) so the
result runs on older systems — see the glibc note above. Release + `strip`
(~29 MB here). vcpkg still links wx + the GTK stack statically, so the AppDir
does not need to carry those `.so`s.

### 2. Assemble the AppDir

```
AppDir/
  usr/bin/vortch                      # the stripped Release binary (assets embedded)
  usr/share/applications/vortch.desktop
  usr/share/icons/hicolor/scalable/apps/vortch.svg   # launcher/thumbnail icon (copy of assets/icon.svg)
  AppRun                              # entry script (below)
```

The runtime assets are embedded in the binary, so none ship in the AppDir. The
one SVG above is only the **launcher icon** the desktop/AppImage shows for the
app — unrelated to what the program draws.

`vortch.desktop`:
```
[Desktop Entry]
Type=Application
Name=vortch
Exec=vortch
Icon=vortch
Categories=Utility;
```

`AppRun` (trivial — assets are in the binary, so nothing to export):
```
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/bin/vortch" "$@"
```

### 3. Bundle + produce the .AppImage

```
# tools (download once): linuxdeploy, linuxdeploy-plugin-gtk, appimagetool
linuxdeploy --appdir AppDir \
  --executable AppDir/usr/bin/vortch \
  --desktop-file AppDir/usr/share/applications/vortch.desktop \
  --icon-file AppDir/usr/share/icons/hicolor/scalable/apps/vortch.svg \
  --plugin gtk \
  --output appimage
```

**Static-GTK wrinkle:** `linuxdeploy-plugin-gtk` assumes a *dynamic* GTK and
bundles `libgtk` + friends. Our binary already embeds them, so the plugin's main
remaining value is the **runtime pieces** GTK `dlopen`s or reads: gdk-pixbuf
loaders, GTK IM modules, gsettings schemas, theme/icon data. If a clean-machine
test shows none are actually needed (plausible: vortch draws SVG via wx's
built-in nanosvg, not gdk-pixbuf loaders), you can **drop `--plugin gtk`** and
ship a near-trivial AppImage that is just the binary + assets + AppRun.

### 4. Test on a clean target

Validate on a machine/container **without** GTK or dev libs installed (not a dev
box). Confirm the window renders, the icon loads, drag-drop/move/peek work, and
watch stderr for missing-module warnings (`Gtk-WARNING`, gdk-pixbuf loader
errors). What surfaces there decides whether step 3 needs the gtk plugin.

### Arch & scaling

Build one AppImage per arch: **x86-64** first, **arm64** if demand warrants
(same recipe on an arm64 old-glibc base). Optionally add a Flatpak later.
