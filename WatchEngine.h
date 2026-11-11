#pragma once

#include "UppBuilder.h"
#include <map>
#include <vector>
#include <string>

namespace UppBuilder {

struct WatchOptions {
    std::vector<std::string> directories;
    std::string test_atom;        // Specific atom to test (or empty for auto-detection)
    int         port = 7421;      // AgentIPC TCP loopback port
    bool        daemon = false;
    int         interval_ms = 1000;
    int         debounce_ms = 400;
    bool        run_immediate = false;
    bool        verbose = false;
    int         max_iterations = 0; // 0 = infinite (until Ctrl+C/SIGINT), >0 for test harness
};

struct FileSnapshot {
    std::string path;
    uint64_t    mtime = 0;
    uint64_t    size = 0;
};

class WatchEngine {
public:
    WatchEngine();
    ~WatchEngine();

    // Starts watch loop; blocks until interrupted or max_iterations reached
    bool Run(const WatchOptions& options);
    void Stop();

    // Snapshot directory state
    static std::map<std::string, FileSnapshot> Snapshot(const std::vector<std::string>& dirs);

    // Broadcast JSON frame to connected AgentIPC clients
    void Broadcast(const std::string& type, const std::map<std::string, std::string>& fields);

    // Map a source file path to the most relevant test atom
    static std::string InferTestAtom(const std::string& path);

private:
    bool SetupServer(int port);
    void CloseServer();
    void PollClients();

    int  server_fd = -1;
    bool running = false;
    std::vector<int> client_fds;
};

}
