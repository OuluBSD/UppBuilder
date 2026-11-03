## Build & Sandbox Policy

### Build Entrypoints And Thin Wrappers

- Build projects with `bin/build.exe` (Windows) / `bin/build` (POSIX), not by invoking `theide` directly.
- Do **not** use `script/build.py` as a build entrypoint; it is not part of the supported build workflow. Some older package AGENTS files mention it — follow `bin/build.exe`/umk instead (VideoServer/VideoRecorder explicitly forbid it).
- Build entrypoints live in `bin/`. We do **not** maintain repo-level `Makefile` or `umkMakefile`; use our custom U++ make utility `uppsrc/umk` via the helper scripts.
- Common entrypoints such as `bin/build.exe` are examples of that workflow.
- A thin wrapper under `script/` that only calls `bin/build` and reports its result is fine, and is the right home for a check that builds several packages in one command. What is not fine is a wrapper that reimplements build steps or bypasses `bin/build`. `script/build_desktop_packages.sh` is the worked example.

### Verifying A Build Succeeded

- `bin/build` **exits 0 even when the build fails.** Measured 2026-10-03: `bin/build -m CLANG -M NoSuchConfig DesktopManager` printed `There were errors. (0:08.83)` and returned 0. Judging success by `$?` silently accepts a failed build — including in scripts and CI steps. Read the log: a good run ends with `OK. (time)`, a bad one with `There were errors. (time)`.
- An **unknown `--mainconfig` name is not an error either.** `bin/build` falls back to the package default, so a typo builds a different configuration than the one you asked for. For packages whose mainconfigs carry load-bearing flags this is a silent-wrong-config build. List what a package actually offers with `bin/build --list-mainconfigs <package>` before building.
- **A flag-gated `#ifdef` that "does not run" can be a stale per-mainconfig object.** Object caches live under `~/.cache/upp.out/<package>/<flagset>/`, and switching `-M` between configs of the same package has twice (2026-10-14, the `WMANAGER` flag in `DesktopManager`) left a cached object compiled without a flag that the current run claims to use: the relink reported `up to date`, the binary linked, and the `#ifdef` block silently held nothing — the ctor called nothing. The compile banner (`DesktopManager ( GUI X11 WMANAGER … )`) reflects the flags **requested**, not what the cached object was compiled with, so the log cannot contradict it. When flag-gated behavior is suspected, force the recompile (`rm -rf ~/.cache/upp.out/<package>/<flagset dir>`), and after the build confirm the gate from the binary itself: `objdump -d --demangle` on the calling function and grep for the gated call.
- **Packages sharing an output binary overwrite each other.** Every mainconfig of a `MAIN` package links into `bin/CLANG/<package>`; `bin/build -m CLANG -M X11 DesktopManager` and `bin/build -m CLANG -M "X11 (Manager)" DesktopManager` write the same file, last one wins. Run the binary only after the build whose flags you intend to test was the last one, or you will test a different configuration and the behavior flaps between runs (observed for hours while the actual cause was build order).

### Bootstrap When `bin/build` Is Missing

`bin/build` vs `bin/build.exe` is platform-dependent — a fresh clone or a platform switch may have neither:

- POSIX: run `stdsrc/build/bootstrap.sh` — compiles `stdsrc/build/main.cpp` with a plain C++ compiler (no umk, no script/build.py needed), producing `bin/build`, then runs `bin/build --bootstrap`, which builds `bin/umk` via `uppsrc/umk/Makefile.linux64` / `Makefile.macos15`.
- Windows/MSVC: run `stdsrc/build/bootstrap.ps1` — the equivalent producing `bin/build.exe`.
- After modifying `stdsrc/build/main.cpp`, re-bootstrap with `bin/build --bootstrap` (see `ai/skills/build-upp-package.md`).

### Binary Output Location

- **Deprecated Legacy Path**: `<repo>/bin/<buildmethod>/<target>` (e.g. `bin/CLANG/Android`). This simplistic path discarded configuration flags (Debug/Release, Blitz, GUI/NOGUI) and package directories, causing flag/configuration collisions.
- **Full Technical Path (Non-colliding)**: Built executables go to `<repo>/bin/<CONFIG_KEY>/<target>` (e.g. `bin/CLANG-Blitz-Clang-Debug-Debug_Full-Linux-Posix-Shared/Android` or `bin/CLANG-Blitz-Clang-Debug-Debug_Full-Gui-Linux-Posix-Shared/App`).
  - `<CONFIG_KEY>` mirrors `umk`'s output signature under `$(UPPOUT)/<package>/<METHOD>-<FLAGS>/`.
  - Flags are sorted alphabetically, deduplicated, and converted to TitleCase (`InitCaps`).
- **User-Friendly Entrypoints**:
  - `bin/<target>` (e.g. `bin/Android`, `bin/App`)
  - `bin/<package_path>` (e.g. `bin/ide/Android`, `bin/DevEnv/App`) if the target package has a subdirectory.
  - **POSIX**: Created as relative symbolic links pointing to `<CONFIG_KEY>/<target>` and `../<CONFIG_KEY>/<target>`.
  - **Windows**: Copied as executables, because non-admin users cannot create symlinks without Developer Mode (`ERROR_PRIVILEGE_NOT_HELD`).
- Overridable on command line with `--output` / `-o`.
- **Exception**: `bin/build` / `bin/build.exe` and `uppbuilder` itself live directly in `bin/` — they are bootstrap tools, built before any build method exists.

### Platform Targets

- Supported platform combo: **Linux, Windows, macOS, FreeBSD** (macOS/FreeBSD verified rarely — treat as unverified until exercised).
- Additional targets: **Emscripten (wasm)** and **Android**, available per-package via mainconfigs (e.g. `uppsrc/Shell/Shell.upp` has "Have everything (Emscripten)"). Check `bin/build --list-mainconfigs <package>` before assuming a target works.

- Repository build tools (e.g., those under `bin/`) assume full filesystem access. Running them inside a sandboxed environment (read-only cache paths) causes permission failures in `~/.cache/upp.out`.
- AI agents must detect sandboxed execution before invoking `bin/build.exe`. If sandboxing is active (no write access to `~/.cache`), halt and report instead of attempting the build.
- **Windows Environment**: In Windows environments, `busybox` might be available and should be preferred for shell-like operations where standard Windows commands (cmd/PowerShell) might behave unexpectedly or when Unix-like behavior is needed (e.g., `busybox sh`, `busybox base64`).

See also: `ai/skills/build-upp-package.md`, `ai/skills/build-all-packages-and-document.md`.
