#include "EmergeEngine.h"
#include "VdbManager.h"
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
    out_plan.profile_name = profile_name.empty() ? ProfileManager::GetActiveProfileName(overlay_root) : profile_name;
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

bool EmergeEngine::Execute(const EmergePlan& plan, bool update, bool verbose, bool run_tests) {
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
        } else if (!item.build_info.default_package.empty()) {
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
            // Custom make in src_compile
            if (item.build_info.fn_src_compile.find("make") != std::string::npos) {
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

        // 3. Register package in VDB
        VdbEntry vdb_entry;
        vdb_entry.atom = item.atom;
        vdb_entry.version = item.build_info.version.empty() ? "0.1.0" : item.build_info.version;
        vdb_entry.profile = plan.profile_name;
        for (const auto& f : item.active_use_flags) {
            if (!vdb_entry.use.empty()) vdb_entry.use += " ";
            vdb_entry.use += f;
        }
        vdb_entry.repo_path = repo_dir;
        vdb_entry.target = item.build_info.default_package;
        vdb_entry.rdepend = item.build_info.rdepend;

        std::string bin_dir = repo_dir + "/bin";
        if (DirectoryExists(bin_dir)) {
            vdb_entry.files.push_back(bin_dir);
        }
        VdbManager::RegisterPackage(vdb_entry);
        if (verbose) {
            std::cout << ">>> Registered " << item.atom << " in VDB database.\n";
        }

        // 4. Run tests if requested
        if (run_tests && !item.is_virtual) {
            std::cout << ">>> Running tests for " << item.atom << "...\n";
            TestResult res;
            if (!ExecuteSingleTest(item.build_info, repo_dir, res, verbose)) {
                std::cerr << "Error: Test suite failed for " << item.atom << ": " << res.message << "\n";
                return false;
            }
            if (res.message != "No tests defined") {
                std::cout << ">>> Test suite passed for " << item.atom << " (" << res.duration_ms << "ms).\n";
            }
        }

        std::cout << ">>> Successfully emerged " << item.atom << "!\n";
    }

    std::cout << "\n*** Emerge process completed successfully.\n";
    return true;
}

static std::string FindBinaryInRepo(const std::string& repo_dir, const std::string& target) {
    std::string bin_dir = JoinPath(repo_dir, "bin");
    if (DirectoryExists(bin_dir)) {
        std::string p1 = JoinPath(bin_dir, target);
        if (FileExists(p1)) return p1;
#ifdef _WIN32
        std::string p1_exe = JoinPath(bin_dir, target + ".exe");
        if (FileExists(p1_exe)) return p1_exe;
#endif
        std::vector<std::string> files;
        Walk(bin_dir, files);
        for (const auto& f : files) {
            std::string name = GetFileName(f);
            if (name == target) {
#ifndef _WIN32
                if (access(f.c_str(), X_OK) == 0) return f;
#else
                return f;
#endif
            }
#ifdef _WIN32
            if (name == target + ".exe") return f;
#endif
        }
    }
    std::string p_tests = JoinPath(JoinPath(repo_dir, "tests"), target);
    if (FileExists(p_tests)) return p_tests;
    std::string p_root = JoinPath(repo_dir, target);
    if (FileExists(p_root)) return p_root;
    return "";
}

bool EmergeEngine::ExecuteSingleTest(const EscBuildInfo& info,
                                     const std::string& repo_dir,
                                     TestResult& out_result,
                                     bool verbose)
{
    out_result.atom = info.package_name;
    out_result.success = true;
    out_result.duration_ms = 0;
    out_result.suite = "";

    if (info.fn_src_test.empty()) {
        // Fallback: check if default_package is a test package
        if (!info.default_package.empty() &&
            (info.default_package.find("Check") != std::string::npos ||
             info.default_package.find("Test") != std::string::npos ||
             info.default_package.find("tests/") != std::string::npos))
        {
            std::string binary_name = GetFileName(info.default_package);
            out_result.suite = binary_name;
            std::string bin_path = FindBinaryInRepo(repo_dir, binary_name);
            if (bin_path.empty()) {
                out_result.success = false;
                out_result.message = "Binary '" + binary_name + "' not found in " + repo_dir;
                return false;
            }
            std::string cmd;
#ifdef _WIN32
            cmd = "cd /d \"" + repo_dir + "\" && \"" + bin_path + "\"";
#else
            cmd = "cd \"" + repo_dir + "\" && \"" + bin_path + "\"";
#endif
            if (verbose) std::cout << ">>> Running " << cmd << "...\n";
            auto t0 = std::chrono::steady_clock::now();
            int ret = RunCommand(cmd);
            auto t1 = std::chrono::steady_clock::now();
            out_result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            if (ret != 0) {
                out_result.success = false;
                out_result.message = "Failed with exit code " + std::to_string(ret);
                return false;
            }
            out_result.message = "PASS";
            return true;
        }

        out_result.message = "No tests defined";
        return true;
    }

    // Parse commands inside fn_src_test
    std::istringstream iss(info.fn_src_test);
    std::string line;
    long total_ms = 0;
    std::vector<std::string> suites_run;

    while (std::getline(iss, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || StartsWith(line, "//")) continue;

        if (StartsWith(line, "UppBuild(") || StartsWith(line, "uppbuild(")) {
            size_t q1 = line.find('"');
            size_t q2 = line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                std::string target = line.substr(q1 + 1, q2 - q1 - 1);
                std::string bcmd;
#ifdef _WIN32
                bcmd = "cd /d \"" + repo_dir + "\" && uppbuilder.exe " + target;
#else
                bcmd = "cd \"" + repo_dir + "\" && uppbuilder " + target;
#endif
                if (verbose) std::cout << ">>> Building test target: " << bcmd << "\n";
                int ret = RunCommand(bcmd);
                if (ret != 0) {
                    out_result.success = false;
                    out_result.message = "Failed to build test target: " + target;
                    return false;
                }
            }
        } else if (StartsWith(line, "Run(") || StartsWith(line, "run(")) {
            size_t q1 = line.find('"');
            size_t q2 = line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                std::string target = line.substr(q1 + 1, q2 - q1 - 1);
                suites_run.push_back(target);
                std::string bin_path = FindBinaryInRepo(repo_dir, target);
                if (bin_path.empty()) {
                    out_result.success = false;
                    out_result.suite = target;
                    out_result.message = "Binary '" + target + "' not found in " + repo_dir;
                    return false;
                }
                std::string cmd;
#ifdef _WIN32
                cmd = "cd /d \"" + repo_dir + "\" && \"" + bin_path + "\"";
#else
                cmd = "cd \"" + repo_dir + "\" && \"" + bin_path + "\"";
#endif
                if (verbose) std::cout << ">>> Running " << cmd << "...\n";
                auto t0 = std::chrono::steady_clock::now();
                int ret = RunCommand(cmd);
                auto t1 = std::chrono::steady_clock::now();
                long ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
                total_ms += ms;
                if (ret != 0) {
                    out_result.success = false;
                    out_result.suite = target;
                    out_result.duration_ms = total_ms;
                    out_result.message = "Suite '" + target + "' failed with exit code " + std::to_string(ret);
                    return false;
                }
            }
        } else if (StartsWith(line, "Exec(") || StartsWith(line, "exec(")) {
            size_t q1 = line.find('"');
            size_t q2 = line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                std::string subcmd = line.substr(q1 + 1, q2 - q1 - 1);
                suites_run.push_back(subcmd);
                std::string cmd;
#ifdef _WIN32
                cmd = "cd /d \"" + repo_dir + "\" && " + subcmd;
#else
                cmd = "cd \"" + repo_dir + "\" && " + subcmd;
#endif
                if (verbose) std::cout << ">>> Executing " << cmd << "...\n";
                auto t0 = std::chrono::steady_clock::now();
                int ret = RunCommand(cmd);
                auto t1 = std::chrono::steady_clock::now();
                long ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
                total_ms += ms;
                if (ret != 0) {
                    out_result.success = false;
                    out_result.suite = subcmd;
                    out_result.duration_ms = total_ms;
                    out_result.message = "Command '" + subcmd + "' failed with exit code " + std::to_string(ret);
                    return false;
                }
            }
        }
    }

    out_result.duration_ms = total_ms;
    out_result.suite = JoinStrings(suites_run, ", ");
    out_result.message = "PASS";
    return true;
}

static std::vector<std::string> DiscoverAssemblyPackages(const std::string& assembly_dir) {
    std::vector<std::string> pkgs;
    if (!DirectoryExists(assembly_dir)) return pkgs;

#if defined(__has_include) && __has_include(<filesystem>)
    try {
        for (const auto& entry : fs::directory_iterator(assembly_dir)) {
            if (entry.is_directory()) {
                std::string pkg_name = entry.path().filename().string();
                if (pkg_name.empty() || pkg_name[0] == '.') continue;
                std::string upp_file = JoinPath(entry.path().string(), pkg_name + ".upp");
                if (FileExists(upp_file)) {
                    pkgs.push_back(pkg_name);
                }
            }
        }
    } catch (...) {}
#endif
    std::sort(pkgs.begin(), pkgs.end());
    return pkgs;
}

bool EmergeEngine::RunTests(const std::vector<std::string>& atoms,
                            TestSummary& out_summary,
                            bool verbose,
                            const std::string& overlay_root,
                            const std::string& bazaarcore_root)
{
    std::string bc = bazaarcore_root;
    if (bc.empty()) {
        std::string home = GetEnvValue("HOME");
        if (!home.empty()) {
            if (DirectoryExists(JoinPath(home, "OuluBSD/BazaarCore"))) bc = JoinPath(home, "OuluBSD/BazaarCore");
            else if (DirectoryExists(JoinPath(home, "Dev/BazaarCore"))) bc = JoinPath(home, "Dev/BazaarCore");
        }
    }

    std::vector<std::string> target_atoms;
    for (const auto& a : atoms) {
        if (a == "@installed" || (atoms.size() == 1 && atoms[0].empty())) {
            auto installed = VdbManager::ListInstalled();
            for (const auto& entry : installed) {
                target_atoms.push_back(entry.atom);
            }
        } else if (a == "@all") {
            auto all_pkgs = OverlayManager::ListAllPackages(overlay_root);
            for (const auto& pkg : all_pkgs) {
                target_atoms.push_back(pkg.category + "/" + pkg.package_name);
            }
        } else if (a == "@autotest") {
            std::string autotest_dir = JoinPath(bc, "applied-autotest");
            auto pkgs = DiscoverAssemblyPackages(autotest_dir);
            for (const auto& p : pkgs) {
                target_atoms.push_back("autotest/" + p);
            }
        } else if (a == "@upptst") {
            std::string upptst_dir = JoinPath(bc, "applied-upptst");
            auto pkgs = DiscoverAssemblyPackages(upptst_dir);
            for (const auto& p : pkgs) {
                target_atoms.push_back("upptst/" + p);
            }
        } else {
            target_atoms.push_back(a);
        }
    }

    if (target_atoms.empty()) {
        std::cout << ">>> No packages to test.\n";
        return true;
    }

    std::cout << ">>> Starting test execution across " << target_atoms.size() << " package(s)...\n\n";

    long grand_total_ms = 0;
    out_summary.passed = 0;
    out_summary.failed = 0;
    out_summary.skipped = 0;
    out_summary.results.clear();

    for (const auto& atom : target_atoms) {
        // Handle upstream assembly test packages (autotest/*, upptst/*)
        if (StartsWith(atom, "autotest/") || StartsWith(atom, "upptst/")) {
            size_t slash = atom.find('/');
            std::string prefix = atom.substr(0, slash);
            std::string pkg_name = atom.substr(slash + 1);

            // 1. Build test target via uppbuilder
            std::string bcmd;
#ifdef _WIN32
            bcmd = "uppbuilder.exe " + atom;
#else
            bcmd = "uppbuilder " + atom;
#endif
            if (verbose) std::cout << ">>> Building upstream test: " << bcmd << "\n";
            int b_ret = RunCommand(bcmd + (verbose ? "" : " >/dev/null 2>&1"));
            if (b_ret != 0) {
                out_summary.failed++;
                TestResult res;
                res.atom = atom;
                res.suite = pkg_name;
                res.success = false;
                res.duration_ms = 0;
                res.message = "Build failed (exit code " + std::to_string(b_ret) + ")";
                out_summary.results.push_back(res);
                std::cout << "  [FAIL] " << atom << " (0ms): " << res.message << "\n";
                continue;
            }

            // 2. Locate built executable
            std::string bin_path;
            std::string local_bin = JoinPath("bin", pkg_name);
            if (FileExists(local_bin)) bin_path = local_bin;
            else if (FileExists(local_bin + ".exe")) bin_path = local_bin + ".exe";
            else if (!bc.empty()) {
                std::string bc_bin = JoinPath(JoinPath(bc, "bin"), pkg_name);
                if (FileExists(bc_bin)) bin_path = bc_bin;
                else if (FileExists(bc_bin + ".exe")) bin_path = bc_bin + ".exe";
            }
            if (bin_path.empty()) {
                bin_path = FindBinaryInRepo(".", pkg_name);
                if (bin_path.empty() && !bc.empty()) bin_path = FindBinaryInRepo(bc, pkg_name);
            }

            if (bin_path.empty()) {
                out_summary.failed++;
                TestResult res;
                res.atom = atom;
                res.suite = pkg_name;
                res.success = false;
                res.duration_ms = 0;
                res.message = "Test executable '" + pkg_name + "' not found";
                out_summary.results.push_back(res);
                std::cout << "  [FAIL] " << atom << " (0ms): " << res.message << "\n";
                continue;
            }

            // 3. Execute test binary and record metrics
            std::string run_cmd = "\"" + bin_path + "\"";
            auto t0 = std::chrono::steady_clock::now();
            int run_exit = 0;
            std::string output = CaptureCommand(run_cmd + " 2>&1", &run_exit);
            auto t1 = std::chrono::steady_clock::now();
            long ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            grand_total_ms += ms;

            TestResult res;
            res.atom = atom;
            res.suite = pkg_name;
            res.duration_ms = ms;

            if (run_exit == 0) {
                out_summary.passed++;
                res.success = true;
                res.message = "PASS";
                std::cout << "  [PASS] " << atom << " (" << ms << "ms)\n";
            } else {
                out_summary.failed++;
                res.success = false;
                res.message = "Process exited with code " + std::to_string(run_exit);
                std::cout << "  [FAIL] " << atom << " (" << ms << "ms): " << res.message << "\n";
                if (!output.empty()) {
                    std::cout << "    Output:\n";
                    std::istringstream oss(output);
                    std::string line;
                    int lcnt = 0;
                    while (std::getline(oss, line) && lcnt++ < 10) {
                        std::cout << "      " << line << "\n";
                    }
                }
            }
            out_summary.results.push_back(res);
            continue;
        }

        EscBuildInfo info;
        if (!OverlayManager::FindPackage(atom, info, overlay_root)) {
            if (verbose) std::cout << "--- " << atom << ": .escbuild definition not found.\n";
            out_summary.skipped++;
            continue;
        }

        // Determine repo directory: check Vdb first, then canonical directory
        std::string repo_dir;
        VdbEntry vdb_entry;
        if (VdbManager::IsInstalled(atom, &vdb_entry) && !vdb_entry.repo_path.empty() && DirectoryExists(vdb_entry.repo_path)) {
            repo_dir = vdb_entry.repo_path;
        } else {
            repo_dir = GetCanonicalPackageDir(info);
        }

        if (repo_dir.empty() || !DirectoryExists(repo_dir)) {
            if (verbose) std::cout << "--- " << atom << ": Repository not checked out or not found.\n";
            out_summary.skipped++;
            continue;
        }

        if (info.fn_src_test.empty() && info.default_package.empty()) {
            if (verbose) std::cout << "--- " << atom << ": No test suite defined.\n";
            out_summary.skipped++;
            continue;
        }

        // Execute tests
        TestResult res;
        bool ok = ExecuteSingleTest(info, repo_dir, res, verbose);
        res.atom = atom;
        out_summary.results.push_back(res);
        grand_total_ms += res.duration_ms;

        if (res.message == "No tests defined") {
            out_summary.skipped++;
            if (verbose) {
                std::cout << "--- " << atom << ": No tests defined (skipped)\n";
            }
        } else if (ok) {
            out_summary.passed++;
            std::cout << "  [PASS] " << atom;
            if (!res.suite.empty()) std::cout << " [" << res.suite << "]";
            std::cout << " (" << res.duration_ms << "ms)\n";
        } else {
            out_summary.failed++;
            std::cout << "  [FAIL] " << atom;
            if (!res.suite.empty()) std::cout << " [" << res.suite << "]";
            std::cout << " (" << res.duration_ms << "ms): " << res.message << "\n";
        }
    }

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Test Summary: " << out_summary.passed << " passed, "
              << out_summary.failed << " failed, "
              << out_summary.skipped << " skipped (Total time: "
              << (grand_total_ms / 1000.0) << "s)\n";
    std::cout << std::string(70, '=') << "\n";

    return (out_summary.failed == 0);
}

bool EmergeEngine::Unmerge(const std::vector<std::string>& atoms, bool force, bool verbose) {
    if (atoms.empty()) {
        std::cerr << "Error: No packages specified to unmerge.\n";
        return false;
    }

    bool all_ok = true;
    for (const auto& atom : atoms) {
        std::cout << "\n>>> Unmerging " << atom << "...\n";
        VdbEntry entry;
        if (!VdbManager::IsInstalled(atom, &entry)) {
            std::cout << "--- " << atom << " is not recorded in VDB.\n";
            continue;
        }

        // Check reverse dependencies
        auto rev = VdbManager::FindReverseDependencies(atom);
        if (!rev.empty() && !force) {
            std::cerr << "Error: Cannot unmerge " << atom << " because the following installed packages depend on it:\n";
            for (const auto& r : rev) {
                std::cerr << "  * " << r << "\n";
            }
            std::cerr << "Use --force to unmerge anyway.\n";
            all_ok = false;
            continue;
        }

        // Clean built outputs recorded in files
        for (const auto& f : entry.files) {
            if (verbose) std::cout << ">>> Cleaning build artifacts: " << f << "\n";
        }

        if (VdbManager::UnregisterPackage(atom)) {
            std::cout << ">>> Successfully unmerged " << atom << ".\n";
        } else {
            std::cerr << "Error: Failed to unregister " << atom << " from VDB.\n";
            all_ok = false;
        }
    }
    return all_ok;
}

} // namespace UppBuilder
