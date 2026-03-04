#include "pch.h"
#include "ClientInit.h"
#include "Config.h"
#include "WebSocketClient.h"
#include <thread>
#include <sstream>
#include <atomic>

namespace {

Remote::WebSocketClient* g_client = nullptr;
std::thread g_initThread;
std::atomic<bool> g_shutdownRequested{ false };

static void LogException(const char* context, const std::exception& e) {
    OutputDebugStringA("[Remote] ");
    OutputDebugStringA(context);
    OutputDebugStringA(" exception: ");
    OutputDebugStringA(e.what());
    OutputDebugStringA("\n");
}
static void LogExceptionUnknown(const char* context) {
    OutputDebugStringA("[Remote] ");
    OutputDebugStringA(context);
    OutputDebugStringA(": unknown exception\n");
}

void SendTargetHit(Remote::WebSocketClient& client) {
    std::ostringstream json;
    json << "{\"type\":\"target_hit\",\"message\":\"client test\"}";
    client.SendText(json.str());
}

void InitWorker() {
    try {
        Remote::ClientConfig cfg = Remote::LoadConfig();
        if (!cfg.valid) return;
        std::wstring url = cfg.serverUrl;
        if (url.find(L"https://") == 0)
            url = L"wss://" + url.substr(8);
        else if (url.find(L"wss://") != 0 && url.find(L"ws://") != 0)
            url = L"wss://" + url;

        Remote::WebSocketClient client;
        g_client = &client;
        if (client.Connect(url)) {
            SendTargetHit(client);
            Sleep(500);
            while (!g_shutdownRequested && client.IsConnected()) {
                Sleep(500);
            }
            if (g_client && g_client->IsConnected()) {
                g_client->SendText("{\"type\":\"end\"}");
                Sleep(200);
            }
        }
        g_client = nullptr;
    } catch (const std::exception& e) {
        LogException("InitWorker", e);
        g_client = nullptr;
    } catch (...) {
        LogExceptionUnknown("InitWorker");
        g_client = nullptr;
    }
}

} // namespace

void ClientInit(HMODULE /*hModule*/) {
    try {
        g_initThread = std::thread(InitWorker);
    } catch (const std::exception& e) {
        LogException("ClientInit", e);
    } catch (...) {
        LogExceptionUnknown("ClientInit");
    }
}

void ClientShutdown() {
    try {
        g_shutdownRequested = true;
        if (g_initThread.joinable())
            g_initThread.join();
        g_shutdownRequested = false;
    } catch (const std::exception& e) {
        LogException("ClientShutdown", e);
        g_shutdownRequested = false;
    } catch (...) {
        LogExceptionUnknown("ClientShutdown");
        g_shutdownRequested = false;
    }
}
