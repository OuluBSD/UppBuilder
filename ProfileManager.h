#pragma once

#include "UppBuilder.h"
#include <map>
#include <vector>

namespace UppBuilder {

struct TargetProfile {
    std::string name;             // e.g. "NativeClang", "NativeGcc", "MsvcX64", "WasmBrowser"
    std::string path;             // Directory path containing make.conf
    std::string build_method;     // e.g. "CLANG", "GCC", "MSVS22x64", "EMCC"
    std::string default_use;      // e.g. "blitz posix linux"
    std::string output_cache;     // e.g. "~/.cache/upp.out"
    std::vector<std::string> parent_profiles;
    std::map<std::string, std::string> vars;
};

class ProfileManager {
public:
    // Resolves default profile name based on host OS and available compiler
    static std::string GetDefaultProfileName();

    // Resolves active profile (from ~/.config/uppbuilder/active_profile or default)
    static std::string GetActiveProfileName(const std::string& overlay_path = "");

    // Sets active profile name in configuration
    static bool SetActiveProfileName(const std::string& profile_name);

    // Resolves default shared global cache path (~/.cache/upp.out or %LOCALAPPDATA%/u++/out)
    static std::string GetDefaultOutputCache();

    // Lists all available target profile names found in overlay or built-in defaults
    static std::vector<std::string> ListProfiles(const std::string& overlay_path = "");

    // Loads a profile by name (evaluating base make.conf inheritance and target make.conf)
    static TargetProfile LoadProfile(const std::string& profile_name, const std::string& overlay_path = "");

    // Returns a built-in fallback profile if not found on disk
    static TargetProfile GetBuiltinProfile(const std::string& profile_name);
};

} // namespace UppBuilder
