#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <memory>
#include <vector>
#include <map>
#include <winsock2.h>

class CViewerDlg;

// Minimal WebSocket server: TCP listen, HTTP upgrade handshake, WebSocket text frames.
// Runs in a background thread; callbacks are invoked on that thread (marshal to UI as needed).
class WebSocketServer
{
public:
    using MessageCallback = std::function<void(const std::string& text, const std::string& clientIp)>;

    WebSocketServer();
    ~WebSocketServer();

    bool Start(unsigned short port = 8765);
    void Stop();

    void SetMessageCallback(MessageCallback cb) { m_onMessage = std::move(cb); }
    bool IsRunning() const { return m_running; }

    void HandleClient(SOCKET clientSocket, const std::string& clientIp);

private:
    static DWORD WINAPI ListenThreadProc(LPVOID param);
    void ListenLoop();
    std::string DoHandshake(SOCKET s, std::string& request);
    bool ReadWebSocketFrame(SOCKET s, std::string& outPayload);
    bool SendWebSocketText(SOCKET s, const std::string& text);
    std::string ComputeAcceptKey(const std::string& key);

    unsigned short m_port = 8765;
    SOCKET m_listenSocket = INVALID_SOCKET;
    HANDLE m_thread = nullptr;
    std::atomic<bool> m_running{ false };
    MessageCallback m_onMessage;
};
