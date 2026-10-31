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

class BridgeEngine {
public:
    BridgeEngine();

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

private:
    std::map<std::string, std::string> redirects_;
    std::map<std::string, std::string> reverse_redirects_;
    std::vector<OverlayEntry> overlays_;
    std::vector<PatchEntry> patches_;

    bool PlatformMatches(const std::vector<std::string>& platforms) const;
    bool ApplySinglePatch(const std::string& patch_path, const std::string& target_dir, int strip, bool verbose);
};

} // namespace UppBuilder
