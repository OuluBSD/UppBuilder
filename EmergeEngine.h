#pragma once

#include "UppBuilder.h"
#include "EscBuildParser.h"
#include "OverlayManager.h"
#include "ProviderPlanner.h"
#include "ProfileManager.h"
#include <vector>
#include <set>
#include <map>

namespace UppBuilder {

struct EmergeItem {
    EscBuildInfo build_info;
    std::string atom;                  // e.g. "DevCpp/Eon"
    std::string resolved_repo_path;    // e.g. "/home/sblo/OuluBSD/Eon"
    bool is_already_cloned = false;
    bool is_virtual = false;
    ResolvedProvider virtual_provider; // if is_virtual
    std::vector<std::string> active_use_flags;
};

struct EmergePlan {
    std::vector<EmergeItem> items; // In topological build order (dependencies first)
    std::vector<std::string> circular_dependencies;
    std::string profile_name;
    TargetProfile profile;
    std::string error;
};

class EmergeEngine {
public:
    // Resolves dependencies and builds a topological merge plan for one or more atoms
    static bool Plan(const std::vector<std::string>& atoms,
                     const std::string& profile_name,
                     EmergePlan& out_plan,
                     const std::string& overlay_root = "");

    // Formats the plan in Portage style (e.g. "[ebuild   R   ] DevCpp/Eon-0.1.0 ...")
    static std::string FormatPlan(const EmergePlan& plan);

    // Executes the emerge plan:
    // 1. For virtuals: installs host packages (e.g. vcpkg/apt/brew) or reports plugin
    // 2. For repositories: clones/fetches into ~/<org>/<repo> if missing or update requested
    // 3. For packages: builds default_package with UppBuilder/umk with active profile flags
    static bool Execute(const EmergePlan& plan,
                        bool update = false,
                        bool verbose = false);

    // Resolves canonical local checkout path for a package:
    // Follows convention ~/<organization>/<repo_name>
    static std::string GetCanonicalPackageDir(const EscBuildInfo& info);
};

} // namespace UppBuilder
