#pragma once

#include "UppBuilder.h"
#include "EscBuildParser.h"
#include <vector>

namespace UppBuilder {

class OverlayManager {
public:
    // Finds the UppOverlay directory
    static std::string FindOverlayRoot(const std::string& hint = "");

    // Lists all packages found across all categories in the overlay
    static std::vector<EscBuildInfo> ListAllPackages(const std::string& overlay_root = "");

    // Looks up a package by category/name or name (e.g. "DevCpp/BazaarCore" or "BazaarCore")
    static bool FindPackage(const std::string& atom, EscBuildInfo& out_info, const std::string& overlay_root = "");

    // Pulls latest changes for UppOverlay if it is a git repository
    static bool SyncOverlay(const std::string& overlay_root = "");
};

} // namespace UppBuilder
