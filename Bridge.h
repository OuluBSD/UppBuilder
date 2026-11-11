#pragma once

#include "UppBuilder.h"

namespace UppBuilder {

struct OverlayEntry {
    std::string target;       // e.g. "uppsrc/Core/config.h"
    std::string source;       // e.g. "overlay/Core/config.h"
    std::vector<std::string> platforms; // e.g. ["posix", "win32", "bsd", "all"]
};

struct PatchEntry {
    std::string patch_file;   // path to .patch or .diff
    std::string target_dir;   // relative to staged root
    int strip_level = 1;      // -p1 by default
    std::string description;
};

struct RestructureRule {
    std::string upstream_pkg;    // e.g. "CtrlCore"
    std::string staged_rel_path; // e.g. "Ctrl/Core"
    std::string staged_upp_name; // e.g. "Core.upp"
    std::string patch_file;      // e.g. "CtrlCore.patch"
};

class BridgeEngine {
public:
    BridgeEngine();

    // Package Restructuring (e.g. CtrlCore -> Ctrl/Core, CtrlLib -> Ctrl/Lib)
    void AddRestructureRule(const std::string& upstream_pkg,
                            const std::string& staged_rel_path,
                            const std::string& staged_upp_name = "",
                            const std::string& patch_file = "");
    const std::vector<RestructureRule>& GetRestructureRules() const { return restructure_rules_; }

    // Pillar 1: Package Redirection
    void AddRedirect(const std::string& from, const std::string& to);
    std::string ResolveRedirect(const std::string& pkg_path) const;
    std::string ReverseRedirect(const std::string& pkg_path) const;
    bool LoadRedirectsFile(const std::string& json_path);
    const std::map<std::string, std::string>& GetRedirects() const { return redirects_; }

    // Pillar 2: File Overlays
    void AddOverlay(const std::string& target, const std::string& source, const std::vector<std::string>& platforms = {"all"});
    bool LoadOverlaysFile(const std::string& json_path);
    const std::vector<OverlayEntry>& GetOverlays() const { return overlays_; }

    // Pillar 3: Surgical Patches
    void AddPatch(const std::string& patch_file, const std::string& target_dir = "", int strip = 1, const std::string& desc = "");
    bool LoadPatchesFile(const std::string& json_path);
    const std::vector<PatchEntry>& GetPatches() const { return patches_; }

    // Staging / Virtual Workspace Materialization
    // Creates or updates a staged overlay assembly at `staging_dir` combining source roots, overlays, and patches.
    bool MaterializeStagedAssembly(const std::string& repo_root,
                                  const std::vector<std::string>& source_roots,
                                  const std::string& staging_dir,
                                  std::vector<std::string>& out_effective_roots,
                                  bool verbose = false);

    // Staged applied-patches directory lifecycle
    // Populates applied_dir with restructured and patched upstream packages
    bool MaterializeAppliedPatches(const std::string& upstream_uppsrc,
                                  const std::string& applied_dir,
                                  const std::string& patches_dir,
                                  bool force = false,
                                  bool verbose = false);

    // Materializes a specific upstream assembly (e.g. "uppsrc", "reference", "tutorial", "examples", "upptst", "autotest")
    // into applied_dir with include/manifest rewriting and surgical patch application.
    bool MaterializeAssembly(const std::string& upstream_assembly_dir,
                            const std::string& applied_dir,
                            const std::string& patches_dir = "",
                            bool force = false,
                            bool verbose = false);

    // Materializes all standard upstream assemblies into base_bazaarcore_dir:
    // uppsrc -> patched-uppsrc / applied-patches
    // reference -> applied-reference
    // tutorial -> applied-tutorial
    // examples -> applied-examples
    // upptst -> applied-upptst
    // autotest -> applied-autotest
    bool MaterializeAllAssemblies(const std::string& upstream_root,
                                 const std::string& base_bazaarcore_dir,
                                 const std::string& patches_dir = "",
                                 bool force = false,
                                 bool verbose = false);

    // Manifest & header dependency rewriter:
    // Rewrites uses CtrlCore -> Ctrl/Core, uses CtrlLib -> Ctrl/Lib in all .upp files
    // Rewrites #include <CtrlCore/ -> <Ctrl/Core/, <CtrlLib/ -> <Ctrl/Lib/ in headers and sources
    bool RewriteManifestsAndIncludes(const std::string& target_uppsrc, bool verbose = false);

    // Creates compatibility forwarding shims for CtrlCore and CtrlLib
    bool CreateCompatibilityShims(const std::string& target_uppsrc, bool verbose = false);

    // Reverse-diff: Diffs applied_dir against pristine upstream and writes clean diffs to patches_dir
    bool GeneratePatches(const std::string& upstream_uppsrc,
                         const std::string& applied_dir,
                         const std::string& patches_dir,
                         bool verbose = false);

    // Audits surgical patches against upstream pristine sources and reports cleanliness / drift
    bool AuditUpstreamPatches(const std::string& upstream_uppsrc,
                             const std::string& applied_dir,
                             const std::string& patches_dir,
                             const std::string& target_pkg = "",
                             bool verbose = false);

private:
    std::vector<RestructureRule> restructure_rules_;
    std::map<std::string, std::string> redirects_;
    std::map<std::string, std::string> reverse_redirects_;
    std::vector<OverlayEntry> overlays_;
    std::vector<PatchEntry> patches_;

    bool PlatformMatches(const std::vector<std::string>& platforms) const;
    bool ApplySinglePatch(const std::string& patch_path, const std::string& target_dir, int strip, bool verbose);
};

} // namespace UppBuilder
