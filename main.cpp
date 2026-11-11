#include "UppBuilder.h"
#include "Bridge.h"
#include "UppParser.h"
#include "DepGraph.h"
#include "HeaderScanner.h"
#include "UmkDriver.h"
#include "RuntimeDeps.h"
#include "FlagAuditor.h"
#include "UseModel.h"
#include "PolicyEngine.h"
#include "EscBuildParser.h"
#include "ProfileManager.h"
#include "ProviderPlanner.h"
#include "OverlayManager.h"
#include "EmergeEngine.h"
#include "VdbManager.h"
#include "BinPkgManager.h"

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
    std::cout << "  --vfs <dir>                   Path to VFS repository (virtual filesystem & workbench tree template)\n";
    std::cout << "  --softscript <dir>            Path to SoftScript repository (ByteVM, ScriptCommon, etc.)\n";
    std::cout << "  --uppgeometry <dir>           Path to UppGeometry repository (Geometry, Draw/Extensions, etc.)\n";
    std::cout << "  --apply-patches               Materialize applied-patches with restructuring & apply patches\n";
    std::cout << "  --make-patches                Reverse-diff applied-patches against upstream into patches/\n";
    std::cout << "  materialize [all|asm]         Materialize upstream assemblies (uppsrc, reference, tutorial, examples, upptst, autotest)\n";
    std::cout << "  --materialize-all             Materialize all upstream assemblies into applied-*/\n";
    std::cout << "  upstream-diff [pkg]           Audit surgical patches against upstream U++ repository\n";
    std::cout << "  --applied-patches <dir>       Path to applied-patches directory (default: applied-patches)\n";
    std::cout << "  --upstream <dir>              Path to pristine upstream ultimatepp/uppsrc\n";
    std::cout << "  --patches-dir <dir>           Path to patches directory (default: patches)\n\n";
    std::cout << "Inspection & Graph Analysis:\n";
    std::cout << "  --list-methods                List available build methods\n";
    std::cout << "  --list-mainconfigs <pkg>      List mainconfigs defined in package\n";
    std::cout << "  --dump-deps <pkg>             Print resolved active dependency tree\n";
    std::cout << "  --graph <format>              Output dependency graph (mermaid, dot, json)\n";
    std::cout << "  --scan-headers [pkg]          Scan header inclusions and report inter-package coupling\n";
    std::cout << "  --report-headers <file.md>    Save header coupling analysis to markdown file\n";
    std::cout << "  --audit-flags [pkg]           Audit package acceptflags against source code usage\n";
    std::cout << "  --audit-patch                 Output suggested acceptflags patch with audit results\n\n";
    std::cout << "Portage Package & USE Management:\n";
    std::cout << "  --use \"<flags>\"               Specify USE flags (e.g. \"+sqlite -gtk st\")\n";
    std::cout << "  use <pkg>                     Inspect resolved USE model and flag projections for package\n";
    std::cout << "  explain-use [flag]            Explain semantic meaning and mappings of a USE flag\n";
    std::cout << "  --policy-dir <dir>            Explicit path to .uppbuilder policy directory\n\n";
    std::cout << "Target Profiles & Portage Overlay:\n";
    std::cout << "  emerge <atoms...>             Resolve dependencies and emerge packages\n";
    std::cout << "  install <atoms...>            Alias for emerge\n";
    std::cout << "  test [atoms...]               Run test suites for packages (default: @installed)\n";
    std::cout << "  --test                        Run test suites after emerging packages\n";
    std::cout << "  unmerge <atoms...>            Unmerge package and remove artifacts\n";
    std::cout << "  --force                       Force unmerge ignoring reverse dependencies\n";
    std::cout << "  -p, --pretend                 Display merge plan without executing changes\n";
    std::cout << "  -u, --update                  Update existing repository checkouts before build\n";
    std::cout << "  -b, --buildpkg                Build binary packages (.tar.gz) alongside source emerge\n";
    std::cout << "  -k, --usepkg                  Use binary package if available, else compile from source\n";
    std::cout << "  -K, --usepkgonly              Only use binary packages; fail if missing\n";
    std::cout << "  binpkg list                   List cached binary packages for active profile\n";
    std::cout << "  binpkg clean                  Clean cached binary packages for active profile\n";
    std::cout << "  vdb list                      List installed packages in VDB database\n";
    std::cout << "  vdb info <atom>               Show installed package details from VDB database\n";
    std::cout << "  vdb diff <p1> <p2>            Compare installed packages between two profiles\n";
    std::cout << "  vdb profiles                  List all profiles with installed packages in VDB\n";
    std::cout << "  --profile <name>              Select target profile (e.g. NativeClang, NativeGcc, MsvcX64, WasmBrowser)\n";
    std::cout << "  profile list                  List all available target profiles\n";
    std::cout << "  profile active                Show currently active target profile\n";
    std::cout << "  profile switch <name>         Switch default active target profile\n";
    std::cout << "  profile show [name]           Show configuration details for a target profile\n";
    std::cout << "  virtual list                  List all virtual capability packages (Virtual/*)\n";
    std::cout << "  virtual resolve <name>        Resolve virtual package to host provider (vcpkg, apt, brew, plugin)\n";
    std::cout << "  overlay list                  List all packages indexed in UppOverlay\n";
    std::cout << "  overlay info <atom>           Show package metadata from .escbuild file\n";
    std::cout << "  overlay sync                  Pull latest updates from UppOverlay repository\n";
    std::cout << "  --overlay-dir <dir>           Explicit path to UppOverlay repository\n\n";
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
    bool audit_flags = false;
    std::string audit_flags_pkg;
    bool audit_patch = false;

    std::string cli_use;
    bool show_use = false;
    std::string show_use_pkg;
    bool explain_use = false;
    std::string explain_use_flag;
    std::string custom_policy_dir;

    std::string selected_profile;
    bool profile_list = false;
    bool profile_active = false;
    bool profile_switch = false;
    std::string profile_switch_name;
    bool profile_show = false;
    std::string profile_show_name;

    bool virtual_list = false;
    bool virtual_resolve = false;
    std::string virtual_resolve_name;

    bool overlay_list = false;
    bool overlay_info = false;
    std::string overlay_info_atom;
    bool overlay_sync = false;
    std::string custom_overlay_dir;

    bool do_emerge = false;
    bool emerge_pretend = false;
    bool emerge_update = false;
    bool emerge_test = false;
    std::vector<std::string> emerge_atoms;

    bool do_test = false;
    std::vector<std::string> test_atoms;

    bool emerge_buildpkg = false;
    bool emerge_usepkg = false;
    bool emerge_usepkgonly = false;
    bool binpkg_list = false;
    bool binpkg_clean = false;

    bool do_unmerge = false;
    bool unmerge_force = false;
    std::vector<std::string> unmerge_atoms;

    bool vdb_list = false;
    bool vdb_info = false;
    std::string vdb_info_atom;
    bool vdb_profiles = false;
    bool vdb_diff = false;
    std::string vdb_diff_p1, vdb_diff_p2;

    std::string staging_dir = ".uppbuilder/staging";
    std::string applied_patches_dir;
    std::string patches_dir = "patches";
    std::string upstream_dir;
    std::string custom_bazaarcore;
    std::string custom_superintelligence;
    std::string custom_commandcenter;
    std::string custom_ai;
    std::string custom_vfs;
    std::string custom_softscript;
    std::string custom_uppgeometry;
    bool apply_patches_cmd = false;
    bool make_patches_cmd = false;
    bool materialize_cmd = false;
    std::string materialize_target = "all";
    bool upstream_diff_cmd = false;
    std::string upstream_diff_pkg;
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
        } else if ((arg == "--commandcenter" || arg == "--commandbridge") && i + 1 < argc) {
            custom_commandcenter = argv[++i];
        } else if ((arg == "--ai" || arg == "--artificialintelligence" || arg == "--artificalintelligence") && i + 1 < argc) {
            custom_ai = argv[++i];
        } else if (arg == "--vfs" && i + 1 < argc) {
            custom_vfs = argv[++i];
        } else if (arg == "--softscript" && i + 1 < argc) {
            custom_softscript = argv[++i];
        } else if (arg == "--uppgeometry" && i + 1 < argc) {
            custom_uppgeometry = argv[++i];
        } else if (arg == "--apply-patches") {
            apply_patches_cmd = true;
        } else if (arg == "--make-patches") {
            make_patches_cmd = true;
        } else if (arg == "materialize") {
            materialize_cmd = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                materialize_target = argv[++i];
            }
        } else if (arg == "--materialize-all" || arg == "--apply-all") {
            materialize_cmd = true;
            materialize_target = "all";
        } else if (arg == "upstream-diff" || arg == "--upstream-diff") {
            upstream_diff_cmd = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                upstream_diff_pkg = argv[++i];
            }
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
        } else if (arg == "--audit-flags" || arg == "audit-flags") {
            audit_flags = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                audit_flags_pkg = argv[++i];
            }
        } else if (arg == "--audit-patch") {
            audit_patch = true;
        } else if (arg == "--use" && i + 1 < argc) {
            cli_use = argv[++i];
        } else if (arg == "--show-use" || arg == "use") {
            show_use = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                show_use_pkg = argv[++i];
            }
        } else if (arg == "explain-use" || arg == "--explain-use") {
            explain_use = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                explain_use_flag = argv[++i];
            }
        } else if (arg == "--policy-dir" && i + 1 < argc) {
            custom_policy_dir = argv[++i];
        } else if (arg == "profile") {
            if (i + 1 < argc) {
                std::string sub = argv[++i];
                if (sub == "list") {
                    profile_list = true;
                } else if (sub == "active") {
                    profile_active = true;
                } else if (sub == "switch" && i + 1 < argc) {
                    profile_switch = true;
                    profile_switch_name = argv[++i];
                } else if (sub == "show") {
                    profile_show = true;
                    if (i + 1 < argc && argv[i + 1][0] != '-') {
                        profile_show_name = argv[++i];
                    }
                } else {
                    selected_profile = sub;
                }
            } else {
                profile_list = true;
            }
        } else if (arg == "--profile" && i + 1 < argc) {
            selected_profile = argv[++i];
        } else if (arg == "--list-profiles") {
            profile_list = true;
        } else if (arg == "--show-profile") {
            profile_show = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                profile_show_name = argv[++i];
            }
        } else if (arg == "virtual") {
            if (i + 1 < argc) {
                std::string sub = argv[++i];
                if (sub == "list") {
                    virtual_list = true;
                } else if (sub == "resolve") {
                    virtual_resolve = true;
                    if (i + 1 < argc && argv[i + 1][0] != '-') {
                        virtual_resolve_name = argv[++i];
                    }
                }
            } else {
                virtual_list = true;
            }
        } else if (arg == "--list-virtuals") {
            virtual_list = true;
        } else if (arg == "--resolve-virtual" && i + 1 < argc) {
            virtual_resolve = true;
            virtual_resolve_name = argv[++i];
        } else if (arg == "overlay") {
            if (i + 1 < argc) {
                std::string sub = argv[++i];
                if (sub == "list") {
                    overlay_list = true;
                } else if (sub == "info") {
                    overlay_info = true;
                    if (i + 1 < argc && argv[i + 1][0] != '-') {
                        overlay_info_atom = argv[++i];
                    }
                } else if (sub == "sync") {
                    overlay_sync = true;
                }
            } else {
                overlay_list = true;
            }
        } else if (arg == "--list-overlay" || arg == "--list-packages") {
            overlay_list = true;
        } else if (arg == "--overlay-info" && i + 1 < argc) {
            overlay_info = true;
            overlay_info_atom = argv[++i];
        } else if (arg == "--overlay-sync") {
            overlay_sync = true;
        } else if (arg == "--overlay-dir" && i + 1 < argc) {
            custom_overlay_dir = argv[++i];
        } else if (arg == "emerge" || arg == "install") {
            do_emerge = true;
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                emerge_atoms.push_back(argv[++i]);
            }
        } else if (arg == "test") {
            do_test = true;
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                test_atoms.push_back(argv[++i]);
            }
        } else if (arg == "-t" || arg == "--test") {
            emerge_test = true;
        } else if (arg == "unmerge") {
            do_unmerge = true;
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                unmerge_atoms.push_back(argv[++i]);
            }
        } else if (arg == "--force") {
            unmerge_force = true;
        } else if (arg == "vdb") {
            if (i + 1 < argc) {
                std::string sub = argv[++i];
                if (sub == "list") {
                    vdb_list = true;
                } else if (sub == "profiles") {
                    vdb_profiles = true;
                } else if (sub == "diff" && i + 2 < argc) {
                    vdb_diff = true;
                    vdb_diff_p1 = argv[++i];
                    vdb_diff_p2 = argv[++i];
                } else if (sub == "info" && i + 1 < argc) {
                    vdb_info = true;
                    vdb_info_atom = argv[++i];
                }
            }
        } else if (arg == "-p" || arg == "--pretend") {
            emerge_pretend = true;
        } else if (arg == "-u" || arg == "--update") {
            emerge_update = true;
        } else if (arg == "-b" || arg == "--buildpkg") {
            emerge_buildpkg = true;
        } else if (arg == "-k" || arg == "--usepkg") {
            emerge_usepkg = true;
        } else if (arg == "-K" || arg == "--usepkgonly") {
            emerge_usepkgonly = true;
        } else if (arg == "binpkg") {
            if (i + 1 < argc) {
                std::string sub = argv[++i];
                if (sub == "list") {
                    binpkg_list = true;
                } else if (sub == "clean") {
                    binpkg_clean = true;
                }
            } else {
                binpkg_list = true;
            }
        } else if (arg == "-a" || arg == "--add-root") {
            if (i + 1 < argc) extra_roots.push_back(argv[++i]);
        } else if (arg[0] != '-') {
            if (do_emerge) emerge_atoms.push_back(arg);
            else if (do_test) test_atoms.push_back(arg);
            else if (do_unmerge) unmerge_atoms.push_back(arg);
            else target_pkg = arg;
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
        } else {
            std::string inspect_cand = !target_pkg.empty() ? target_pkg : (!show_use_pkg.empty() ? show_use_pkg : audit_flags_pkg);
            if (!inspect_cand.empty() && (inspect_cand.find('/') != std::string::npos || inspect_cand.find('\\') != std::string::npos)) {
                std::string t_search = GetAbsolutePath(inspect_cand);
                while (!t_search.empty()) {
                    if (DirectoryExists(JoinPath(t_search, ".git")) || DirectoryExists(JoinPath(t_search, "uppsrc"))) {
                        repo_root = t_search;
                        break;
                    }
                    std::string parent = GetParentDir(t_search);
                    if (parent == t_search || parent.empty()) break;
                    t_search = parent;
                }
            }
        }
    }

    std::string policy_dir = !custom_policy_dir.empty() ? custom_policy_dir : PolicyEngine::FindPolicyDir(repo_root);
    LocalPolicy local_policy;
    if (!policy_dir.empty()) {
        local_policy = PolicyEngine::LoadLocalPolicy(policy_dir);
        if (jobs == 0 && local_policy.make_jobs > 0) {
            jobs = local_policy.make_jobs;
        }
        if (method_pref.empty() && !local_policy.make_compiler.empty()) {
            method_pref = local_policy.make_compiler;
        }
    }

    if (explain_use) {
        if (explain_use_flag.empty()) {
            std::cout << "Available built-in USE flags:\n";
            for (const auto& p : UseModelEngine::GetBuiltinPolicies()) {
                std::cout << "  " << p.name << " - " << p.description << " ["
                          << (p.default_on ? "default: on" : "default: off") << "]\n";
            }
            return 0;
        }
        std::cout << UseModelEngine::ExplainUse(explain_use_flag);
        return 0;
    }

    if (profile_active) {
        std::cout << "Active Target Profile: " << ProfileManager::GetActiveProfileName(custom_overlay_dir) << "\n";
        return 0;
    }

    if (profile_switch) {
        if (profile_switch_name.empty()) {
            std::cerr << "Error: No target profile specified to switch to.\n";
            return 1;
        }
        if (ProfileManager::SetActiveProfileName(profile_switch_name)) {
            std::cout << ">>> Switched active profile to: " << profile_switch_name << "\n";
            return 0;
        } else {
            std::cerr << "Error: Failed to set active profile.\n";
            return 1;
        }
    }

    if (profile_list) {
        std::string active_prof = ProfileManager::GetActiveProfileName(custom_overlay_dir);
        auto profiles = ProfileManager::ListProfiles(custom_overlay_dir);
        std::cout << "Available Target Profiles (" << profiles.size() << "):\n";
        for (const auto& name : profiles) {
            TargetProfile p = ProfileManager::LoadProfile(name, custom_overlay_dir);
            if (name == active_prof) {
                std::cout << "  * " << p.name << " (active)";
            } else {
                std::cout << "    " << p.name;
            }
            if (!p.build_method.empty()) std::cout << " (method: " << p.build_method << ")";
            if (!p.default_use.empty()) std::cout << " [USE: " << p.default_use << "]";
            std::cout << "\n";
        }
        return 0;
    }

    if (profile_show) {
        std::string name = profile_show_name.empty() ? ProfileManager::GetDefaultProfileName() : profile_show_name;
        TargetProfile p = ProfileManager::LoadProfile(name, custom_overlay_dir);
        std::cout << "Profile:      " << p.name << "\n";
        if (!p.path.empty()) {
            std::cout << "Config Path:  " << p.path << "\n";
        }
        std::cout << "Build Method: " << (p.build_method.empty() ? "(none)" : p.build_method) << "\n";
        std::cout << "Default USE:  " << (p.default_use.empty() ? "(none)" : p.default_use) << "\n";
        std::cout << "Output Cache: " << (p.output_cache.empty() ? "(none)" : p.output_cache) << "\n";
        if (!p.vars.empty()) {
            std::cout << "make.conf variables:\n";
            for (const auto& [k, v] : p.vars) {
                std::cout << "  " << k << " = \"" << v << "\"\n";
            }
        }
        return 0;
    }

    if (virtual_list) {
        auto vlist = ProviderPlanner::ListVirtuals(custom_overlay_dir);
        std::cout << "Available Virtual Packages (" << vlist.size() << "):\n";
        for (const auto& v : vlist) {
            std::cout << "  * " << v << "\n";
        }
        return 0;
    }

    if (virtual_resolve) {
        if (virtual_resolve_name.empty()) {
            std::cerr << "Error: No virtual package specified to resolve.\n";
            return 1;
        }
        auto rp = ProviderPlanner::Resolve(virtual_resolve_name, custom_overlay_dir);
        std::cout << "Virtual:        " << rp.virtual_name << "\n";
        std::cout << "Provider Type:  " << rp.provider_type << "\n";
        std::cout << "Package Ref:    " << rp.package_ref << "\n";
        std::cout << "Install Cmd:    " << rp.command << "\n";
        std::cout << "Host Available: " << (rp.is_available ? "yes" : "no") << "\n";
        return 0;
    }

    if (overlay_list) {
        auto pkgs = OverlayManager::ListAllPackages(custom_overlay_dir);
        std::string root = OverlayManager::FindOverlayRoot(custom_overlay_dir);
        std::cout << "UppOverlay Packages (" << pkgs.size() << ") at " << (root.empty() ? "[built-in]" : root) << ":\n";
        for (const auto& pkg : pkgs) {
            std::cout << "  * " << pkg.category << "/" << pkg.package_name;
            if (!pkg.version.empty()) std::cout << "-" << pkg.version;
            if (!pkg.description.empty()) std::cout << " - " << pkg.description;
            std::cout << "\n";
        }
        return 0;
    }

    if (overlay_info) {
        if (overlay_info_atom.empty()) {
            std::cerr << "Error: No package atom specified for overlay info.\n";
            return 1;
        }
        EscBuildInfo info;
        if (!OverlayManager::FindPackage(overlay_info_atom, info, custom_overlay_dir)) {
            std::cerr << "Error: Package '" << overlay_info_atom << "' not found in UppOverlay.\n";
            return 1;
        }
        std::cout << "Package:      " << info.category << "/" << info.package_name << "\n";
        std::cout << "Version:      " << info.version << "\n";
        std::cout << "File:         " << info.file_path << "\n";
        if (!info.description.empty()) std::cout << "Description:  " << info.description << "\n";
        if (!info.homepage.empty())    std::cout << "Homepage:     " << info.homepage << "\n";
        if (!info.git_repo.empty())    std::cout << "Git Repo:     " << info.git_repo << " (branch: " << info.git_branch << ")\n";
        if (!info.upp_assembly.empty()) std::cout << "Assembly:     " << info.upp_assembly << "\n";
        if (!info.default_package.empty()) std::cout << "Default Pkg:  " << info.default_package << "\n";
        if (!info.iuse.empty())        std::cout << "IUSE:         " << JoinStrings(info.iuse, " ") << "\n";
        if (!info.acceptflags.empty()) std::cout << "AcceptFlags:  " << JoinStrings(info.acceptflags, " ") << "\n";
        if (!info.rdepend.empty())     std::cout << "RDEPEND:      " << JoinStrings(info.rdepend, " ") << "\n";
        if (!info.providers.empty()) {
            std::cout << "Providers:\n";
            for (const auto& [plat, pkg] : info.providers) {
                std::cout << "  " << plat << " -> " << pkg << "\n";
            }
        }
        return 0;
    }

    if (overlay_sync) {
        std::string root = OverlayManager::FindOverlayRoot(custom_overlay_dir);
        if (root.empty()) {
            std::cerr << "Error: UppOverlay directory not found.\n";
            return 1;
        }
        std::cout << "==> UppBuilder: Synchronizing overlay at " << root << "...\n";
        if (OverlayManager::SyncOverlay(root)) {
            std::cout << "Overlay synchronization completed successfully.\n";
            return 0;
        } else {
            std::cerr << "Error: Failed to sync overlay.\n";
            return 1;
        }
    }

    if (do_emerge) {
        if (emerge_atoms.empty()) {
            std::cerr << "Error: No package atom(s) specified for emerge/install.\n";
            return 1;
        }
        std::string profile = selected_profile.empty() ? ProfileManager::GetActiveProfileName() : selected_profile;
        EmergePlan plan;
        if (!EmergeEngine::Plan(emerge_atoms, profile, plan, custom_overlay_dir)) {
            std::cerr << "Error: " << plan.error << "\n";
            return 1;
        }
        std::cout << EmergeEngine::FormatPlan(plan);
        if (emerge_pretend) {
            return 0;
        }
        if (!EmergeEngine::Execute(plan, emerge_update, verbose, emerge_test, emerge_buildpkg, emerge_usepkg, emerge_usepkgonly)) {
            return 1;
        }
        return 0;
    }

    if (binpkg_list) {
        std::string prof = selected_profile.empty() ? ProfileManager::GetActiveProfileName() : selected_profile;
        auto list = BinPkgManager::ListBinPkgs(prof);
        std::cout << "Cached Binary Packages [" << prof << "] (" << list.size() << "):\n\n";
        if (list.empty()) {
            std::cout << "  (no binary packages cached)\n";
        } else {
            std::cout << std::left << std::setw(32) << "ATOM"
                      << std::setw(12) << "VERSION"
                      << std::setw(12) << "SIZE"
                      << "PATH\n";
            std::cout << std::string(75, '-') << "\n";
            for (const auto& item : list) {
                std::string size_str = std::to_string(item.file_size / 1024) + " KB";
                std::cout << std::left << std::setw(32) << item.atom
                          << std::setw(12) << item.version
                          << std::setw(12) << size_str
                          << item.path << "\n";
            }
        }
        return 0;
    }

    if (binpkg_clean) {
        std::string prof = selected_profile.empty() ? ProfileManager::GetActiveProfileName() : selected_profile;
        BinPkgManager::CleanBinPkgs(prof, verbose);
        std::cout << ">>> Cleaned binary package cache for profile [" << prof << "].\n";
        return 0;
    }

    if (vdb_diff) {
        return VdbManager::DiffProfiles(vdb_diff_p1, vdb_diff_p2) ? 0 : 1;
    }

    if (vdb_profiles) {
        auto profs = VdbManager::ListVdbProfiles();
        std::string active = ProfileManager::GetActiveProfileName();
        std::cout << "Target Profiles with VDB Installations (" << profs.size() << "):\n";
        for (const auto& p : profs) {
            std::cout << "  " << (p == active ? "* " : "  ") << p << "\n";
        }
        return 0;
    }

    if (vdb_list) {
        std::string prof = selected_profile.empty() ? ProfileManager::GetActiveProfileName() : selected_profile;
        auto installed = VdbManager::ListInstalled("", prof);
        std::cout << "Installed Packages in VDB [" << prof << "] (" << installed.size() << "):\n";
        for (const auto& pkg : installed) {
            std::cout << "  * " << pkg.atom;
            if (!pkg.version.empty()) std::cout << "-" << pkg.version;
            if (!pkg.profile.empty()) std::cout << " [" << pkg.profile << "]";
            if (!pkg.use.empty()) std::cout << " (USE: " << pkg.use << ")";
            std::cout << "\n";
        }
        return 0;
    }

    if (vdb_info) {
        VdbEntry entry;
        std::string prof = selected_profile.empty() ? ProfileManager::GetActiveProfileName() : selected_profile;
        if (!VdbManager::IsInstalled(vdb_info_atom, &entry, "", prof)) {
            std::cerr << "Package '" << vdb_info_atom << "' is not installed in VDB [" << prof << "].\n";
            return 1;
        }
        std::cout << "Package:      " << entry.atom << "\n";
        std::cout << "Version:      " << entry.version << "\n";
        std::cout << "Profile:      " << entry.profile << "\n";
        std::cout << "USE:          " << entry.use << "\n";
        std::cout << "Repository:   " << entry.repo_path << "\n";
        std::cout << "Target:       " << entry.target << "\n";
        std::cout << "Installed:    " << entry.install_time << "\n";
        if (!entry.rdepend.empty()) {
            std::cout << "Dependencies:\n";
            for (const auto& d : entry.rdepend) std::cout << "  - " << d << "\n";
        }
        if (!entry.files.empty()) {
            std::cout << "Files:\n";
            for (const auto& f : entry.files) std::cout << "  - " << f << "\n";
        }
        return 0;
    }

    std::string bazaarcore_dir = ResolveBazaarCorePath(custom_bazaarcore, repo_root);
    std::string base_for_patches = !bazaarcore_dir.empty() ? bazaarcore_dir : repo_root;

    if (do_unmerge) {
        return EmergeEngine::Unmerge(unmerge_atoms, unmerge_force, verbose) ? 0 : 1;
    }

    if (do_test) {
        if (test_atoms.empty()) {
            test_atoms.push_back("@installed");
        }
        TestSummary summary;
        bool ok = EmergeEngine::RunTests(test_atoms, summary, verbose, custom_overlay_dir, base_for_patches);
        return ok ? 0 : 1;
    }

    TargetProfile target_profile;
    if (!selected_profile.empty()) {
        target_profile = ProfileManager::LoadProfile(selected_profile, custom_overlay_dir);
        if (method_pref.empty() && !target_profile.build_method.empty()) {
            method_pref = target_profile.build_method;
        }
        if (!target_profile.default_use.empty()) {
            if (cli_use.empty()) cli_use = target_profile.default_use;
            else cli_use = target_profile.default_use + " " + cli_use;
        }
        if (verbose) {
            std::cout << "==> UppBuilder: Target Profile: " << target_profile.name
                      << " (method: " << target_profile.build_method
                      << ", cache: " << target_profile.output_cache << ")\n";
        }
    }

    std::string upstream_dir_resolved = ResolveUpstreamPath(upstream_dir, base_for_patches);
    std::string full_applied = ResolvePatchedUppsrcPath(applied_patches_dir, base_for_patches);
    std::string full_patches = (StartsWith(patches_dir, "/") || (patches_dir.size() >= 2 && patches_dir[1] == ':')) ?
                               patches_dir : JoinPath(base_for_patches, patches_dir);

    // Handle upstream-diff command
    if (upstream_diff_cmd) {
        if (upstream_dir_resolved.empty() || !DirectoryExists(upstream_dir_resolved)) {
            std::string home = GetEnvValue("HOME");
            if (!home.empty()) {
                std::string ai_upp = JoinPath(home, "Dev/ai-upp/uppsrc");
                if (DirectoryExists(ai_upp)) upstream_dir_resolved = ai_upp;
            }
        }
        if (upstream_dir_resolved.empty() || !DirectoryExists(upstream_dir_resolved)) {
            std::cerr << "Error: Upstream ultimatepp/uppsrc directory not found. Specify with --upstream <path>\n";
            return 1;
        }
        if (!bridge.AuditUpstreamPatches(upstream_dir_resolved, full_applied, full_patches, upstream_diff_pkg, verbose)) {
            return 1;
        }
        return 0;
    }

    // Handle materialize command
    if (materialize_cmd) {
        if (upstream_dir_resolved.empty() || !DirectoryExists(upstream_dir_resolved)) {
            std::string home = GetEnvValue("HOME");
            if (!home.empty()) {
                std::string ai_upp = JoinPath(home, "Dev/ai-upp/uppsrc");
                if (DirectoryExists(ai_upp)) upstream_dir_resolved = ai_upp;
            }
        }
        if (upstream_dir_resolved.empty() || !DirectoryExists(upstream_dir_resolved)) {
            std::cerr << "Error: Upstream ultimatepp/uppsrc directory not found. Specify with --upstream <path>\n";
            return 1;
        }
        std::string upstream_root = GetParentDir(upstream_dir_resolved);
        if (materialize_target == "all") {
            if (!bridge.MaterializeAllAssemblies(upstream_root, base_for_patches, full_patches, true, verbose)) {
                return 1;
            }
        } else {
            std::string src_asm = (materialize_target == "uppsrc") ? upstream_dir_resolved : JoinPath(upstream_root, materialize_target);
            std::string dst_asm = (materialize_target == "uppsrc") ? full_applied : JoinPath(base_for_patches, "applied-" + materialize_target);
            if (!bridge.MaterializeAssembly(src_asm, dst_asm, full_patches, true, verbose)) {
                return 1;
            }
        }
        return 0;
    }

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

    // Check if target package references an upstream assembly (examples/, reference/, tutorial/, upptst/, autotest/)
    std::string extra_upstream_asm_dir;
    size_t first_slash = target_pkg.find('/');
    if (first_slash != std::string::npos) {
        std::string prefix = target_pkg.substr(0, first_slash);
        if (prefix == "examples" || prefix == "reference" || prefix == "tutorial" || prefix == "upptst" || prefix == "autotest") {
            std::string applied_asm_name = "applied-" + prefix;
            extra_upstream_asm_dir = JoinPath(base_for_patches, applied_asm_name);
            if (!DirectoryExists(extra_upstream_asm_dir) && !upstream_dir_resolved.empty()) {
                std::string upstream_root = GetParentDir(upstream_dir_resolved);
                std::string src_asm = JoinPath(upstream_root, prefix);
                if (DirectoryExists(src_asm)) {
                    std::cout << "Notice: " << applied_asm_name << " assembly not found at " << extra_upstream_asm_dir << ". Materializing...\n";
                    bridge.MaterializeAssembly(src_asm, extra_upstream_asm_dir, full_patches, false, verbose);
                }
            }
            target_pkg = target_pkg.substr(first_slash + 1);
        }
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
    if (!extra_upstream_asm_dir.empty() && DirectoryExists(extra_upstream_asm_dir)) {
        assembly_roots.push_back(extra_upstream_asm_dir);
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
    std::string ai_dir = ResolveSiblingRepoPath("ArtificialIntelligence", custom_ai, repo_root);
    if (ai_dir.empty()) {
        ai_dir = ResolveSiblingRepoPath("ArtificalIntelligence", custom_ai, repo_root);
    }
    if (!ai_dir.empty() && ai_dir != repo_root) {
        std::string a_uppsrc = JoinPath(ai_dir, "uppsrc");
        if (DirectoryExists(a_uppsrc)) {
            assembly_roots.push_back(a_uppsrc);
        } else {
            assembly_roots.push_back(ai_dir);
        }
    }
    std::string commandcenter_dir = ResolveSiblingRepoPath("CommandCenter", custom_commandcenter, repo_root);
    if (commandcenter_dir.empty()) {
        commandcenter_dir = ResolveSiblingRepoPath("CommandBridge", custom_commandcenter, repo_root);
    }
    if (!commandcenter_dir.empty() && commandcenter_dir != repo_root) {
        std::string c_uppsrc = JoinPath(commandcenter_dir, "uppsrc");
        if (DirectoryExists(c_uppsrc)) {
            assembly_roots.push_back(c_uppsrc);
        }
        assembly_roots.push_back(commandcenter_dir);
    }
    std::string vfs_dir = ResolveSiblingRepoPath("VFS", custom_vfs, repo_root);
    if (vfs_dir.empty()) {
        vfs_dir = ResolveSiblingRepoPath("Vfs", custom_vfs, repo_root);
    }
    if (!vfs_dir.empty() && vfs_dir != repo_root) {
        std::string v_uppsrc = JoinPath(vfs_dir, "uppsrc");
        if (DirectoryExists(v_uppsrc)) {
            assembly_roots.push_back(v_uppsrc);
        } else {
            assembly_roots.push_back(vfs_dir);
        }
    }
    std::string softscript_dir = ResolveSiblingRepoPath("SoftScript", custom_softscript, repo_root);
    if (!softscript_dir.empty() && softscript_dir != repo_root) {
        std::string s_uppsrc = JoinPath(softscript_dir, "uppsrc");
        if (DirectoryExists(s_uppsrc)) {
            assembly_roots.push_back(s_uppsrc);
        } else {
            assembly_roots.push_back(softscript_dir);
        }
    }
    std::string uppgeom_dir = ResolveSiblingRepoPath("UppGeometry", custom_uppgeometry, repo_root);
    if (!uppgeom_dir.empty() && uppgeom_dir != repo_root) {
        std::string g_uppsrc = JoinPath(uppgeom_dir, "uppsrc");
        if (DirectoryExists(g_uppsrc)) {
            assembly_roots.push_back(g_uppsrc);
        } else {
            assembly_roots.push_back(uppgeom_dir);
        }
    }
    assembly_roots.push_back(JoinPath(repo_root, "uppsrc"));
    std::string ex_root = JoinPath(repo_root, "examples");
    if (DirectoryExists(ex_root)) assembly_roots.push_back(ex_root);
    std::string ref_root = JoinPath(repo_root, "reference");
    if (DirectoryExists(ref_root)) assembly_roots.push_back(ref_root);
    std::string tut_root = JoinPath(repo_root, "tutorial");
    if (DirectoryExists(tut_root)) assembly_roots.push_back(tut_root);
    assembly_roots.push_back(JoinPath(repo_root, "upptst"));
    std::string tests_root = JoinPath(repo_root, "tests");
    if (DirectoryExists(tests_root)) assembly_roots.push_back(tests_root);
    assembly_roots.push_back(JoinPath(repo_root, "src"));
    if (DirectoryExists(JoinPath(repo_root, ".git")) || DirectoryExists(JoinPath(repo_root, "uppsrc"))) {
        assembly_roots.push_back(repo_root);
    }
    std::string parent_dir = GetParentDir(repo_root);
    if (!parent_dir.empty() && DirectoryExists(parent_dir)) {
        if (FileExists(JoinPath(repo_root, target_pkg + ".upp")) ||
            FileExists(JoinPath(repo_root, GetFileTitle(repo_root) + ".upp")) ||
            FileExists(JoinPath(repo_root, GetFileName(repo_root) + ".upp"))) {
            assembly_roots.push_back(parent_dir);
        }
    }
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

    if (audit_flags) {
        std::cout << "==> UppBuilder: Auditing acceptflags against source code...\n";
        DependencyGraph graph(bridge, assembly_roots);

        std::string focus_pkg = !audit_flags_pkg.empty() ? audit_flags_pkg : target_pkg;
        if (!focus_pkg.empty() && focus_pkg != "all" && focus_pkg != "--all") {
            std::string upp_path = graph.FindPackagePath(focus_pkg);
            if (upp_path.empty()) {
                std::cerr << "Error: Package '" << focus_pkg << "' not found in assembly roots.\n";
                return 1;
            }
            UppPackage pkg;
            if (!UppParser::Parse(upp_path, pkg)) {
                std::cerr << "Error: Failed to parse package: " << upp_path << "\n";
                return 1;
            }
            std::string pkg_dir = GetParentDir(upp_path);
            auto res = FlagAuditor::AuditPackage(pkg, pkg_dir);
            FlagAuditor::PrintResult(res, audit_patch);
        } else {
            HeaderScanner scanner(assembly_roots);
            scanner.IndexPackages();
            int total = 0, warnings = 0, clean = 0, skipped = 0;
            for (const auto& [name, profile] : scanner.GetProfiles()) {
                UppPackage pkg;
                if (!UppParser::Parse(profile.upp_path, pkg)) continue;
                std::string pkg_dir = GetParentDir(profile.upp_path);
                auto res = FlagAuditor::AuditPackage(pkg, pkg_dir);
                FlagAuditor::PrintResult(res, audit_patch);
                total++;
                if (!res.missing_flags.empty()) warnings++;
                else if (res.used_flags.empty()) skipped++;
                else clean++;
            }
            std::cout << "\n==> Audit Summary: " << total << " packages audited ("
                      << clean << " ok, " << warnings << " warnings, " << skipped << " skipped)\n";
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

    if (show_use) {
        std::string inspect_pkg = !show_use_pkg.empty() ? show_use_pkg : target_pkg;
        if (inspect_pkg.empty()) {
            std::cerr << "Error: No package specified for USE inspection.\n";
            return 1;
        }
        std::string resolved_inspect = bridge.ResolveRedirect(inspect_pkg);
        DependencyGraph graph(bridge, assembly_roots);
        std::string pkg_upp = graph.FindPackagePath(resolved_inspect);
        if (pkg_upp.empty()) {
            if (FileExists(inspect_pkg)) pkg_upp = GetAbsolutePath(inspect_pkg);
            else if (FileExists(JoinPath(inspect_pkg, GetFileTitle(inspect_pkg) + ".upp"))) {
                pkg_upp = GetAbsolutePath(JoinPath(inspect_pkg, GetFileTitle(inspect_pkg) + ".upp"));
            }
        }

        UppPackage pkg_info;
        bool has_pkg = !pkg_upp.empty() && UppParser::Parse(pkg_upp, pkg_info);

        if (local_policy.policy_dir.empty() && !pkg_upp.empty()) {
            std::string sub_policy = PolicyEngine::FindPolicyDir(GetParentDir(pkg_upp));
            if (!sub_policy.empty()) {
                local_policy = PolicyEngine::LoadLocalPolicy(sub_policy);
            }
        }

        auto combined_use = PolicyEngine::CombineUseArgs(local_policy, inspect_pkg, cli_use);
        auto forced = PolicyEngine::GetForcedFlags(local_policy, inspect_pkg);
        auto masked = PolicyEngine::GetMaskedFlags(local_policy, inspect_pkg);

        UseModel model = UseModelEngine::BuildUseModel(has_pkg ? &pkg_info : nullptr, combined_use, forced, masked);
        UppProjection proj = UseModelEngine::ProjectUpp(model, has_pkg ? &pkg_info : nullptr);

        std::cout << "Package: " << inspect_pkg << (has_pkg ? " (" + pkg_upp + ")" : " [not found on disk]") << "\n";
        if (has_pkg && !pkg_info.acceptflags.empty()) {
            std::cout << "Declared acceptflags: " << JoinStrings(pkg_info.acceptflags, ", ") << "\n";
        }
        std::cout << UseModelEngine::FormatUseModel(model);
        std::cout << UseModelEngine::FormatProjection(proj);
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

    if (local_policy.policy_dir.empty() && !pkg_upp.empty()) {
        std::string sub_policy = PolicyEngine::FindPolicyDir(GetParentDir(pkg_upp));
        if (!sub_policy.empty()) {
            local_policy = PolicyEngine::LoadLocalPolicy(sub_policy);
        }
    }

    // Portage USE Model & Policy Flag Projection
    auto combined_use = PolicyEngine::CombineUseArgs(local_policy, target_pkg, cli_use);
    auto forced_use = PolicyEngine::GetForcedFlags(local_policy, target_pkg);
    auto masked_use = PolicyEngine::GetMaskedFlags(local_policy, target_pkg);

    if (!combined_use.empty() || !forced_use.empty() || !masked_use.empty()) {
        UseModel use_model = UseModelEngine::BuildUseModel(has_pkg_info ? &target_info : nullptr,
                                                           combined_use, forced_use, masked_use);
        UppProjection proj = UseModelEngine::ProjectUpp(use_model, has_pkg_info ? &target_info : nullptr);

        if (verbose) {
            std::cout << "==> UppBuilder: Applying Portage USE flags:\n";
            std::cout << UseModelEngine::FormatUseModel(use_model);
            std::cout << UseModelEngine::FormatProjection(proj);
        }

        // Apply disabled flags removal
        std::set<std::string> current_flags = UppParser::ParseFlags(effective_flags);
        for (const auto& dis : use_model.disabled) {
            const UsePolicy* p = UseModelEngine::FindPolicy(dis);
            if (p) {
                for (const auto& m : p->maps_to) {
                    current_flags.erase(ToUpper(m.upp_flag));
                }
            } else {
                current_flags.erase(ToUpper(dis));
            }
        }

        // Apply projected global and main-only flags
        for (const auto& g : proj.global) {
            current_flags.insert(g);
        }
        for (const auto& mo : proj.main_only) {
            current_flags.insert(mo);
        }

        std::vector<std::string> flag_list(current_flags.begin(), current_flags.end());
        effective_flags = JoinStrings(flag_list, " ");
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
