#pragma once

#include <string>

namespace Remote {

struct ClientConfig {
    std::wstring serverUrl;  // e.g. wss://your-worker.workers.dev/ws
    bool valid = false;
};

// Load config.json from the same directory as the DLL.
// Expects JSON with "server_url": "wss://..." (or "server_url": "https://...")
ClientConfig LoadConfig();

} // namespace Remote
