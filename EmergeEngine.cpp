#include "EmergeEngine.h"
#include <functional>
#include <sstream>

namespace UppBuilder {

std::string EmergeEngine::GetCanonicalPackageDir(const EscBuildInfo& info) {
    if (info.git_repo.empty()) {
        return "";
    }
    // Parse organization and repo from git_repo URL:
    // e.g. "https://github.com/OuluBSD/Eon.git" or "git@github.com:OuluBSD/Eon.git"
    std::string org = "OuluBSD";
    std::string repo = info.package_name;

    std::string url = info.git_repo;
    if (EndsWith(url, ".git")) {
        url = url.substr(0, url.size() - 4);
    }
    size_t last_slash = url.find_last_of("/:");
    if (last_slash != std::string::npos) {
        repo = url.substr(last_slash + 1);
        size_t prev_slash = url.find_last_of("/:", last_slash - 1);
        if (prev_slash != std::string::npos) {
            org = url.substr(prev_slash + 1, last_slash - prev_slash - 1);
        }
    }

    // Base directory is home directory ~/<org>/<repo>
    std::string home;
#ifdef _WIN32
    home = GetEnvValue("USERPROFILE");
    if (home.empty()) {
        std::string drive = GetEnvValue("HOMEDRIVE");
        std::string path = GetEnvValue("HOMEPATH");
        if (!drive.empty() && !path.empty()) home = drive + path;
    }
#else
    home = GetEnvValue("HOME");
#endif
    if (home.empty()) home = ".";

    std::string target = JoinPath(home, org);
    target = JoinPath(target, repo);
    return NormalizePath(target);
}

bool EmergeEngine::Plan(const std::vector<std::string>& atoms,
                        const std::string& profile_name,
                        EmergePlan& out_plan,
                        const std::string& overlay_root)
{
    out_plan.profile_name = profile_name.empty() ? ProfileManager::GetDefaultProfileName() : profile_name;
    out_plan.profile = ProfileManager::LoadProfile(out_plan.profile_name, overlay_root);

    std::set<std::string> visited;
    std::set<std::string> visiting;

    std::function<bool(const std::string&)> dfs = [&](const std::string& atom_name) -> bool {
        if (visiting.count(atom_name)) {
            out_plan.circular_dependencies.push_back(atom_name);
            return true; // Cycle detected, break recursion
        }
        if (visited.count(atom_name)) {
            return true;
        }

        visiting.insert(atom_name);

        EmergeItem item;
        item.atom = atom_name;

        if (StartsWith(atom_name, "Virtual/") || StartsWith(atom_name, "virtual/")) {
            item.is_virtual = true;
            item.virtual_provider = ProviderPlanner::Resolve(atom_name, overlay_root);
            item.build_info.category = "Virtual";
            item.build_info.package_name = atom_name.substr(8);
            item.build_info.version = "1";
            item.build_info.description = "Virtual capability: " + atom_name;
        } else {
            item.is_virtual = false;
            if (!OverlayManager::FindPackage(atom_name, item.build_info, overlay_root)) {
                out_plan.error = "Package atom not found in UppOverlay: " + atom_name;
                return false;
            }

            item.resolved_repo_path = GetCanonicalPackageDir(item.build_info);
            item.is_already_cloned = DirectoryExists(item.resolved_repo_path);

            // Compute active USE flags from profile and IUSE
            std::vector<std::string> profile_use = Split(out_plan.profile.default_use, ' ');
            for (const auto& u : item.build_info.iuse) {
                for (const auto& pu : profile_use) {
                    if (ToLower(u) == ToLower(pu)) {
                        item.active_use_flags.push_back(u);
                        break;
                    }
                }
            }

            // Recursively visit RDEPEND
            for (const auto& dep : item.build_info.rdepend) {
                if (!dfs(dep)) {
                    return false;
                }
            }
        }

        visiting.erase(atom_name);
        visited.insert(atom_name);
        out_plan.items.push_back(item);
        return true;
    };

    for (const auto& root_atom : atoms) {
        if (!dfs(root_atom)) {
            return false;
        }
    }

    return true;
}

std::string EmergeEngine::FormatPlan(const EmergePlan& plan) {
    std::ostringstream ss;
    ss << "These are the packages that would be merged, in order:\n\n";
    ss << "Calculating dependencies... done!\n";

    int new_count = 0;
    int inplace_count = 0;
    int virtual_count = 0;

    for (const auto& item : plan.items) {
        if (item.is_virtual) {
            virtual_count++;
            ss << "[virtual       ] " << item.atom;
            if (item.virtual_provider.is_available) {
                ss << " [satisfied via " << item.virtual_provider.provider_type << ": " << item.virtual_provider.package_ref << "]";
            } else {
                ss << " [install via " << item.virtual_provider.command << "]";
            }
            ss << "\n";
        } else {
            std::string status = item.is_already_cloned ? "  R    " : " N     ";
            if (item.is_already_cloned) inplace_count++; else new_count++;

            ss << "[ebuild " << status << "] " << item.build_info.category << "/" << item.build_info.package_name;
            if (!item.build_info.version.empty()) {
                ss << "-" << item.build_info.version;
            }
            if (!item.active_use_flags.empty()) {
                ss << " USE=\"";
                for (size_t i = 0; i < item.active_use_flags.size(); i++) {
                    if (i > 0) ss << " ";
                    ss << item.active_use_flags[i];
                }
                ss << "\"";
            }
            if (!item.resolved_repo_path.empty()) {
                ss << " (" << item.resolved_repo_path << ")";
            }
            ss << "\n";
        }
    }

    ss << "\nTotal: " << plan.items.size() << " package(s) ("
       << new_count << " new, " << inplace_count << " in-place, "
       << virtual_count << " virtual(s))\n";
    ss << "Profile: " << plan.profile_name
       << " (Method: " << (plan.profile.build_method.empty() ? "(default)" : plan.profile.build_method)
       << ", Cache: " << plan.profile.output_cache << ")\n";

    if (!plan.circular_dependencies.empty()) {
        ss << "\n* Note: Circular dependency detected involving: ";
        for (size_t i = 0; i < plan.circular_dependencies.size(); i++) {
            if (i > 0) ss << ", ";
            ss << plan.circular_dependencies[i];
        }
        ss << "\n";
    }

    return ss.str();
}

bool EmergeEngine::Execute(const EmergePlan& plan, bool update, bool verbose) {
    std::cout << ">>> Emerging packages with profile '" << plan.profile_name << "'...\n";
    for (size_t i = 0; i < plan.items.size(); i++) {
        const auto& item = plan.items[i];
        std::cout << "\n>>> (" << (i + 1) << " of " << plan.items.size() << ") Emerging " << item.atom << "...\n";

        if (item.is_virtual) {
            if (item.virtual_provider.provider_type == "upp-plugin") {
                std::cout << ">>> Using bundled U++ plugin provider: " << item.virtual_provider.package_ref << "\n";
            } else if (item.virtual_provider.is_available) {
                std::cout << ">>> Host dependency already satisfied: " << item.virtual_provider.package_ref << " (" << item.virtual_provider.provider_type << ")\n";
            } else {
                std::cout << ">>> Installing host dependency: " << item.virtual_provider.command << "\n";
                int code = RunCommand(item.virtual_provider.command);
                if (code != 0) {
                    std::cerr << ">>> Warning: Failed to install host dependency via '" << item.virtual_provider.command << "' (exit code " << code << ")\n";
                }
            }
            continue;
        }

        // 1. Unpack / clone / fetch git repo
        std::string repo_dir = item.resolved_repo_path;
        if (!DirectoryExists(repo_dir)) {
            std::string parent = GetParentDir(repo_dir);
            RealizeDirectory(parent);
            std::string clone_cmd = "git clone";
            if (!item.build_info.git_branch.empty()) {
                clone_cmd += " --branch " + item.build_info.git_branch;
            }
            clone_cmd += " " + item.build_info.git_repo + " \"" + repo_dir + "\"";
            std::cout << ">>> Unpacking " << item.atom << " to " << repo_dir << "...\n";
            if (verbose) std::cout << "  " << clone_cmd << "\n";
            int ret = RunCommand(clone_cmd);
            if (ret != 0) {
                std::cerr << "Error: Failed to clone repository for " << item.atom << "\n";
                return false;
            }
        } else if (update) {
            std::cout << ">>> Updating existing repository at " << repo_dir << "...\n";
            std::string pull_cmd = "git -C \"" + repo_dir + "\" pull";
            if (verbose) std::cout << "  " << pull_cmd << "\n";
            RunCommand(pull_cmd);
        }

        // 2. Compile package
        if (!item.build_info.default_package.empty()) {
            std::string target = item.build_info.default_package;
            std::cout << ">>> Building target '" << target << "' in " << repo_dir << "...\n";

            // Build command using uppbuilder
            std::string cmd;
#ifdef _WIN32
            cmd = "uppbuilder.exe";
#else
            cmd = "uppbuilder";
#endif
            cmd += " " + target;
            if (!plan.profile.build_method.empty()) {
                cmd += " -m " + plan.profile.build_method;
            }
            if (!plan.profile_name.empty()) {
                cmd += " --profile " + plan.profile_name;
            }
            if (verbose) {
                cmd += " -v";
            }

            // Execute build inside the repo directory
            std::string run_in_dir;
#ifdef _WIN32
            run_in_dir = "cd /d \"" + repo_dir + "\" && " + cmd;
#else
            run_in_dir = "cd \"" + repo_dir + "\" && " + cmd;
#endif
            if (verbose) std::cout << "  " << run_in_dir << "\n";
            int ret = RunCommand(run_in_dir);
            if (ret != 0) {
                std::cerr << "Error: Build failed for " << item.atom << " with exit code " << ret << "\n";
                return false;
            }
        } else if (!item.build_info.fn_src_compile.empty()) {
            // Custom bootstrap or make in src_compile
            if (item.build_info.fn_src_compile.find("bootstrap") != std::string::npos) {
                std::cout << ">>> Running bootstrap in " << repo_dir << "...\n";
                std::string bcmd;
#ifdef _WIN32
                bcmd = "cd /d \"" + repo_dir + "\" && bootstrap.bat";
#else
                bcmd = "cd \"" + repo_dir + "\" && ./bootstrap.sh";
#endif
                int ret = RunCommand(bcmd);
                if (ret != 0) {
                    std::cerr << "Error: Bootstrap failed for " << item.atom << "\n";
                    return false;
                }
            } else if (item.build_info.fn_src_compile.find("make") != std::string::npos) {
                std::cout << ">>> Running make in " << repo_dir << "...\n";
                std::string mcmd;
#ifdef _WIN32
                mcmd = "cd /d \"" + repo_dir + "\" && nmake";
#else
                mcmd = "cd \"" + repo_dir + "\" && make";
#endif
                int ret = RunCommand(mcmd);
                if (ret != 0) {
                    std::cerr << "Error: Make failed for " << item.atom << "\n";
                    return false;
                }
            }
        }

        std::cout << ">>> Successfully emerged " << item.atom << "!\n";
    }

    std::cout << "\n*** Emerge process completed successfully.\n";
    return true;
}

} // namespace UppBuilder
