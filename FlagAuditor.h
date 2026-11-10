#pragma once

#include "UppBuilder.h"
#include "UppParser.h"

namespace UppBuilder {

struct AuditHit {
    std::string flag;
    std::string path;
    int line = 0;
    std::string snippet;
};

struct PackageAuditResult {
    std::string package_name;
    std::string upp_path;
    std::vector<std::string> used_flags;     // Discovered code flags (excluding globals)
    std::vector<std::string> accepted_flags; // Flags declared in .upp acceptflags
    std::vector<std::string> missing_flags;  // Used in code, but NOT in acceptflags
    std::vector<std::string> unused_flags;   // In acceptflags, but NOT found in code
    std::vector<AuditHit> hits;
};

class FlagAuditor {
public:
    // Identifies standard global toolchain/OS/compiler flags that do not require package-level acceptflags
    static bool IsGlobalAuditFlag(const std::string& flag);

    // Scans a single C++ source or header file for flagSOMEFLAG macros
    static bool ScanFile(const std::string& file_path, std::vector<AuditHit>& hits);

    // Audits an entire package against its declared acceptflags
    static PackageAuditResult AuditPackage(const UppPackage& pkg, const std::string& package_dir);

    // Prints human-readable audit diagnostics (and optional patch) to stdout
    static void PrintResult(const PackageAuditResult& res, bool emit_patch);

    // Generates a unified diff snippet adding missing acceptflags to the .upp file
    static std::string GeneratePatch(const PackageAuditResult& res);
};

} // namespace UppBuilder
