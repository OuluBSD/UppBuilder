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
        JoinPath(JoinPath(GetParentDir(exe_dir), "UppBootstrap"), binary_name),
        JoinPath(JoinPath(JoinPath(GetParentDir(exe_dir), "UppBootstrap"), "bin"), binary_name),
        JoinPath(JoinPath(JoinPath(GetParentDir(GetParentDir(exe_dir)), "UppBootstrap"), "bin"), binary_name)
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

std::string UmkDriver::ComputeConfigKey(const BuildMethodInfo& method,
                                       const std::string& mainconfig_flags,
                                       bool release,
                                       bool blitz,
                                       int shared_mode,
                                       bool minimal_debug) const {
    std::set<std::string> flags;

    // Builder flag
    std::string b_upper = ToUpper(method.builder);
    std::string n_upper = ToUpper(method.name);
    if (b_upper.find("CLANG") != std::string::npos || n_upper.find("CLANG") != std::string::npos) {
        flags.insert("Clang");
    } else if (b_upper.find("GCC") != std::string::npos || n_upper.find("GCC") != std::string::npos ||
               b_upper.find("MINGW") != std::string::npos || n_upper.find("MINGW") != std::string::npos) {
        flags.insert("Gcc");
    } else if (b_upper.find("MSC") != std::string::npos || n_upper.find("MSC") != std::string::npos ||
               b_upper.find("MSVS") != std::string::npos || n_upper.find("MSVS") != std::string::npos) {
        flags.insert("Msc");
    }

    // Blitz
    if (blitz) {
        flags.insert("Blitz");
    } else if (!release) {
        flags.insert("NoBlitz");
    }

    // Debug / Release
    if (release) {
        flags.insert("Release");
    } else {
        flags.insert("Debug");
        if (minimal_debug) {
            flags.insert("Debug_Minimal");
        } else {
            flags.insert("Debug_Full");
        }
    }

    // Platform flags
#if defined(__linux__) || defined(__gnu_linux__)
    flags.insert("Linux");
    flags.insert("Posix");
#elif defined(_WIN32)
    flags.insert("Win32");
#elif defined(__APPLE__)
    flags.insert("Cocoa");
    flags.insert("Posix");
    flags.insert("Osx");
#elif defined(__FreeBSD__)
    flags.insert("Freebsd");
    flags.insert("Posix");
    flags.insert("Bsd");
#elif defined(__OpenBSD__)
    flags.insert("Openbsd");
    flags.insert("Posix");
    flags.insert("Bsd");
#else
    flags.insert("Posix");
#endif

    // Shared / Static
    bool is_shared = false;
    if (shared_mode == 2 || shared_mode == 3) {
        is_shared = true;
    } else if (shared_mode == 0) {
#ifndef _WIN32
        is_shared = true;
#else
        if (b_upper.find("CLANG") != std::string::npos || b_upper.find("GCC") != std::string::npos ||
            b_upper.find("MINGW") != std::string::npos) {
            is_shared = true;
        }
#endif
    }
    if (is_shared) {
        flags.insert("Shared");
        if (shared_mode == 3) {
            flags.insert("So");
        }
    }

    // Mainconfig custom flags
    if (!mainconfig_flags.empty()) {
        std::string cur;
        for (char c : mainconfig_flags) {
            if (c == ' ' || c == ',' || c == ';' || c == '+') {
                if (!cur.empty()) {
                    std::string u = ToUpper(cur);
                    if (u != "DEBUG" && u != "DEBUG_FULL" && u != "DEBUG_MINIMAL" &&
                        u != "RELEASE" && u != "BLITZ" && u != "NOBLITZ" &&
                        u != "SHARED" && u != "SO" && u != "MAIN") {
                        flags.insert(ToInitCaps(cur));
                    }
                    cur.clear();
                }
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) {
            std::string u = ToUpper(cur);
            if (u != "DEBUG" && u != "DEBUG_FULL" && u != "DEBUG_MINIMAL" &&
                u != "RELEASE" && u != "BLITZ" && u != "NOBLITZ" &&
                u != "SHARED" && u != "SO" && u != "MAIN") {
                flags.insert(ToInitCaps(cur));
            }
        }
    }

    // Build key: <METHOD_NAME>-<Sorted_Flags>
    std::string key = method.name;
    for (const auto& f : flags) {
        key += "-" + f;
    }
    return key;
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

#ifdef _WIN32
    // On Windows with MSVC, upstream U++ packages expect legacy library names like zs.lib (zlib)
    // whereas vcpkg provides zlib.lib/zlibd.lib. Automatically create compatibility shims in LIB directories and cwd.
    std::vector<std::string> search_dirs = Split(GetEnvValue("LIB"), ';');
    search_dirs.push_back("C:\\vcpkg\\installed\\x64-windows\\lib");
    search_dirs.push_back("C:\\vcpkg\\installed\\x64-windows\\debug\\lib");
    for (const auto& dir : search_dirs) {
        if (!DirectoryExists(dir)) continue;
        for (const auto& f : ListDir(dir)) {
            std::string fl = ToLower(f);
            if (!EndsWith(fl, ".lib")) continue;
            std::string src = JoinPath(dir, f);
            if (fl == "z.lib" || fl == "zd.lib" || fl == "zlib.lib" || fl == "zlibd.lib") {
                std::string zs_lib = JoinPath(dir, "zs.lib");
                if (!FileExists(zs_lib)) CopyFile(src, zs_lib);
                std::string z_lib = JoinPath(dir, "z.lib");
                if (!FileExists(z_lib)) CopyFile(src, z_lib);
                if (!FileExists("zs.lib")) CopyFile(src, "zs.lib");
                if (!FileExists("z.lib")) CopyFile(src, "z.lib");
            }
            if (fl.find("pcre2") != std::string::npos) {
                std::string pcre2_lib = JoinPath(dir, "pcre2.lib");
                if (!FileExists(pcre2_lib)) CopyFile(src, pcre2_lib);
                if (!FileExists("pcre2.lib")) CopyFile(src, "pcre2.lib");
            }
            if (fl.find("png") != std::string::npos) {
                std::string png_lib = JoinPath(dir, "png.lib");
                if (!FileExists(png_lib)) CopyFile(src, png_lib);
                if (!FileExists("png.lib")) CopyFile(src, "png.lib");
            }
        }
    }
#elif defined(__APPLE__)
    // On macOS, Core/SSL requests libcrypto.a and libssl.a directly.
    // If OpenSSL was installed via Homebrew, copy them into the working directory so clang++ resolves them.
    std::vector<std::string> brew_ssl = {
        "/opt/homebrew/opt/openssl@3/lib",
        "/usr/local/opt/openssl@3/lib"
    };
    for (const auto& dir : brew_ssl) {
        std::string crypto_a = JoinPath(dir, "libcrypto.a");
        std::string ssl_a = JoinPath(dir, "libssl.a");
        if (FileExists(crypto_a) && !FileExists("libcrypto.a")) {
            CopyFile(crypto_a, "libcrypto.a");
        }
        if (FileExists(ssl_a) && !FileExists("libssl.a")) {
            CopyFile(ssl_a, "libssl.a");
        }
    }
#endif

    if (verbose) {
        std::cout << "Executing umk command:\n  " << ss.str() << "\n";
    }

    int res = RunCommand(ss.str());

#ifdef _WIN32
    if (res == 0) {
        std::vector<std::string> vcpkg_bin_dirs = {
            "C:\\vcpkg\\installed\\x64-windows\\bin",
            "C:\\vcpkg\\installed\\x64-windows\\debug\\bin"
        };
        std::string target_dir = "";
        if (!output_binary.empty()) {
            target_dir = GetParentDir(output_binary);
        } else if (DirectoryExists("bin")) {
            target_dir = "bin";
        }
        for (const auto& bdir : vcpkg_bin_dirs) {
            if (!DirectoryExists(bdir)) continue;
            for (const auto& f : ListDir(bdir)) {
                if (EndsWith(ToLower(f), ".dll")) {
                    std::string src = JoinPath(bdir, f);
                    if (!target_dir.empty() && DirectoryExists(target_dir)) {
                        std::string dst = JoinPath(target_dir, f);
                        if (!FileExists(dst)) CopyFile(src, dst);
                    }
                    if (!FileExists(f)) CopyFile(src, f);
                }
            }
        }
    }
#endif

    return res;
}

} // namespace UppBuilder
