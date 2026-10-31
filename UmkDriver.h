#pragma once

#include "UppBuilder.h"

namespace UppBuilder {

struct BuildMethodInfo {
    std::string name;
    std::string path;
    std::string builder;
};

class UmkDriver {
public:
    UmkDriver();

    std::string ResolveUmkExecutable(const std::string& custom_path = "") const;
    std::vector<BuildMethodInfo> CollectMethods(const std::string& repo_root = "") const;
    BuildMethodInfo FindBestMethod(const std::vector<BuildMethodInfo>& methods, const std::string& preferred = "") const;

    // Executes umk
    int ExecuteBuild(const std::string& umk_exe,
                     const std::vector<std::string>& assembly_roots,
                     const std::string& target_package,
                     const std::string& method_path,
                     const std::string& output_binary,
                     const std::string& flags,
                     bool release,
                     bool clean,
                     int jobs,
                     int shared_mode, // 0 = default, 1 = static, 2 = shared (-s), 3 = build-shared (-S)
                     bool verbose = false);
};

} // namespace UppBuilder
