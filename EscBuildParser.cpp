#include "EscBuildParser.h"
#include <fstream>
#include <sstream>
#include <cctype>

namespace UppBuilder {

void EscBuildParser::DeduceMetadataFromPath(const std::string& path, EscBuildInfo& out_info) {
    out_info.file_path = GetAbsolutePath(path);
    std::string clean = Replace(path, "\\", "/");
    size_t last_slash = clean.rfind('/');
    std::string filename = (last_slash != std::string::npos) ? clean.substr(last_slash + 1) : clean;

    // Remove .escbuild or .ebuild
    std::string base = filename;
    if (EndsWith(base, ".escbuild")) {
        base = base.substr(0, base.size() - 9);
    } else if (EndsWith(base, ".ebuild")) {
        base = base.substr(0, base.size() - 7);
    }

    // Split Name-Version: e.g. "BazaarCore-0.1.0"
    size_t dash = base.rfind('-');
    if (dash != std::string::npos && dash + 1 < base.size() && std::isdigit(static_cast<unsigned char>(base[dash + 1]))) {
        out_info.package_name = base.substr(0, dash);
        out_info.version = base.substr(dash + 1);
    } else {
        out_info.package_name = base;
        out_info.version = "1.0.0";
    }

    // Determine category: parent of parent, or parent
    if (last_slash != std::string::npos) {
        std::string dir = clean.substr(0, last_slash);
        size_t prev_slash = dir.rfind('/');
        if (prev_slash != std::string::npos) {
            std::string parent_dir = dir.substr(prev_slash + 1);
            std::string grandparent_dir = dir.substr(0, prev_slash);
            size_t g_slash = grandparent_dir.rfind('/');
            std::string cat_cand = (g_slash != std::string::npos) ? grandparent_dir.substr(g_slash + 1) : grandparent_dir;
            if (parent_dir == out_info.package_name) {
                out_info.category = cat_cand;
            } else {
                out_info.category = parent_dir;
            }
        }
    }
}

static std::string StripComments(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    bool in_string = false;
    char quote = '\0';
    bool in_line_comment = false;
    bool in_block_comment = false;

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        char next = (i + 1 < input.size()) ? input[i + 1] : '\0';

        if (in_line_comment) {
            if (c == '\n') {
                in_line_comment = false;
                out += c;
            }
            continue;
        }
        if (in_block_comment) {
            if (c == '*' && next == '/') {
                in_block_comment = false;
                ++i;
            }
            continue;
        }

        if (in_string) {
            out += c;
            if (c == '\\' && next != '\0') {
                out += next;
                ++i;
            } else if (c == quote) {
                in_string = false;
            }
            continue;
        }

        if (c == '"' || c == '\'') {
            in_string = true;
            quote = c;
            out += c;
            continue;
        }

        if (c == '/' && next == '/') {
            in_line_comment = true;
            ++i;
            continue;
        }
        if (c == '/' && next == '*') {
            in_block_comment = true;
            ++i;
            continue;
        }

        out += c;
    }
    return out;
}

static std::string ExtractStringLiteral(const std::string& val) {
    std::string s = Trim(val);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

static std::vector<std::string> ParseArrayLiteral(const std::string& val) {
    std::vector<std::string> res;
    std::string s = Trim(val);
    if (s.empty()) return res;
    if (s.front() == '[') s = s.substr(1);
    if (!s.empty() && s.back() == ']') s.pop_back();

    std::stringstream ss;
    bool in_str = false;
    char quote = '\0';
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (in_str) {
            if (c == '\\' && i + 1 < s.size()) {
                ss << s[++i];
            } else if (c == quote) {
                in_str = false;
                std::string item = Trim(ss.str());
                if (!item.empty()) res.push_back(item);
                ss.str("");
                ss.clear();
            } else {
                ss << c;
            }
        } else {
            if (c == '"' || c == '\'') {
                in_str = true;
                quote = c;
            } else if (c == ',' || std::isspace(static_cast<unsigned char>(c))) {
                std::string item = Trim(ss.str());
                if (!item.empty()) {
                    res.push_back(item);
                    ss.str("");
                    ss.clear();
                }
            } else {
                ss << c;
            }
        }
    }
    std::string item = Trim(ss.str());
    if (!item.empty()) res.push_back(item);
    return res;
}

static std::map<std::string, std::string> ParseMapLiteral(const std::string& val) {
    std::map<std::string, std::string> res;
    std::string s = Trim(val);
    if (s.empty()) return res;
    if (s.front() == '{') s = s.substr(1);
    if (!s.empty() && s.back() == '}') s.pop_back();

    // Parse "key": "value" pairs
    std::string current_key;
    std::string current_val;
    bool parsing_key = true;
    bool in_str = false;
    char quote = '\0';
    std::stringstream ss;

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (in_str) {
            if (c == '\\' && i + 1 < s.size()) {
                ss << s[++i];
            } else if (c == quote) {
                in_str = false;
            } else {
                ss << c;
            }
        } else {
            if (c == '"' || c == '\'') {
                in_str = true;
                quote = c;
            } else if (c == ':' && parsing_key) {
                current_key = Trim(ss.str());
                ss.str("");
                ss.clear();
                parsing_key = false;
            } else if (c == ',' && !parsing_key) {
                current_val = Trim(ss.str());
                if (!current_key.empty()) {
                    res[current_key] = current_val;
                }
                ss.str("");
                ss.clear();
                current_key.clear();
                current_val.clear();
                parsing_key = true;
            } else if (!std::isspace(static_cast<unsigned char>(c))) {
                ss << c;
            }
        }
    }
    if (!parsing_key) {
        current_val = Trim(ss.str());
        if (!current_key.empty()) {
            res[current_key] = current_val;
        }
    }
    return res;
}

bool EscBuildParser::ParseFile(const std::string& path, EscBuildInfo& out_info) {
    std::ifstream is(path);
    if (!is) {
        out_info.warnings.push_back("Could not open file: " + path);
        return false;
    }
    std::stringstream ss;
    ss << is.rdbuf();
    DeduceMetadataFromPath(path, out_info);
    return ParseContent(ss.str(), out_info);
}

bool EscBuildParser::ParseContent(const std::string& content, EscBuildInfo& out_info) {
    std::string clean = StripComments(content);

    size_t i = 0;
    const size_t len = clean.size();

    while (i < len) {
        // Skip whitespace
        while (i < len && std::isspace(static_cast<unsigned char>(clean[i]))) ++i;
        if (i >= len) break;

        // Read identifier or statement
        size_t start = i;
        while (i < len && (std::isalnum(static_cast<unsigned char>(clean[i])) || clean[i] == '_')) {
            ++i;
        }
        std::string id = clean.substr(start, i - start);

        while (i < len && std::isspace(static_cast<unsigned char>(clean[i]))) ++i;
        if (i >= len) break;

        // Check if it's a function: id ( ... ) { ... }
        if (clean[i] == '(') {
            // Skip params
            while (i < len && clean[i] != ')') ++i;
            if (i < len && clean[i] == ')') ++i;
            while (i < len && std::isspace(static_cast<unsigned char>(clean[i]))) ++i;
            if (i < len && clean[i] == '{') {
                int brace_depth = 1;
                size_t fn_start = ++i;
                while (i < len && brace_depth > 0) {
                    if (clean[i] == '{') ++brace_depth;
                    else if (clean[i] == '}') --brace_depth;
                    if (brace_depth > 0) ++i;
                }
                std::string body = clean.substr(fn_start, i - fn_start);
                if (i < len && clean[i] == '}') ++i;

                if (id == "src_unpack") out_info.fn_src_unpack = Trim(body);
                else if (id == "src_compile") out_info.fn_src_compile = Trim(body);
                else if (id == "src_install") out_info.fn_src_install = Trim(body);
                continue;
            }
        }

        // Check assignment: id = value;
        if (clean[i] == '=') {
            ++i;
            while (i < len && std::isspace(static_cast<unsigned char>(clean[i]))) ++i;

            size_t val_start = i;
            bool in_str = false;
            char quote = '\0';
            int bracket_depth = 0;
            int brace_depth = 0;

            while (i < len) {
                char c = clean[i];
                if (in_str) {
                    if (c == '\\' && i + 1 < len) ++i;
                    else if (c == quote) in_str = false;
                } else {
                    if (c == '"' || c == '\'') {
                        in_str = true;
                        quote = c;
                    } else if (c == '[') {
                        ++bracket_depth;
                    } else if (c == ']') {
                        --bracket_depth;
                    } else if (c == '{') {
                        ++brace_depth;
                    } else if (c == '}') {
                        --brace_depth;
                    } else if (c == ';' && bracket_depth == 0 && brace_depth == 0) {
                        break;
                    }
                }
                ++i;
            }

            std::string val = clean.substr(val_start, i - val_start);
            if (i < len && clean[i] == ';') ++i;

            val = Trim(val);
            std::string upper_id = ToUpper(id);

            if (upper_id == "DESCRIPTION") {
                out_info.description = ExtractStringLiteral(val);
            } else if (upper_id == "HOMEPAGE") {
                out_info.homepage = ExtractStringLiteral(val);
            } else if (upper_id == "GIT_REPO") {
                out_info.git_repo = ExtractStringLiteral(val);
            } else if (upper_id == "GIT_BRANCH") {
                out_info.git_branch = ExtractStringLiteral(val);
            } else if (upper_id == "UPP_ASSEMBLY") {
                out_info.upp_assembly = ExtractStringLiteral(val);
            } else if (upper_id == "DEFAULT_PACKAGE") {
                out_info.default_package = ExtractStringLiteral(val);
            } else if (upper_id == "IUSE") {
                out_info.iuse = ParseArrayLiteral(val);
            } else if (upper_id == "ACCEPTFLAGS") {
                out_info.acceptflags = ParseArrayLiteral(val);
            } else if (upper_id == "RDEPEND" || upper_id == "DEPEND") {
                out_info.rdepend = ParseArrayLiteral(val);
            } else if (upper_id == "PROVIDERS") {
                out_info.providers = ParseMapLiteral(val);
            }
            continue;
        }

        // Otherwise advance to next semicolon or newline
        while (i < len && clean[i] != ';' && clean[i] != '\n') ++i;
        if (i < len && clean[i] == ';') ++i;
    }

    return true;
}

} // namespace UppBuilder
