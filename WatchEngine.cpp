#include "WatchEngine.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
  #define CLOSE_SOCK closesocket
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/stat.h>
  #define CLOSE_SOCK close
#endif

namespace UppBuilder {

static volatile std::sig_atomic_t g_stop_requested = 0;

static void HandleSignal(int sig) {
    (void)sig;
    g_stop_requested = 1;
}

static bool SetNonBlocking(int fd) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

WatchEngine::WatchEngine() {}

WatchEngine::~WatchEngine() {
    Stop();
}

void WatchEngine::Stop() {
    running = false;
    CloseServer();
}

bool WatchEngine::SetupServer(int port) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    server_fd = (int)::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return false;

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (::bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        CLOSE_SOCK(server_fd);
        server_fd = -1;

        // Port is already occupied (e.g. by AgentIPC daemon); connect as publisher client
        int cfd = (int)::socket(AF_INET, SOCK_STREAM, 0);
        if (cfd >= 0) {
            if (::connect(cfd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                SetNonBlocking(cfd);
                client_fds.push_back(cfd);
                return true;
            }
            CLOSE_SOCK(cfd);
        }
        return false;
    }

    if (::listen(server_fd, 8) != 0) {
        CLOSE_SOCK(server_fd);
        server_fd = -1;
        return false;
    }

    SetNonBlocking(server_fd);
    return true;
}

void WatchEngine::CloseServer() {
    for (int cfd : client_fds) {
        CLOSE_SOCK(cfd);
    }
    client_fds.clear();

    if (server_fd >= 0) {
        CLOSE_SOCK(server_fd);
        server_fd = -1;
    }
}

void WatchEngine::PollClients() {
    if (server_fd < 0) return;

    // Accept new connections
    while (true) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int cfd = (int)::accept(server_fd, (struct sockaddr*)&client_addr, &len);
        if (cfd < 0) break;
        SetNonBlocking(cfd);
        client_fds.push_back(cfd);
    }

    // Drain read buffer from clients and filter active
    std::vector<int> active;
    for (int cfd : client_fds) {
        char buf[512];
        int n = (int)::recv(cfd, buf, sizeof(buf) - 1, 0);
        if (n == 0) {
            CLOSE_SOCK(cfd);
        } else if (n < 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) active.push_back(cfd);
            else CLOSE_SOCK(cfd);
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) active.push_back(cfd);
            else CLOSE_SOCK(cfd);
#endif
        } else {
            active.push_back(cfd);
        }
    }
    client_fds = active;
}

void WatchEngine::Broadcast(const std::string& type, const std::map<std::string, std::string>& fields) {
    std::string json = "{\"type\":\"" + type + "\"";
    for (const auto& kv : fields) {
        json += ",\"" + kv.first + "\":\"" + kv.second + "\"";
    }
    json += "}\n";

    PollClients();
    std::vector<int> alive;
    for (int cfd : client_fds) {
#ifdef MSG_NOSIGNAL
        int flags = MSG_NOSIGNAL;
#else
        int flags = 0;
#endif
        int sent = ::send(cfd, json.c_str(), (int)json.length(), flags);
        if (sent > 0) {
            alive.push_back(cfd);
        } else {
            CLOSE_SOCK(cfd);
        }
    }
    client_fds = alive;
}

std::map<std::string, FileSnapshot> WatchEngine::Snapshot(const std::vector<std::string>& dirs) {
    std::map<std::string, FileSnapshot> res;
    static const std::set<std::string> valid_exts = {
        ".cpp", ".c", ".cc", ".cxx", ".h", ".hpp", ".hxx",
        ".icpp", ".upp", ".json", ".bm", ".t"
    };

    for (const auto& dir : dirs) {
        if (!DirectoryExists(dir)) continue;

        std::vector<std::string> files;
        Walk(dir, files);

        for (const auto& f : files) {
            // Filter noise directories
            if (f.find("/.git/") != std::string::npos || f.find("\\.git\\") != std::string::npos ||
                f.find("/bin/") != std::string::npos || f.find("\\bin\\") != std::string::npos ||
                f.find("/out/") != std::string::npos || f.find("\\out\\") != std::string::npos ||
                f.find("/.cache/") != std::string::npos || f.find("\\.cache\\") != std::string::npos ||
                f.find("/tmp/") != std::string::npos || f.find("\\tmp\\") != std::string::npos) {
                continue;
            }

            std::string ext = ToLower(GetFileExt(f));
            if (valid_exts.find(ext) == valid_exts.end()) continue;

#ifdef _WIN32
            struct _stat st;
            if (_stat(f.c_str(), &st) == 0) {
                FileSnapshot snap;
                snap.path = f;
                snap.mtime = (uint64_t)st.st_mtime;
                snap.size = (uint64_t)st.st_size;
                res[f] = snap;
            }
#else
            struct stat st;
            if (stat(f.c_str(), &st) == 0) {
                FileSnapshot snap;
                snap.path = f;
                snap.mtime = (uint64_t)st.st_mtime;
                snap.size = (uint64_t)st.st_size;
                res[f] = snap;
            }
#endif
        }
    }

    return res;
}

std::string WatchEngine::InferTestAtom(const std::string& path) {
    std::string norm = NormalizePath(path);

    // Assembly matching
    for (const char* prefix : {"autotest", "upptst", "reference", "tutorial", "examples"}) {
        std::string p_str = prefix;
        size_t p = norm.find("/applied-" + p_str + "/");
        if (p == std::string::npos) p = norm.find("/" + p_str + "/");
        if (p != std::string::npos) {
            size_t start = norm.find('/', p + 1);
            if (start != std::string::npos) {
                start++;
                size_t end = norm.find('/', start);
                if (end != std::string::npos) {
                    return p_str + "/" + norm.substr(start, end - start);
                }
            }
        }
    }

    // Portage Overlay package matching (e.g. .../DevCpp/BasicHtmlRenderer/...)
    for (const char* cat : {"DevCpp", "DevNetwork", "DevGraphics", "DevUtil", "DevMedia", "DevGui"}) {
        std::string cat_str = cat;
        size_t p = norm.find("/" + cat_str + "/");
        if (p != std::string::npos) {
            size_t start = p + cat_str.length() + 2;
            size_t end = norm.find('/', start);
            if (end != std::string::npos) {
                return cat_str + "/" + norm.substr(start, end - start);
            }
        }
    }

    return "@installed";
}

bool WatchEngine::Run(const WatchOptions& options) {
    running = true;
    g_stop_requested = 0;

    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif

    std::vector<std::string> watch_dirs = options.directories;
    if (watch_dirs.empty()) {
        watch_dirs.push_back(GetCurrentDir());
    }

    bool server_ok = SetupServer(options.port);
    std::cout << ">>> WatchEngine: Monitoring " << watch_dirs.size() << " directory/directories.\n";
    if (server_ok) {
        if (server_fd >= 0) {
            std::cout << ">>> WatchEngine: AgentIPC TCP server active on 127.0.0.1:" << options.port << "\n";
        } else {
            std::cout << ">>> WatchEngine: Connected as AgentIPC publisher to 127.0.0.1:" << options.port << "\n";
        }
    } else {
        std::cout << "Notice: AgentIPC port " << options.port << " in use or unavailable. Running in local event mode.\n";
    }

    // Broadcast daemon startup
    Broadcast("agent_status", {{"status", "online"}, {"port", std::to_string(options.port)}});

    // Initial snapshot
    auto prev_snapshot = Snapshot(watch_dirs);
    std::cout << ">>> WatchEngine: Indexed " << prev_snapshot.size() << " source files. Waiting for mutations...\n";

    if (options.run_immediate && !options.test_atom.empty()) {
        std::cout << ">>> Running immediate initial test target: " << options.test_atom << "\n";
        Broadcast("build_started", {{"target", options.test_atom}});
        int ret = 0;
        auto t0 = std::chrono::steady_clock::now();
        std::string out = CaptureCommand("uppbuilder test " + options.test_atom, &ret);
        auto t1 = std::chrono::steady_clock::now();
        int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        bool pass = (ret == 0 && (out.find("[PASS]") != std::string::npos || out.find("0 failed") != std::string::npos));
        Broadcast("test_result", {
            {"target", options.test_atom},
            {"status", pass ? "pass" : "fail"},
            {"duration_ms", std::to_string(ms)},
            {"message", pass ? "Initial test passed" : "Initial test failed"}
        });
        std::cout << (pass ? "  [PASS] " : "  [FAIL] ") << options.test_atom << " (" << ms << "ms)\n";
    }

    int iteration = 0;
    while (running && !g_stop_requested) {
        PollClients();

        std::this_thread::sleep_for(std::chrono::milliseconds(options.interval_ms));
        iteration++;

        auto current_snapshot = Snapshot(watch_dirs);

        // Detect modifications
        std::string changed_file;
        for (const auto& kv : current_snapshot) {
            auto it = prev_snapshot.find(kv.first);
            if (it == prev_snapshot.end()) {
                changed_file = kv.first;
                break;
            } else if (it->second.mtime != kv.second.mtime || it->second.size != kv.second.size) {
                changed_file = kv.first;
                break;
            }
        }

        if (!changed_file.empty()) {
            // Debounce window
            if (options.debounce_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(options.debounce_ms));
            }

            std::cout << "\n>>> [WATCH] Mutation detected in: " << changed_file << "\n";

            Broadcast("file_changed", {
                {"path", changed_file},
                {"status", "modified"}
            });

            std::string target = options.test_atom;
            if (target.empty()) {
                target = InferTestAtom(changed_file);
            }

            std::cout << ">>> [WATCH] Triggering incremental target: " << target << "\n";
            Broadcast("build_started", {{"target", target}});

            int exit_code = 0;
            auto t0 = std::chrono::steady_clock::now();
            std::string log = CaptureCommand("uppbuilder test " + target, &exit_code);
            auto t1 = std::chrono::steady_clock::now();
            int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            bool pass = (exit_code == 0 && (log.find("[PASS]") != std::string::npos || log.find("0 failed") != std::string::npos));

            std::cout << (pass ? "  [PASS] " : "  [FAIL] ") << target << " (" << ms << "ms)\n";

            Broadcast("test_result", {
                {"target", target},
                {"status", pass ? "pass" : "fail"},
                {"duration_ms", std::to_string(ms)},
                {"message", pass ? "Incremental test passed" : "Incremental test failed"}
            });

            // Update snapshot to current
            prev_snapshot = Snapshot(watch_dirs);
        }

        if (options.max_iterations > 0 && iteration >= options.max_iterations) {
            break;
        }
    }

    std::cout << ">>> WatchEngine: Stopping daemon...\n";
    Broadcast("agent_status", {{"status", "offline"}});
    CloseServer();
    return true;
}

}
