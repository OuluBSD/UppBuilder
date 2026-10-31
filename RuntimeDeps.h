#pragma once

#include "UppBuilder.h"

namespace UppBuilder {

class RuntimeDepsHarvester {
public:
    static std::vector<std::string> ExtractDependencies(const std::string& binary_path);
    static bool IsSystemDependency(const std::string& library_path);
    static void CopyDependenciesRecursive(const std::string& binary_path,
                                          const std::string& target_dir,
                                          const std::vector<std::string>& search_paths,
                                          std::set<std::string>& out_copied);

private:
    static std::vector<std::string> ExtractPeDependencies(const std::string& path);
    static std::vector<std::string> ExtractElfDependencies(const std::string& path);
    static std::vector<std::string> ExtractMachODependencies(const std::string& path);
};

} // namespace UppBuilder
