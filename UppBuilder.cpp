#include "UppBuilder.h"

namespace UppBuilder {

bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string Trim(const std::string& s) {
    auto first = s.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return "";
    auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, (last - first + 1));
}

std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, sep)) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::string ToUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

std::string Replace(std::string s, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = s.find(from, start_pos)) != std::string::npos) {
        s.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return s;
}

std::string JoinStrings(const std::vector<std::string>& items, const std::string& sep) {
    std::stringstream ss;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) ss << sep;
        ss << items[i];
    }
    return ss.str();
}

std::string GetEnvValue(const char* name) {
    const char* val = getenv(name);
    return val ? std::string(val) : "";
}

bool FileExists(const std::string& path) {
#ifdef _WIN32
    return _access(path.c_str(), 0) == 0;
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && !S_ISDIR(st.st_mode);
#endif
}

bool DirectoryExists(const std::string& path) {
#ifdef _WIN32
    DWORD dwAttrib = GetFileAttributesA(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

bool RealizeDirectory(const std::string& path) {
    if (path.empty() || DirectoryExists(path)) return true;
    std::string parent = GetParentDir(path);
    if (!parent.empty() && parent != path && !DirectoryExists(parent)) {
        RealizeDirectory(parent);
    }
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0 || DirectoryExists(path);
#else
    return mkdir(path.c_str(), 0755) == 0 || DirectoryExists(path);
#endif
}

std::string GetCurrentDir() {
    char buf[4096];
#ifdef _WIN32
    if (_getcwd(buf, sizeof(buf))) return std::string(buf);
#else
    if (getcwd(buf, sizeof(buf))) return std::string(buf);
#endif
    return ".";
}

std::string JoinPath(const std::string& p1, const std::string& p2) {
    if (p1.empty()) return p2;
    if (p2.empty()) return p1;
    char last = p1.back();
    if (last == '/' || last == '\\') return p1 + p2;
    return p1 + PATH_SEP + p2;
}

std::string GetParentDir(const std::string& path) {
    std::string p = NormalizePath(path);
    while (!p.empty() && p.back() == '/') p.pop_back();
    size_t pos = p.find_last_of('/');
    if (pos == std::string::npos) return ".";
    if (pos == 0) return "/";
    return p.substr(0, pos);
}

std::string GetFileName(const std::string& path) {
    std::string p = NormalizePath(path);
    while (!p.empty() && p.back() == '/') p.pop_back();
    size_t pos = p.find_last_of('/');
    if (pos == std::string::npos) return p;
    return p.substr(pos + 1);
}

std::string GetFileTitle(const std::string& path) {
    std::string fn = GetFileName(path);
    size_t dot = fn.find_last_of('.');
    if (dot != std::string::npos && dot > 0) return fn.substr(0, dot);
    return fn;
}

std::string GetFileExt(const std::string& path) {
    std::string fn = GetFileName(path);
    size_t dot = fn.find_last_of('.');
    if (dot != std::string::npos) return fn.substr(dot);
    return "";
}

std::string NormalizePath(std::string path) {
    for (char& c : path) {
        if (c == '\\') c = '/';
    }
    return path;
}

std::string GetAbsolutePath(const std::string& path) {
#ifdef _WIN32
    char full[MAX_PATH];
    if (GetFullPathNameA(path.c_str(), MAX_PATH, full, nullptr)) {
        return NormalizePath(std::string(full));
    }
#else
    char full[4096];
    if (realpath(path.c_str(), full)) {
        return NormalizePath(std::string(full));
    }
#endif
    if (StartsWith(path, "/") || (path.size() >= 2 && path[1] == ':')) return NormalizePath(path);
    return NormalizePath(JoinPath(GetCurrentDir(), path));
}

std::vector<std::string> ListDir(const std::string& path) {
    std::vector<std::string> entries;
#ifdef _WIN32
    std::string pattern = JoinPath(path, "*");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::string n = fd.cFileName;
            if (n != "." && n != "..") entries.push_back(n);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR* d = opendir(path.c_str());
    if (d) {
        struct dirent* de;
        while ((de = readdir(d)) != nullptr) {
            std::string n = de->d_name;
            if (n != "." && n != "..") entries.push_back(n);
        }
        closedir(d);
    }
#endif
    return entries;
}

void Walk(const std::string& path, std::vector<std::string>& files, const std::string& ext) {
    for (const auto& item : ListDir(path)) {
        std::string full = JoinPath(path, item);
        if (DirectoryExists(full)) {
            Walk(full, files, ext);
        } else if (ext.empty() || EndsWith(item, ext)) {
            files.push_back(full);
        }
    }
}

std::string ReadTextFile(const std::string& path) {
    std::ifstream is(path);
    if (!is) return "";
    std::stringstream ss;
    ss << is.rdbuf();
    return ss.str();
}

bool WriteTextFile(const std::string& path, const std::string& content) {
    RealizeDirectory(GetParentDir(path));
    std::ofstream os(path);
    if (!os) return false;
    os << content;
    return true;
}

bool CopyFile(const std::string& src, const std::string& dst) {
    RealizeDirectory(GetParentDir(dst));
    std::ifstream is(src, std::ios::binary);
    if (!is) return false;
    std::ofstream os(dst, std::ios::binary);
    if (!os) return false;
    os << is.rdbuf();
    return true;
}

bool CopyDirectoryRecursive(const std::string& src, const std::string& dst) {
    if (!DirectoryExists(src)) return false;
    RealizeDirectory(dst);
    for (const auto& item : ListDir(src)) {
        std::string s = JoinPath(src, item);
        std::string d = JoinPath(dst, item);
        if (DirectoryExists(s)) {
            if (!CopyDirectoryRecursive(s, d)) return false;
        } else {
            if (!CopyFile(s, d)) return false;
        }
    }
    return true;
}

bool RemoveDirectoryRecursive(const std::string& path) {
    if (!DirectoryExists(path)) return true;
    for (const auto& item : ListDir(path)) {
        std::string full = JoinPath(path, item);
        if (DirectoryExists(full)) {
            RemoveDirectoryRecursive(full);
        } else {
#ifdef _WIN32
            DeleteFileA(full.c_str());
#else
            unlink(full.c_str());
#endif
        }
    }
#ifdef _WIN32
    return RemoveDirectoryA(path.c_str()) != 0;
#else
    return rmdir(path.c_str()) == 0;
#endif
}

int RunCommand(const std::string& cmd) {
    return std::system(cmd.c_str());
}

std::string CaptureCommand(const std::string& cmd) {
    std::string result;
#ifdef _WIN32
    FILE* fp = _popen(cmd.c_str(), "r");
#else
    FILE* fp = popen(cmd.c_str(), "r");
#endif
    if (!fp) return "";
    char buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
        result += buf;
    }
#ifdef _WIN32
    _pclose(fp);
#else
    pclose(fp);
#endif
    return result;
}

std::string GetUserConfigDir() {
#ifdef _WIN32
    std::string appdata = GetEnvValue("APPDATA");
    if (!appdata.empty()) return JoinPath(appdata, "uppbuilder");
    return "C:\\uppbuilder";
#else
    std::string xdg = GetEnvValue("XDG_CONFIG_HOME");
    if (!xdg.empty()) return JoinPath(xdg, "uppbuilder");
    std::string home = GetEnvValue("HOME");
    return JoinPath(home.empty() ? "/tmp" : home, ".config/uppbuilder");
#endif
}

std::string GetUserCacheDir() {
#ifdef _WIN32
    std::string local = GetEnvValue("LOCALAPPDATA");
    if (!local.empty()) return JoinPath(local, "uppbuilder");
    std::string appdata = GetEnvValue("APPDATA");
    if (!appdata.empty()) return JoinPath(appdata, "uppbuilder");
    return "C:\\uppbuilder\\cache";
#else
    std::string xdg = GetEnvValue("XDG_CACHE_HOME");
    if (!xdg.empty()) return JoinPath(xdg, "uppbuilder");
    std::string home = GetEnvValue("HOME");
    return JoinPath(home.empty() ? "/tmp" : home, ".cache/uppbuilder");
#endif
}

static std::string ExtractJsonValue(const std::string& json, const std::string& key) {
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

std::string ResolvePatchedUppsrcPath(const std::string& cli_override, const std::string& repo_root) {
    if (!cli_override.empty()) return NormalizePath(cli_override);

    std::string env = GetEnvValue("UPP_PATCHED_UPPSRC");
    if (!env.empty()) return NormalizePath(env);

    // Check local repo options
    std::vector<std::string> local_candidates = {
        JoinPath(repo_root, "patched-uppsrc"),
        JoinPath(repo_root, "applied-patches")
    };
    for (const auto& c : local_candidates) {
        if (DirectoryExists(c)) return NormalizePath(c);
    }

    // Check global config file
    std::string cfg_file = JoinPath(GetUserConfigDir(), "config.json");
    if (FileExists(cfg_file)) {
        std::string json = ReadTextFile(cfg_file);
        std::string val = ExtractJsonValue(json, "patched_uppsrc");
        if (val.empty()) val = ExtractJsonValue(json, "applied_patches");
        if (!val.empty()) return NormalizePath(val);
    }

    // Default to user cache
    return NormalizePath(JoinPath(GetUserCacheDir(), "patched-uppsrc"));
}

std::string ResolveUpstreamPath(const std::string& cli_override, const std::string& repo_root) {
    if (!cli_override.empty()) return NormalizePath(cli_override);

    std::string env = GetEnvValue("UPP_UPSTREAM");
    if (!env.empty()) return NormalizePath(env);

    // Check global config file
    std::string cfg_file = JoinPath(GetUserConfigDir(), "config.json");
    if (FileExists(cfg_file)) {
        std::string json = ReadTextFile(cfg_file);
        std::string val = ExtractJsonValue(json, "upstream");
        if (!val.empty() && DirectoryExists(val)) return NormalizePath(val);
    }

    // Auto-detect common candidate paths
    std::vector<std::string> candidates = {
        JoinPath(GetParentDir(repo_root), "ultimatepp/uppsrc"),
        JoinPath(repo_root, "../ultimatepp/uppsrc"),
        JoinPath(GetEnvValue("HOME"), "Dev/ultimatepp/uppsrc"),
        "/yhteinen/active/sblo/Dev/ultimatepp/uppsrc",
        "/home/sblo/Dev/ultimatepp/uppsrc"
    };
    for (const auto& c : candidates) {
        if (!c.empty() && DirectoryExists(c)) {
            return NormalizePath(c);
        }
    }
    return "";
}

std::string ReplaceWholeWord(const std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return str;
    std::string result;
    size_t last_pos = 0;
    size_t pos = 0;
    while ((pos = str.find(from, last_pos)) != std::string::npos) {
        bool left_ok = (pos == 0) || (!isalnum((unsigned char)str[pos - 1]) && str[pos - 1] != '_' && str[pos - 1] != '/');
        bool right_ok = (pos + from.size() >= str.size()) || (!isalnum((unsigned char)str[pos + from.size()]) && str[pos + from.size()] != '_' && str[pos + from.size()] != '/');
        if (left_ok && right_ok) {
            result.append(str, last_pos, pos - last_pos);
            result.append(to);
            last_pos = pos + from.size();
        } else {
            result.append(str, last_pos, pos - last_pos + 1);
            last_pos = pos + 1;
        }
    }
    result.append(str, last_pos, str.size() - last_pos);
    return result;
}

} // namespace UppBuilder
