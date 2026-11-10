#include "PolicyEngine.h"

namespace UppBuilder {

static std::string StripComment(const std::string& line) {
    size_t hash_pos = line.find('#');
    if (hash_pos != std::string::npos) {
        return Trim(line.substr(0, hash_pos));
    }
    return Trim(line);
}

static std::string Unquote(const std::string& val) {
    std::string s = Trim(val);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::string PolicyEngine::FindPolicyDir(const std::string& start_dir) {
    std::string search = GetAbsolutePath(start_dir);
    while (!search.empty()) {
        std::string cand = JoinPath(search, ".uppbuilder");
        if (DirectoryExists(cand)) {
            return cand;
        }
        std::string cand_legacy = JoinPath(search, ".ai-upp");
        if (DirectoryExists(cand_legacy)) {
            return cand_legacy;
        }
        std::string parent = GetParentDir(search);
        if (parent == search || parent.empty()) break;
        search = parent;
    }
    return "";
}

static void ParsePolicyRuleFile(const std::string& path, std::vector<PolicyRule>& dst, std::vector<std::string>& warnings) {
    if (path.empty() || !FileExists(path)) return;

    std::string content = ReadTextFile(path);
    std::vector<std::string> lines = Split(content, '\n');

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string line = StripComment(lines[i]);
        if (line.empty()) continue;

        std::vector<std::string> words = UseModelEngine::ParseUseString(line);
        if (words.empty()) continue;

        PolicyRule rule;
        rule.file = path;
        rule.line = static_cast<int>(i + 1);
        rule.atom = words[0];

        for (size_t j = 1; j < words.size(); ++j) {
            rule.flags.push_back(words[j]);
        }
        dst.push_back(rule);
    }
}

LocalPolicy PolicyEngine::LoadLocalPolicy(const std::string& policy_dir) {
    LocalPolicy policy;
    if (policy_dir.empty() || !DirectoryExists(policy_dir)) {
        return policy;
    }

    policy.policy_dir = policy_dir;
    policy.make_conf_path = JoinPath(policy_dir, "make.conf");
    policy.package_use_path = JoinPath(policy_dir, "package.use");
    policy.package_mask_path = JoinPath(policy_dir, "package.mask");
    policy.package_force_path = JoinPath(policy_dir, "package.force");

    // Parse make.conf
    if (FileExists(policy.make_conf_path)) {
        std::string content = ReadTextFile(policy.make_conf_path);
        std::vector<std::string> lines = Split(content, '\n');

        for (size_t i = 0; i < lines.size(); ++i) {
            std::string line = StripComment(lines[i]);
            if (line.empty()) continue;

            size_t eq = line.find('=');
            if (eq == std::string::npos) {
                policy.warnings.push_back(policy.make_conf_path + ":" + std::to_string(i + 1) + ": malformed directive (missing '=')");
                continue;
            }

            std::string key = ToUpper(Trim(line.substr(0, eq)));
            std::string val = Unquote(line.substr(eq + 1));

            if (key == "USE") {
                policy.make_use = val;
            } else if (key == "PROFILE") {
                policy.make_profile = val;
            } else if (key == "COMPILER") {
                policy.make_compiler = val;
            } else if (key == "TARGET") {
                policy.make_target = val;
            } else if (key == "JOBS") {
                policy.make_jobs = std::atoi(val.c_str());
            }
        }
    }

    ParsePolicyRuleFile(policy.package_use_path, policy.package_use, policy.warnings);
    ParsePolicyRuleFile(policy.package_mask_path, policy.package_mask, policy.warnings);
    ParsePolicyRuleFile(policy.package_force_path, policy.package_force, policy.warnings);

    return policy;
}

bool PolicyEngine::RuleMatches(const std::string& rule_atom, const std::string& package_name) {
    std::string r = ToLower(Trim(rule_atom));
    std::string p = ToLower(Trim(package_name));

    if (r == "*" || r == "all") return true;
    if (r == p) return true;

    // Normalize slashes
    std::replace(r.begin(), r.end(), '\\', '/');
    std::replace(p.begin(), p.end(), '\\', '/');
    if (r == p) return true;

    // Check basename / title match (e.g. "AuditTestPkg" matching "examples/AuditTestPkg")
    std::string p_title = GetFileTitle(p);
    if (r == p_title) return true;

    // Wildcard prefix / suffix match: e.g. "Ctrl/*" or "*/AuditTestPkg"
    if (EndsWith(r, "/*")) {
        std::string prefix = r.substr(0, r.size() - 1); // e.g. "ctrl/"
        if (StartsWith(p, prefix)) return true;
    }

    return false;
}

std::vector<std::string> PolicyEngine::GetPackageUseFlags(const LocalPolicy& policy, const std::string& package_name) {
    std::vector<std::string> flags;
    for (const auto& rule : policy.package_use) {
        if (RuleMatches(rule.atom, package_name)) {
            for (const auto& f : rule.flags) {
                flags.push_back(f);
            }
        }
    }
    return flags;
}

std::vector<std::string> PolicyEngine::GetForcedFlags(const LocalPolicy& policy, const std::string& package_name) {
    std::vector<std::string> flags;
    for (const auto& rule : policy.package_force) {
        if (RuleMatches(rule.atom, package_name)) {
            for (const auto& f : rule.flags) {
                flags.push_back(f);
            }
        }
    }
    return flags;
}

std::vector<std::string> PolicyEngine::GetMaskedFlags(const LocalPolicy& policy, const std::string& package_name) {
    std::vector<std::string> flags;
    for (const auto& rule : policy.package_mask) {
        if (RuleMatches(rule.atom, package_name)) {
            for (const auto& f : rule.flags) {
                flags.push_back(f);
            }
        }
    }
    return flags;
}

std::vector<std::string> PolicyEngine::CombineUseArgs(const LocalPolicy& policy,
                                                      const std::string& package_name,
                                                      const std::string& cli_use) {
    std::vector<std::string> combined;

    // 1. make.conf USE
    if (!policy.make_use.empty()) {
        std::vector<std::string> make_tokens = UseModelEngine::ParseUseString(policy.make_use);
        for (const auto& t : make_tokens) combined.push_back(t);
    }

    // 2. package.use
    std::vector<std::string> pkg_tokens = GetPackageUseFlags(policy, package_name);
    for (const auto& t : pkg_tokens) combined.push_back(t);

    // 3. CLI --use overrides
    if (!cli_use.empty()) {
        std::vector<std::string> cli_tokens = UseModelEngine::ParseUseString(cli_use);
        for (const auto& t : cli_tokens) combined.push_back(t);
    }

    return combined;
}

} // namespace UppBuilder
