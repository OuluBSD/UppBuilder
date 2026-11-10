#pragma once

#include "UppBuilder.h"

namespace UppBuilder {

struct MainConfigEntry {
    std::string name;
    std::string flags;
};

struct UppPackage {
    std::string name;               // e.g. "Core" or "Ctrl/Core"
    std::string path;               // full path to .upp file
    std::string description;
    std::string target;
    std::vector<MainConfigEntry> mainconfigs;
    std::vector<std::string> unconditional_uses;
    std::vector<std::pair<std::string, std::vector<std::string>>> conditional_uses; // condition -> list of pkgs
    std::vector<std::string> acceptflags;
    std::vector<std::string> files;
    std::vector<std::string> includes;
    std::vector<std::string> libraries;
    std::vector<std::string> links;
};

class UppParser {
public:
    static bool Parse(const std::string& upp_path, UppPackage& out_pkg);
    
    // Evaluates conditional uses against the active flags set
    static std::vector<std::string> ResolveActiveUses(const UppPackage& pkg, const std::set<std::string>& active_flags);

    // Boolean expression evaluator: handles IDENT, !, &, |, (, )
    static bool EvaluateCondition(const std::string& expr, const std::set<std::string>& active_flags);

    // Parses a comma-separated or space-separated flag string into a normalized set
    static std::set<std::string> ParseFlags(const std::string& flags_str);
};

} // namespace UppBuilder
