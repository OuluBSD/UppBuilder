# Build With UppBuilder Skill

## Purpose

This skill guides AI agents and developers on building U++ packages and assemblies using **`uppbuilder`**, the OuluBSD meta-builder and `umk` driver.

`uppbuilder` replaces legacy shell wrappers and manual `umk` invocations. It automatically discovers multi-assembly dependencies (such as `BazaarCore`, `SuperIntelligence`, and `CommandCenter`), applies the **3-Pillar Bridge** (Package Redirects, File Overlays, Surgical Patches), and generates deterministic, non-colliding binary directories.

---

## Tool Availability & Location

`uppbuilder` is available at:
- `~/bin/uppbuilder` (recommended standard path)
- `/home/sblo/Dev/UppBuilder/uppbuilder` (or `bin/uppbuilder` within the `UppBuilder` repository)
- Globally on system `PATH` if configured.

If `uppbuilder` is missing on a fresh system, bootstrap it with zero external dependencies:
```bash
# POSIX (Linux, macOS, FreeBSD)
cd /path/to/UppBuilder && ./bootstrap.sh

# Windows (MSVC)
cd C:\path\to\UppBuilder && bootstrap.bat
```

---

## Core Build Workflow

### 1. Basic Package Compilation
Run `uppbuilder` from the root of any repository containing U++ packages:

```bash
# Build target in Debug mode (uses default method in ~/.config/u++/theide/default_method):
uppbuilder TargetPackage

# Build target in Release mode:
uppbuilder TargetPackage -r

# Force clean build before compiling:
uppbuilder TargetPackage -c

# Specify compiler method (CLANG, GCC, MSVS22x64...):
uppbuilder TargetPackage -m CLANG

# Parallel jobs (defaults to CPU core count):
uppbuilder TargetPackage -H 8

# Link with shared libraries:
uppbuilder TargetPackage -s

# Verbose output (prints resolved assembly roots, umk flags, and compile commands):
uppbuilder TargetPackage -v
```

### 2. Multi-Assembly Resolution
`uppbuilder` automatically constructs the multi-assembly search tree. When run from any repository, it scans and chains roots in the following priority:
1. `BazaarCore/patched-uppsrc` (patched upstream Core and foundation)
2. `BazaarCore/uppsrc` (modular packages: `Core/Ext`, `Docking`, `Terminal`, `Manager`, `VirtualGui`)
3. `SuperIntelligence/uppsrc` or `SuperIntelligence` (AI and agent infrastructure: `SweAgent`)
4. `CommandCenter/uppsrc` or `CommandCenter` (DropTerm and terminal multiplexing)
5. Current workspace roots: `uppsrc`, `upptst`, `src`, and `.`
6. Explicit extra roots passed via `-a <path>` / `--add-root <path>`

No manual nest configuration in TheIDE or `umk` is required.

---

## Verifying Build Success

1. **Strict Exit Codes**:
   - `0` indicates compilation and linking completed successfully.
   - Non-zero indicates failure.
2. **Log Verification**:
   - Successful builds end with:
     ```text
     OK. (mm:ss.cc)
     ==> UppBuilder: Build succeeded: /path/to/binary
     ==> UppBuilder: Entrypoint created: /path/to/bin/TargetPackage -> ...
     ```
   - Errors will output:
     ```text
     There were errors. (mm:ss.cc)
     ==> UppBuilder: Build failed with exit code 1
     ```

---

## Output Binary Locations

`uppbuilder` enforces non-colliding, deterministic output paths:

1. **Technical Binary**:
   `<repo>/bin/<CONFIG_KEY>/<TargetPackage>`
   *Example*: `bin/CLANG-Blitz-Clang-Debug-Debug_Full-Linux-Mt-Posix-Shared/ManagerSkylark`
   *Config Key*: Formed by alphabetically sorted, TitleCase flags matching `umk` caching.

2. **User-Friendly Entrypoints**:
   - `bin/<TargetPackage>`: Relative symbolic link pointing directly to the technical binary.
   - `bin/<SubDir>/<TargetPackage>`: Created if the package lives in a subfolder (e.g., `examples/WebShowcase` -> `bin/examples/WebShowcase`).
   - *Windows Note*: On Windows without Developer Mode, entrypoints are copied automatically rather than symlinked.

---

## 3-Pillar Bridge Usage

The 3-Pillar Bridge enables compiling against pristine upstream checkouts without modifying vendor source code directly.

### Pillar 1: Package Redirection
Maps legacy package names in `.upp` `uses` directives to modular subpackages:
```bash
# Command line:
uppbuilder TargetPackage --redirect uppsrc/CtrlCore=uppsrc/Ctrl/Core

# Or from JSON configuration:
uppbuilder TargetPackage --redirects-file redirects.json
```

### Pillar 2: File Overlays
Injects replacement files into a virtual staged build:
```bash
# Command line:
uppbuilder TargetPackage --overlay uppsrc/Core/config.h=custom/Core/config.h

# Or from JSON configuration:
uppbuilder TargetPackage --overlays-file overlays.json
```

### Pillar 3: Surgical Patches
Applies unified diffs to the staged workspace before driving `umk`:
```bash
uppbuilder TargetPackage --patch patches/0001-upphub-fix.patch
```

---

## Foundation Assembly Materialization

When maintaining `BazaarCore` or synchronizing with upstream `ultimatepp`:

### 1. Apply Patches & Restructure (`--apply-patches`)
Materializes `patched-uppsrc` from pristine `ultimatepp/uppsrc` and applies all surgical patches:
```bash
cd /path/to/BazaarCore
uppbuilder --apply-patches \
  --upstream /path/to/ultimatepp/uppsrc \
  --applied-patches /path/to/BazaarCore/patched-uppsrc \
  -v
```

### 2. Generate Surgical Patches (`--make-patches`)
Reverse-diffs modifications made in `patched-uppsrc` back into unified diffs inside `patches/`:
```bash
cd /path/to/BazaarCore
uppbuilder --make-patches \
  --upstream /path/to/ultimatepp/uppsrc \
  --applied-patches /path/to/BazaarCore/patched-uppsrc
```

---

## Inspection & Architecture Tools

### 1. Dependency Tree Inspection (`--dump-deps`)
Prints the resolved topological build order taking into account conditional flags:
```bash
uppbuilder --dump-deps TargetPackage
```

### 2. Architecture Graph Generation (`--graph`)
Exports the package dependency DAG in Mermaid, DOT, or JSON format:
```bash
# Generate Mermaid diagram:
uppbuilder --graph mermaid TargetPackage

# Generate Graphviz DOT file:
uppbuilder --graph dot TargetPackage > deps.dot

# Output machine-readable JSON:
uppbuilder --graph json TargetPackage
```

### 3. Header Coupling Analysis (`--scan-headers`)
Scans `#include` directives across all packages to detect undeclared package coupling:
```bash
# Scan all packages and print console summary:
uppbuilder --scan-headers

# Focus on a specific package:
uppbuilder --scan-headers Core

# Export full coupling analysis to Markdown:
uppbuilder --scan-headers Core --report-headers core-coupling.md
```

### 4. Build Methods & Mainconfigs Discovery
```bash
# List all registered build methods in ~/.config/u++/theide:
uppbuilder --list-methods

# List all mainconfig configurations defined in a package:
uppbuilder --list-mainconfigs TargetPackage
```

---

## Troubleshooting Guide

| Issue | Cause | Resolution |
|-------|-------|------------|
| `Could not locate umk executable` | `umk` is not in `~/bin` or `PATH`. | Run `UppBootstrap/bootstrap.sh` or pass `--umk /path/to/umk`. |
| `Cannot invoke compiler 'em++'` | `default_method` was set to `EMCC_WASM`. | Explicitly pass `-m CLANG` or write `CLANG` into `~/.config/u++/theide/default_method`. |
| `Package 'X' does not exist in assembly roots` | Package name misspelled or missing sibling repository. | Check suggestion list ("Did you mean..."), or pass `-a /path/to/repo`. |
| Circular package dependencies | Two or more packages declare mutual `uses(...)`. | Run `uppbuilder --dump-deps <pkg>` to identify the cycle loop and extract common logic to a lower-level package. |
| Stale compiler cache | Cached objects in `~/.cache/upp.out` compiled with different flags. | Run with `-c` / `--clean`, or remove `~/.cache/upp.out/<pkg>/`. |
