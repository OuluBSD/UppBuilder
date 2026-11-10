#include "FlagAuditor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace UppBuilder {

bool FlagAuditor::IsGlobalAuditFlag(const std::string& flag) {
    static const std::set<std::string> global_flags = {
        "GUI", "DEBUG", "RELEASE", "POSIX", "WIN32", "LINUX",
        "MSC", "GCC", "CLANG", "BLITZ", "MAIN", "X11", "COCOA",
        "FREEBSD", "MACOS", "ANDROID", "MINGW", "MSVC",
        "ST", "MT", "USEMALLOC", "SHARED", "STATIC", "NDEBUG",
        "DEBUG_RT", "RELEASE_RT", "SO", "EXE", "DLL"
    };
    return global_flags.count(flag) > 0;
}

static bool IsWordBoundaryBefore(const std::string& text, size_t pos) {
    if (pos == 0) return true;
    char prev = text[pos - 1];
    return !(std::isalnum(static_cast<unsigned char>(prev)) || prev == '_');
}

bool FlagAuditor::ScanFile(const std::string& file_path, std::vector<AuditHit>& hits) {
    std::ifstream is(file_path);
    if (!is) return false;

    std::stringstream buffer;
    buffer << is.rdbuf();
    std::string txt = buffer.str();

    int line = 1;
    bool line_comment = false;
    bool block_comment = false;
    char quote = 0;
    size_t len = txt.size();

    for (size_t i = 0; i < len; ) {
        char c = txt[i];
        char n = (i + 1 < len) ? txt[i + 1] : '\0';

        if (c == '\n') {
            line++;
            line_comment = false;
            i++;
            continue;
        }

        if (line_comment) {
            i++;
            continue;
        }

        if (block_comment) {
            if (c == '*' && n == '/') {
                block_comment = false;
                i += 2;
            } else {
                i++;
            }
            continue;
        }

        if (quote) {
            if (c == '\\' && n != '\0') {
                i += 2;
                continue;
            }
            if (c == quote) {
                quote = 0;
            }
            i++;
            continue;
        }

        if (c == '/' && n == '/') {
            line_comment = true;
            i += 2;
            continue;
        }

        if (c == '/' && n == '*') {
            block_comment = true;
            i += 2;
            continue;
        }

        if (c == '"' || c == '\'') {
            quote = c;
            i++;
            continue;
        }

        // Look for flagSOMEFLAG
        if (c == 'f' && (i + 4 < len) && txt[i + 1] == 'l' && txt[i + 2] == 'a' && txt[i + 3] == 'g' &&
            IsWordBoundaryBefore(txt, i)) {
            size_t j = i + 4;
            if (j < len && ((txt[j] >= 'A' && txt[j] <= 'Z') || txt[j] == '_')) {
                size_t k = j + 1;
                bool valid = true;
                while (k < len && (std::isalnum(static_cast<unsigned char>(txt[k])) || txt[k] == '_')) {
                    if (!((txt[k] >= 'A' && txt[k] <= 'Z') || (txt[k] >= '0' && txt[k] <= '9') || txt[k] == '_')) {
                        valid = false;
                    }
                    k++;
                }

                if (valid) {
                    std::string flag_name = txt.substr(j, k - j);
                    size_t line_start = txt.rfind('\n', i);
                    line_start = (line_start == std::string::npos) ? 0 : line_start + 1;
                    size_t line_end = txt.find('\n', i);
                    line_end = (line_end == std::string::npos) ? len : line_end;
                    std::string snippet = Trim(txt.substr(line_start, line_end - line_start));

                    hits.push_back({flag_name, file_path, line, snippet});
                    i = k;
                    continue;
                }
            }
        }

        i++;
    }

    return true;
}

PackageAuditResult FlagAuditor::AuditPackage(const UppPackage& pkg, const std::string& package_dir) {
    PackageAuditResult res;
    res.package_name = pkg.name;
    res.upp_path = pkg.path;

    for (const auto& a : pkg.acceptflags) {
        std::string upper = ToUpper(Trim(a));
        if (!upper.empty() && std::find(res.accepted_flags.begin(), res.accepted_flags.end(), upper) == res.accepted_flags.end()) {
            res.accepted_flags.push_back(upper);
        }
    }
    std::sort(res.accepted_flags.begin(), res.accepted_flags.end());

    std::set<std::string> used_set;
    for (const auto& f : pkg.files) {
        std::string ext = ToLower(GetFileExt(f));
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".icpp" || ext == ".c" || ext == ".cc") {
            std::string full_path = package_dir + "/" + f;
            std::vector<AuditHit> file_hits;
            if (ScanFile(full_path, file_hits)) {
                for (const auto& hit : file_hits) {
                    if (!IsGlobalAuditFlag(hit.flag)) {
                        used_set.insert(hit.flag);
                        res.hits.push_back(hit);
                    }
                }
            }
        }
    }

    for (const auto& u : used_set) {
        res.used_flags.push_back(u);
        if (std::find(res.accepted_flags.begin(), res.accepted_flags.end(), u) == res.accepted_flags.end()) {
            res.missing_flags.push_back(u);
        }
    }
    std::sort(res.used_flags.begin(), res.used_flags.end());
    std::sort(res.missing_flags.begin(), res.missing_flags.end());

    for (const auto& a : res.accepted_flags) {
        if (used_set.count(a) == 0) {
            res.unused_flags.push_back(a);
        }
    }
    std::sort(res.unused_flags.begin(), res.unused_flags.end());

    return res;
}

void FlagAuditor::PrintResult(const PackageAuditResult& res, bool emit_patch) {
    if (!res.missing_flags.empty()) {
        std::cout << "[warn] " << res.package_name << " has unaccepted code flags (missing in .upp acceptflags):\n";
        for (const auto& m : res.missing_flags) {
            std::cout << "       - flag" << m << "\n";
            for (const auto& hit : res.hits) {
                if (hit.flag == m) {
                    std::cout << "         source: " << hit.path << ":" << hit.line << "  " << hit.snippet << "\n";
                }
            }
        }
    } else if (res.used_flags.empty()) {
        std::cout << "[skip] " << res.package_name << " (no package-local flags found in source)\n";
    } else {
        std::cout << "[ok]   " << res.package_name << " (all " << res.used_flags.size() << " code flags declared in acceptflags)\n";
    }

    if (!res.unused_flags.empty()) {
        std::cout << "[info] " << res.package_name << " has acceptflags unused in code: ";
        for (size_t i = 0; i < res.unused_flags.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << res.unused_flags[i];
        }
        std::cout << "\n";
    }

    if (emit_patch && !res.missing_flags.empty()) {
        std::cout << "\nSuggested patch for " << res.upp_path << ":\n";
        std::cout << GeneratePatch(res) << "\n";
    }
}

std::string FlagAuditor::GeneratePatch(const PackageAuditResult& res) {
    std::ostringstream oss;
    oss << "--- a/" << res.package_name << ".upp\n";
    oss << "+++ b/" << res.package_name << ".upp\n";

    if (res.accepted_flags.empty()) {
        oss << "@@ ... @@\n";
        oss << "+acceptflags\n";
        for (size_t i = 0; i < res.missing_flags.size(); i++) {
            oss << "+\t" << res.missing_flags[i];
            if (i + 1 < res.missing_flags.size()) oss << ",";
            else oss << ";";
            oss << "\n";
        }
    } else {
        oss << "@@ acceptflags @@\n";
        for (const auto& m : res.missing_flags) {
            oss << "+\t" << m << ",\n";
        }
    }

    return oss.str();
}

} // namespace UppBuilder
