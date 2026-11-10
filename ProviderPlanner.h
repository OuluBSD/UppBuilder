#pragma once

#include "UppBuilder.h"
#include "EscBuildParser.h"
#include <map>
#include <vector>

namespace UppBuilder {

struct ResolvedProvider {
    std::string virtual_name;     // e.g. "Virtual/Sqlite"
    std::string provider_type;    // "windows-vcpkg", "posix-apt", "posix-brew", "upp-plugin"
    std::string package_ref;      // e.g. "sqlite3:x64-windows", "libsqlite3-dev", "plugin/sqlite"
    std::string command;          // e.g. "vcpkg install sqlite3:x64-windows"
    bool is_available = false;
};

class ProviderPlanner {
public:
    // Resolves provider for a virtual package (e.g. "Virtual/Sqlite" or "sqlite")
    static ResolvedProvider Resolve(const std::string& virtual_name, const std::string& overlay_path = "");

    // Lists all virtual packages available in UppOverlay/Virtual/
    static std::vector<std::string> ListVirtuals(const std::string& overlay_path = "");

    // Checks whether a tool or package manager exists on host system
    static bool HostHasTool(const std::string& tool_name);
};

} // namespace UppBuilder
