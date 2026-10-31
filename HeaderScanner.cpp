#include "HeaderScanner.h"

namespace UppBuilder {

HeaderScanner::HeaderScanner(const std::vector<std::string>& package_roots)
    : package_roots_(package_roots) {}

void HeaderScanner::IndexPackages() {
    profiles_.clear();
    header_to_package_.clear();

    for (const auto& root : package_roots_) {
        std::vector<std::string> upp_files;
        Walk(root, upp_files, ".upp");

        for (const auto& upp_path : upp_files) {
            UppPackage pkg;
            if (!UppParser::Parse(upp_path, pkg)) continue;

            // Determine canonical package name
            std::string abs_upp = GetAbsolutePath(upp_path);
            std::string abs_root = GetAbsolutePath(root);
            std::string rel_pkg = abs_upp;
            if (StartsWith(rel_pkg, abs_root)) {
                rel_pkg = rel_pkg.substr(abs_root.size());
                while (!rel_pkg.empty() && (rel_pkg[0] == '/' || rel_pkg[0] == '\\')) rel_pkg = rel_pkg.substr(1);
            }
            std::string pkg_name = GetFileTitle(rel_pkg);
            std::string parent_rel = GetParentDir(rel_pkg);
            if (!parent_rel.empty() && parent_rel != ".") {
                pkg_name = parent_rel;
                for (char& c : pkg_name) if (c == '\\') c = '/';
            }

            PackageHeaderProfile prof;
            prof.package_name = pkg_name;
            prof.upp_path = abs_upp;

            for (const auto& u : pkg.unconditional_uses) prof.declared_uses.insert(u);
            for (const auto& cu : pkg.conditional_uses) {
                for (const auto& u : cu.second) prof.declared_uses.insert(u);
            }

            std::string pkg_dir = GetParentDir(abs_upp);
            for (const auto& f : pkg.files) {
                std::string full_path = JoinPath(pkg_dir, f);
                std::string ext = ToLower(GetFileExt(f));
                if (ext == ".h" || ext == ".hpp" || ext == ".i") {
                    prof.owned_headers.push_back(full_path);
                    header_to_package_[GetFileName(f)] = pkg_name;
                    // Also store with subpath if file is in subdir
                    header_to_package_[f] = pkg_name;
                } else if (ext == ".cpp" || ext == ".c" || ext == ".cc" || ext == ".cxx" || ext == ".m" || ext == ".mm") {
                    prof.source_files.push_back(full_path);
                }
            }

            profiles_[pkg_name] = prof;
        }
    }
}

void HeaderScanner::ExtractIncludes(const std::string& file_path, std::vector<std::string>& out_includes) const {
    std::ifstream is(file_path);
    if (!is) return;
    std::string line;
    std::regex inc_re(R"(^\s*#\s*include\s*["<]([^">]+)[">])");
    std::smatch match;

    while (std::getline(is, line)) {
        if (std::regex_search(line, match, inc_re)) {
            if (match.size() > 1) {
                std::string header = match[1].str();
                for (char& c : header) if (c == '\\') c = '/';
                out_includes.push_back(header);
            }
        }
    }
}

void HeaderScanner::ScanPackage(const std::string& package_name) {
    auto it = profiles_.find(package_name);
    if (it == profiles_.end()) return;

    const auto& prof = it->second;
    std::map<std::string, std::pair<int, std::set<std::string>>> target_coupling; // target_pkg -> (count, headers)

    std::vector<std::string> all_files = prof.owned_headers;
    all_files.insert(all_files.end(), prof.source_files.begin(), prof.source_files.end());

    for (const auto& f : all_files) {
        std::vector<std::string> incs;
        ExtractIncludes(f, incs);
        for (const auto& inc : incs) {
            std::string inc_base = GetFileName(inc);
            std::string target_pkg;
            auto hit = header_to_package_.find(inc);
            if (hit != header_to_package_.end()) {
                target_pkg = hit->second;
            } else {
                hit = header_to_package_.find(inc_base);
                if (hit != header_to_package_.end()) {
                    target_pkg = hit->second;
                }
            }

            if (!target_pkg.empty() && target_pkg != package_name) {
                target_coupling[target_pkg].first++;
                target_coupling[target_pkg].second.insert(inc);
            }
        }
    }

    for (const auto& pair : target_coupling) {
        InterPackageCoupling coup;
        coup.from_package = package_name;
        coup.to_package = pair.first;
        coup.include_count = pair.second.first;
        coup.headers_included = pair.second.second;
        coup.is_declared_in_uses = prof.declared_uses.count(pair.first) > 0;
        couplings_.push_back(coup);
    }
}

void HeaderScanner::ScanAll() {
    couplings_.clear();
    for (const auto& pair : profiles_) {
        ScanPackage(pair.first);
    }
}

std::string HeaderScanner::GenerateMarkdownReport(const std::string& focus_package) const {
    std::stringstream ss;
    ss << "# Header Dependency & Coupling Analysis Report\n\n";
    ss << "- Total indexed packages: `" << profiles_.size() << "`\n";
    ss << "- Total indexed headers: `" << header_to_package_.size() << "`\n\n";

    if (!focus_package.empty()) {
        ss << "## Package: `" << focus_package << "`\n\n";
        auto it = profiles_.find(focus_package);
        if (it != profiles_.end()) {
            ss << "- **Path**: `" << it->second.upp_path << "`\n";
            ss << "- **Owned Headers**: " << it->second.owned_headers.size() << "\n";
            ss << "- **Source Files**: " << it->second.source_files.size() << "\n";
            ss << "- **Declared `.upp` Uses**: ";
            for (const auto& u : it->second.declared_uses) ss << "`" << u << "` ";
            ss << "\n\n";
        }
    }

    ss << "### Inter-Package Inclusions\n\n";
    ss << "| Source Package | Target Package | Includes Count | Declared in `.upp`? | Headers Included |\n";
    ss << "| :--- | :--- | :--- | :--- | :--- |\n";

    for (const auto& coup : couplings_) {
        if (!focus_package.empty() && coup.from_package != focus_package && coup.to_package != focus_package) {
            continue;
        }
        ss << "| `" << coup.from_package << "` ";
        ss << "| `" << coup.to_package << "` ";
        ss << "| " << coup.include_count << " ";
        ss << "| " << (coup.is_declared_in_uses ? "✅ Yes" : "⚠️ **NO (Undeclared!)**") << " ";
        ss << "| ";
        size_t count = 0;
        for (const auto& h : coup.headers_included) {
            if (count++ > 0) ss << ", ";
            if (count > 5) { ss << "... (" << coup.headers_included.size() << " total)"; break; }
            ss << "`" << h << "`";
        }
        ss << " |\n";
    }

    return ss.str();
}

void HeaderScanner::PrintConsoleSummary(const std::string& focus_package) const {
    std::cout << "\n=== Header Coupling Summary ===\n";
    int undeclared = 0;
    for (const auto& coup : couplings_) {
        if (!focus_package.empty() && coup.from_package != focus_package && coup.to_package != focus_package) {
            continue;
        }
        std::cout << "  " << coup.from_package << " -> " << coup.to_package
                  << " (" << coup.include_count << " includes across "
                  << coup.headers_included.size() << " headers)";
        if (!coup.is_declared_in_uses) {
            std::cout << " [UNDECLARED in .upp!]";
            undeclared++;
        }
        std::cout << "\n";
    }
    if (undeclared > 0) {
        std::cout << "\nWarning: Found " << undeclared << " undeclared package header dependencies!\n";
    }
}

} // namespace UppBuilder
