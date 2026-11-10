#include "OverlayManager.h"
#include <iostream>
#include <algorithm>

namespace UppBuilder {

std::string OverlayManager::FindOverlayRoot(const std::string& hint) {
    if (!hint.empty() && DirectoryExists(hint)) {
        return GetAbsolutePath(hint);
    }

    std::string cwd = GetCurrentDir();
    std::vector<std::string> candidates = {
        JoinPath(cwd, "UppOverlay"),
        JoinPath(GetParentDir(cwd), "UppOverlay"),
        JoinPath(JoinPath(GetParentDir(cwd), "OuluBSD"), "UppOverlay")
    };

    std::string home = GetEnvValue("HOME");
    if (!home.empty()) {
        candidates.push_back(JoinPath(JoinPath(home, "OuluBSD"), "UppOverlay"));
        candidates.push_back(JoinPath(JoinPath(home, "Dev"), "UppOverlay"));
    }

    for (const auto& c : candidates) {
        if (DirectoryExists(c) && DirectoryExists(JoinPath(c, "profiles"))) {
            return GetAbsolutePath(c);
        }
    }
    return "";
}

std::vector<EscBuildInfo> OverlayManager::ListAllPackages(const std::string& overlay_root) {
    std::vector<EscBuildInfo> list;
    std::string root = FindOverlayRoot(overlay_root);
    if (root.empty()) return list;

    for (const auto& cat : ListDir(root)) {
        if (cat.empty() || cat.front() == '.' || cat == "metadata" || cat == "profiles") continue;
        std::string cat_dir = JoinPath(root, cat);
        if (!DirectoryExists(cat_dir)) continue;

        for (const auto& pkg : ListDir(cat_dir)) {
            if (pkg.empty() || pkg.front() == '.') continue;
            std::string pkg_dir = JoinPath(cat_dir, pkg);
            if (!DirectoryExists(pkg_dir)) continue;

            for (const auto& f : ListDir(pkg_dir)) {
                if (EndsWith(f, ".escbuild") || EndsWith(f, ".ebuild")) {
                    EscBuildInfo info;
                    if (EscBuildParser::ParseFile(JoinPath(pkg_dir, f), info)) {
                        info.category = cat;
                        list.push_back(info);
                    }
                }
            }
        }
    }
    std::sort(list.begin(), list.end(), [](const EscBuildInfo& a, const EscBuildInfo& b) {
        if (a.category != b.category) return a.category < b.category;
        return a.package_name < b.package_name;
    });
    return list;
}

bool OverlayManager::FindPackage(const std::string& atom, EscBuildInfo& out_info, const std::string& overlay_root) {
    std::string root = FindOverlayRoot(overlay_root);
    if (root.empty()) return false;

    std::string clean = Replace(atom, "\\", "/");
    size_t slash = clean.find('/');

    std::string cat_target;
    std::string pkg_target;
    if (slash != std::string::npos) {
        cat_target = clean.substr(0, slash);
        pkg_target = clean.substr(slash + 1);
    } else {
        pkg_target = clean;
    }

    for (const auto& info : ListAllPackages(root)) {
        if (!cat_target.empty()) {
            if (info.category == cat_target && (info.package_name == pkg_target || ToLower(info.package_name) == ToLower(pkg_target))) {
                out_info = info;
                return true;
            }
        } else {
            if (info.package_name == pkg_target || ToLower(info.package_name) == ToLower(pkg_target)) {
                out_info = info;
                return true;
            }
        }
    }
    return false;
}

bool OverlayManager::SyncOverlay(const std::string& overlay_root) {
    std::string root = FindOverlayRoot(overlay_root);
    if (root.empty()) return false;
    std::string git_dir = JoinPath(root, ".git");
    if (!DirectoryExists(git_dir)) return false;

    std::string cmd = "cd \"" + root + "\" && git pull";
    return (RunCommand(cmd) == 0);
}

} // namespace UppBuilder
