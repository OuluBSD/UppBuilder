#include "BinPkgManager.h"
#include "ProfileManager.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace UppBuilder {

static std::string GetCurrentTimestampString() {
    std::time_t now = std::time(nullptr);
    std::tm* tm_now = std::localtime(&now);
    std::ostringstream ss;
    if (tm_now) {
        ss << std::put_time(tm_now, "%Y-%m-%d %H:%M:%S");
    }
    return ss.str();
}

std::string BinPkgManager::GetBinPkgDir(const std::string& profile_name) {
    std::string prof = profile_name.empty() ? ProfileManager::GetActiveProfileName() : profile_name;
    return JoinPath(JoinPath(GetUserCacheDir(), "binpkgs"), prof);
}

bool BinPkgManager::HasBinPkg(const std::string& atom,
                              const std::string& version,
                              const std::string& profile_name,
                              std::string* out_path)
{
    std::string dir = GetBinPkgDir(profile_name);
    std::string cat = "sys";
    std::string pkg = atom;
    size_t slash = atom.find('/');
    if (slash != std::string::npos) {
        cat = atom.substr(0, slash);
        pkg = atom.substr(slash + 1);
    }
    std::string ver = version.empty() ? "0.1.0" : version;
    std::string base = pkg + "-" + ver;

    std::string p_tar_gz = JoinPath(JoinPath(dir, cat), base + ".tar.gz");
    if (FileExists(p_tar_gz)) {
        if (out_path) *out_path = p_tar_gz;
        return true;
    }

    std::string p_tbz2 = JoinPath(JoinPath(dir, cat), base + ".tbz2");
    if (FileExists(p_tbz2)) {
        if (out_path) *out_path = p_tbz2;
        return true;
    }

    return false;
}

bool BinPkgManager::CreateBinPkg(const EscBuildInfo& info,
                                 const std::string& profile_name,
                                 const std::string& repo_dir,
                                 const std::vector<std::string>& active_use,
                                 bool verbose)
{
    std::string dir = GetBinPkgDir(profile_name);
    std::string cat = info.category.empty() ? "sys" : info.category;
    std::string pkg = info.package_name;
    std::string ver = info.version.empty() ? "0.1.0" : info.version;

    std::string cat_dir = JoinPath(dir, cat);
    RealizeDirectory(cat_dir);

    std::string archive_path = JoinPath(cat_dir, pkg + "-" + ver + ".tar.gz");
    std::string staging_dir = JoinPath(JoinPath(GetUserCacheDir(), "tmp"), "binpkg_" + cat + "_" + pkg);

    RemoveDirectoryRecursive(staging_dir);
    RealizeDirectory(staging_dir);

    // 1. Copy bin directory if it exists
    std::string repo_bin = JoinPath(repo_dir, "bin");
    if (DirectoryExists(repo_bin)) {
        std::string stage_bin = JoinPath(staging_dir, "bin");
        RealizeDirectory(stage_bin);
        CopyDirectoryRecursive(repo_bin, stage_bin);
    }

    // 2. Write metadata files
    std::string meta_dir = JoinPath(staging_dir, "METADATA");
    RealizeDirectory(meta_dir);

    WriteTextFile(JoinPath(meta_dir, "ATOM"), cat + "/" + pkg);
    WriteTextFile(JoinPath(meta_dir, "VERSION"), ver);
    WriteTextFile(JoinPath(meta_dir, "PROFILE"), profile_name);
    WriteTextFile(JoinPath(meta_dir, "BUILD_TIME"), GetCurrentTimestampString());
    WriteTextFile(JoinPath(meta_dir, "TARGET"), info.default_package);
    WriteTextFile(JoinPath(meta_dir, "RDEPEND"), JoinStrings(info.rdepend, "\n"));

    std::string use_str;
    for (size_t i = 0; i < active_use.size(); i++) {
        if (i > 0) use_str += " ";
        use_str += active_use[i];
    }
    WriteTextFile(JoinPath(meta_dir, "USE"), use_str);

    // 3. Create tar.gz archive
    std::string tar_cmd;
#ifdef _WIN32
    tar_cmd = "tar.exe -czf \"" + archive_path + "\" -C \"" + staging_dir + "\" .";
#else
    tar_cmd = "tar -czf \"" + archive_path + "\" -C \"" + staging_dir + "\" .";
#endif
    if (verbose) std::cout << ">>> Archiving binary package: " << archive_path << "\n";
    int ret = RunCommand(tar_cmd);

    // 4. Cleanup staging
    RemoveDirectoryRecursive(staging_dir);

    if (ret == 0) {
        std::cout << ">>> Created binary package: " << archive_path << "\n";
        return true;
    } else {
        std::cerr << "Error: Failed to create binary package archive (exit code " << ret << ")\n";
        return false;
    }
}

bool BinPkgManager::InstallBinPkg(const std::string& archive_path,
                                  const std::string& profile_name,
                                  const std::string& repo_dir,
                                  bool verbose)
{
    if (!FileExists(archive_path)) {
        std::cerr << "Error: Binary package archive not found: " << archive_path << "\n";
        return false;
    }

    std::string target_dir = repo_dir;
    if (target_dir.empty() || !DirectoryExists(target_dir)) {
        std::string home = GetEnvValue("HOME");
        if (target_dir.empty() && !home.empty()) {
            target_dir = JoinPath(home, "OuluBSD");
        }
    }
    RealizeDirectory(target_dir);

    // 1. Extract archive into destination
    std::string untar_cmd;
#ifdef _WIN32
    untar_cmd = "tar.exe -xzf \"" + archive_path + "\" -C \"" + target_dir + "\"";
#else
    untar_cmd = "tar -xzf \"" + archive_path + "\" -C \"" + target_dir + "\"";
#endif
    if (verbose) std::cout << ">>> Extracting binary package: " << untar_cmd << "\n";
    int ret = RunCommand(untar_cmd);
    if (ret != 0) {
        std::cerr << "Error: Failed to extract binary package (exit code " << ret << ")\n";
        return false;
    }

    // 2. Read METADATA to register in VDB
    std::string meta_dir = JoinPath(target_dir, "METADATA");
    VdbEntry entry;
    if (DirectoryExists(meta_dir)) {
        entry.atom = Trim(ReadTextFile(JoinPath(meta_dir, "ATOM")));
        entry.version = Trim(ReadTextFile(JoinPath(meta_dir, "VERSION")));
        entry.profile = profile_name.empty() ? Trim(ReadTextFile(JoinPath(meta_dir, "PROFILE"))) : profile_name;
        entry.use = Trim(ReadTextFile(JoinPath(meta_dir, "USE")));
        entry.target = Trim(ReadTextFile(JoinPath(meta_dir, "TARGET")));
        entry.rdepend = Split(ReadTextFile(JoinPath(meta_dir, "RDEPEND")), '\n');
        entry.repo_path = target_dir;

        std::string bin_dir = JoinPath(target_dir, "bin");
        if (DirectoryExists(bin_dir)) {
            entry.files.push_back(bin_dir);
#ifndef _WIN32
            std::vector<std::string> bin_files;
            Walk(bin_dir, bin_files);
            for (const auto& bf : bin_files) {
                chmod(bf.c_str(), 0755);
            }
#endif
        }

        VdbManager::RegisterPackage(entry);

        // Remove unpacked METADATA directory so workspace stays clean
        RemoveDirectoryRecursive(meta_dir);
        if (verbose) std::cout << ">>> Registered binary package in VDB for " << entry.atom << "\n";
    }

    return true;
}

std::vector<BinPkgInfo> BinPkgManager::ListBinPkgs(const std::string& profile_name) {
    std::vector<BinPkgInfo> list;
    std::string dir = GetBinPkgDir(profile_name);
    if (!DirectoryExists(dir)) return list;

#if defined(__has_include) && __has_include(<filesystem>)
    try {
        for (const auto& cat_entry : fs::directory_iterator(dir)) {
            if (cat_entry.is_directory()) {
                std::string cat = cat_entry.path().filename().string();
                if (cat.empty() || cat[0] == '.') continue;

                for (const auto& pkg_entry : fs::directory_iterator(cat_entry.path())) {
                    if (pkg_entry.is_regular_file()) {
                        std::string fn = pkg_entry.path().filename().string();
                        if (EndsWith(fn, ".tar.gz") || EndsWith(fn, ".tbz2")) {
                            BinPkgInfo info;
                            info.path = pkg_entry.path().string();
                            info.file_size = fs::file_size(pkg_entry.path());
                            info.profile = profile_name.empty() ? ProfileManager::GetActiveProfileName() : profile_name;

                            std::string base = fn;
                            if (EndsWith(base, ".tar.gz")) base = base.substr(0, base.size() - 7);
                            else if (EndsWith(base, ".tbz2")) base = base.substr(0, base.size() - 5);

                            size_t last_dash = base.rfind('-');
                            if (last_dash != std::string::npos) {
                                info.atom = cat + "/" + base.substr(0, last_dash);
                                info.version = base.substr(last_dash + 1);
                            } else {
                                info.atom = cat + "/" + base;
                                info.version = "0.1.0";
                            }
                            list.push_back(info);
                        }
                    }
                }
            }
        }
    } catch (...) {}
#endif
    return list;
}

bool BinPkgManager::CleanBinPkgs(const std::string& profile_name, bool verbose) {
    std::string dir = GetBinPkgDir(profile_name);
    if (DirectoryExists(dir)) {
        if (verbose) std::cout << ">>> Cleaning binary packages in " << dir << "...\n";
        return RemoveDirectoryRecursive(dir);
    }
    return true;
}

} // namespace UppBuilder
