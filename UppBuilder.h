#pragma once

#ifdef _WIN32
  #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
  #endif
#endif

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <memory>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <chrono>

#if defined(__has_include)
  #if __has_include(<filesystem>)
    #include <filesystem>
    namespace fs = std::filesystem;
  #elif __has_include(<experimental/filesystem>)
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
  #endif
#endif

#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
  #include <direct.h>
  #define PATH_SEP '\\'
  #define PATH_SEPS "\\"
#else
  #include <unistd.h>
  #include <dirent.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <sys/wait.h>
  #define PATH_SEP '/'
  #define PATH_SEPS "/"
#endif

namespace UppBuilder {

// Common string & path utility functions
bool StartsWith(const std::string& s, const std::string& prefix);
bool EndsWith(const std::string& s, const std::string& suffix);
std::string Trim(const std::string& s);
std::vector<std::string> Split(const std::string& s, char sep);
std::string ToLower(std::string s);
std::string ToUpper(std::string s);
std::string ToInitCaps(const std::string& s);
std::string Replace(std::string s, const std::string& from, const std::string& to);
std::string JoinStrings(const std::vector<std::string>& items, const std::string& sep);
std::string GetEnvValue(const char* name);

bool FileExists(const std::string& path);
bool DirectoryExists(const std::string& path);
bool RealizeDirectory(const std::string& path);
std::string GetCurrentDir();
std::string JoinPath(const std::string& p1, const std::string& p2);
std::string GetParentDir(const std::string& path);
std::string GetFileName(const std::string& path);
std::string GetFileTitle(const std::string& path);
std::string GetFileExt(const std::string& path);
std::string NormalizePath(std::string path);
std::string GetAbsolutePath(const std::string& path);
std::vector<std::string> ListDir(const std::string& path);
void Walk(const std::string& path, std::vector<std::string>& files, const std::string& ext = "");

std::string ReadTextFile(const std::string& path);
bool WriteTextFile(const std::string& path, const std::string& content);
bool CopyFile(const std::string& src, const std::string& dst);
bool CopyDirectoryRecursive(const std::string& src, const std::string& dst);
bool RemoveDirectoryRecursive(const std::string& path);
bool CreateEntrypoint(const std::string& technical_binary,
                      const std::string& entrypoint_path,
                      const std::string& relative_target);
int RunCommand(const std::string& cmd);
std::string CaptureCommand(const std::string& cmd);

// User configuration and cache directories
std::string GetUserConfigDir();
std::string GetUserCacheDir();
std::string ResolveBazaarCorePath(const std::string& cli_override, const std::string& repo_root);
std::string ResolvePatchedUppsrcPath(const std::string& cli_override, const std::string& repo_root);
std::string ResolveUpstreamPath(const std::string& cli_override, const std::string& repo_root);
std::string ReplaceWholeWord(const std::string& str, const std::string& from, const std::string& to);

} // namespace UppBuilder
