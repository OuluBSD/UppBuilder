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

struct TestResult {
    std::string atom;
    std::string suite;
    bool success = false;
    long duration_ms = 0;
    std::string message;
};

struct TestSummary {
    std::vector<TestResult> results;
    int passed = 0;
    int failed = 0;
    int skipped = 0;
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
    // 4. Records installed package into VDB
    // 5. If run_tests is true: runs src_test() for emerged packages
    static bool Execute(const EmergePlan& plan,
                        bool update = false,
                        bool verbose = false,
                        bool run_tests = false);

    // Safely unmerges packages from VDB and deletes recorded artifacts
    static bool Unmerge(const std::vector<std::string>& atoms,
                        bool force = false,
                        bool verbose = false);

    // Executes tests for specified atoms, @installed, @all, @autotest, or @upptst
    static bool RunTests(const std::vector<std::string>& atoms,
                         TestSummary& out_summary,
                         bool verbose = false,
                         const std::string& overlay_root = "",
                         const std::string& bazaarcore_root = "");

    // Executes test for a single package given its EscBuildInfo and repo directory
    static bool ExecuteSingleTest(const EscBuildInfo& info,
                                 const std::string& repo_dir,
                                 TestResult& out_result,
                                 bool verbose = false);

    // Resolves canonical local checkout path for a package:
    // Follows convention ~/<organization>/<repo_name>
    static std::string GetCanonicalPackageDir(const EscBuildInfo& info);
};

} // namespace UppBuilder
