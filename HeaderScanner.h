#pragma once

#include "UppBuilder.h"
#include "UppParser.h"

namespace UppBuilder {

struct PackageHeaderProfile {
    std::string package_name;
    std::string upp_path;
    std::vector<std::string> owned_headers;
    std::vector<std::string> source_files;
    std::set<std::string> declared_uses; // from .upp
};

struct InterPackageCoupling {
    std::string from_package;
    std::string to_package;
    int include_count = 0;
    std::set<std::string> headers_included;
    bool is_declared_in_uses = false;
};

class HeaderScanner {
public:
    HeaderScanner(const std::vector<std::string>& package_roots);

    // Index all packages and their owned headers
    void IndexPackages();

    // Scan a single package or all indexed packages
    void ScanPackage(const std::string& package_name);
    void ScanAll();

    // Analysis Reports
    std::string GenerateMarkdownReport(const std::string& focus_package = "") const;
    void PrintConsoleSummary(const std::string& focus_package = "") const;

    const std::map<std::string, PackageHeaderProfile>& GetProfiles() const { return profiles_; }
    const std::vector<InterPackageCoupling>& GetCouplings() const { return couplings_; }

private:
    std::vector<std::string> package_roots_;
    std::map<std::string, PackageHeaderProfile> profiles_;
    std::map<std::string, std::string> header_to_package_; // header filename -> owning package
    std::vector<InterPackageCoupling> couplings_;

    void ExtractIncludes(const std::string& file_path, std::vector<std::string>& out_includes) const;
};

} // namespace UppBuilder
