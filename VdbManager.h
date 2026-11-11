#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace UppBuilder {

struct VdbEntry {
    std::string atom;                 // e.g. "DevCpp/SoftPhys"
    std::string version;              // e.g. "0.1.0"
    std::string profile;              // e.g. "NativeClang"
    std::string use;                  // e.g. "+blitz +shared"
    std::string repo_path;            // e.g. "/home/sblo/OuluBSD/SoftPhys"
    std::string target;               // e.g. "uppsrc/SoftPhys/SoftPhys"
    uint64_t install_time = 0;        // Epoch seconds
    std::vector<std::string> files;   // Built binaries/libraries
    std::vector<std::string> rdepend; // Recorded dependencies
};

class VdbManager {
public:
    // Returns root directory of VDB for a given profile
    static std::string GetVdbDir(const std::string& custom_dir = "", const std::string& profile = "");

    // Registers or updates an installed package entry
    static bool RegisterPackage(const VdbEntry& entry, const std::string& custom_dir = "");

    // Unregisters a package entry
    static bool UnregisterPackage(const std::string& atom, const std::string& custom_dir = "", const std::string& profile = "");

    // Checks if a package is installed
    static bool IsInstalled(const std::string& atom, VdbEntry* out_entry = nullptr, const std::string& custom_dir = "", const std::string& profile = "");

    // Lists all installed packages for a profile
    static std::vector<VdbEntry> ListInstalled(const std::string& custom_dir = "", const std::string& profile = "");

    // Lists all profiles that currently have VDB entries
    static std::vector<std::string> ListVdbProfiles(const std::string& custom_dir = "");

    // Finds which installed packages depend on the specified atom
    static std::vector<std::string> FindReverseDependencies(const std::string& atom, const std::string& custom_dir = "", const std::string& profile = "");

    // Compares two profiles and prints diff
    static bool DiffProfiles(const std::string& profile1, const std::string& profile2, const std::string& custom_dir = "");
};

} // namespace UppBuilder
