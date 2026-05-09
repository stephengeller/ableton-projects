# Max for Live devices

Empty for now. Future Max for Live (`.amxd`) devices will live here, one
folder per device.

## Conventions

- One folder per device, named after the device (kebab-case).
- Each folder has the `.amxd` plus a short README explaining what the device
  does, parameters, and usage notes.
- Patcher source (the `.maxpat` exported from the M4L editor for reference)
  goes alongside the `.amxd` if it's useful for diff-friendly history.

`.amxd` files are binary, so version control mostly tracks "snapshots" — the
real per-patch diffing happens in Max itself.
