#include "VdbManager.h"
#include "ProfileManager.h"
#include "UppBuilder.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <map>
#include <set>
#include <algorithm>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

namespace UppBuilder {

namespace {

static bool MkdirP(const std::string& path) {
    if (path.empty()) return false;
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        if (c == '/' || c == '\\') {
            if (!current.empty() && current != "." && current != "/") {
#ifdef _WIN32
                _mkdir(current.c_str());
#else
                mkdir(current.c_str(), 0755);
#endif
            }
        }
        current += c;
    }
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

static std::string ReadFileString(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return "";
    std::stringstream ss;
    ss << in.rdbuf();
    std::string s = ss.str();
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) s.pop_back();
    return s;
}

static std::vector<std::string> ReadFileLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    if (!in.is_open()) return lines;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

static void WriteFileString(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    if (out.is_open()) {
        out << content << "\n";
    }
}

static void WriteFileLines(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream out(path);
    if (out.is_open()) {
        for (const auto& l : lines) out << l << "\n";
    }
}

static std::vector<std::string> ListSubdirs(const std::string& path) {
    std::vector<std::string> result;
#ifdef _WIN32
    std::string pattern = path + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                std::string name = fd.cFileName;
                if (name != "." && name != "..") result.push_back(name);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR* d = opendir(path.c_str());
    if (d) {
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            std::string full = path + "/" + name;
            struct stat st;
            if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                result.push_back(name);
            }
        }
        closedir(d);
    }
#endif
    return result;
}

static void EnsureProfileMigration(const std::string& base_vdb) {
    std::string test_dir = JoinPath(base_vdb, "DevCpp");
    if (DirectoryExists(test_dir)) {
        std::string target_profile_dir = JoinPath(base_vdb, "NativeClang");
        RealizeDirectory(target_profile_dir);
        for (const auto& item : ListDir(base_vdb)) {
            if (item == "." || item == ".." || item == "NativeClang" || item == "NativeGcc" || item == "MsvcX64" || item == "WasmBrowser") continue;
            std::string src = JoinPath(base_vdb, item);
            if (DirectoryExists(src)) {
                std::string dst = JoinPath(target_profile_dir, item);
                rename(src.c_str(), dst.c_str());
            }
        }
    }
}

} // namespace

std::string VdbManager::GetVdbDir(const std::string& custom_dir, const std::string& profile) {
    std::string base;
    if (!custom_dir.empty()) {
        base = custom_dir;
    } else {
        const char* env_dir = getenv("UPPBUILDER_VDB_DIR");
        if (env_dir && *env_dir) {
            base = env_dir;
        } else {
#ifdef _WIN32
            const char* appdata = getenv("APPDATA");
            if (appdata && *appdata) {
                base = std::string(appdata) + "\\uppbuilder\\vdb";
            } else {
                base = "C:\\uppbuilder\\vdb";
            }
#else
            const char* xdg_config = getenv("XDG_CONFIG_HOME");
            if (xdg_config && *xdg_config) {
                base = std::string(xdg_config) + "/uppbuilder/vdb";
            } else {
                const char* home = getenv("HOME");
                if (home && *home) {
                    base = std::string(home) + "/.config/uppbuilder/vdb";
                } else {
                    base = "/var/db/uppbuilder/vdb";
                }
            }
#endif
        }
    }

    EnsureProfileMigration(base);

    std::string prof = profile.empty() ? ProfileManager::GetActiveProfileName() : profile;
    return JoinPath(base, prof);
}

bool VdbManager::RegisterPackage(const VdbEntry& entry, const std::string& custom_dir) {
    if (entry.atom.empty()) return false;

    std::string prof = entry.profile.empty() ? ProfileManager::GetActiveProfileName() : entry.profile;
    std::string vdb_root = GetVdbDir(custom_dir, prof);
    std::string cat = "sys";
    std::string pkg = entry.atom;
    size_t slash = entry.atom.find('/');
    if (slash != std::string::npos) {
        cat = entry.atom.substr(0, slash);
        pkg = entry.atom.substr(slash + 1);
    }

    std::string pf = pkg;
    if (!entry.version.empty()) pf += "-" + entry.version;

    std::string pkg_dir = JoinPath(JoinPath(vdb_root, cat), pf);
    MkdirP(pkg_dir);

    WriteFileString(pkg_dir + "/ATOM", entry.atom);
    WriteFileString(pkg_dir + "/VERSION", entry.version.empty() ? "0.1.0" : entry.version);
    WriteFileString(pkg_dir + "/PROFILE", prof);
    WriteFileString(pkg_dir + "/USE", entry.use);
    WriteFileString(pkg_dir + "/REPO", entry.repo_path);
    WriteFileString(pkg_dir + "/TARGET", entry.target);

    uint64_t t = entry.install_time;
    if (t == 0) {
        t = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    WriteFileString(pkg_dir + "/INSTALL_TIME", std::to_string(t));

    WriteFileLines(pkg_dir + "/RDEPEND", entry.rdepend);
    WriteFileLines(pkg_dir + "/CONTENTS", entry.files);

    return true;
}

bool VdbManager::UnregisterPackage(const std::string& atom, const std::string& custom_dir, const std::string& profile) {
    std::string vdb_root = GetVdbDir(custom_dir, profile);
    std::string cat = "sys";
    std::string pkg = atom;
    size_t slash = atom.find('/');
    if (slash != std::string::npos) {
        cat = atom.substr(0, slash);
        pkg = atom.substr(slash + 1);
    }

    std::string cat_dir = JoinPath(vdb_root, cat);
    auto dirs = ListSubdirs(cat_dir);
    for (const auto& d : dirs) {
        if (d == pkg || d.rfind(pkg + "-", 0) == 0) {
            std::string full_dir = JoinPath(cat_dir, d);
            return RemoveDirectoryRecursive(full_dir);
        }
    }
    return false;
}

bool VdbManager::IsInstalled(const std::string& atom, VdbEntry* out_entry, const std::string& custom_dir, const std::string& profile) {
    std::string vdb_root = GetVdbDir(custom_dir, profile);
    std::string cat = "sys";
    std::string pkg = atom;
    size_t slash = atom.find('/');
    if (slash != std::string::npos) {
        cat = atom.substr(0, slash);
        pkg = atom.substr(slash + 1);
    }

    std::string cat_dir = JoinPath(vdb_root, cat);
    auto dirs = ListSubdirs(cat_dir);
    for (const auto& d : dirs) {
        if (d == pkg || d.rfind(pkg + "-", 0) == 0) {
            if (out_entry) {
                std::string pkg_dir = JoinPath(cat_dir, d);
                out_entry->atom = ReadFileString(pkg_dir + "/ATOM");
                if (out_entry->atom.empty()) out_entry->atom = cat + "/" + pkg;
                out_entry->version = ReadFileString(pkg_dir + "/VERSION");
                out_entry->profile = ReadFileString(pkg_dir + "/PROFILE");
                out_entry->use = ReadFileString(pkg_dir + "/USE");
                out_entry->repo_path = ReadFileString(pkg_dir + "/REPO");
                out_entry->target = ReadFileString(pkg_dir + "/TARGET");
                std::string t_str = ReadFileString(pkg_dir + "/INSTALL_TIME");
                if (!t_str.empty()) out_entry->install_time = std::stoull(t_str);
                out_entry->rdepend = ReadFileLines(pkg_dir + "/RDEPEND");
                out_entry->files = ReadFileLines(pkg_dir + "/CONTENTS");
            }
            return true;
        }
    }
    return false;
}

std::vector<VdbEntry> VdbManager::ListInstalled(const std::string& custom_dir, const std::string& profile) {
    std::vector<VdbEntry> result;
    std::string vdb_root = GetVdbDir(custom_dir, profile);
    auto cats = ListSubdirs(vdb_root);
    for (const auto& cat : cats) {
        std::string cat_dir = JoinPath(vdb_root, cat);
        auto pkgs = ListSubdirs(cat_dir);
        for (const auto& pf : pkgs) {
            std::string pkg_dir = JoinPath(cat_dir, pf);
            VdbEntry entry;
            entry.atom = ReadFileString(pkg_dir + "/ATOM");
            if (entry.atom.empty()) {
                std::string name = pf;
                size_t dash = name.find('-');
                if (dash != std::string::npos) name = name.substr(0, dash);
                entry.atom = cat + "/" + name;
            }
            entry.version = ReadFileString(pkg_dir + "/VERSION");
            entry.profile = ReadFileString(pkg_dir + "/PROFILE");
            entry.use = ReadFileString(pkg_dir + "/USE");
            entry.repo_path = ReadFileString(pkg_dir + "/REPO");
            entry.target = ReadFileString(pkg_dir + "/TARGET");
            std::string t_str = ReadFileString(pkg_dir + "/INSTALL_TIME");
            if (!t_str.empty()) entry.install_time = std::stoull(t_str);
            entry.rdepend = ReadFileLines(pkg_dir + "/RDEPEND");
            entry.files = ReadFileLines(pkg_dir + "/CONTENTS");
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<std::string> VdbManager::ListVdbProfiles(const std::string& custom_dir) {
    std::string base;
    if (!custom_dir.empty()) {
        base = custom_dir;
    } else {
        const char* env_dir = getenv("UPPBUILDER_VDB_DIR");
        if (env_dir && *env_dir) base = env_dir;
        else {
#ifdef _WIN32
            const char* appdata = getenv("APPDATA");
            if (appdata && *appdata) base = std::string(appdata) + "\\uppbuilder\\vdb";
            else base = "C:\\uppbuilder\\vdb";
#else
            const char* xdg_config = getenv("XDG_CONFIG_HOME");
            if (xdg_config && *xdg_config) base = std::string(xdg_config) + "/uppbuilder/vdb";
            else {
                const char* home = getenv("HOME");
                if (home && *home) base = std::string(home) + "/.config/uppbuilder/vdb";
                else base = "/var/db/uppbuilder/vdb";
            }
#endif
        }
    }
    EnsureProfileMigration(base);
    std::vector<std::string> profiles;
    for (const auto& item : ListSubdirs(base)) {
        if (item != "." && item != "..") profiles.push_back(item);
    }
    return profiles;
}

std::vector<std::string> VdbManager::FindReverseDependencies(const std::string& atom, const std::string& custom_dir, const std::string& profile) {
    std::vector<std::string> rev_deps;
    auto all = ListInstalled(custom_dir, profile);
    for (const auto& pkg : all) {
        if (pkg.atom == atom) continue;
        for (const auto& dep : pkg.rdepend) {
            if (dep == atom || dep.find(atom) != std::string::npos) {
                rev_deps.push_back(pkg.atom);
                break;
            }
        }
    }
    return rev_deps;
}

bool VdbManager::DiffProfiles(const std::string& profile1, const std::string& profile2, const std::string& custom_dir) {
    auto list1 = ListInstalled(custom_dir, profile1);
    auto list2 = ListInstalled(custom_dir, profile2);

    std::map<std::string, std::pair<std::string, std::string>> all_atoms;
    for (const auto& e : list1) {
        all_atoms[e.atom].first = e.version.empty() ? "0.1.0" : e.version;
    }
    for (const auto& e : list2) {
        all_atoms[e.atom].second = e.version.empty() ? "0.1.0" : e.version;
    }

    std::cout << "\nComparing VDB installations: " << profile1 << " vs " << profile2 << "\n";
    std::cout << std::string(70, '-') << "\n";
    std::cout << std::left << std::setw(35) << "Package Atom"
              << std::setw(18) << profile1
              << std::setw(18) << profile2 << "\n";
    std::cout << std::string(70, '-') << "\n";

    int diff_count = 0;
    for (const auto& kv : all_atoms) {
        std::string v1 = kv.second.first.empty() ? "(none)" : kv.second.first;
        std::string v2 = kv.second.second.empty() ? "(none)" : kv.second.second;
        if (v1 != v2) diff_count++;
        std::cout << std::left << std::setw(35) << kv.first
                  << std::setw(18) << v1
                  << std::setw(18) << v2 << "\n";
    }
    std::cout << std::string(70, '-') << "\n";
    std::cout << "Total packages: " << all_atoms.size() << " (" << diff_count << " difference(s))\n\n";
    return true;
}

} // namespace UppBuilder
