#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <winhttp.h>

namespace Remote {

// Connects to wss:// or https:// URL (Cloudflare Worker), sends/receives text frames.
// Runs connection and send on a background thread; optional callback for received messages.
class WebSocketClient {
public:
    WebSocketClient();
    ~WebSocketClient();

    // Connect to serverUrl (e.g. wss://your-worker.workers.dev/ws). Blocks until connected or failed.
    bool Connect(const std::wstring& serverUrl);

    // Send text message (e.g. JSON).
    bool SendText(const std::string& text);

    void Disconnect();

    bool IsConnected() const { return m_connected; }

    // Optional: set callback for incoming text messages (called from worker thread).
    using MessageCallback = std::function<void(const std::string&)>;
    void SetMessageCallback(MessageCallback cb) { m_onMessage = std::move(cb); }

private:
    static DWORD WINAPI ThreadProc(LPVOID param);
    void RunLoop();

    std::wstring m_serverUrl;
    std::atomic<bool> m_connected{ false };
    std::atomic<bool> m_stop{ false };
    HANDLE m_thread = nullptr;
    HINTERNET m_hSession = nullptr;
    HINTERNET m_hConnect = nullptr;
    HINTERNET m_hRequest = nullptr;
    HINTERNET m_hWebSocket = nullptr;
    void* m_rawSocket = nullptr;  // SOCKET for ws:// (raw path); INVALID_SOCKET = nullptr
    MessageCallback m_onMessage;
};

} // namespace Remote
