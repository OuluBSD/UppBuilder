#include "UppBuilder.h"
#include "Bridge.h"
#include "UppParser.h"
#include "DepGraph.h"
#include "HeaderScanner.h"
#include "UmkDriver.h"
#include "RuntimeDeps.h"

using namespace UppBuilder;

void PrintUsage(const char* prog) {
    std::cout << "UppBuilder - Advanced U++ Meta-Builder & 3-Pillar Bridge\n";
    std::cout << "Usage: " << prog << " [options] <target_package>\n\n";
    std::cout << "Build Options:\n";
    std::cout << "  -r, --release                 Build in release mode (default: debug)\n";
    std::cout << "  -c, --clean                   Clean build before compiling\n";
    std::cout << "  -m, --method <name>           Specify build method (e.g. CLANG, GCC, MSVS22x64)\n";
    std::cout << "  --mainconf <name|idx>         Select specific mainconfig configuration\n";
    std::cout << "  -H, --jobs <N>                Number of parallel compilation jobs\n";
    std::cout << "  -s, --shared                  Link with shared libraries\n";
    std::cout << "  -S, --build-shared            Build and link as shared libraries\n";
    std::cout << "  --static                      Force static library linking\n";
    std::cout << "  -o, --output <path>           Custom output binary path\n";
    std::cout << "  --copy-deps                   Copy non-system runtime libraries next to binary\n";
    std::cout << "  -v, --verbose                 Verbose diagnostics and commands\n\n";
    std::cout << "3-Pillar Bridge Options:\n";
    std::cout << "  --redirect <from>=<to>        Add package redirection mapping\n";
    std::cout << "  --redirects-file <file.json>  Load redirects from JSON file\n";
    std::cout << "  --overlay <target>=<source>   Add file overlay rule\n";
    std::cout << "  --overlays-file <file.json>   Load overlays from JSON file\n";
    std::cout << "  --patch <file>[@<dir>]        Add surgical patch to apply to staged workspace\n";
    std::cout << "  --patches-file <file.json>    Load patch list from JSON file\n";
    std::cout << "  --staging-dir <dir>           Custom staging directory (default: .uppbuilder/staging)\n\n";
    std::cout << "Applied-Patches & Restructuring Options:\n";
    std::cout << "  --bazaarcore <dir>            Path to BazaarCore assembly repository (foundation for other repos)\n";
    std::cout << "  --apply-patches               Materialize applied-patches with restructuring & apply patches\n";
    std::cout << "  --make-patches                Reverse-diff applied-patches against upstream into patches/\n";
    std::cout << "  --applied-patches <dir>       Path to applied-patches directory (default: applied-patches)\n";
    std::cout << "  --upstream <dir>              Path to pristine upstream ultimatepp/uppsrc\n";
    std::cout << "  --patches-dir <dir>           Path to patches directory (default: patches)\n\n";
    std::cout << "Inspection & Graph Analysis:\n";
    std::cout << "  --list-methods                List available build methods\n";
    std::cout << "  --list-mainconfigs <pkg>      List mainconfigs defined in package\n";
    std::cout << "  --dump-deps <pkg>             Print resolved active dependency tree\n";
    std::cout << "  --graph <format>              Output dependency graph (mermaid, dot, json)\n";
    std::cout << "  --scan-headers [pkg]          Scan header inclusions and report inter-package coupling\n";
    std::cout << "  --report-headers <file.md>    Save header coupling analysis to markdown file\n\n";
    std::cout << "General:\n";
    std::cout << "  -h, --help                    Show this help message\n";
    std::cout << "  --version                     Print version information\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 0;
    }

    std::string target_pkg;
    bool release = false;
    bool clean = false;
    std::string method_pref;
    std::string mainconf_pref;
    int jobs = 0;
    int shared_mode = 0; // default
    std::string output_path;
    bool copy_deps = false;
    bool verbose = false;

    bool list_methods = false;
    std::string list_mainconfs_pkg;
    std::string dump_deps_pkg;
    std::string graph_format;
    bool scan_headers = false;
    std::string scan_headers_focus;
    std::string report_headers_file;

    std::string staging_dir = ".uppbuilder/staging";
    std::string applied_patches_dir;
    std::string patches_dir = "patches";
    std::string upstream_dir;
    std::string custom_bazaarcore;
    std::string custom_superintelligence;
    std::string custom_commandbridge;
    bool apply_patches_cmd = false;
    bool make_patches_cmd = false;
    std::string custom_umk;
    std::vector<std::string> extra_roots;

    BridgeEngine bridge;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            std::cout << "UppBuilder version 0.1.0 (Advanced U++ Meta-Builder)\n";
            return 0;
        } else if (arg == "-r" || arg == "--release") {
            release = true;
        } else if (arg == "-c" || arg == "--clean") {
            clean = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-s" || arg == "--shared") {
            shared_mode = 2;
        } else if (arg == "-S" || arg == "--build-shared") {
            shared_mode = 3;
        } else if (arg == "--static") {
            shared_mode = 1;
        } else if (arg == "--copy-deps") {
            copy_deps = true;
        } else if ((arg == "-m" || arg == "--method") && i + 1 < argc) {
            method_pref = argv[++i];
        } else if (arg == "--mainconf" && i + 1 < argc) {
            mainconf_pref = argv[++i];
        } else if ((arg == "-H" || arg == "--jobs") && i + 1 < argc) {
            jobs = std::atoi(argv[++i]);
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--umk" && i + 1 < argc) {
            custom_umk = argv[++i];
        } else if (arg == "--staging-dir" && i + 1 < argc) {
            staging_dir = argv[++i];
        } else if ((arg == "--applied-patches" || arg == "--patched-dir") && i + 1 < argc) {
            applied_patches_dir = argv[++i];
        } else if (arg == "--patches-dir" && i + 1 < argc) {
            patches_dir = argv[++i];
        } else if (arg == "--upstream" && i + 1 < argc) {
            upstream_dir = argv[++i];
        } else if (arg == "--bazaarcore" && i + 1 < argc) {
            custom_bazaarcore = argv[++i];
        } else if (arg == "--superintelligence" && i + 1 < argc) {
            custom_superintelligence = argv[++i];
        } else if (arg == "--commandbridge" && i + 1 < argc) {
            custom_commandbridge = argv[++i];
        } else if (arg == "--apply-patches") {
            apply_patches_cmd = true;
        } else if (arg == "--make-patches") {
            make_patches_cmd = true;
        } else if (arg == "--redirect" && i + 1 < argc) {
            std::string mapping = argv[++i];
            size_t eq = mapping.find('=');
            if (eq != std::string::npos) {
                bridge.AddRedirect(mapping.substr(0, eq), mapping.substr(eq + 1));
            }
        } else if (arg == "--redirects-file" && i + 1 < argc) {
            bridge.LoadRedirectsFile(argv[++i]);
        } else if (arg == "--overlay" && i + 1 < argc) {
            std::string rule = argv[++i];
            size_t eq = rule.find('=');
            if (eq != std::string::npos) {
                bridge.AddOverlay(rule.substr(0, eq), rule.substr(eq + 1));
            }
        } else if (arg == "--overlays-file" && i + 1 < argc) {
            bridge.LoadOverlaysFile(argv[++i]);
        } else if (arg == "--patch" && i + 1 < argc) {
            std::string p_arg = argv[++i];
            size_t at = p_arg.find('@');
            if (at != std::string::npos) {
                bridge.AddPatch(p_arg.substr(0, at), p_arg.substr(at + 1));
            } else {
                bridge.AddPatch(p_arg);
            }
        } else if (arg == "--patches-file" && i + 1 < argc) {
            bridge.LoadPatchesFile(argv[++i]);
        } else if (arg == "--list-methods") {
            list_methods = true;
        } else if (arg == "--list-mainconfigs" && i + 1 < argc) {
            list_mainconfs_pkg = argv[++i];
        } else if (arg == "--dump-deps" && i + 1 < argc) {
            dump_deps_pkg = argv[++i];
        } else if (arg == "--graph" && i + 1 < argc) {
            graph_format = ToLower(argv[++i]);
        } else if (arg == "--scan-headers") {
            scan_headers = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                scan_headers_focus = argv[++i];
            }
        } else if (arg == "--report-headers" && i + 1 < argc) {
            report_headers_file = argv[++i];
            scan_headers = true;
        } else if (arg == "-a" || arg == "--add-root") {
            if (i + 1 < argc) extra_roots.push_back(argv[++i]);
        } else if (arg[0] != '-') {
            target_pkg = arg;
        }
    }

    std::string cwd = GetCurrentDir();
    std::string repo_root = cwd;
    // Discover repo root: search upwards for .git first, then fallback to uppsrc
    std::string search = cwd;
    while (!search.empty()) {
        if (DirectoryExists(JoinPath(search, ".git"))) {
            repo_root = search;
            break;
        }
        std::string parent = GetParentDir(search);
        if (parent == search || parent.empty()) break;
        search = parent;
    }
    if (repo_root == cwd && !DirectoryExists(JoinPath(cwd, ".git"))) {
        if (DirectoryExists(JoinPath(cwd, "uppsrc"))) {
            repo_root = cwd;
        } else if (DirectoryExists(JoinPath(GetParentDir(cwd), "uppsrc"))) {
            repo_root = GetParentDir(cwd);
        }
    }

    std::string bazaarcore_dir = ResolveBazaarCorePath(custom_bazaarcore, repo_root);
    std::string base_for_patches = !bazaarcore_dir.empty() ? bazaarcore_dir : repo_root;

    std::string upstream_dir_resolved = ResolveUpstreamPath(upstream_dir, base_for_patches);
    std::string full_applied = ResolvePatchedUppsrcPath(applied_patches_dir, base_for_patches);
    std::string full_patches = (StartsWith(patches_dir, "/") || (patches_dir.size() >= 2 && patches_dir[1] == ':')) ?
                               patches_dir : JoinPath(base_for_patches, patches_dir);

    // Handle --make-patches command
    if (make_patches_cmd) {
        if (upstream_dir_resolved.empty() || !DirectoryExists(upstream_dir_resolved)) {
            std::cerr << "Error: Upstream ultimatepp/uppsrc directory not found. Specify with --upstream <path>\n";
            return 1;
        }
        if (!bridge.GeneratePatches(upstream_dir_resolved, full_applied, full_patches, verbose)) {
            return 1;
        }
        return 0;
    }

    // Handle --apply-patches command
    if (apply_patches_cmd) {
        if (upstream_dir_resolved.empty() || !DirectoryExists(upstream_dir_resolved)) {
            std::cerr << "Error: Upstream ultimatepp/uppsrc directory not found. Specify with --upstream <path>\n";
            return 1;
        }
        if (!bridge.MaterializeAppliedPatches(upstream_dir_resolved, full_applied, full_patches, true, verbose)) {
            return 1;
        }
        if (target_pkg.empty()) return 0;
    }

    // Auto-materialize patched-uppsrc if missing and upstream is available
    if (!DirectoryExists(full_applied) && !upstream_dir_resolved.empty()) {
        if (verbose) std::cout << "Notice: patched assembly not found at " << full_applied << ". Materializing...\n";
        bridge.MaterializeAppliedPatches(upstream_dir_resolved, full_applied, full_patches, false, verbose);
    }

    // Assemble roots for umk:
    // 1. Patched upstream foundation assembly (patched Core + upstream packages)
    // 2. BazaarCore uppsrc (Core/Ext, shims, Terminal, Docking, Manager, etc.) if external
    // 3. Current repository roots (uppsrc, upptst, src, root)
    // 4. Any extra roots passed via -a / --add-root
    std::vector<std::string> assembly_roots;
    if (DirectoryExists(full_applied)) {
        assembly_roots.push_back(full_applied);
    }
    if (!bazaarcore_dir.empty() && bazaarcore_dir != repo_root) {
        std::string bz_uppsrc = JoinPath(bazaarcore_dir, "uppsrc");
        if (DirectoryExists(bz_uppsrc)) {
            assembly_roots.push_back(bz_uppsrc);
        }
    }
    std::string superintel_dir = ResolveSiblingRepoPath("SuperIntelligence", custom_superintelligence, repo_root);
    if (!superintel_dir.empty() && superintel_dir != repo_root) {
        std::string s_uppsrc = JoinPath(superintel_dir, "uppsrc");
        if (DirectoryExists(s_uppsrc)) {
            assembly_roots.push_back(s_uppsrc);
        } else {
            assembly_roots.push_back(superintel_dir);
        }
    }
    std::string commandbridge_dir = ResolveSiblingRepoPath("CommandBridge", custom_commandbridge, repo_root);
    if (!commandbridge_dir.empty() && commandbridge_dir != repo_root) {
        std::string c_uppsrc = JoinPath(commandbridge_dir, "uppsrc");
        if (DirectoryExists(c_uppsrc)) {
            assembly_roots.push_back(c_uppsrc);
        }
        assembly_roots.push_back(commandbridge_dir);
    }
    assembly_roots.push_back(JoinPath(repo_root, "uppsrc"));
    assembly_roots.push_back(JoinPath(repo_root, "upptst"));
    assembly_roots.push_back(JoinPath(repo_root, "src"));
    assembly_roots.push_back(repo_root);
    for (const auto& er : extra_roots) assembly_roots.push_back(er);

    UmkDriver umk_driver;

    if (list_methods) {
        auto methods = umk_driver.CollectMethods(repo_root);
        std::cout << "Available Build Methods (" << methods.size() << "):\n";
        for (size_t i = 0; i < methods.size(); ++i) {
            std::cout << "  [" << (i + 1) << "] " << methods[i].name << " (" << methods[i].path << ") -> " << methods[i].builder << "\n";
        }
        return 0;
    }

    if (scan_headers) {
        std::cout << "==> UppBuilder: Scanning header inclusions across assembly roots...\n";
        HeaderScanner scanner(assembly_roots);
        scanner.IndexPackages();
        scanner.ScanAll();
        scanner.PrintConsoleSummary(scan_headers_focus);
        if (!report_headers_file.empty()) {
            std::string md = scanner.GenerateMarkdownReport(scan_headers_focus);
            WriteTextFile(report_headers_file, md);
            std::cout << "Header coupling report written to: " << report_headers_file << "\n";
        }
        return 0;
    }

    std::string pkg_for_deps = !dump_deps_pkg.empty() ? dump_deps_pkg : target_pkg;
    if (!graph_format.empty() || !dump_deps_pkg.empty()) {
        if (pkg_for_deps.empty()) {
            std::cerr << "Error: Target package must be specified for dependency analysis.\n";
            return 1;
        }

        std::set<std::string> flags;
        if (release) flags.insert("RELEASE");
        else flags.insert("DEBUG");

        DependencyGraph graph(bridge, assembly_roots);
        if (!graph.BuildGraph(pkg_for_deps, flags)) {
            std::cerr << "Dependency error: Circular dependency failed.\n";
            return 1;
        }

        if (graph.HasCycle()) {
            std::cout << "Notice: Circular package dependencies detected:\n";
            for (const auto& w : graph.GetCycleWarnings()) {
                std::cout << "  * " << w << "\n";
            }
            std::cout << "\n";
        }

        if (graph_format == "mermaid") {
            std::cout << graph.ToMermaid();
        } else if (graph_format == "dot") {
            std::cout << graph.ToDot();
        } else if (graph_format == "json") {
            std::cout << graph.ToJson();
        } else {
            graph.PrintSummary();
        }
        return 0;
    }

    if (target_pkg.empty()) {
        std::cerr << "Error: No target package specified.\n";
        PrintUsage(argv[0]);
        return 1;
    }

    // Materialize staged virtual workspace if overlays/patches exist
    std::vector<std::string> effective_assembly_roots;
    if (!bridge.MaterializeStagedAssembly(repo_root, assembly_roots, staging_dir, effective_assembly_roots, verbose)) {
        std::cerr << "Error: 3-Pillar Bridge failed to materialize staged workspace.\n";
        return 1;
    }

    // Resolve umk binary and build method
    std::string umk_exe = umk_driver.ResolveUmkExecutable(custom_umk);
    if (umk_exe.empty()) {
        std::cerr << "Error: Could not locate umk executable. Specify with --umk <path> or run UppBootstrap first.\n";
        return 1;
    }

    auto methods = umk_driver.CollectMethods(repo_root);
    auto method = umk_driver.FindBestMethod(methods, method_pref);
    if (method.path.empty()) {
        std::cerr << "Error: No suitable build method found in ~/.config/u++/theide or system directories.\n";
        return 1;
    }

    std::string resolved_target = bridge.ResolveRedirect(target_pkg);

    std::string effective_flags;
    DependencyGraph graph(bridge, effective_assembly_roots);
    std::string pkg_upp = graph.FindPackagePath(resolved_target);
    if (pkg_upp.empty()) {
        std::cerr << "Error: Package '" << target_pkg << "' does not exist in assembly roots.\n";
        auto suggestions = graph.FindCloseMatches(resolved_target);
        if (!suggestions.empty()) {
            std::cerr << "Did you mean:\n";
            for (const auto& s : suggestions) {
                std::cerr << "  * " << s << "\n";
            }
        }
        return 1;
    }
    UppPackage target_info;
    bool has_pkg_info = UppParser::Parse(pkg_upp, target_info);

    if (!mainconf_pref.empty()) {
        if (has_pkg_info) {
            bool found = false;
            char* endp = nullptr;
            long idx = strtol(mainconf_pref.c_str(), &endp, 10);
            if (*endp == '\0' && idx >= 0 && idx < (long)target_info.mainconfigs.size()) {
                effective_flags = target_info.mainconfigs[idx].flags;
                found = true;
            } else {
                for (const auto& mc : target_info.mainconfigs) {
                    if (mc.name == mainconf_pref) {
                        effective_flags = mc.flags;
                        found = true;
                        break;
                    }
                }
            }
            if (!found) effective_flags = mainconf_pref;
        } else {
            effective_flags = mainconf_pref;
        }
    } else if (has_pkg_info && !target_info.mainconfigs.empty()) {
        effective_flags = target_info.mainconfigs[0].flags;
    }

    // Deterministic configuration key (matching umk output structure)
    std::string config_key = umk_driver.ComputeConfigKey(method, effective_flags, release, true, shared_mode, false);

    bool is_default_output = output_path.empty();
    std::string binary_name = GetFileTitle(target_pkg);
#ifdef _WIN32
    binary_name += ".exe";
#endif

    // Target technical binary location: bin/<CONFIG_KEY>/<target_name>
    if (is_default_output) {
        std::string technical_dir = JoinPath(JoinPath(repo_root, "bin"), config_key);
        output_path = JoinPath(technical_dir, binary_name);
    }

    if (verbose) {
        std::cout << "Target:     " << target_pkg << "\n";
        std::cout << "ConfigKey:  " << config_key << "\n";
        std::cout << "Output:     " << output_path << "\n";
        std::cout << "Method:     " << method.name << " (" << method.path << ")\n";
        std::cout << "umk:        " << umk_exe << "\n";
    }

    int res = umk_driver.ExecuteBuild(umk_exe,
                                     effective_assembly_roots,
                                     resolved_target,
                                     method.path,
                                     output_path,
                                     effective_flags,
                                     release,
                                     clean,
                                     jobs,
                                     shared_mode,
                                     verbose);

    if (res == 0) {
        std::string actual_binary = output_path;
        bool is_app_bundle = false;
        std::string app_bundle_path = output_path + ".app";
        std::string bundle_exec = JoinPath(JoinPath(JoinPath(app_bundle_path, "Contents"), "MacOS"), binary_name);
        if (FileExists(bundle_exec)) {
            actual_binary = bundle_exec;
            is_app_bundle = true;
        }

        std::cout << "==> UppBuilder: Build succeeded: " << actual_binary << "\n";

        if (is_default_output) {
            std::string bin_dir = JoinPath(repo_root, "bin");
            std::string top_entry = JoinPath(bin_dir, binary_name);
            std::string rel_to_tech = config_key + "/" + binary_name;

            if (is_app_bundle) {
                rel_to_tech = config_key + "/" + binary_name + ".app/Contents/MacOS/" + binary_name;
                std::string app_entry = JoinPath(bin_dir, binary_name + ".app");
                std::string rel_app = config_key + "/" + binary_name + ".app";
                CreateEntrypoint(app_bundle_path, app_entry, rel_app);
            }

            if (CreateEntrypoint(actual_binary, top_entry, rel_to_tech)) {
                std::cout << "==> UppBuilder: Entrypoint created: " << top_entry << " -> " << rel_to_tech << "\n";
            }

            // If target_pkg has a subfolder (e.g. ide/Android, DevEnv/App), also create bin/<target_pkg>
            std::string normalized_pkg = target_pkg;
            std::replace(normalized_pkg.begin(), normalized_pkg.end(), '\\', '/');
            size_t slash_pos = normalized_pkg.rfind('/');
            if (slash_pos != std::string::npos) {
                std::string sub_entry = JoinPath(bin_dir, target_pkg);
#ifdef _WIN32
                if (!EndsWith(sub_entry, ".exe")) sub_entry += ".exe";
#endif
                int depth = 0;
                for (size_t k = 0; k <= slash_pos; ++k) {
                    if (normalized_pkg[k] == '/') depth++;
                }
                std::string prefix;
                for (int d = 0; d < depth; ++d) prefix += "../";
                std::string sub_rel = prefix + config_key + "/" + binary_name;
                if (is_app_bundle) {
                    sub_rel = prefix + config_key + "/" + binary_name + ".app/Contents/MacOS/" + binary_name;
                    std::string sub_app_entry = sub_entry + ".app";
                    std::string sub_app_rel = prefix + config_key + "/" + binary_name + ".app";
                    CreateEntrypoint(app_bundle_path, sub_app_entry, sub_app_rel);
                }

                if (CreateEntrypoint(actual_binary, sub_entry, sub_rel)) {
                    std::cout << "==> UppBuilder: Entrypoint created: " << sub_entry << " -> " << sub_rel << "\n";
                }
            }
        }

        if (copy_deps) {
            std::set<std::string> copied;
            std::vector<std::string> search_dirs = { GetParentDir(output_path), GetParentDir(method.path) };
            RuntimeDepsHarvester::CopyDependenciesRecursive(output_path, GetParentDir(output_path), search_dirs, copied);
            if (!copied.empty()) {
                std::cout << "Copied " << copied.size() << " runtime dependencies into output directory.\n";
            }
        }
    } else {
        std::cerr << "==> UppBuilder: Build failed with exit code " << res << "\n";
    }

    return res;
}
