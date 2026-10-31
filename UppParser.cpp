#include "UppParser.h"

namespace UppBuilder {

// --- Boolean Expression Evaluator for flags ---

enum TokenType { TOK_END, TOK_IDENT, TOK_NOT, TOK_AND, TOK_OR, TOK_LPAREN, TOK_RPAREN };

struct Token {
    TokenType type;
    std::string text;
};

class ExprLexer {
public:
    ExprLexer(const std::string& input) : input_(input), pos_(0) {}

    Token Next() {
        SkipWhitespace();
        if (pos_ >= input_.size()) return {TOK_END, ""};
        char c = input_[pos_];
        if (c == '!') { pos_++; return {TOK_NOT, "!"}; }
        if (c == '&') {
            pos_++;
            if (pos_ < input_.size() && input_[pos_] == '&') pos_++;
            return {TOK_AND, "&"};
        }
        if (c == '|') {
            pos_++;
            if (pos_ < input_.size() && input_[pos_] == '|') pos_++;
            return {TOK_OR, "|"};
        }
        if (c == '(') { pos_++; return {TOK_LPAREN, "("}; }
        if (c == ')') { pos_++; return {TOK_RPAREN, ")"}; }

        if (std::isalnum(c) || c == '_' || c == '.') {
            size_t start = pos_;
            while (pos_ < input_.size() && (std::isalnum(input_[pos_]) || input_[pos_] == '_' || input_[pos_] == '.')) {
                pos_++;
            }
            return {TOK_IDENT, input_.substr(start, pos_ - start)};
        }

        pos_++;
        return Next();
    }

private:
    void SkipWhitespace() {
        while (pos_ < input_.size() && std::isspace(input_[pos_])) pos_++;
    }

    std::string input_;
    size_t pos_;
};

class ExprParser {
public:
    ExprParser(const std::string& expr, const std::set<std::string>& flags)
        : lexer_(expr), flags_(flags) {
        current_ = lexer_.Next();
    }

    bool Parse() {
        if (current_.type == TOK_END) return true;
        return ParseOr();
    }

private:
    bool ParseOr() {
        bool left = ParseAnd();
        while (current_.type == TOK_OR) {
            current_ = lexer_.Next();
            bool right = ParseAnd();
            left = left || right;
        }
        return left;
    }

    bool ParseAnd() {
        bool left = ParseUnary();
        while (current_.type == TOK_AND) {
            current_ = lexer_.Next();
            bool right = ParseUnary();
            left = left && right;
        }
        return left;
    }

    bool ParseUnary() {
        if (current_.type == TOK_NOT) {
            current_ = lexer_.Next();
            return !ParseUnary();
        }
        return ParsePrimary();
    }

    bool ParsePrimary() {
        if (current_.type == TOK_IDENT) {
            std::string ident = ToUpper(current_.text);
            current_ = lexer_.Next();
            return flags_.count(ident) > 0;
        }
        if (current_.type == TOK_LPAREN) {
            current_ = lexer_.Next();
            bool res = ParseOr();
            if (current_.type == TOK_RPAREN) {
                current_ = lexer_.Next();
            }
            return res;
        }
        return false;
    }

    ExprLexer lexer_;
    const std::set<std::string>& flags_;
    Token current_;
};

bool UppParser::EvaluateCondition(const std::string& expr, const std::set<std::string>& active_flags) {
    std::string trimmed = Trim(expr);
    if (trimmed.empty()) return true;
    ExprParser parser(trimmed, active_flags);
    return parser.Parse();
}

std::set<std::string> UppParser::ParseFlags(const std::string& flags_str) {
    std::set<std::string> flags;
    std::string norm = flags_str;
    for (char& ch : norm) {
        if (ch == ',' || ch == ';') ch = ' ';
    }
    std::stringstream ss(norm);
    std::string tok;
    while (ss >> tok) {
        tok = Trim(tok);
        if (!tok.empty()) {
            flags.insert(ToUpper(tok));
        }
    }
    return flags;
}

// Parses items in comma-separated blocks like `Core, Draw;`
static void ParseCommaTokens(const std::string& line, std::vector<std::string>& out, bool& block_finished) {
    std::stringstream ss(line);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        tok = Trim(tok);
        while (!tok.empty() && (tok.back() == ';' || tok.back() == ',')) {
            if (tok.back() == ';') block_finished = true;
            tok.pop_back();
        }
        tok = Trim(tok);
        for (char& ch : tok) if (ch == '\\') ch = '/';
        if (!tok.empty()) out.push_back(tok);
    }
    if (line.find(';') != std::string::npos) {
        block_finished = true;
    }
}

bool UppParser::Parse(const std::string& upp_path, UppPackage& out_pkg) {
    std::ifstream is(upp_path);
    if (!is) return false;

    out_pkg.path = upp_path;
    out_pkg.name = GetFileTitle(upp_path);

    std::string line;
    enum class UppSection { None, Uses, File, MainConfig, Include, Library, Link };
    UppSection current_sec = UppSection::None;
    std::string current_condition = "";

    while (std::getline(is, line)) {
        std::string trimmed = Trim(line);
        // Strip line comments
        size_t cpos = trimmed.find("//");
        if (cpos != std::string::npos) trimmed = Trim(trimmed.substr(0, cpos));
        if (trimmed.empty()) continue;

        // Check for section starters
        if (StartsWith(trimmed, "description")) {
            size_t q1 = trimmed.find('"');
            size_t q2 = trimmed.rfind('"');
            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                out_pkg.description = trimmed.substr(q1 + 1, q2 - q1 - 1);
            }
            current_sec = UppSection::None;
            continue;
        }

        if (StartsWith(trimmed, "target")) {
            size_t q1 = trimmed.find('"');
            size_t q2 = trimmed.rfind('"');
            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                out_pkg.target = trimmed.substr(q1 + 1, q2 - q1 - 1);
            }
            current_sec = UppSection::None;
            continue;
        }

        if (StartsWith(trimmed, "mainconfig")) {
            current_sec = UppSection::MainConfig;
            continue;
        }

        if (StartsWith(trimmed, "uses")) {
            current_sec = UppSection::Uses;
            current_condition = "";
            std::string rest = Trim(trimmed.substr(4));
            if (!rest.empty() && rest[0] == '(') {
                size_t cparen = rest.find(')');
                if (cparen != std::string::npos) {
                    current_condition = Trim(rest.substr(1, cparen - 1));
                    rest = Trim(rest.substr(cparen + 1));
                }
            }

            if (!rest.empty()) {
                bool finished = false;
                std::vector<std::string> pkgs;
                ParseCommaTokens(rest, pkgs, finished);
                if (current_condition.empty()) {
                    out_pkg.unconditional_uses.insert(out_pkg.unconditional_uses.end(), pkgs.begin(), pkgs.end());
                } else {
                    out_pkg.conditional_uses.push_back({current_condition, pkgs});
                }
                if (finished) current_sec = UppSection::None;
            }
            continue;
        }

        if (StartsWith(trimmed, "file")) {
            current_sec = UppSection::File;
            std::string rest = Trim(trimmed.substr(4));
            if (!rest.empty()) {
                bool finished = false;
                ParseCommaTokens(rest, out_pkg.files, finished);
                if (finished) current_sec = UppSection::None;
            }
            continue;
        }

        if (StartsWith(trimmed, "include")) {
            current_sec = UppSection::Include;
            std::string rest = Trim(trimmed.substr(7));
            if (!rest.empty()) {
                bool finished = false;
                ParseCommaTokens(rest, out_pkg.includes, finished);
                if (finished) current_sec = UppSection::None;
            }
            continue;
        }

        if (StartsWith(trimmed, "library")) {
            current_sec = UppSection::Library;
            std::string rest = Trim(trimmed.substr(7));
            if (!rest.empty()) {
                bool finished = false;
                ParseCommaTokens(rest, out_pkg.libraries, finished);
                if (finished) current_sec = UppSection::None;
            }
            continue;
        }

        if (StartsWith(trimmed, "link")) {
            current_sec = UppSection::Link;
            std::string rest = Trim(trimmed.substr(4));
            if (!rest.empty()) {
                bool finished = false;
                ParseCommaTokens(rest, out_pkg.links, finished);
                if (finished) current_sec = UppSection::None;
            }
            continue;
        }

        // Section body parsing
        if (current_sec == UppSection::MainConfig) {
            size_t eq = trimmed.find('=');
            if (eq != std::string::npos) {
                std::string k_str = Trim(trimmed.substr(0, eq));
                std::string v_str = Trim(trimmed.substr(eq + 1));
                size_t k1 = k_str.find('"');
                size_t k2 = k_str.rfind('"');
                size_t v1 = v_str.find('"');
                size_t v2 = v_str.rfind('"');
                std::string name = (k1 != std::string::npos && k2 != std::string::npos && k2 > k1) ? k_str.substr(k1 + 1, k2 - k1 - 1) : "";
                std::string flags = (v1 != std::string::npos && v2 != std::string::npos && v2 > v1) ? v_str.substr(v1 + 1, v2 - v1 - 1) : "";
                out_pkg.mainconfigs.push_back({name, flags});
            }
            if (trimmed.find(';') != std::string::npos) current_sec = UppSection::None;
            continue;
        }

        if (current_sec == UppSection::Uses) {
            bool finished = false;
            std::vector<std::string> pkgs;
            ParseCommaTokens(trimmed, pkgs, finished);
            if (current_condition.empty()) {
                out_pkg.unconditional_uses.insert(out_pkg.unconditional_uses.end(), pkgs.begin(), pkgs.end());
            } else {
                out_pkg.conditional_uses.push_back({current_condition, pkgs});
            }
            if (finished) current_sec = UppSection::None;
            continue;
        }

        if (current_sec == UppSection::File) {
            bool finished = false;
            ParseCommaTokens(trimmed, out_pkg.files, finished);
            if (finished) current_sec = UppSection::None;
            continue;
        }

        if (current_sec == UppSection::Include) {
            bool finished = false;
            ParseCommaTokens(trimmed, out_pkg.includes, finished);
            if (finished) current_sec = UppSection::None;
            continue;
        }

        if (current_sec == UppSection::Library) {
            bool finished = false;
            ParseCommaTokens(trimmed, out_pkg.libraries, finished);
            if (finished) current_sec = UppSection::None;
            continue;
        }

        if (current_sec == UppSection::Link) {
            bool finished = false;
            ParseCommaTokens(trimmed, out_pkg.links, finished);
            if (finished) current_sec = UppSection::None;
            continue;
        }
    }

    return true;
}

std::vector<std::string> UppParser::ResolveActiveUses(const UppPackage& pkg, const std::set<std::string>& active_flags) {
    std::vector<std::string> result = pkg.unconditional_uses;
    for (const auto& cond_entry : pkg.conditional_uses) {
        if (EvaluateCondition(cond_entry.first, active_flags)) {
            result.insert(result.end(), cond_entry.second.begin(), cond_entry.second.end());
        }
    }
    return result;
}

} // namespace UppBuilder
