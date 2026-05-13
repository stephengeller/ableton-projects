# CLAUDE.md — ableton-projects (monorepo)

Personal collection of audio tooling for Ableton Live. Multiple
plugins / devices coexist under this repo; each has its own per-
project CLAUDE.md with the substantive operating manual.

## Start here

- **Plugins**: `plugins/<name>/CLAUDE.md` for the plugin-specific
  architecture, conventions, and gotchas. The big one currently is
  `plugins/ClipToZero/CLAUDE.md`.
- **Max for Live**: `max-for-live/` — not under active development.

## Repo-wide conventions

### Commits

- Subject: `Topic: short imperative description` (under 70 chars).
- Body: substantive. Explains _why_ before _what_, links cause to
  effect, calls out trade-offs and known limitations. Lets `git log`
  read as project history rather than a list of changes.
- Footer: `Co-Authored-By: Claude Opus 4.7 (1M context)
<noreply@anthropic.com>` on AI-assisted commits, so future
  archaeologists can identify which work was AI-collaborative.
- HEREDOC for multi-line messages so `$(...)` and `${...}` in the body
  don't expand at commit time:

  ```sh
  git commit -m "$(cat <<'EOF'
  Subject line

  Body paragraphs...

  Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
  EOF
  )"
  ```

### Tags

- **Always annotated** (`git tag -a`), never lightweight. Each tag
  body lists what changed since the previous tag in "N changes since
  vX.Y.Z" format, so `git tag -n99 --sort=-version:refname` produces
  a readable release tour.
- Semantic versioning: patch for bug fixes / small UX, minor for
  meaningful feature shifts or semantic changes to existing features,
  major for breaking changes (we haven't hit one yet).

### Releases

- Push a `v*` tag to trigger `.github/workflows/build.yml`, which
  matrix-builds macOS / Windows / Linux and attaches per-platform zips
  to a GitHub Release.
- The per-plugin `install.sh` queries the latest-release API on each
  invocation — users running the one-liner installer auto-pick-up the
  newest version.
- Don't push to a `v*` tag that already exists. If you need to redo a
  release, increment the patch number.

### Branches

- Solo dev — main branch development is the default workflow.
- Use a `fix/` or `feat/` branch + worktree when:
  - The change is risky and needs isolation from other work-in-progress
    (e.g., the v0.5.1 deadlock hotfix lived on
    `fix/link-bypass-deadlock` while website work continued on main).
  - Multiple parallel sessions are touching the repo simultaneously.
- Delete branches on merge (`git branch -d` locally, `git push origin
--delete <branch>` on remote). Don't accumulate a graveyard.

### Working with PostToolUse formatters

The repo has a formatter that runs after Write/Edit and may slightly
reformat files (markdown line wrapping, code style). If you need to
edit a file you just wrote, Read it first to get the post-formatter
state — otherwise `old_string` may not match.

## Build

Each plugin is a standalone JUCE/CMake project. On macOS:

```sh
cd plugins/<plugin-name>
brew install cmake          # one-time
cmake -B build -G Xcode
cmake --build build --config Release
```

JUCE is fetched automatically by CMake on first build (pinned to 8.0.4
in each plugin's CMakeLists). Successful Release builds auto-install
to `~/Library/Audio/Plug-Ins/{VST3,Components}/`.

**macOS won't reload .vst3 / .component dylibs while a DAW has them
loaded.** Quit Ableton fully (Cmd+Q, not just close project) to pick
up local builds.

## License

MIT — see `LICENSE`. JUCE is fetched at build time under its own
licence (GPL3 for free use, commercial otherwise).
