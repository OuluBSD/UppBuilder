#pragma once

#include "UppBuilder.h"
#include "UppParser.h"

namespace UppBuilder {

enum class UseScope {
    GLOBAL,    // Passed as global compiler flag to all packages in build graph (e.g. ST, SO)
    ACCEPTED,  // Projected only to packages accepting this flag in acceptflags (e.g. SQLITE, GTK)
    MAIN_ONLY  // Projected only to the top-level main package
};

struct UsePolicyMap {
    std::string upp_flag;
    UseScope scope = UseScope::ACCEPTED;
    std::string thread_model;
};

struct UsePolicy {
    std::string name;             // e.g. "sqlite", "st", "virtualgui"
    std::string description;
    bool default_on = false;
    std::vector<UsePolicyMap> maps_to;
};

struct UseTransition {
    std::string flag;
    std::string marker; // "+" or "-"
    std::string reason; // "selected", "default", "forced", "masked", "disabled"
};

struct UseModel {
    std::vector<std::string> requested;
    std::vector<std::string> declared;
    std::vector<std::string> defaults;
    std::vector<std::string> forced;
    std::vector<std::string> masked;
    std::vector<std::string> conflicts;
    std::vector<std::string> selected;
    std::vector<std::string> disabled;
    std::vector<std::string> effective;
    std::vector<UseTransition> transitions;
};

struct UppFlag {
    std::string name;
    UseScope scope = UseScope::GLOBAL;
    std::string reason;
};

struct UppProjection {
    std::vector<UppFlag> flags;
    std::vector<std::string> global;
    std::vector<std::string> accepted;
    std::vector<std::string> main_only;
};

class UseModelEngine {
public:
    // Returns built-in standard semantic policy definitions
    static const std::vector<UsePolicy>& GetBuiltinPolicies();

    // Finds a policy by semantic name (case-insensitive)
    static const UsePolicy* FindPolicy(const std::string& name);

    // Converts a scope name string to UseScope enum
    static UseScope ScopeFromText(const std::string& text);

    // Converts UseScope enum to text
    static std::string ScopeToText(UseScope scope);

    // Splits a space/comma separated USE string into token arguments
    static std::vector<std::string> ParseUseString(const std::string& use_str);

    // Tokenizes into positive (selected) and negative (disabled) sets
    static void TokenizeUseArgs(const std::vector<std::string>& args,
                                std::set<std::string>& selected,
                                std::set<std::string>& disabled);

    // Builds the full USE model for a package
    static UseModel BuildUseModel(const UppPackage* pkg,
                                  const std::vector<std::string>& use_args,
                                  const std::vector<std::string>& forced_flags = {},
                                  const std::vector<std::string>& masked_flags = {});

    // Projects effective USE flags into target U++ compile flags
    static UppProjection ProjectUpp(const UseModel& use, const UppPackage* pkg = nullptr);

    // Formats USE model status for human-readable CLI display
    static std::string FormatUseModel(const UseModel& use);

    // Formats U++ projection for CLI display
    static std::string FormatProjection(const UppProjection& proj);

    // Formats a detailed explanation for a given semantic USE flag
    static std::string ExplainUse(const std::string& flag_name);
};

} // namespace UppBuilder
