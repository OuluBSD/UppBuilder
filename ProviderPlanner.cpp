#include "ProviderPlanner.h"
#include "OverlayManager.h"
#include <algorithm>

namespace UppBuilder {

bool ProviderPlanner::HostHasTool(const std::string& tool_name) {
#ifdef _WIN32
    std::string test_cmd = "where " + tool_name + " >nul 2>&1";
#else
    std::string test_cmd = "command -v " + tool_name + " >/dev/null 2>&1";
#endif
    return (RunCommand(test_cmd) == 0);
}

std::vector<std::string> ProviderPlanner::ListVirtuals(const std::string& overlay_path) {
    std::vector<std::string> res;
    std::string root = OverlayManager::FindOverlayRoot(overlay_path);
    if (!root.empty()) {
        std::string virt_dir = JoinPath(root, "Virtual");
        if (DirectoryExists(virt_dir)) {
            for (const auto& item : ListDir(virt_dir)) {
                if (item.empty() || item.front() == '.') continue;
                std::string full = JoinPath(virt_dir, item);
                if (DirectoryExists(full)) {
                    res.push_back("Virtual/" + item);
                }
            }
            if (!res.empty()) {
                std::sort(res.begin(), res.end());
                return res;
            }
        }
    }
    return { "Virtual/Sqlite", "Virtual/Ssl", "Virtual/Png", "Virtual/Zlib" };
}

ResolvedProvider ProviderPlanner::Resolve(const std::string& virtual_name, const std::string& overlay_path) {
    ResolvedProvider rp;
    rp.virtual_name = virtual_name;

    std::string leaf = virtual_name;
    if (StartsWith(leaf, "Virtual/")) leaf = leaf.substr(8);
    else if (StartsWith(leaf, "virtual/")) leaf = leaf.substr(8);

    // Look for .escbuild in Virtual/<leaf>
    EscBuildInfo info;
    bool found = false;
    std::string root = OverlayManager::FindOverlayRoot(overlay_path);
    if (!root.empty()) {
        std::string pkg_dir = JoinPath(JoinPath(root, "Virtual"), leaf);
        if (DirectoryExists(pkg_dir)) {
            for (const auto& f : ListDir(pkg_dir)) {
                if (EndsWith(f, ".escbuild") || EndsWith(f, ".ebuild")) {
                    if (EscBuildParser::ParseFile(JoinPath(pkg_dir, f), info)) {
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    if (!found) {
        // Builtin defaults
        std::string lower = ToLower(leaf);
        if (lower == "sqlite") {
            info.providers["windows-vcpkg"] = "sqlite3:x64-windows";
            info.providers["posix-apt"] = "libsqlite3-dev";
            info.providers["posix-brew"] = "sqlite";
            info.providers["upp-plugin"] = "plugin/sqlite";
        } else if (lower == "ssl" || lower == "openssl") {
            info.providers["windows-vcpkg"] = "openssl:x64-windows";
            info.providers["posix-apt"] = "libssl-dev";
            info.providers["posix-brew"] = "openssl";
        } else if (lower == "png") {
            info.providers["windows-vcpkg"] = "libpng:x64-windows";
            info.providers["posix-apt"] = "libpng-dev";
            info.providers["posix-brew"] = "libpng";
            info.providers["upp-plugin"] = "plugin/png";
        } else if (lower == "zlib" || lower == "z") {
            info.providers["windows-vcpkg"] = "zlib:x64-windows";
            info.providers["posix-apt"] = "zlib1g-dev";
            info.providers["posix-brew"] = "zlib";
            info.providers["upp-plugin"] = "plugin/z";
        }
    }

#ifdef _WIN32
    if (info.providers.count("windows-vcpkg")) {
        rp.provider_type = "windows-vcpkg";
        rp.package_ref = info.providers["windows-vcpkg"];
        rp.command = "vcpkg install " + rp.package_ref;
        rp.is_available = HostHasTool("vcpkg");
        return rp;
    }
#elif defined(__APPLE__)
    if (info.providers.count("posix-brew") && HostHasTool("brew")) {
        rp.provider_type = "posix-brew";
        rp.package_ref = info.providers["posix-brew"];
        rp.command = "brew install " + rp.package_ref;
        rp.is_available = true;
        return rp;
    }
#else
    if (info.providers.count("posix-apt") && HostHasTool("apt-get")) {
        rp.provider_type = "posix-apt";
        rp.package_ref = info.providers["posix-apt"];
        rp.command = "sudo apt-get install -y " + rp.package_ref;
        rp.is_available = true;
        return rp;
    }
#endif

    if (info.providers.count("upp-plugin")) {
        rp.provider_type = "upp-plugin";
        rp.package_ref = info.providers["upp-plugin"];
        rp.command = "(internal U++ plugin)";
        rp.is_available = true;
        return rp;
    }

    rp.provider_type = "unknown";
    rp.package_ref = leaf;
    rp.command = "(no provider matching host OS)";
    rp.is_available = false;
    return rp;
}

} // namespace UppBuilder
