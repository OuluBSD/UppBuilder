#include "DepGraph.h"

namespace UppBuilder {

DependencyGraph::DependencyGraph(const BridgeEngine& bridge, const std::vector<std::string>& search_roots)
    : bridge_(bridge), search_roots_(search_roots) {}

std::string DependencyGraph::FindPackagePath(const std::string& pkg_name) const {
    std::string norm = pkg_name;
    for (char& c : norm) if (c == '\\') c = '/';

    std::string file_title = norm;
    size_t slash = file_title.rfind('/');
    if (slash != std::string::npos) {
        file_title = file_title.substr(slash + 1);
    }
    std::string target_upp = file_title + ".upp";

    // 0. Direct file or directory path check
    if (FileExists(norm)) return GetAbsolutePath(norm);
    if (DirectoryExists(norm)) {
        std::string direct = JoinPath(norm, target_upp);
        if (FileExists(direct)) return GetAbsolutePath(direct);
        std::string cand_upp = norm + ".upp";
        if (FileExists(cand_upp)) return GetAbsolutePath(cand_upp);
    }

    // 1. Direct path check relative to search roots
    for (const auto& root : search_roots_) {
        std::string candidate = JoinPath(root, norm);
        std::string direct = JoinPath(candidate, target_upp);
        if (FileExists(direct)) return GetAbsolutePath(direct);

        std::string cand_upp = candidate + ".upp";
        if (FileExists(cand_upp)) return GetAbsolutePath(cand_upp);
    }

    // 2. Recursive walk in search roots
    for (const auto& root : search_roots_) {
        std::vector<std::string> found;
        Walk(root, found, ".upp");
        for (const auto& f : found) {
            std::string fn = GetFileName(f);
            if (fn == target_upp) {
                // Ensure subpath matches if norm contains slashes
                std::string abs_f = GetAbsolutePath(f);
                std::string parent_dir = GetFileTitle(GetParentDir(abs_f));
                if (norm.find('/') != std::string::npos) {
                    if (abs_f.find(norm) != std::string::npos) {
                        return abs_f;
                    }
                } else if (parent_dir == norm || GetFileTitle(fn) == norm) {
                    return abs_f;
                }
            }
        }
    }
    return "";
}

static int LevenshteinDistance(const std::string& s1, const std::string& s2) {
    const size_t m = s1.size();
    const size_t n = s2.size();
    std::vector<std::vector<int>> d(m + 1, std::vector<int>(n + 1));
    for (size_t i = 0; i <= m; ++i) d[i][0] = (int)i;
    for (size_t j = 0; j <= n; ++j) d[0][j] = (int)j;
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            int cost = (std::tolower((unsigned char)s1[i - 1]) == std::tolower((unsigned char)s2[j - 1])) ? 0 : 1;
            d[i][j] = (std::min)({ d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost });
        }
    }
    return d[m][n];
}

std::vector<std::string> DependencyGraph::FindCloseMatches(const std::string& pkg_name, int max_matches) const {
    std::string norm = pkg_name;
    for (char& c : norm) if (c == '\\') c = '/';

    // Collect candidate packages across search roots
    std::set<std::string> all_packages;
    for (const auto& root : search_roots_) {
        std::vector<std::string> found;
        Walk(root, found, ".upp");
        for (const auto& f : found) {
            std::string abs_f = GetAbsolutePath(f);
            std::string abs_root = GetAbsolutePath(root);
            if (abs_f.rfind(abs_root, 0) == 0 && abs_f.size() > abs_root.size()) {
                std::string rel = abs_f.substr(abs_root.size());
                while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) rel.erase(0, 1);
                for (char& c : rel) if (c == '\\') c = '/';
                if (EndsWith(rel, ".upp")) rel = rel.substr(0, rel.size() - 4);
                size_t s = rel.rfind('/');
                std::string title = (s != std::string::npos) ? rel.substr(s + 1) : rel;
                std::string parent = (s != std::string::npos) ? rel.substr(0, s) : "";
                size_t s2 = parent.rfind('/');
                std::string parent_title = (s2 != std::string::npos) ? parent.substr(s2 + 1) : parent;
                if (!parent.empty() && title == parent_title) {
                    all_packages.insert(parent);
                } else {
                    all_packages.insert(rel);
                }
            }
        }
    }

    struct Candidate {
        std::string name;
        int distance;
    };
    std::vector<Candidate> candidates;
    for (const auto& p : all_packages) {
        int dist = LevenshteinDistance(norm, p);
        if (dist <= 3 || ToLower(p).find(ToLower(norm)) != std::string::npos ||
            ToLower(norm).find(ToLower(p)) != std::string::npos) {
            candidates.push_back({p, dist});
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.distance < b.distance;
    });

    std::vector<std::string> result;
    for (size_t i = 0; i < candidates.size() && (int)i < max_matches; ++i) {
        result.push_back(candidates[i].name);
    }
    return result;
}

bool DependencyGraph::BuildGraph(const std::string& root_pkg_name, const std::set<std::string>& active_flags, bool strict_cycles) {
    nodes_.clear();
    topo_order_.clear();
    has_cycle_ = false;
    cycle_warnings_.clear();

    std::set<std::string> visiting;
    std::set<std::string> visited;
    std::vector<std::string> current_path;

    std::string redirected_root = bridge_.ResolveRedirect(root_pkg_name);
    return VisitNode(redirected_root, active_flags, strict_cycles, visiting, visited, current_path);
}

bool DependencyGraph::VisitNode(const std::string& pkg_name,
                                const std::set<std::string>& active_flags,
                                bool strict_cycles,
                                std::set<std::string>& visiting,
                                std::set<std::string>& visited,
                                std::vector<std::string>& current_path) {
    std::string norm = bridge_.ResolveRedirect(pkg_name);

    if (visiting.count(norm)) {
        has_cycle_ = true;
        std::stringstream ss;
        ss << "Circular dependency detected: ";
        for (const auto& p : current_path) ss << p << " -> ";
        ss << norm;
        cycle_warnings_.push_back(ss.str());
        if (strict_cycles) {
            return false;
        }
        return true; // Skip cycle and continue traversal
    }

    if (visited.count(norm)) return true;

    visiting.insert(norm);
    current_path.push_back(norm);

    std::string upp_path = FindPackagePath(norm);
    DependencyNode node;
    node.name = norm;
    node.path = upp_path;

    if (!upp_path.empty()) {
        UppPackage pkg;
        if (UppParser::Parse(upp_path, pkg)) {
            std::vector<std::string> active_uses = UppParser::ResolveActiveUses(pkg, active_flags);
            for (const auto& dep : active_uses) {
                std::string redirected_dep = bridge_.ResolveRedirect(dep);
                node.active_dependencies.push_back(redirected_dep);
                if (!VisitNode(redirected_dep, active_flags, strict_cycles, visiting, visited, current_path)) {
                    return false;
                }
            }
        }
    }

    visiting.erase(norm);
    visited.insert(norm);
    current_path.pop_back();

    nodes_[norm] = node;
    topo_order_.push_back(norm);
    return true;
}

std::string DependencyGraph::ToMermaid() const {
    std::stringstream ss;
    ss << "```mermaid\nflowchart TD\n";
    for (const auto& pair : nodes_) {
        const auto& node = pair.second;
        std::string safe_name = Replace(node.name, "/", "_");
        safe_name = Replace(safe_name, "-", "_");
        ss << "    " << safe_name << "[\"" << node.name << "\"]\n";
        for (const auto& dep : node.active_dependencies) {
            std::string safe_dep = Replace(dep, "/", "_");
            safe_dep = Replace(safe_dep, "-", "_");
            ss << "    " << safe_name << " --> " << safe_dep << "\n";
        }
    }
    ss << "```\n";
    return ss.str();
}

std::string DependencyGraph::ToDot() const {
    std::stringstream ss;
    ss << "digraph Dependencies {\n";
    ss << "    rankdir=LR;\n";
    ss << "    node [shape=box, style=rounded];\n";
    for (const auto& pair : nodes_) {
        const auto& node = pair.second;
        for (const auto& dep : node.active_dependencies) {
            ss << "    \"" << node.name << "\" -> \"" << dep << "\";\n";
        }
    }
    ss << "}\n";
    return ss.str();
}

std::string DependencyGraph::ToJson() const {
    std::stringstream ss;
    ss << "{\n  \"packages\": [\n";
    bool first_node = true;
    for (const auto& p : topo_order_) {
        if (!first_node) ss << ",\n";
        first_node = false;
        const auto& node = nodes_.at(p);
        ss << "    {\n";
        ss << "      \"name\": \"" << node.name << "\",\n";
        ss << "      \"path\": \"" << node.path << "\",\n";
        ss << "      \"dependencies\": [";
        for (size_t i = 0; i < node.active_dependencies.size(); ++i) {
            ss << "\"" << node.active_dependencies[i] << "\"" << (i + 1 < node.active_dependencies.size() ? ", " : "");
        }
        ss << "]\n";
        ss << "    }";
    }
    ss << "\n  ],\n";
    ss << "  \"topological_order\": [";
    for (size_t i = 0; i < topo_order_.size(); ++i) {
        ss << "\"" << topo_order_[i] << "\"" << (i + 1 < topo_order_.size() ? ", " : "");
    }
    ss << "]\n}\n";
    return ss.str();
}

void DependencyGraph::PrintSummary() const {
    std::cout << "Resolved Packages (" << topo_order_.size() << " total, compile order):\n";
    for (size_t i = 0; i < topo_order_.size(); ++i) {
        const auto& name = topo_order_[i];
        const auto& node = nodes_.at(name);
        std::cout << "  [" << (i + 1) << "] " << name;
        if (node.path.empty()) {
            std::cout << " (NOT FOUND)";
        } else {
            std::cout << " -> " << node.path;
        }
        std::cout << "\n";
        if (!node.active_dependencies.empty()) {
            std::cout << "      uses: ";
            for (size_t d = 0; d < node.active_dependencies.size(); ++d) {
                std::cout << node.active_dependencies[d] << (d + 1 < node.active_dependencies.size() ? ", " : "");
            }
            std::cout << "\n";
        }
    }
}

} // namespace UppBuilder
