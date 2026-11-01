#include "UmkDriver.h"

namespace UppBuilder {

UmkDriver::UmkDriver() {}

std::string UmkDriver::ResolveUmkExecutable(const std::string& custom_path) const {
    if (!custom_path.empty() && FileExists(custom_path)) {
        return GetAbsolutePath(custom_path);
    }

    std::string exe_dir = GetCurrentDir();
    std::string binary_name = "umk";
#ifdef _WIN32
    binary_name = "umk.exe";
#endif

    // Check adjacent bin dirs and repo locations
    std::vector<std::string> candidates = {
        JoinPath(exe_dir, binary_name),
        JoinPath(JoinPath(exe_dir, "bin"), binary_name),
        JoinPath(JoinPath(GetParentDir(exe_dir), "bin"), binary_name),
        JoinPath(JoinPath(GetParentDir(GetParentDir(exe_dir)), "bin"), binary_name),
        JoinPath(JoinPath(GetParentDir(exe_dir), "UppBootstrap"), binary_name)
    };

    for (const auto& c : candidates) {
        if (FileExists(c)) return GetAbsolutePath(c);
    }

    // Check system PATH
    std::string env_path = GetEnvValue("PATH");
    char sep =
#ifdef _WIN32
        ';';
#else
        ':';
#endif
    for (const auto& dir : Split(env_path, sep)) {
        std::string cand = JoinPath(dir, binary_name);
        if (FileExists(cand)) return cand;
    }

    return "";
}

static std::string ReadBuilderFromBm(const std::string& path) {
    std::ifstream is(path);
    if (!is) return "";
    std::string line;
    while (std::getline(is, line)) {
        std::string trimmed = Trim(line);
        if (StartsWith(trimmed, "BUILDER")) {
            size_t q1 = trimmed.find('"');
            size_t q2 = trimmed.rfind('"');
            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                return trimmed.substr(q1 + 1, q2 - q1 - 1);
            }
        }
    }
    return "";
}

std::vector<BuildMethodInfo> UmkDriver::CollectMethods(const std::string& repo_root) const {
    std::vector<BuildMethodInfo> methods;
#ifdef _WIN32
    std::string appdata = GetEnvValue("APPDATA");
    std::string userprof = GetEnvValue("USERPROFILE");
    std::vector<std::string> search_dirs = {
        JoinPath(appdata, "u++\\theide"),
        JoinPath(userprof, ".config\\u++\\theide"),
        JoinPath(userprof, "upp")
    };
#else
    std::string home = GetEnvValue("HOME");
    std::vector<std::string> search_dirs = {
        JoinPath(home, ".config/u++/theide"),
        JoinPath(home, ".config/u++/umk"),
        JoinPath(home, ".upp/theide"),
        JoinPath(home, ".upp/umk"),
        "/etc/u++/theide"
    };
#endif

    std::set<std::string> seen;
    for (const auto& dir : search_dirs) {
        if (!DirectoryExists(dir)) continue;
        for (const auto& f : ListDir(dir)) {
            if (EndsWith(f, ".bm")) {
                std::string full = JoinPath(dir, f);
                std::string title = GetFileTitle(f);
                std::string key = ToLower(title);
                if (seen.count(key)) continue;
                seen.insert(key);
                std::string builder = ReadBuilderFromBm(full);
                methods.push_back({title, full, builder});
            }
        }
    }
    return methods;
}

BuildMethodInfo UmkDriver::FindBestMethod(const std::vector<BuildMethodInfo>& methods, const std::string& preferred) const {
    if (methods.empty()) return {"", "", ""};

    if (!preferred.empty()) {
        std::string pref_lower = ToLower(preferred);
        for (const auto& m : methods) {
            if (ToLower(m.name) == pref_lower) return m;
        }
        for (const auto& m : methods) {
            if (ToLower(m.name).find(pref_lower) != std::string::npos) return m;
        }
    }

    // Default heuristic: Clang or GCC on POSIX, MSVC on Windows
#ifdef _WIN32
    for (const auto& m : methods) {
        std::string b = ToLower(m.builder);
        if (b.find("msc") != std::string::npos && b.find("x64") != std::string::npos) return m;
    }
    for (const auto& m : methods) {
        if (ToLower(m.builder).find("msc") != std::string::npos) return m;
    }
#else
    for (const auto& m : methods) {
        std::string b = ToLower(m.builder);
        if (b.find("clang") != std::string::npos) return m;
    }
    for (const auto& m : methods) {
        std::string b = ToLower(m.builder);
        if (b.find("gcc") != std::string::npos) return m;
    }
#endif

    return methods[0];
}

int UmkDriver::ExecuteBuild(const std::string& umk_exe,
                           const std::vector<std::string>& assembly_roots,
                           const std::string& target_package,
                           const std::string& method_path,
                           const std::string& output_binary,
                           const std::string& flags,
                           bool release,
                           bool clean,
                           int jobs,
                           int shared_mode,
                           bool verbose) {
    if (umk_exe.empty()) {
        std::cerr << "Error: umk executable could not be found.\n";
        return 1;
    }

    std::stringstream ss;
    ss << umk_exe << " ";

    // Assembly roots: comma-separated
    for (size_t i = 0; i < assembly_roots.size(); ++i) {
        ss << assembly_roots[i] << (i + 1 < assembly_roots.size() ? "," : "");
    }

    ss << " " << target_package << " \"" << method_path << "\"";

    std::string umk_opts = "-b"; // BLITZ
    if (release) umk_opts += "r";

    if (shared_mode == 2) {
        umk_opts += "s";
    } else if (shared_mode == 3) {
        umk_opts += "S";
    } else if (shared_mode == 0) {
#ifndef _WIN32
        umk_opts += "s"; // Shared by default on POSIX
#endif
    }

    umk_opts += "uH1";
    if (clean) umk_opts += "a";
    ss << " " << umk_opts;

    if (jobs > 0) ss << " -H" << jobs;

    std::string active_flags = flags;
    if (!release) {
        if (active_flags.find("DEBUG") == std::string::npos) {
            active_flags = active_flags.empty() ? "DEBUG" : (active_flags + ",DEBUG");
        }
    }
    if (!active_flags.empty()) {
        ss << " +" << Replace(active_flags, " ", ",");
    }

    if (!output_binary.empty()) {
        RealizeDirectory(GetParentDir(output_binary));
        ss << " \"" << output_binary << "\"";
    }

    if (verbose) {
        std::cout << "Executing umk command:\n  " << ss.str() << "\n";
    }

    return RunCommand(ss.str());
}

} // namespace UppBuilder
