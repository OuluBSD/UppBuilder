# AGENTS - UppBuilder Guide

**Target Audience**: AI agents and developers building, maintaining, or extending `UppBuilder`.

---

## 🏛️ Role in the Ecosystem

`UppBuilder` is the **Tier 2 smart meta-builder and `umk` orchestrator** in the OuluBSD U++ modularization hierarchy:

```
UppBootstrap  -->  UppBuilder  -->  BazaarCore  -->  Downstream Repos
(Stage 1 umk)     (3-Pillar)        (Foundation)      (SkylarkManager, SuperIntelligence, DevEnv...)
```

1. **`UppBootstrap`**: Bootstraps the primitive `umk` compiler from pristine C++ source.
2. **`UppBuilder` (this repo)**: Orchestrates builds, evaluates `.upp` manifests with conditional flags, runs the **3-Pillar Bridge** (Redirects, Overlays, Patches), materializes `patched-uppsrc`, generates dependency/coupling graphs, and manages deterministic output binaries.
3. **`BazaarCore`**: Supplies the modular foundation assembly (`patched-uppsrc` + `uppsrc/Core/Ext` + Tier 3 packages).
4. **Downstream Repos**: Consumes `UppBuilder` for single-command builds with automatic multi-assembly resolution.

---

## 🔒 Essential Invariants

1. **Zero External Runtime Dependencies**:
   - `UppBuilder` must compile with standard C++17 compilers (`clang++`, `g++`, `cl.exe`) without requiring Python, Perl, Node, or external libraries.
   - Bootstrappable standalone via `./bootstrap.sh` (POSIX) or `bootstrap.bat` (Windows).
2. **Strict Exit Codes**:
   - Return `0` strictly when build or analysis succeeds.
   - Return non-zero (`1`) on any compiler failure, link failure, circular dependency failure, or missing package. Never swallow errors.
3. **Deterministic Output Paths**:
   - All technical binaries are placed into `bin/<CONFIG_KEY>/<target>` (e.g., `bin/CLANG-Blitz-Clang-Debug-Debug_Full-Linux-Mt-Posix-Shared/ManagerSkylark`).
   - User-friendly entrypoints (`bin/<target>` and `bin/<subpath>/<target>`) are created as relative symlinks on POSIX, or copied on Windows when unprivileged.
4. **Non-Destructive Upstream**:
   - Upstream `ultimatepp/uppsrc` is never modified directly. Changes are projected via staging overlays or materialized into `BazaarCore/patched-uppsrc`.

---

## 🧩 Component & Class Map

| Component | Files | Description & Responsibilities |
|-----------|-------|--------------------------------|
| **CLI & Dispatch** | [`main.cpp`](file:///home/sblo/Dev/UppBuilder/main.cpp) | Argument parsing, sibling repo auto-discovery, assembly root ordering, exit code orchestration. |
| **Manifest Parser** | [`UppParser.h`](file:///home/sblo/Dev/UppBuilder/UppParser.h), [`UppParser.cpp`](file:///home/sblo/Dev/UppBuilder/UppParser.cpp) | Parses `.upp` packages, extracts flags, files, mainconfigs, and evaluates conditional `uses(FLAG)`. |
| **3-Pillar Bridge** | [`Bridge.h`](file:///home/sblo/Dev/UppBuilder/Bridge.h), [`Bridge.cpp`](file:///home/sblo/Dev/UppBuilder/Bridge.cpp) | Implements Package Redirection (`redirects.json`), File Overlays (`overlays.json`), and Surgical Patches (`patches/`). Materializes `patched-uppsrc` and staging workspaces. |
| **Dependency Graph**| [`DepGraph.h`](file:///home/sblo/Dev/UppBuilder/DepGraph.h), [`DepGraph.cpp`](file:///home/sblo/Dev/UppBuilder/DepGraph.cpp) | Builds active package dependency DAGs, detects cycles, topological sorts, and exports to Mermaid, Graphviz DOT, or JSON. |
| **Header Coupling** | [`HeaderScanner.h`](file:///home/sblo/Dev/UppBuilder/HeaderScanner.h), [`HeaderScanner.cpp`](file:///home/sblo/Dev/UppBuilder/HeaderScanner.cpp) | Scans `#include` directives across assemblies, maps headers to packages, identifies undeclared dependencies, and generates coupling markdown reports. |
| **Flag Auditor** | [`FlagAuditor.h`](file:///home/sblo/OuluBSD/UppBuilder/FlagAuditor.h), [`FlagAuditor.cpp`](file:///home/sblo/OuluBSD/UppBuilder/FlagAuditor.cpp) | Portage-style package flag auditor: scans code flags (`flagFLAG`), compares against `.upp` `acceptflags`, identifies unaccepted code flags, and generates patches. |
| **USE Model** | [`UseModel.h`](file:///home/sblo/OuluBSD/UppBuilder/UseModel.h), [`UseModel.cpp`](file:///home/sblo/OuluBSD/UppBuilder/UseModel.cpp) | Semantic USE flags, scopes (`global`, `accepted`, `main_only`), transitions, conflict detection, and compilation flag projection. |
| **Policy Engine** | [`PolicyEngine.h`](file:///home/sblo/OuluBSD/UppBuilder/PolicyEngine.h), [`PolicyEngine.cpp`](file:///home/sblo/OuluBSD/UppBuilder/PolicyEngine.cpp) | Portage-style `.uppbuilder/` hierarchy parser (`make.conf`, `package.use`, `package.mask`, `package.force`). |
| **EscBuild Parser** | [`EscBuildParser.h`](file:///home/sblo/OuluBSD/UppBuilder/EscBuildParser.h), [`EscBuildParser.cpp`](file:///home/sblo/OuluBSD/UppBuilder/EscBuildParser.cpp) | U++ Esc-syntax parser extracting `.escbuild` metadata, functions (`src_unpack`, `src_compile`, `src_install`), providers, and dependencies. |
| **Profile Manager** | [`ProfileManager.h`](file:///home/sblo/OuluBSD/UppBuilder/ProfileManager.h), [`ProfileManager.cpp`](file:///home/sblo/OuluBSD/UppBuilder/ProfileManager.cpp) | Target profile resolution (`NativeClang`, `NativeGcc`, `MsvcX64`, `WasmBrowser`), profile inheritance, and shared global build cache resolution (`~/.cache/upp.out`, `%LOCALAPPDATA%/u++/out`). |
| **Provider Planner** | [`ProviderPlanner.h`](file:///home/sblo/OuluBSD/UppBuilder/ProviderPlanner.h), [`ProviderPlanner.cpp`](file:///home/sblo/OuluBSD/UppBuilder/ProviderPlanner.cpp) | Cross-platform capability resolver for `Virtual/*` dependencies (`vcpkg`, `apt-get`, `brew`, `plugin/*`). |
| **Overlay Manager** | [`OverlayManager.h`](file:///home/sblo/OuluBSD/UppBuilder/OverlayManager.h), [`OverlayManager.cpp`](file:///home/sblo/OuluBSD/UppBuilder/OverlayManager.cpp) | Portage-style overlay discovery, package traversal across CapitalCase categories, package search, and git overlay synchronization. |
| **Umk Driver** | [`UmkDriver.h`](file:///home/sblo/Dev/UppBuilder/UmkDriver.h), [`UmkDriver.cpp`](file:///home/sblo/Dev/UppBuilder/UmkDriver.cpp) | Discovers `umk`, scans `~/.config/u++/theide/*.bm` build methods, generates canonical config keys, and executes compilation jobs. |
| **Runtime Harvester**| [`RuntimeDeps.h`](file:///home/sblo/Dev/UppBuilder/RuntimeDeps.h), [`RuntimeDeps.cpp`](file:///home/sblo/Dev/UppBuilder/RuntimeDeps.cpp) | Recursively scans ELF `DT_NEEDED`, PE import tables, or Mach-O `LC_LOAD_DYLIB` to bundle dynamic dependencies with `--copy-deps`. |
| **Shared Utilities**| [`UppBuilder.h`](file:///home/sblo/Dev/UppBuilder/UppBuilder.h), [`UppBuilder.cpp`](file:///home/sblo/Dev/UppBuilder/UppBuilder.cpp) | Cross-platform filesystem helpers, string processing, child process spawning, and entrypoint creation. |

---

## 🔍 Multi-Assembly Resolution Order

When building a package from any workspace, `UppBuilder` auto-resolves sibling repositories in the following order:

1. **`BazaarCore/patched-uppsrc`**: Patched upstream Core and foundation assemblies.
2. **`BazaarCore/uppsrc`**: Modular packages (`Core/Ext`, `Docking`, `Terminal`, `Manager`, `VirtualGui`...).
3. **`SuperIntelligence/uppsrc`** (or `SuperIntelligence`): AI/LLM agent packages (`SweAgent`, etc.).
4. **`CommandCenter/uppsrc`** (or `CommandCenter`): DropTerm and terminal multiplexing.
5. **Target Repository**: Local roots (`uppsrc`, `upptst`, `src`, `.`).
6. **Command-Line Overrides**: `--add-root <dir>` / `-a <dir>`.

`--uppsrc-dir <path>` overrides the first assembly root (and patched-root discovery) without changing the `--upstream` source used for patch materialization.

---

## 🛠️ Common Recipes & Commands

### 1. Building Packages
```bash
# Debug build (default method)
uppbuilder TargetPackage

# Explicit compiler method and release mode
uppbuilder TargetPackage -m CLANG -r

# Multi-threaded build with specific job count
uppbuilder TargetPackage -m GCC -H 8 -v

# Link shared libraries
uppbuilder TargetPackage -s
```

### 2. Dependency & Coupling Inspection
```bash
# Print resolved dependency list in topological build order:
uppbuilder --dump-deps TargetPackage

# Generate Mermaid architecture diagram:
uppbuilder --graph mermaid TargetPackage

# Scan header coupling and export markdown report:
uppbuilder --scan-headers Core --report-headers core-coupling-report.md
```

### 3. Portage USE Management & Auditing
```bash
# Explain a semantic USE flag:
uppbuilder explain-use sqlite
uppbuilder explain-use virtualgui

# Inspect effective USE model and U++ flag projections for a package:
uppbuilder use TargetPackage --use "+sqlite -gtk st"

# Audit package acceptflags against code usages:
uppbuilder --audit-flags TargetPackage
uppbuilder --audit-flags --audit-patch

# Build package applying USE flags:
uppbuilder TargetPackage --use "+sqlite +virtualgui"
```

### 4. Assembly Patching & Materialization
```bash
# Materialize patched-uppsrc from pristine upstream ultimatepp + patches:
uppbuilder --apply-patches --upstream ../ultimatepp/uppsrc --applied-patches ../BazaarCore/patched-uppsrc -v

# Reverse-diff modified packages into surgical patch files:
uppbuilder --make-patches --upstream ../ultimatepp/uppsrc --applied-patches ../BazaarCore/patched-uppsrc
```

### 5. Target Profiles & UppOverlay Catalog
```bash
# List all target profiles:
uppbuilder profile list

# Show detailed profile configuration:
uppbuilder profile show NativeClang

# List all virtual capability providers:
uppbuilder virtual list

# Resolve a virtual capability to host package manager command:
uppbuilder virtual resolve Virtual/Sqlite

# List all packages indexed in UppOverlay:
uppbuilder overlay list

# Inspect an overlay package from its .escbuild specification:
uppbuilder overlay info DevCpp/BazaarCore
uppbuilder overlay info DevUtil/UppBuilder

# Build with a specific target profile (overriding method, USE flags, and cache):
uppbuilder TargetPackage --profile NativeClang
```
