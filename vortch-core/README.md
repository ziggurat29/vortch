# vortch-core (stub — implementation pending discussion)

Portable, UI-free logic used by the `vortch-ui` front end. **No platform APIs, no UI.**

Intended responsibilities (per `docs/design.md`):
- Instance / parameter model: GUID, icon path, config values, on-screen
  position (incl. monitor identity for restore).
- JSON read/write of persisted vortch state.
- The launch-identity contract: a downstream tool receives a vortch's GUID
  and looks up that instance's parameters here.

Explicitly **out of scope**: dropped-file dispatch-by-type and any "real"
processing — those live downstream, not in the front end or core.

> Status: directory reserved. Build system and code intentionally deferred
> pending design discussion (see project plan).
