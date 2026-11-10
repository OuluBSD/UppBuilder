#include "ProfileManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace UppBuilder {

std::string ProfileManager::GetDefaultProfileName() {
#ifdef _WIN32
    return "MsvcX64";
#elif defined(__APPLE__)
    return "NativeClang";
#else
    // On Linux: check compiler presence
    std::string env_path = GetEnvValue("PATH");
    for (const auto& dir : Split(env_path, ':')) {
        if (FileExists(JoinPath(dir, "clang++"))) return "NativeClang";
    }
    return "NativeGcc";
#endif
}

std::string ProfileManager::GetDefaultOutputCache() {
#ifdef _WIN32
    std::string local_app_data = GetEnvValue("LOCALAPPDATA");
    if (!local_app_data.empty()) {
        return JoinPath(JoinPath(local_app_data, "u++"), "out");
    }
    std::string app_data = GetEnvValue("APPDATA");
    if (!app_data.empty()) {
        return JoinPath(JoinPath(app_data, "u++"), "out");
    }
    return "out";
#else
    std::string home = GetEnvValue("HOME");
    if (!home.empty()) {
        return JoinPath(JoinPath(home, ".cache"), "upp.out");
    }
    return ".cache/upp.out";
#endif
}

TargetProfile ProfileManager::GetBuiltinProfile(const std::string& name) {
    TargetProfile p;
    p.name = name;
    p.output_cache = GetDefaultOutputCache();

    if (name == "NativeClang") {
        p.build_method = "CLANG";
        p.default_use = "blitz posix linux";
    } else if (name == "NativeGcc") {
        p.build_method = "GCC";
        p.default_use = "blitz posix linux";
    } else if (name == "MsvcX64") {
        p.build_method = "MSVS22x64";
        p.default_use = "blitz win32 msvc";
    } else if (name == "WasmBrowser") {
        p.build_method = "EMCC";
        p.default_use = "wasm -gui";
    } else {
        p.build_method = "CLANG";
        p.default_use = "blitz";
    }
    return p;
}

static std::string DeduplicateTokens(const std::string& str) {
    std::vector<std::string> tokens = Split(str, ' ');
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (const auto& t : tokens) {
        std::string trimmed = Trim(t);
        if (trimmed.empty()) continue;
        if (seen.insert(trimmed).second) {
            result.push_back(trimmed);
        }
    }
    return JoinStrings(result, " ");
}

static void ParseMakeConf(const std::string& path, TargetProfile& p) {
    std::ifstream is(path);
    if (!is) return;
    std::string line;
    while (std::getline(is, line)) {
        std::string s = Trim(line);
        if (s.empty() || s.front() == '#') continue;
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;

        std::string key = Trim(s.substr(0, eq));
        std::string val = Trim(s.substr(eq + 1));
        if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
            val = val.substr(1, val.size() - 2);
        }

        p.vars[key] = val;
        std::string upper_key = ToUpper(key);
        if (upper_key == "BUILD_METHOD") p.build_method = val;
        else if (upper_key == "USE") {
            p.default_use = DeduplicateTokens(p.default_use.empty() ? val : (p.default_use + " " + val));
        } else if (upper_key == "OUTPUT_CACHE") {
            if (StartsWith(val, "~")) {
                std::string home = GetEnvValue("HOME");
                p.output_cache = home + val.substr(1);
            } else {
                p.output_cache = val;
            }
        }
    }
}

static std::vector<std::string> FindOverlayCandidates(const std::string& overlay_path) {
    std::vector<std::string> candidates;
    if (!overlay_path.empty()) candidates.push_back(overlay_path);

    std::string cwd = GetCurrentDir();
    candidates.push_back(JoinPath(cwd, "UppOverlay"));
    candidates.push_back(JoinPath(GetParentDir(cwd), "UppOverlay"));
    candidates.push_back(JoinPath(JoinPath(GetParentDir(cwd), "OuluBSD"), "UppOverlay"));

    std::string home = GetEnvValue("HOME");
    if (!home.empty()) {
        candidates.push_back(JoinPath(JoinPath(home, "OuluBSD"), "UppOverlay"));
        candidates.push_back(JoinPath(JoinPath(home, "Dev"), "UppOverlay"));
    }
    return candidates;
}

std::vector<std::string> ProfileManager::ListProfiles(const std::string& overlay_path) {
    std::vector<std::string> list;
    std::vector<std::string> cands = FindOverlayCandidates(overlay_path);
    for (const auto& cand : cands) {
        std::string targets_dir = JoinPath(JoinPath(cand, "profiles"), "targets");
        if (DirectoryExists(targets_dir)) {
            for (const auto& item : ListDir(targets_dir)) {
                if (item.empty() || item.front() == '.') continue;
                std::string full = JoinPath(targets_dir, item);
                if (DirectoryExists(full)) {
                    list.push_back(item);
                }
            }
            if (!list.empty()) {
                std::sort(list.begin(), list.end());
                return list;
            }
        }
    }

    // Built-in defaults
    return { "NativeClang", "NativeGcc", "MsvcX64", "WasmBrowser" };
}

TargetProfile ProfileManager::LoadProfile(const std::string& profile_name, const std::string& overlay_path) {
    std::string name = profile_name.empty() ? GetDefaultProfileName() : profile_name;
    TargetProfile p;
    p.name = name;
    p.output_cache = GetDefaultOutputCache();

    bool found_overlay = false;
    std::vector<std::string> cands = FindOverlayCandidates(overlay_path);
    for (const auto& cand : cands) {
        std::string base_conf = JoinPath(JoinPath(JoinPath(cand, "profiles"), "base"), "make.conf");
        if (FileExists(base_conf)) {
            ParseMakeConf(base_conf, p);
            found_overlay = true;
        }

        std::string target_dir = JoinPath(JoinPath(JoinPath(cand, "profiles"), "targets"), name);
        std::string target_conf = JoinPath(target_dir, "make.conf");
        if (FileExists(target_conf)) {
            p.path = target_dir;
            ParseMakeConf(target_conf, p);
            p.default_use = DeduplicateTokens(p.default_use);
            return p;
        }
    }

    if (!found_overlay) {
        return GetBuiltinProfile(name);
    }

    p.default_use = DeduplicateTokens(p.default_use);
    return p;
}

} // namespace UppBuilder
