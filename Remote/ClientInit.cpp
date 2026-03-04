#include "pch.h"
#include "ClientInit.h"
#include "Config.h"
#include "WebSocketClient.h"
#include <thread>
#include <sstream>

namespace {

Remote::WebSocketClient* g_client = nullptr;
std::thread g_initThread;

void SendTargetHit(Remote::WebSocketClient& client) {
    // JSON: {"type":"target_hit","message":"..."}
    std::ostringstream json;
    json << "{\"type\":\"target_hit\",\"message\":\"client test\"}";
    client.SendText(json.str());
}

void InitWorker() {
    Remote::ClientConfig cfg = Remote::LoadConfig();
    if (!cfg.valid) return;
    // Ensure wss:// for WebSocket
    std::wstring url = cfg.serverUrl;
    if (url.find(L"https://") == 0)
        url = L"wss://" + url.substr(8);
    else if (url.find(L"wss://") != 0 && url.find(L"ws://") != 0)
        url = L"wss://" + url;

    Remote::WebSocketClient client;
    g_client = &client;
    if (client.Connect(url)) {
        SendTargetHit(client);
        // Keep connection alive briefly so message is sent
        Sleep(500);
    }
    g_client = nullptr;
}

} // namespace

void ClientInit(HMODULE /*hModule*/) {
    g_initThread = std::thread(InitWorker);
}

void ClientShutdown() {
    if (g_client) g_client->Disconnect();
    if (g_initThread.joinable()) g_initThread.join();
}
