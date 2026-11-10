#pragma once

#include "UppBuilder.h"
#include "UseModel.h"

namespace UppBuilder {

struct PolicyRule {
    std::string file;
    int line = 0;
    std::string atom;                // Package pattern: "*", "Core", "CtrlLib", "examples/AuditTestPkg"
    std::vector<std::string> flags;  // USE flags or arguments
};

struct LocalPolicy {
    std::string policy_dir;
    std::string make_conf_path;
    std::string package_use_path;
    std::string package_mask_path;
    std::string package_force_path;

    // Directives from make.conf
    std::string make_use;
    std::string make_profile;
    std::string make_compiler;
    std::string make_target;
    int make_jobs = 0;

    // Rules from package.*
    std::vector<PolicyRule> package_use;
    std::vector<PolicyRule> package_mask;
    std::vector<PolicyRule> package_force;

    std::vector<std::string> warnings;
};

class PolicyEngine {
public:
    // Finds the .uppbuilder directory (or legacy .ai-upp) searching up from start_dir
    static std::string FindPolicyDir(const std::string& start_dir);

    // Loads policy files (make.conf, package.use, package.mask, package.force) from policy_dir
    static LocalPolicy LoadLocalPolicy(const std::string& policy_dir);

    // Checks if a policy rule's atom matches a given package name or relative path
    static bool RuleMatches(const std::string& rule_atom, const std::string& package_name);

    // Collects package.use flags for a specific package
    static std::vector<std::string> GetPackageUseFlags(const LocalPolicy& policy, const std::string& package_name);

    // Collects forced flags for a package
    static std::vector<std::string> GetForcedFlags(const LocalPolicy& policy, const std::string& package_name);

    // Collects masked flags for a package
    static std::vector<std::string> GetMaskedFlags(const LocalPolicy& policy, const std::string& package_name);

    // Combines make.conf USE + package.use + CLI --use into an ordered vector of USE arguments
    static std::vector<std::string> CombineUseArgs(const LocalPolicy& policy,
                                                   const std::string& package_name,
                                                   const std::string& cli_use);
};

} // namespace UppBuilder
