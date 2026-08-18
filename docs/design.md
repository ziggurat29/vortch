# "Vortch" / "Portal" Desktop Drop-Target — Design Notes

## Concept

**vortch** (aka "portal") is an active desktop icon that acts as a drop
target / launcher for a class of tools. It's a reusable UI metaphor, not tied
to any single project. Multiple vortch instances can exist on the desktop at
once, each with its own identity and parameters.

### Required behaviors

- **Drag-drop target**: the user drops one or more files onto the icon; vortch
  receives the list of fully-qualified filenames and dispatches them for
  processing. (The dispatch-by-type logic is a separate, downstream concern —
  not part of this design.)
- **Double-click** (i.e. the shell's context-menu *default action*) opens a
  richer UI — a compact "data intake" panel — with no files involved.
- **Custom context menu** on right-click.
- **Multiple instances**, each with its own persisted parameters (GUID, config
  values, etc.).
- **Launched-tool awareness**: when vortch launches a processing tool, the tool
  receives (e.g. via a command-line parameter) enough info to identify *which*
  vortch launched it, so it can look up that vortch's parameters.
- **Runtime-changeable icon** per instance (the user picks a distinctive icon
  per purpose).
- **Status overlay/badge** on the icon, if feasible.
- **Cross-platform aspiration**: Windows first, but Mac/Linux equivalents
  eventually. The aspiration is about **UX/packaging parity**, not shared UI
  code — the real cross-platform effort should go into the downstream
  processing pipeline, which is a separate concern from the front end.

### Hard constraints

- Prefer a **small, monolithic binary** — no heavy runtime, no bundled browser
  engine.
- Must support **self-install / self-uninstall** — the binary wires up its own
  shell integration (registry keys, autostart entries, config files) without a
  separate installer.
- **License**: keep it uncomplicated/permissive — LGPL-with-relinking is
  acceptable; no per-seat or royalty obligations.

---

## Conceptual model: Vortch as a job launcher / manager

Vortch is fundamentally a **UI metaphor for launching and managing jobs**; its
value is the *simplicity of launching*. The desktop icon is the launch surface
and an at-a-glance status display; the real work is done by external processors.

- A **drop** (typically a bundled list of filenames, possibly other data) is
  **classified** and **dispatched** to a **job processor**.
- Each dispatched **job** has a status/state. Vortch stays in contact with
  running jobs as they report progress.
- Multiple jobs may run concurrently, so Vortch **arbitrates/aggregates** the
  competing reports into a single **best-effort summary**, shown at-a-glance as
  the icon's overlay badge.
- **Detailed** state — the list of running jobs, per-job status, and control
  actions (pause/abort) — is presented by a **secondary UI**, reached via
  double-click, the context menu, or the tray icon (when the portal window is
  hidden).

### Processor protocol (processors are a SEPARATE project; core owns the surface)

Job processors live in their own project and **register** with Vortch over a
protocol. `vortch-core` owns the protocol surface; the processors themselves are
out of scope here. The protocol provides:

- **Registration** — a processor advertises itself and its capabilities.
- **Match criteria** — processor-declared rules Vortch uses to classify dropped
  items and map them to a suitable processor.
- **Dispatch** — Vortch hands the matched items to the processor, tagged with
  the originating vortch identity (GUID).
- **Status reporting** — the processor reports progress; Vortch post-processes /
  aggregates these into the instance's displayed disposition.
- **Config store** — a processor can read (and possibly write) configuration
  state held by the vortch instance; i.e. **Vortch is the processor's config
  store**. Rationale: makes roaming tractable — only Vortch state needs to sync,
  not each processor's separate state.

#### Transports (support several — meet processors where they are)

Vortch should speak an **array of protocols**, richest to simplest, so authors
pick the least effort that works — and so **vortch-unaware tools** can be used:
- **Native / preferred**: a cross-platform full-duplex channel (named pipe or
  local socket) for registered, long-running processors that report status.
- **stdin/stdout**: lighter processors that read items and emit status/result
  lines.
- **Command line + return code**: the simplest tier — launch a tool with the
  items as args and interpret the exit code. Lets pre-existing, vortch-unaware
  CLI tools serve as processors.

#### Matcher pipeline

- Matchers are **registered** by installed processors (native protocol) or
  **hand-written** (e.g. JSON in the config) to wire up pre-existing tools or
  vortch-unaware scripts.
- Matchers are **ordered / prioritized** (initially just by list position): more
  specialized processors get first crack; more general ones bid later.
- A matched processor may **reject** a candidate job (for any reason); the next
  matching processor then gets a crack.
- A **default processor** acts as a **dead-letter** sink (logs "nobody wanted
  this"); it is just a processor like any other.

#### Marketplace (future direction)

- Aim: a **marketplace** of processors. This pulls the replication/roaming idea
  forward, since discovering processors aids local installation.
- Vortch may take on an **installer / uninstaller** role for processors.
- Implies **user-facing authoring/publishing docs** so third parties can build
  and distribute processors. (Later — but it argues for keeping the protocol
  stable and well-documented from early on.)

---

## Approaches considered (Windows shell integration)

| Tier | Mechanism | Drop capability | Context menu | Runtime icon change | Effort / fragility |
|---|---|---|---|---|---|
| **A** | Plain `.lnk` shortcut to an exe | Free — Explorer launches `target.exe file1 file2 ...` on drop. Only `CF_HDROP` (real files), no other data types. | Inherited default shortcut menu; custom entries need an add-on `IContextMenu` shell-extension handler registered under `HKCR\lnkfile\shellex\ContextMenuHandlers` (which then has to self-filter so entries only affect "vortch" shortcuts). | `IShellLinkW::SetIconLocation` + `SHChangeNotify` | Lowest — good for a one-off quick tool. |
| **B** | Custom file type (e.g. `.vortch`) + in-proc COM shell-extension DLL implementing `IDropTarget`, `IExtractIcon`, `IContextMenu`/`IExplorerCommand` | Full `IDataObject` (files, URLs, virtual files, clipboard formats, etc.). | Fully custom, scoped to the file type. | Yes, via `IExtractIcon` reading instance data. | Highest — a real COM DLL loaded into `explorer.exe`; icon overlays are capped at 15 system-wide slots (usually already saturated by OneDrive/AV/git clients — better to bake the badge into the icon bitmap yourself); Windows 11's simplified context menu also hides 3rd-party entries unless you implement the newer sparse-package / `IExplorerCommand` surface. |
| **C** | A normal always-on-top, borderless, layered app window styled to look like a desktop icon (the Rainmeter/Fences/RocketDock pattern — often parented into the `WorkerW` window, between the wallpaper and the icons, for true desktop-plane placement) | Full native drag-drop API in the app itself — trivial, no COM registration. | Fully custom `ContextMenuStrip` / native menu — trivial. | Trivial — just redraw. | Low — this is a normal app, not a shell extension. |

**Decision: Tier C.** It gives 100% control over every surface — drag-drop
(all data types, not file-only), context menu, and icon — with no COM
registration and no code loaded into `explorer.exe`. The one accepted
trade-off is that the window does not join Explorer's icon grid (no
auto-arrange / snap).

Self-install / self-uninstall plan (no shell/registry COM extension needed):

- Install to a per-user location (`%LOCALAPPDATA%\YourVortch\vortch.exe`).
- Register autostart via the `Run` key
  (`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`) or a Startup-folder
  shortcut, launched with **no args** so it recreates whatever vortch instances
  the user had.
- Persist each vortch instance (GUID, icon path, parameters, position) in its
  own file under `%APPDATA%\YourVortch\vortices\*.json`.
- `--install` / `--uninstall` are just flags the exe handles itself (uninstall
  = remove the `Run` key, optionally remove config, optionally self-delete via
  the delayed-rename-on-reboot trick).
- No MSI/WiX/NSIS required as a prerequisite; one could be added later for a
  nicer first-run experience.

---

## Toolkit choice for Tier C (Windows)

**Ruled out:**
- **Qt** — licensing concerns (LGPL/commercial tradeoff).
- **Electron** — binary size / footprint concerns.

**Considered:**

| Toolkit | Size | License | Native look | Notes |
|---|---|---|---|---|
| **Raw Win32** (GDI+/Direct2D, manual message loop) | Smallest (a few hundred KB, static link) | N/A | Perfect (it *is* the platform) | Most boilerplate: manual DPI handling, manual layered-window alpha compositing, `RegisterDragDrop`/`IDropTarget` (the small, in-process-only COM surface — not the system-wide registration Tier B needs). |
| **wxWidgets** | Small–moderate, statically linkable | wxWindows Licence (LGPL + explicit static-linking exception: no relinking obligation, no royalties) | Good — wraps *real* native controls per OS (actual `HWND`s on Windows, `NSWindow`/`NSView` on macOS, GTK on Linux) | **Confirmed still actively maintained** — latest stable release 3.2.11 shipped July 2026, with regular releases throughout 2025–2026. Provides multi-format drag-drop cross-platform without hand-rolling `IDropTarget` per OS. |
| **FLTK** | Small | FLTK Licence (LGPL + static-linking exception) | Non-native (custom-drawn widgets) | Lightweight, but the non-native look works against a polished desktop-icon feel. |
| **GTK** | Moderate (heavier deps off-Linux) | LGPL | Native on Linux (system-provided); non-native / themed elsewhere | Natural fit for the Linux front end; less attractive as the Windows toolkit. |

**Decision: wxWidgets** — chosen; the project starts here. Raw Win32 has the
best size / no-dependency story, but the per-platform boilerplate (DPI,
layered-window compositing, drag-drop) becomes too repetitive to justify
writing three times across Win/Mac/Linux. wxWidgets' license doesn't
reintroduce Qt-style risk, and its footprint is much closer to the "small
monolithic binary" goal than Qt or Electron.

This decision **reverses the earlier "no shared UI code" assumption**: because
wxWidgets wraps real native controls behind one API, the front end is a single
shared codebase rather than three native shells. Per-OS differences that
wxWidgets does *not* abstract (self-install / autostart, badge specifics) are
handled by conditional compilation inside the one UI target — see the
architecture below.

---

## Architecture: shared portable core + one wxWidgets cross-platform front end

wxWidgets lets us share the UI code, so the structure is two targets — portable
logic plus a single cross-platform front end — with per-OS integration isolated
to conditional-compiled platform files:

```
vortch-core/    portable C++17, no UI, no platform APIs
  - instance / parameter model (GUID, icon path, config, on-screen position)
  - JSON read/write of persisted vortch state
  - launch-identity contract (GUID handed to the downstream tool)
  - (dispatch-by-type logic lives downstream — out of scope here)

vortch-ui/      wxWidgets cross-platform front end, links vortch-core
  - borderless, always-on-top wxFrame styled as a desktop icon
  - wxDropTarget: multi-format drag-drop, one implementation for all OSes
  - wxMenu context menu; double-click opens the "data intake" panel
  - icon + overlay-badge painting via wxGraphicsContext / wxDC
  - platform-specific integration via conditional compilation:
      platform_win.cpp    Run key / Startup shortcut, %APPDATA% config
      platform_mac.mm     Login Items (SMAppService), ~/Library/App Support
      platform_linux.cpp  ~/.config/autostart/*.desktop, XDG config dir
```

Native look is preserved because wxWidgets renders real controls per OS (`HWND`
on Windows, `NSWindow`/`NSView` on macOS, GTK on Linux); only the always-on-top
borderless frame and the icon/badge are custom-drawn.

### Cross-platform notes

- **macOS**: no COM / namespace-extension analog is needed. An `.app` bundle is
  inherently double-click-launchable; Finder drop is handled through the
  `NSApplicationDelegate`; context menu and icon-badge drawing are native.
- **Linux**: prefer the XDG-based, Tier-C-style always-on-top window; going
  through EWMH is the most consistent approach across GNOME / KDE / others.
  Caveat: there is no standard cross-DE desktop-icon convention — GNOME needs
  an extension, KDE Plasma's Folder View behaves differently, and Wayland
  compositor behavior varies — so expect the least consistency of the three
  platforms here.

### Interface contract shared across all three front ends

- Each vortch instance has a stable identifier (GUID) and a parameter file.
- Launching a processing tool passes that vortch's identifier (e.g. as a CLI
  arg); the tool uses it to look up the vortch's parameters. This contract is
  platform-independent and belongs in `vortch-core`.
- Dropped-file dispatch-by-type logic and all "real" processing stay entirely
  out of the front end and out of scope for this design. The front end's only
  job is to: receive a drop / detect a double-click (default menu action) /
  launch the downstream tool with the vortch identity attached.

---

## Deployment & storage model (supersedes earlier storage / roaming / config-root decisions)

**Installation — portable ("portable apps" style), no installer.**
- The app runs from wherever it lives; that folder is its home. No
  `%LOCALAPPDATA%`, no MSI/installer. Uninstall = delete the folder (+ remove any
  autostart hook).
- **`--init`** designates the current location as the install and creates the
  local SQLite store there. Store creation and autostart registration are
  **separate** concerns (autostart likely a distinct verb — see **CLI verbs**).
- **Data-dir override**: a command-line **`--data-dir <path>`** points the store
  elsewhere. Escape hatch, and a real case: a system-wide `vortch` (e.g.
  `/usr/share/bin`) with each user's DB under their home. **CLI-only** (env vars
  are awkward at the launch site; the override can be baked into the autostart
  command / launch script). The store's own location is therefore NOT stored in
  the store — bootstrapped from exe-folder default or `--data-dir`.

**Autostart — a separate initialization concern.**
- Conceded: on Windows this uses the **HKCU Run key** (so: *not* strictly
  no-registry). Linux `~/.config/autostart/*.desktop`; macOS Login Item.
- Kept distinct from `--init` (which is about the DB). Likely a separate verb
  (e.g. `--install-autostart` / `--uninstall-autostart`) — see **CLI verbs**. The
  autostart command may embed `--data-dir`.

**CLI verbs & first-run.**
- *(no args — e.g. double-clicked in Explorer)*: show an **info message box**
  (what vortch is + brief how-to + a project link, stubbed to GitHub for now). No
  side effects, no run. (Guards the inevitable double-click.)
- `--startup`: **run normally** (recreate vortices, widgets, tray). This is what
  the autostart hook invokes. Requires an initialized store (default: exe folder,
  or `--data-dir`); if missing, show an error/info box.
- **High-level (typical user):**
  - `--install` = `--init` + `--enable-autostart`, then **launches** (`--startup`)
    unless `--nolaunch`. Accepts `--data-dir`, `--force`, `--nolaunch`.
  - `--uninstall` = `--disable-autostart` today, kept a separate verb (may do more
    later, e.g. offer to remove the store).
- **Fine-grained:**
  - `--init`: create the store at the exe folder (or `--data-dir`): set header
    pragmas, create `objects`/`logs`/`meta`/`local`, seed `meta` (database-id GUID,
    created ts, `welcomed=false`) + `local` (machine identity). **Refuses if a
    store already exists** unless `--force` (destructive re-init).
  - `--enable-autostart` / `--disable-autostart`: write/remove the login hook
    (Windows Run key / Linux `.desktop` / macOS Login Item), **baking exe path +
    `--data-dir` (if any) + `--startup`** into the hook command.
- **Options:** `--data-dir <path>` (store location — used by init/install/enable/
  startup; "remembered" ONLY by being baked into the autostart hook, never stored
  in the DB); `--force` (allow destructive re-init); `--nolaunch` (suppress the
  post-`--install` launch).
- **First-run welcome**: a `meta.welcomed` flag — the first `--startup` shows a
  welcome once, then sets it; `--welcome` forces it (testing / re-show).

**Storage — one SQLite store for everything.**
- A single SQLite file (next to the exe, or at `--data-dir`) holds **config AND
  history/log** — no separate `config.json`. Tidiness + transactions + queries.
- **JSON blobs in columns** where a value is really an object (instance `params`,
  settings) — not shredded into columns; SQLite **JSON1** for light queries. Our
  nlohmann serialization becomes the per-row blob encoder; `ConfigDocument`
  becomes an in-memory shape hydrated from / persisted to rows, not a file.
- **Table split** lets replication policy differ: config tables can replicate
  while history/log tables stay machine-local / prunable.

**Scoping — a `facets` JSON column (flat object store; the `scopeid` idea is dropped).**
- No intrinsic hierarchy. The DB is an object store; most objects carry a **`facets`**
  JSON blob, e.g. `{ "machines": [...], "users": [...], "groups": [...], ... }`.
  Categories are **open-ended** — add freely, no schema change (this achieves the
  loose-coupling the `scopeid` was for, more simply).
- Selection is SQL over the blob via SQLite **JSON1**, e.g. objects whose
  `facets.machines` include this machine:
  `... WHERE EXISTS (SELECT 1 FROM json_each(facets,'$.machines') WHERE value = :machine)`
  (or the `->>` operators). Combine categories with `AND`/`OR`.
- **Empty/absent list = unrestricted (matches all)** — the flat equivalent of
  "global"; every filter is "list empty/absent OR contains me". An object with
  `{}` facets is fully global. *(confirm)*
- No resolver subsystem: the "resolver" reduces to gathering ambient facts
  (machine name / hostname, user, active groups) and building the `WHERE` clause.
  Machine identity = hostname or a stored local id (no GUID resolution).
- Groups are organizational/selection facets for now; group-carried config (a
  separate group-settings object) is deferred.
- **Groups (future) become a separate, genuinely hierarchical table.** By our own
  design groups form a hierarchy, defined **only in the groups table, never in
  objects**. An object's `facets.groups` holds **node associations**: the id(s) of
  the specific group node(s) it belongs to (an object can belong to several, and a
  node may be **interior or leaf**). It is **NOT an arc association** — you store
  the endpoint node, not the full path of names down the tree; ancestry is not
  stored on the object. The hierarchy is resolved at query time against the groups
  table: a group-involving query **expands the target group to its subtree**
  (group + descendants — e.g. a recursive CTE over the groups table), then matches
  objects (via `json_each`) whose `facets.groups` intersect that
  set, so **subgroup membership is included**. Deferred; noted so the facet shape
  stays compatible.

**Data model (SQLite) — hybrid: one polymorphic table + specialized tables.**
Not a general object DB; small dataset. Avoid one-giant-JSON (bad updates/queries,
frozen hierarchy) and one-relation-per-kind (`kind` = table name, not queryable).
- **Polymorphic object table** (config, processor registrations, etc.):
  `object( id, kind, name, facets, body, created, modified )`
  - `id`: **GUID** surrogate key — NOT autoincrement int, so rows are globally
    unique and a replica merge is a union (required by the replication goal).
  - `kind`: discriminator enum ('config','processor', …), coarse + stable,
    parametrically queryable. Fine-grained **sub-types are expressed via `facets`**
    (e.g. app settings = `kind='config'` + a sub-type facet), NOT by proliferating
    `kind` — keeping the enum small.
  - `name`: a free-form, **user-editable presentation name** (display label) —
    NOT a key. A specific/singleton object is identified by `id`, or by `kind` +
    a `facets` match (singleton enforced by app logic), never by `name`.
  - `facets`: free-form JSON (`{ machines:[], users:[], groups:[], … }`) for both
    selection and sub-typing, via JSON1.
  - `body`: kind-specific JSON blob; usually read/written whole, rarely queried
    (index sub-fields via a generated column only if a need arises).
  - `created`/`modified`: **int64 unix seconds (UTC)** for audit + last-writer-wins
    merge. Maintained **explicitly in code, NOT via triggers** — a trigger would
    stamp the local clock on every write, clobbering the source `modified` a merge
    must preserve for LWW.
  - `objects`/`meta`/`local` are **`WITHOUT ROWID`** (natural TEXT primary keys);
    `logs` keeps a rowid + AUTOINCREMENT (append-only, ordered by id).
- **Specialized tables where performance matters** (e.g. **logs/history**):
  intrinsically kind-ed (no `kind` column); real indexed columns for hot fields
  (`timestamp`, `machine`, `user`, `instance`, `level`, …); body JSON optional;
  NOT via the `facets` mechanism. Prunable; typically not replicated (or selectively).
- **Meta vs. local**: `meta(key,value)` holds at least `schema_version` (drives
  migration on open). The machine-local **replica identity** (hostname/local-id)
  lives in a distinguished `local` row/table kept OUT of replication.
- **Facet semantics are contextual (per-category), not one global rule**: each
  category documents whether empty/absent means *match-all* (restriction-style,
  e.g. `machines`) or *match-none* (membership-style, e.g. `groups`). The Store
  offers a predicate helper for each form.
- **Replication/merge (future)**: GUID ids + `modified` enable last-writer-wins;
  deletes need **tombstones** (soft-delete) so a delete propagates instead of
  resurrecting on merge — deferred, but shapes the id/timestamp choices now.

**Tables.**
- `objects` — the polymorphic collection above.
- `logs` — specialized, indexed (perf-critical).
- `meta` — extensible `key`/`value` metadata (may evolve freely).
- `local` — non-replicated machine/replica identity.

**Visual mode & the vortex object.**
- A **vortex is a `kind='vortex'` object**; its `body` holds its parameters (label,
  icon ref, placement = position/size/monitor, and `params` that registered
  processors can query). `facets.machines` scopes it (per-machine).
- **Visual mode is a distinct, named, typed sub-object ("lump")** that MANY object
  kinds can carry (a vortex, or a machine/global config supplying defaults) — so it
  can later be resolved **most-specific-wins** across a set of disparately-typed
  objects. It is **kind-specific**: the vortex visual lump (`zmode`, `peek`, …)
  differs from a future job-status-UI visual lump. Shape:
  `body.visual = { "vortex": { zmode, peek }, "<other>": {…} }` — a container keyed
  by visual-mode type so one object can carry several.
- **v1 simplicity**: attach the visual lump **only to the vortex object**; no
  hierarchical resolution yet (later: extract the same-shaped lump from
  machine/global objects and merge). **Placement** (position/size/monitor) stays in
  the vortex body proper (inherently per-instance/per-machine), NOT in the
  shareable visual lump.

**Bootstrap invariant (fixed forever — defined now).** Kept in the SQLite *file
header* (not a table), so it's readable pre-schema and immune to every future
migration:
- `PRAGMA application_id = 0x564F5254` (ASCII "VORT") — sanity check that a file
  is a vortch DB; refuse to open otherwise.
- `PRAGMA user_version = <schema_version>` — starts at **1**; on open, if
  `< current`, run migrations.
Everything else (database-id GUID, created timestamp, …) lives in the evolvable
`meta` table; only these two header pragmas are the permanent invariant.

**Pre-release migration policy (until first public release).** During this early,
tumultuous phase there are NO deployed instances, so schema changes are made
**freely, without writing migrations** — a **wipe-and-reinstall** (delete the DB /
`--init` fresh) is the expected way to adopt a schema change. Do NOT invest in
migration machinery yet; bump `user_version` only if convenient. This luxury ends
at the first public release, after which real migrations become required.

**Roaming — DB replication + scope filtering.**
- A user's whole DB can be the totality of all vortices across machines/accounts;
  **portability = synchronizing replicas**, each machine `SELECT`ing its rows by
  matching `facets`. Supersedes the earlier "single roamable JSON package +
  machine-local history" framing.

**Supersedes**: the earlier *Persistence* (single JSON config file), *Roaming
reality* (single portable package), the *Config file container* JSON-**file**
shape (now row blobs), and all `%LOCALAPPDATA%`/`%APPDATA%` config-root references.
The *SQLite store* decision is expanded (now holds config too). Instance *fields*
and the *status/badge* model are unchanged.

**Still open (this area)**: confirm
the `facets` empty/absent = matches-all convention; history replication/pruning
policy; facet-query indexing only if object counts ever grow.
- **`--install` vs. portable / per-user DB (needs thought).** `--install` writing
  a Run key is inherently *not* portable. If the exe is a **single shared instance**
  (e.g. `Program Files` / `/usr/share/bin`), the DB must be **per-user**, not next
  to the exe — so `--install` should likely choose and **bake a per-user
  `--data-dir`** into the `--startup` command (and detect a non-writable exe dir).
  Resolution = refining CLI args + minor runtime behavior; deferred.

## Implementation decisions log (settled)

- **UI toolkit**: wxWidgets 3.3.x (vcpkg installed 3.3.3) (see toolkit section).
- **Dependency management**: vcpkg in manifest mode (`vcpkg.json`), covering
  wxWidgets and nlohmann/json; reproducible across MSVC and Docker/Linux.
- **JSON library**: nlohmann/json (header-only).
- **Process model**: a single manager process hosts N vortch windows (simpler
  tray/global-config/clean-uninstall story than N independent processes).
- **Persistence** *(SUPERSEDED — see "Deployment & storage model": now a single
  SQLite store, not a JSON file)*: a **single config file** (one self-contained "package"),
  rewritten atomically by the owning process. Chosen to keep all config in one
  unit that can eventually roam / be synced.
- **Windows autostart**: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
  value `"…\vortch.exe" --autostart` (the flag tells the process it was
  auto-launched). macOS: Login Items via `SMAppService`; Linux:
  `~/.config/autostart/vortch.desktop`.
- **Placement restore**: each instance stores position + a stable monitor id;
  if that monitor is absent at restore, clamp into the primary monitor's work
  area and keep the instance (never drop it).
- **`params`**: free-form opaque blob **for now** (schema deferred).
- **Badge is parameterized** (not a fixed enum→image): `kind` selects the base
  state overlay; `progress` drives a thermometer/meter; optional
  `count`/`text`/`color` decorate. The UI composites icon + overlay + meter.
- **Error stickiness** and the **`done`-pulse duration/visibility** are
  **config-driven** (`settings`), not hardcoded.
- **Local runtime store = SQLite** (single file): the job history/status log and
  the dead-letter log live here, rows tagged by instance GUID. Machine-local and
  **NOT roamed** — kept separate from the roamable JSON config package. Access is
  encapsulated behind a store interface in `vortch-core`; adds a `sqlite`
  dependency via vcpkg.
- **Name**: **vortch** (provisional codename; equivalent fallback `vorch`).
  Package namespaces (npm/PyPI/crates) and GitHub are effectively unclaimed;
  `vortch.dev`/`.app`/`.me` are available (`.com`/`.io` taken). Trademark NOT
  formally cleared — note phonetic proximity to *Vortec*/*Vortech* (different
  Nice classes). Public/marketing name remains swappable.
- **Text encoding**: **UTF-8 everywhere** — every `std::string` in `vortch-core`
  is UTF-8 (NOT `char8_t`/`std::u8string`). Convert only at OS boundaries via
  `vortch/text.hpp` (`utf8ToWide`/`wideToUtf8`/`utf8ToPath`/`pathToUtf8`) and
  `wxString::FromUTF8`/`utf8_string`.
- **Platform floors**: Windows **7+** (`_WIN32_WINNT=0x0601`); XP is out of
  scope (VS2022 + wxWidgets 3.3 dropped it — would require a legacy VS2017 +
  wx 3.0/3.1 track). Linux: Ubuntu 22.04+ (dev/testing target).
- **Distribution**: self-installing single binary (no separate installer),
  per Tier C — Windows = static-linked single `.exe` (flip vcpkg triplet to
  `x64-windows-static`), Linux = AppImage, macOS = self-registering `.app`
  (Login Item). Heavy installers (MSI/deb/rpm/dmg) + code signing/notarization
  are deferred (needed only for system-wide install and SmartScreen/Gatekeeper
  trust).

### Roaming reality (SUPERSEDED — see "Deployment & storage model"; roaming is now DB replication + `scopeid` filtering)
Automatic config roaming is NOT a free OS feature: Windows `%APPDATA%\Roaming`
only syncs under domain roaming profiles; macOS needs iCloud + sandbox opt-in;
Linux has no standard. The portable path is a single self-contained file the
user (or a future sync feature) moves as a unit — hence embedded icon data and
tolerant, advisory placement fields.

### Config file container (SUPERSEDED as a *file* — the shape now informs SQLite row blobs; see "Deployment & storage model")
```jsonc
{
  "schemaVersion": 1,
  "settings": { "collapsed": false, "autostart": true,
                "donePulseMs": 1500, "errorAutoClear": false /* status tuning */ },
  "instances": [
    {
      "id":       "GUID",                 // stable identity; handed to downstream tool
      "label":    "…",                    // user-facing name (menu/title/tooltip)
      "icon":     "<embedded image data>",// base64 — self-contained/roamable (TENTATIVE)
      "iconHint": "…",                    // optional non-authoritative source path
      "position": { "x": 0, "y": 0 },
      "monitor":  "stable-monitor-id",
      "size":     { "w": 64, "h": 64 },   // widget, not fixed icon size
      "visualMode": "collapsed",          // collapsed (icon) | expanded (widget)
      "state":    { "kind": "none", "count": 0, "text": "", "color": "#RRGGBB" },
      "params":   { /* free-form, opaque to vortch-core */ }
    }
  ]
}
```
- **`state`** drives a **self-rendered overlay badge** (works around the capped
  ~15 system overlay-icon slots — no system resource, no cap).
- **`settings.collapsed`** reserved for a future "collapse to tray" mode (hide
  desktop windows, keep process + tray icon, restore on demand). That feature
  is window-lifecycle only; the container is reserved now so it needs no schema
  change later.
- **Quiescent vs. runtime state** (key modeling split): the *persisted* values
  (`icon`, `label`, `params`, and a default `state`) describe *what the portal
  is for* and change rarely. The **displayed** `state` and a dynamic `tooltip`
  are **runtime**, aggregated from live job reports — modeled separately in
  `vortch-core` as a transient `InstanceStatus`, NOT stored as authoritative
  config. `iconPath`/`label` are effectively static; `state`/`tooltip` vary
  moment-to-moment.
- **Resource references** (icons & overlays) use a small URL-esque scheme:
  - `vortch:builtin/<name>` — bundled resources (several overlays + a starter
    icon set provided internally; covers the vast majority of needs);
  - `vortch:embedded/<id>` — per-instance image bytes embedded in the config
    (self-contained / roamable);
  - `file:///abs/path` — external image, for fringe cases.
  All resources carry an **alpha channel / mask** for clean icon+overlay
  compositing.

### Resolved this round
- Icon: **both** — bundled + embedded, plus external `file://`, via the
  resource-reference scheme above; all alpha-masked.
- `tooltip`: yes, a separate field, and it is **runtime/dynamic** (not just a
  static description).
- `state` is **set by processor reports, aggregated/arbitrated by the Vortch
  process** into a final disposition.

### Runtime status model & local store

Status is pure logic in `vortch-core` (headless, unit-testable), fed by processor
reports and rendered by `vortch-ui`. Two small, swappable seams keep coupling low:

- **Pure reducer** — `IStatusAggregator::reduce(activeJobs) -> InstanceStatus`.
  Stateless. v1 rule: N=1 → normalize the single job's report; N>1 → coarse
  Vortch default (`processing`, no percent). Later swap in weighted/priority
  synthesis without touching UI or protocol.
- **Temporal overlay** (`StatusMachine`) — wraps the reducer and adds time-based
  behavior the reducer can't express from active jobs alone: the monostable
  `done` pulse (fires on completion, decays back to `quiescent` after
  `settings.donePulseMs`) and `error` handling (auto-clear vs.
  hold-until-acknowledged per `settings.errorAutoClear`).

```
JobReport       // from a processor: jobId, kind, progress?, text?, richPayload?
InstanceStatus  // for the UI: kind ∈ {quiescent, processing, done, error},
                //   progress? (0..1), shortText?, count?, badgeResourceRef
```

**Badge** is a composite parameterized by `InstanceStatus`: `kind` selects the
base state overlay (from the resource scheme); `progress` renders a
thermometer/meter; optional `count`/`text`/`color` decorate. A processor's *rich*
state is shown verbatim in the secondary UI; only the badge uses the
normalized/aggregated value.

**Local store (SQLite).** A single machine-local SQLite file holds the job
history/status log and the dead-letter log (rows tagged by instance GUID),
deliberately **separate from the roamable JSON config** and **not synced**
(append-heavy, potentially large runtime data). Access lives behind a store
interface in `vortch-core`; SQLite arrives via vcpkg.

### Graphics standards & seed collateral

- **Master format: SVG.** All icon/overlay collateral is authored as SVG
  (scalable, alpha built-in, tiny, git-diffable). Design square on a 256-unit
  artboard (1024 for detailed work), sRGB, 8-bit alpha, with safe padding.
- **Runtime rendering**: wxWidgets 3.2 `wxBitmapBundle::FromSVGFile` rasterizes
  SVG per-DPI — ship SVG, render crisp at any size/DPI (no PNG zoo to maintain).
- **Per-OS packaging rasters** (generated from the SVG masters at build time):
  - Windows `.ico`: 16, 24, 32, 48, 256 (256 stored PNG-compressed); tray
    16/20/24/32.
  - macOS `.icns`: 16/32/128/256/512 + @2x (→1024); follow the rounded-square
    padding convention.
  - Linux: hicolor PNGs at 16..256 plus the SVG in `scalable/` (freedesktop
    icon-theme spec).
- **Overlays/badges**: separate SVGs in a normalized viewBox, composited onto the
  base icon's bottom-right quadrant (~40-50%). State set: quiescent (none/subtle),
  processing, done (check), error (alert). Alpha required.
- **Progress meter** (thermometer): drawn **procedurally** at runtime via
  `wxGraphicsContext` (arc or bar filled to `progress` 0..1), NOT a fixed asset —
  only the static badge shapes are assets.
- **Animation**: icons/overlays may animate. Two mechanisms — (a) **procedural
  animators** (built-ins: `spin`, `pulse`, `throb`) computed per frame at runtime,
  infinitely scalable and tiny (spinning whirlpool; pulsating "thinking" brain);
  (b) **frame-sequence** assets (author frames / sprite sheet) cycled at a
  declared `fps`/`loop`. A resource declares itself `static`,
  `procedural(animator,params)`, or `framed(fps,loop)`. Note: wx's SVG rasterizer
  ignores SMIL/CSS animation, so in-app motion is procedural or framed — SMIL/CSS
  in the SVGs is only for browser/design preview.
  - **Animate only when active**: a `wxTimer` repaints at the animation fps *only*
    while an animated state is showing; quiescent = static, no timer (saves
    CPU/battery).
  - **Pluggable renderer (contemplate early)**: the widget's drawing must go
    through a swappable *renderer/animator* that draws frame N from
    (state, size, phase) via `wxGraphicsContext` — NOT merely blit a scaled
    bitmap. This enables rich per-frame renditions and **metaphor morphs**:
    e.g. the peek→full expand growing from a small ball into an "awakening"
    vortex with lines coming alive, or morphing the vortex into an open mouth
    to receive a drop. The current `OnPaint` + size-interpolation is a
    placeholder; keep the paint path delegating to a renderer so shape-morphing
    (not just scaling) needs no rework later. Transitions (peek↔full, state
    changes) are themselves animators, not just tweened geometry.
- **Contrast/theming**: the icon must read on both light and dark desktops
  (contained shape + subtle outline/shadow); provide a monochrome tray/menu-bar
  variant (macOS menu bar prefers template/monochrome images).
- **Layout**: `assets/` holds `icon.svg`, `tray.svg`, `overlays/*.svg`; a
  CMake/script step generates `.ico`/`.icns`/hicolor PNGs (rsvg-convert / Inkscape
  / ImageMagick, icoutils, png2icns). Resource scheme `vortch:builtin/<name>`
  resolves to these bundled assets.

### Considered and set aside
- **Explorer-native "proxy" icon for positioning.** Idea: place a real
  (alpha-transparent, blank-label) desktop icon as a grid-snapping anchor and
  have Vortch render/track over it. Distinct from the WorkerW z-order trick.
  The anchor's position is readable via the *documented* `IFolderView::
  GetItemPosition` (no undocumented cross-process `SysListView32` memory hack
  needed). Set aside because: no change notification (requires polling), the
  anchor is user-mutable/deletable, drop-target ambiguity between the real icon
  and our window, and it's Windows-only (no mac/Linux analog). Preferred way to
  get the "snaps like an icon" feel: read desktop icon spacing
  (`SPI_ICON*SPACING` / `LVM_GETITEMSPACING`) and round our own stored
  coordinates to that grid — keeping our position model the single source of
  truth.

### Windowing findings (Windows front end, `vortch-ui`)

**Z-order (topmost vs. on-desktop) + cross-platform plan.**
Context-menu toggle, implemented on Windows:
- *Topmost*: `wxSTAY_ON_TOP` / `SetWindowPos(HWND_TOPMOST)`.
- *On desktop (below apps)*: normal window + intercept `WM_WINDOWPOSCHANGING`
  → `hwndInsertAfter = HWND_BOTTOM`. All top-level windows render above the
  desktop window (Progman + its icon list-view), so a bottom-pinned window sits
  **above the icons but below every app** — the requested behavior (distinct
  from WorkerW-parenting, which would sit *below* the icons).
- During **move mode** the window is forced topmost regardless of setting, then
  returns to the chosen z-order on drop/abort.
- Current code is Windows-only (`#ifdef __WXMSW__`); TODO move behind the
  platform seam. Portability: *Topmost* is portable via wx (`_NET_WM_STATE_ABOVE`
  on X11, high `NSWindow` level on macOS). *On-desktop* needs native per-OS:
  **Linux/X11** `_NET_WM_STATE_BELOW` (+ skip taskbar/pager; Ubuntu MATE/Marco
  honors it); **macOS** `NSWindow.level` just below `NSNormalWindowLevel` +
  `collectionBehavior`; **Wayland** has no standard (only wlr-layer-shell on
  wlroots).

**Win+D / "Show Desktop" survival (deferred).**
On-desktop mode is a normal top-level window, so **Win+D minimizes it** (known
limitation). Two routes, both with costs:
- *WorkerW parenting*: immune to Win+D, but sits *below* icons (breaks
  "above icons") and is fragile (cross-process child of Explorer; version /
  monitor / Explorer-restart quirks).
- *Watchdog re-assert*: keep the above-icons window, detect Show Desktop and
  re-show/re-bottom it (timer or shell hook). Keeps above icons but is hacky
  (brief flicker; must distinguish show-desktop from a real user-minimize).
Cross-platform note: mostly free elsewhere — X11
`_NET_WM_WINDOW_TYPE_DESKTOP` rides through `_NET_SHOWING_DESKTOP`; a macOS
desktop-level `NSWindow` survives Mission Control's Show Desktop.

**Non-activating widget + context-menu focus (Win32 foreground lock).**
Widget uses `WS_EX_NOACTIVATE` so idle / click / right-click / menu-display do
not steal foreground. But:
- A popup menu only dismisses correctly (click-away / Esc) and avoids
  foregrounding its owner *when the owner is foreground* — so we briefly
  `SetForegroundWindow(self)` for the menu, then restore focus to the prior app.
- **Move** needs full mouse capture, which Windows grants only to the
  **foreground** window; so during an active move the widget must stay
  foreground, and focus is handed back on drop/abort (NOT right after the menu —
  doing so drops the capture via `WM_CAPTURELOST` and cancels the move).
- **Foreground-lock anomalies** when dismissing the menu *without selecting*
  (benign; not fixed): Esc/selection → still foreground → restore succeeds ✅;
  clicking **another app** → that app takes foreground first → our restore is
  *denied* (app keeps focus; the old window's caret is a stale repaint);
  clicking **desktop/taskbar** → restore denied → Windows' documented fallback
  **flashes the target's taskbar button** instead of focusing it. Root cause:
  `SetForegroundWindow` is restricted to the current foreground process
  (anti-focus-steal). Workaround if ever needed: `AttachThreadInput` (finicky).

### Still open (do NOT scaffold until resolved)
- **Seed graphic collateral** (in `assets/`): woodcut black-hole vortex
  `icon.svg` (per Sgr A* refs), mono `tray.svg`, side-view woodcut pink
  `brain.svg` (throb), AI circuit `ai-brain.svg` (electricity along traces), and
  `overlays/{processing,done,error}.svg`. Remaining polish (non-blocking):
  simplified 16px icon variant; optional carved-woodcut push on the vortex.
  Throb rates (normal + frenetic) are runtime animator params, not separate
  assets. Wire assets through `vortch:builtin/*` during scaffolding.
- **Processor protocol** concrete spec: exact framing for each transport tier,
  registration format, match-criteria language, status-report shape, config
  read/write semantics, and the reject / next-in-line handshake. (Processors are
  a separate project, but the surface lives in `vortch-core`.)
- **Marketplace / installer**: Vortch-as-installer for processors, plus
  third-party authoring & publishing docs (future; keep the protocol stable and
  documented).
- **SQLite store shape**: one DB per install (rows tagged by instance GUID —
  lean) vs. per-instance DB; table layout; retention/pruning of old log rows;
  and the C++ binding (raw `sqlite3` vs. a wrapper such as SQLiteCpp).
- Resource-scheme details (exact `vortch:` syntax, supported image formats).
- `params` schema (later — pending more thought).
- Roaming: custom gossip/replication protocol + a user-specific sync ID (far
  future; keep all syncable state — incl. processor config — inside Vortch).
- **UI capabilities** (secondary UI, tray, badge rendering) — to discuss next,
  before scaffolding.
