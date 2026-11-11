#pragma once

#include "UppBuilder.h"
#include <map>
#include <vector>

namespace UppBuilder {

struct EscBuildInfo {
    std::string category;
    std::string package_name;
    std::string version;
    std::string file_path;

    std::string description;
    std::string homepage;
    std::string git_repo;
    std::string git_branch = "main";
    std::string upp_assembly;
    std::string default_package;

    std::vector<std::string> iuse;
    std::vector<std::string> acceptflags;
    std::vector<std::string> rdepend;
    std::map<std::string, std::string> providers; // e.g. "windows-vcpkg" -> "sqlite3:x64-windows"

    std::string fn_src_unpack;
    std::string fn_src_compile;
    std::string fn_src_test;
    std::string fn_src_install;

    std::vector<std::string> warnings;
};

class EscBuildParser {
public:
    static bool ParseFile(const std::string& path, EscBuildInfo& out_info);
    static bool ParseContent(const std::string& content, EscBuildInfo& out_info);
    
    // Deduce Category, PackageName, and Version from path:
    // e.g. ".../UppOverlay/DevCpp/BazaarCore/BazaarCore-0.1.0.escbuild"
    static void DeduceMetadataFromPath(const std::string& path, EscBuildInfo& out_info);
};

} // namespace UppBuilder
