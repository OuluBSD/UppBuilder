#pragma once

#include "UppBuilder.h"
#include "UppParser.h"
#include "Bridge.h"

namespace UppBuilder {

struct DependencyNode {
    std::string name;
    std::string path;
    std::vector<std::string> active_dependencies;
};

class DependencyGraph {
public:
    DependencyGraph(const BridgeEngine& bridge, const std::vector<std::string>& search_roots);

    bool BuildGraph(const std::string& root_pkg_name, const std::set<std::string>& active_flags, bool strict_cycles = false);

    const std::vector<std::string>& GetTopologicalOrder() const { return topo_order_; }
    const std::map<std::string, DependencyNode>& GetNodes() const { return nodes_; }
    bool HasCycle() const { return has_cycle_; }
    const std::vector<std::string>& GetCycleWarnings() const { return cycle_warnings_; }

    std::string ToMermaid() const;
    std::string ToDot() const;
    std::string ToJson() const;
    void PrintSummary() const;

    std::string FindPackagePath(const std::string& pkg_name) const;

private:
    const BridgeEngine& bridge_;
    std::vector<std::string> search_roots_;
    std::map<std::string, DependencyNode> nodes_;
    std::vector<std::string> topo_order_;
    bool has_cycle_ = false;
    std::vector<std::string> cycle_warnings_;

    bool VisitNode(const std::string& pkg_name,
                   const std::set<std::string>& active_flags,
                   bool strict_cycles,
                   std::set<std::string>& visiting,
                   std::set<std::string>& visited,
                   std::vector<std::string>& current_path);
};

} // namespace UppBuilder
