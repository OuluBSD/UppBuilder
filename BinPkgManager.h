#pragma once

#include "UppBuilder.h"
#include "EscBuildParser.h"
#include "VdbManager.h"
#include <string>
#include <vector>
#include <cstdint>

namespace UppBuilder {

struct BinPkgInfo {
    std::string atom;
    std::string version;
    std::string profile;
    std::string use;
    std::string path;
    uint64_t file_size = 0;
    std::string build_time;
};

class BinPkgManager {
public:
    static std::string GetBinPkgDir(const std::string& profile_name = "");
    
    static bool HasBinPkg(const std::string& atom,
                          const std::string& version,
                          const std::string& profile_name,
                          std::string* out_path = nullptr);

    static bool CreateBinPkg(const EscBuildInfo& info,
                             const std::string& profile_name,
                             const std::string& repo_dir,
                             const std::vector<std::string>& active_use,
                             bool verbose = false);

    static bool InstallBinPkg(const std::string& archive_path,
                              const std::string& profile_name,
                              const std::string& repo_dir = "",
                              bool verbose = false);

    static std::vector<BinPkgInfo> ListBinPkgs(const std::string& profile_name = "");

    static bool CleanBinPkgs(const std::string& profile_name = "", bool verbose = false);
};

} // namespace UppBuilder
