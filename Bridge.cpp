#include "Bridge.h"

namespace UppBuilder {

// Minimal JSON tokenizer/parser for config files
static std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    size_t start = json.find('"', pos);
    if (start == std::string::npos) return "";
    size_t end = json.find('"', start + 1);
    if (end == std::string::npos) return "";
    return json.substr(start + 1, end - start - 1);
}

BridgeEngine::BridgeEngine() {
    // Default built-in redirects for modular U++ Bazaar/Ecosystem
    AddRedirect("uppsrc/CtrlCore", "uppsrc/Ctrl/Core");
    AddRedirect("uppsrc/CtrlLib", "uppsrc/Ctrl/Lib");
    AddRedirect("CtrlCore", "Ctrl/Core");
    AddRedirect("CtrlLib", "Ctrl/Lib");

    // Package Restructuring Rules:
    // Upstream CtrlCore -> applied-patches/Ctrl/Core (manifest Core.upp)
    // Upstream CtrlLib  -> applied-patches/Ctrl/Lib  (manifest Lib.upp)
    AddRestructureRule("CtrlCore", "Ctrl/Core", "Core.upp", "CtrlCore.patch");
    AddRestructureRule("CtrlLib", "Ctrl/Lib", "Lib.upp", "CtrlLib.patch");
}

void BridgeEngine::AddRestructureRule(const std::string& upstream_pkg,
                                     const std::string& staged_rel_path,
                                     const std::string& staged_upp_name,
                                     const std::string& patch_file) {
    RestructureRule r;
    r.upstream_pkg = upstream_pkg;
    r.staged_rel_path = NormalizePath(staged_rel_path);
    r.staged_upp_name = staged_upp_name.empty() ? (GetFileName(staged_rel_path) + ".upp") : staged_upp_name;
    r.patch_file = patch_file.empty() ? (upstream_pkg + ".patch") : patch_file;
    restructure_rules_.push_back(r);

    AddRedirect(upstream_pkg, staged_rel_path);
    AddRedirect("uppsrc/" + upstream_pkg, "uppsrc/" + staged_rel_path);
}

void BridgeEngine::AddRedirect(const std::string& from, const std::string& to) {
    std::string f = NormalizePath(from);
    std::string t = NormalizePath(to);
    redirects_[f] = t;
    reverse_redirects_[t] = f;
}

std::string BridgeEngine::ResolveRedirect(const std::string& pkg_path) const {
    std::string norm = NormalizePath(pkg_path);
    auto it = redirects_.find(norm);
    if (it != redirects_.end()) return it->second;
    return norm;
}

std::string BridgeEngine::ReverseRedirect(const std::string& pkg_path) const {
    std::string norm = NormalizePath(pkg_path);
    auto it = reverse_redirects_.find(norm);
    if (it != reverse_redirects_.end()) return it->second;
    return norm;
}

bool BridgeEngine::LoadRedirectsFile(const std::string& json_path) {
    std::string content = ReadTextFile(json_path);
    if (content.empty()) return false;

    // Parse simple key-value pairs in "redirects": { ... }
    size_t red_pos = content.find("\"redirects\"");
    if (red_pos == std::string::npos) return false;
    size_t brace_start = content.find('{', red_pos);
    size_t brace_end = content.find('}', brace_start);
    if (brace_start == std::string::npos || brace_end == std::string::npos) return false;

    std::string block = content.substr(brace_start + 1, brace_end - brace_start - 1);
    std::stringstream ss(block);
    std::string line;
    while (std::getline(ss, line)) {
        line = Trim(line);
        if (line.empty()) continue;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string k_str = line.substr(0, colon);
            std::string v_str = line.substr(colon + 1);
            size_t k1 = k_str.find('"');
            size_t k2 = k_str.rfind('"');
            size_t v1 = v_str.find('"');
            size_t v2 = v_str.rfind('"');
            if (k1 != std::string::npos && k2 != std::string::npos && k2 > k1 &&
                v1 != std::string::npos && v2 != std::string::npos && v2 > v1) {
                std::string key = k_str.substr(k1 + 1, k2 - k1 - 1);
                std::string val = v_str.substr(v1 + 1, v2 - v1 - 1);
                AddRedirect(key, val);
            }
        }
    }
    return true;
}

void BridgeEngine::AddOverlay(const std::string& target, const std::string& source, const std::vector<std::string>& platforms) {
    overlays_.push_back({NormalizePath(target), NormalizePath(source), platforms});
}

bool BridgeEngine::LoadOverlaysFile(const std::string& json_path) {
    std::string content = ReadTextFile(json_path);
    if (content.empty()) return false;

    // Scan for array objects with "target" and "overlay" / "source"
    size_t pos = 0;
    while ((pos = content.find('{', pos)) != std::string::npos) {
        size_t end = content.find('}', pos);
        if (end == std::string::npos) break;
        std::string obj = content.substr(pos, end - pos + 1);
        std::string target = ExtractJsonString(obj, "target");
        std::string src = ExtractJsonString(obj, "overlay");
        if (src.empty()) src = ExtractJsonString(obj, "source");
        if (!target.empty() && !src.empty()) {
            AddOverlay(target, src);
        }
        pos = end + 1;
    }
    return true;
}

void BridgeEngine::AddPatch(const std::string& patch_file, const std::string& target_dir, int strip, const std::string& desc) {
    patches_.push_back({patch_file, target_dir, strip, desc});
}

bool BridgeEngine::LoadPatchesFile(const std::string& json_path) {
    std::string content = ReadTextFile(json_path);
    if (content.empty()) return false;

    size_t pos = 0;
    while ((pos = content.find('{', pos)) != std::string::npos) {
        size_t end = content.find('}', pos);
        if (end == std::string::npos) break;
        std::string obj = content.substr(pos, end - pos + 1);
        std::string pfile = ExtractJsonString(obj, "patch");
        if (pfile.empty()) pfile = ExtractJsonString(obj, "file");
        std::string tdir = ExtractJsonString(obj, "target");
        std::string desc = ExtractJsonString(obj, "description");
        if (!pfile.empty()) {
            AddPatch(pfile, tdir, 1, desc);
        }
        pos = end + 1;
    }
    return true;
}

bool BridgeEngine::PlatformMatches(const std::vector<std::string>& platforms) const {
    if (platforms.empty()) return true;
    for (const auto& p : platforms) {
        std::string pl = ToLower(p);
        if (pl == "all") return true;
#ifdef _WIN32
        if (pl == "win32" || pl == "windows") return true;
#else
        if (pl == "posix") return true;
  #if defined(__linux__)
        if (pl == "linux") return true;
  #elif defined(__APPLE__)
        if (pl == "macos" || pl == "darwin" || pl == "apple") return true;
  #elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        if (pl == "bsd") return true;
  #endif
#endif
    }
    return false;
}

bool BridgeEngine::ApplySinglePatch(const std::string& patch_path, const std::string& target_dir, int strip, bool verbose) {
    if (!FileExists(patch_path)) {
        std::cerr << "Patch file does not exist: " << patch_path << "\n";
        return false;
    }

    // Try git apply first if inside git or git available
    std::stringstream cmd;
#ifndef _WIN32
    cmd << "patch --batch -N -p" << strip << " -d \"" << target_dir << "\" < \"" << patch_path << "\" > /dev/null 2>&1";
    if (RunCommand(cmd.str()) == 0) {
        if (verbose) std::cout << "Successfully applied patch " << patch_path << " via patch tool\n";
        return true;
    }
#endif
    cmd.str("");
    cmd << "git apply --whitespace=nowarn --directory=\"" << target_dir << "\" -p" << strip << " \"" << patch_path << "\" > /dev/null 2>&1";
    if (RunCommand(cmd.str()) == 0) {
        if (verbose) std::cout << "Successfully applied patch " << patch_path << " via git apply\n";
        return true;
    }

    std::cerr << "Failed to apply patch: " << patch_path << "\n";
    return false;
}

bool BridgeEngine::MaterializeStagedAssembly(const std::string& repo_root,
                                            const std::vector<std::string>& source_roots,
                                            const std::string& staging_dir,
                                            std::vector<std::string>& out_effective_roots,
                                            bool verbose) {
    out_effective_roots = source_roots;
    if (overlays_.empty() && patches_.empty()) {
        return true; // No staging needed
    }

    if (verbose) {
        std::cout << "==> UppBuilder: Materializing 3-Pillar Bridge staged workspace in " << staging_dir << "\n";
    }

    RealizeDirectory(staging_dir);

    // Apply overlays
    for (const auto& ov : overlays_) {
        if (!PlatformMatches(ov.platforms)) continue;

        std::string src_path = ov.source;
        if (!FileExists(src_path)) {
            src_path = JoinPath(repo_root, ov.source);
        }
        if (!FileExists(src_path)) {
            std::cerr << "Warning: Overlay source not found: " << ov.source << "\n";
            continue;
        }

        std::string staged_target = JoinPath(staging_dir, ov.target);
        RealizeDirectory(GetParentDir(staged_target));
        if (CopyFile(src_path, staged_target)) {
            if (verbose) std::cout << "Overlay applied: " << ov.source << " -> " << staged_target << "\n";
        } else {
            std::cerr << "Error: Failed to copy overlay " << src_path << " to " << staged_target << "\n";
            return false;
        }
    }

    // Apply patches
    for (const auto& p : patches_) {
        std::string tdir = JoinPath(staging_dir, p.target_dir);
        RealizeDirectory(tdir);
        if (!ApplySinglePatch(p.patch_file, tdir, p.strip_level, verbose)) {
            return false;
        }
    }

    // Insert staging_dir at the beginning of effective roots so staged files take precedence
    out_effective_roots.insert(out_effective_roots.begin(), staging_dir);
    return true;
}

bool BridgeEngine::MaterializeAppliedPatches(const std::string& upstream_uppsrc,
                                             const std::string& applied_dir,
                                             const std::string& patches_dir,
                                             bool force,
                                             bool verbose) {
    if (!DirectoryExists(upstream_uppsrc)) {
        std::cerr << "Error: Upstream uppsrc directory not found: " << upstream_uppsrc << "\n";
        return false;
    }

    if (verbose) {
        std::cout << "==> UppBuilder: Staging applied-patches workspace in " << applied_dir << "\n";
    }
    RealizeDirectory(applied_dir);

    // 1. Process Restructure Rules (CtrlCore -> Ctrl/Core, CtrlLib -> Ctrl/Lib)
    for (const auto& rule : restructure_rules_) {
        std::string src_pkg_dir = JoinPath(upstream_uppsrc, rule.upstream_pkg);
        std::string dst_pkg_dir = JoinPath(applied_dir, rule.staged_rel_path);

        if (!DirectoryExists(src_pkg_dir)) {
            if (verbose) std::cerr << "Notice: Upstream package not found: " << src_pkg_dir << "\n";
            continue;
        }

        if (force || !DirectoryExists(dst_pkg_dir)) {
            if (DirectoryExists(dst_pkg_dir)) {
                RemoveDirectoryRecursive(dst_pkg_dir);
            }
            RealizeDirectory(dst_pkg_dir);
            if (verbose) {
                std::cout << "Restructuring: " << rule.upstream_pkg << " -> " << rule.staged_rel_path << "\n";
            }
            if (!CopyDirectoryRecursive(src_pkg_dir, dst_pkg_dir)) {
                std::cerr << "Error: Failed to copy " << src_pkg_dir << " to " << dst_pkg_dir << "\n";
                return false;
            }

            // Ensure proper .upp manifest for subpackage structure
            std::string orig_upp = JoinPath(dst_pkg_dir, rule.upstream_pkg + ".upp");
            std::string new_upp = JoinPath(dst_pkg_dir, rule.staged_upp_name);
            if (FileExists(orig_upp) && !rule.staged_upp_name.empty() && orig_upp != new_upp) {
                CopyFile(orig_upp, new_upp);
            }
        }

        // Apply corresponding patch if present
        std::string patch_path = JoinPath(patches_dir, rule.patch_file);
        if (FileExists(patch_path)) {
            if (verbose) std::cout << "Applying patch to " << rule.staged_rel_path << ": " << patch_path << "\n";
            ApplySinglePatch(patch_path, dst_pkg_dir, 1, verbose);
        }
    }

    // 2. Process any other top-level package patches in patches_dir (e.g. Core.patch, Draw.patch)
    if (DirectoryExists(patches_dir)) {
        for (const auto& item : ListDir(patches_dir)) {
            if (!EndsWith(item, ".patch") && !EndsWith(item, ".diff")) continue;

            // Check if already handled by restructure rules
            bool handled = false;
            for (const auto& r : restructure_rules_) {
                if (r.patch_file == item) { handled = true; break; }
            }
            if (handled) continue;

            std::string pkg_name = GetFileTitle(item);
            std::string src_pkg_dir = JoinPath(upstream_uppsrc, pkg_name);
            std::string dst_pkg_dir = JoinPath(applied_dir, pkg_name);

            if (DirectoryExists(src_pkg_dir)) {
                if (force || !DirectoryExists(dst_pkg_dir)) {
                    if (DirectoryExists(dst_pkg_dir)) RemoveDirectoryRecursive(dst_pkg_dir);
                    RealizeDirectory(dst_pkg_dir);
                    if (verbose) std::cout << "Staging base package for patch: " << pkg_name << "\n";
                    CopyDirectoryRecursive(src_pkg_dir, dst_pkg_dir);
                }
                std::string patch_path = JoinPath(patches_dir, item);
                if (verbose) std::cout << "Applying patch to " << pkg_name << ": " << patch_path << "\n";
                ApplySinglePatch(patch_path, dst_pkg_dir, 1, verbose);
            }
        }
    }

    return true;
}

static std::string NormalizePatchHeaders(const std::string& diff_text, const std::string& upstream_dir, const std::string& staged_dir) {
    std::istringstream iss(diff_text);
    std::ostringstream oss;
    std::string line;
    std::string u_prefix = "--- " + upstream_dir;
    std::string s_prefix = "+++ " + staged_dir;
    while (std::getline(iss, line)) {
        if (StartsWith(line, u_prefix)) {
            oss << "--- a" << line.substr(u_prefix.size()) << "\n";
        } else if (StartsWith(line, s_prefix)) {
            oss << "+++ b" << line.substr(s_prefix.size()) << "\n";
        } else {
            oss << line << "\n";
        }
    }
    return oss.str();
}

bool BridgeEngine::GeneratePatches(const std::string& upstream_uppsrc,
                                   const std::string& applied_dir,
                                   const std::string& patches_dir,
                                   bool verbose) {
    if (!DirectoryExists(upstream_uppsrc)) {
        std::cerr << "Error: Upstream uppsrc directory not found: " << upstream_uppsrc << "\n";
        return false;
    }
    if (!DirectoryExists(applied_dir)) {
        std::cerr << "Error: applied-patches directory not found: " << applied_dir << "\n";
        return false;
    }

    RealizeDirectory(patches_dir);
    std::cout << "==> UppBuilder: Regenerating surgical patches against pristine upstream...\n";

    // 1. Diffs for Restructured Packages (Ctrl/Core vs CtrlCore, Ctrl/Lib vs CtrlLib)
    for (const auto& rule : restructure_rules_) {
        std::string staged_pkg_dir = JoinPath(applied_dir, rule.staged_rel_path);
        std::string upstream_pkg_dir = JoinPath(upstream_uppsrc, rule.upstream_pkg);

        if (!DirectoryExists(staged_pkg_dir) || !DirectoryExists(upstream_pkg_dir)) continue;

        // Build diff command ignoring build artifacts and generated alias manifests
        std::stringstream cmd;
        cmd << "diff -uNr -x \"*.o\" -x \"*.a\" -x \"*.so\" -x \"*.obj\" -x \"*.d\" -x \"*.upp.bak\" ";
        if (!rule.staged_upp_name.empty() && rule.staged_upp_name != (rule.upstream_pkg + ".upp")) {
            cmd << "-x \"" << rule.staged_upp_name << "\" ";
        }
        cmd << "\"" << upstream_pkg_dir << "\" \"" << staged_pkg_dir << "\"";

        std::string diff_out = CaptureCommand(cmd.str());
        std::string patch_path = JoinPath(patches_dir, rule.patch_file);

        if (!diff_out.empty()) {
            std::string norm_diff = NormalizePatchHeaders(diff_out, upstream_pkg_dir, staged_pkg_dir);
            WriteTextFile(patch_path, norm_diff);
            std::cout << "  * Generated " << rule.patch_file << " (" << norm_diff.size() << " bytes) [" 
                      << rule.staged_rel_path << " <-> " << rule.upstream_pkg << "]\n";
        } else {
            if (FileExists(patch_path)) {
                std::cout << "  * No differences for " << rule.staged_rel_path << " (pristine match).\n";
            }
        }
    }

    // 2. Diffs for other top-level packages staged in applied_dir (e.g. Core, Draw)
    for (const auto& item : ListDir(applied_dir)) {
        std::string staged_dir = JoinPath(applied_dir, item);
        if (!DirectoryExists(staged_dir)) continue;

        // Skip restructured folders like "Ctrl"
        bool is_restructured = false;
        for (const auto& r : restructure_rules_) {
            if (StartsWith(r.staged_rel_path, item + "/") || r.staged_rel_path == item) {
                is_restructured = true;
                break;
            }
        }
        if (is_restructured) continue;

        std::string upstream_dir = JoinPath(upstream_uppsrc, item);
        if (!DirectoryExists(upstream_dir)) continue;

        std::stringstream cmd;
        cmd << "diff -uNr -x \"*.o\" -x \"*.a\" -x \"*.so\" -x \"*.obj\" -x \"*.d\" -x \"*.upp.bak\" "
            << "\"" << upstream_dir << "\" \"" << staged_dir << "\"";

        std::string diff_out = CaptureCommand(cmd.str());
        std::string patch_name = item + ".patch";
        std::string patch_path = JoinPath(patches_dir, patch_name);

        if (!diff_out.empty()) {
            std::string norm_diff = NormalizePatchHeaders(diff_out, upstream_dir, staged_dir);
            WriteTextFile(patch_path, norm_diff);
            std::cout << "  * Generated " << patch_name << " (" << norm_diff.size() << " bytes) [" << item << "]\n";
        } else {
            if (FileExists(patch_path)) {
                std::cout << "  * No differences for " << item << " (pristine match).\n";
            }
        }
    }

    std::cout << "==> UppBuilder: Patch regeneration complete. Patches are ready to commit.\n";
    return true;
}

} // namespace UppBuilder
