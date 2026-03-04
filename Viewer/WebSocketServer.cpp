#include "pch.h"
#include "WebSocketServer.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#pragma comment(lib, "ws2_32.lib")

#pragma comment(lib, "bcrypt.lib")

namespace {

std::string Base64Encode(const unsigned char* data, size_t len) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = (unsigned int)data[i] << 16;
        if (i + 1 < len) n |= (unsigned int)data[i + 1] << 8;
        if (i + 2 < len) n |= (unsigned int)data[i + 2];
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += (i + 1 < len) ? tbl[(n >> 6) & 63] : '=';
        out += (i + 2 < len) ? tbl[n & 63] : '=';
    }
    return out;
}

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

} // namespace

WebSocketServer::WebSocketServer() = default;

WebSocketServer::~WebSocketServer() {
    Stop();
}

std::string WebSocketServer::ComputeAcceptKey(const std::string& key) {
    const char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC11B11";
    std::string input = Trim(key) + magic;
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr, 0) != 0)
        return {};
    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }
    if (BCryptHashData(hHash, (PUCHAR)input.data(), (ULONG)input.size(), 0) != 0) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }
    UCHAR hash[20];
    ULONG hashLen = 20;
    if (BCryptFinishHash(hHash, hash, hashLen, 0) != 0) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return Base64Encode(hash, 20);
}

std::string WebSocketServer::DoHandshake(SOCKET s, std::string& request) {
    char buf[4096];
    int total = 0;
    while (total < (int)sizeof(buf) - 1) {
        int n = recv(s, buf + total, (int)(sizeof(buf) - 1 - total), 0);
        if (n <= 0) return {};
        total += n;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) break;
    }
    request.assign(buf, total);
    std::string key;
    const char* p = buf;
    const char* endBuf = buf + total;
    const char keyName[] = "sec-websocket-key:";
    const size_t keyNameLen = sizeof(keyName) - 1;
    while (p + keyNameLen <= endBuf) {
        if (_strnicmp(p, keyName, keyNameLen) == 0) {
            p += keyNameLen;
            while (p < endBuf && (*p == ' ' || *p == '\t')) p++;
            const char* lineEnd = (const char*)strpbrk(p, "\r\n");
            if (lineEnd && lineEnd > p) { key.assign(p, lineEnd); break; }
        }
        p++;
    }
    if (key.empty()) return {};
    std::string accept = ComputeAcceptKey(key);
    if (accept.empty()) return {};
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    if (send(s, response.data(), (int)response.size(), 0) != (int)response.size())
        return {};
    return key;
}

bool WebSocketServer::ReadWebSocketFrame(SOCKET s, std::string& outPayload) {
    unsigned char hdr[2];
    if (recv(s, (char*)hdr, 2, 0) != 2) return false;
    unsigned int payloadLen = hdr[1] & 0x7F;
    if (payloadLen == 126) {
        unsigned char ext[2];
        if (recv(s, (char*)ext, 2, 0) != 2) return false;
        payloadLen = (ext[0] << 8) | ext[1];
    } else if (payloadLen == 127) {
        unsigned char ext[8];
        if (recv(s, (char*)ext, 8, 0) != 8) return false;
        payloadLen = 0;
        for (int i = 0; i < 8; i++) payloadLen = (payloadLen << 8) | ext[i];
    }
    bool masked = (hdr[1] & 0x80) != 0;
    unsigned char mask[4];
    if (masked && recv(s, (char*)mask, 4, 0) != 4) return false;
    outPayload.resize(payloadLen);
    int got = 0;
    while (got < (int)payloadLen) {
        int n = recv(s, &outPayload[0] + got, (int)(payloadLen - got), 0);
        if (n <= 0) return false;
        if (masked) for (int i = 0; i < n; i++) outPayload[got + i] ^= mask[(got + i) % 4];
        got += n;
    }
    return true;
}

bool WebSocketServer::SendWebSocketText(SOCKET s, const std::string& text) {
    std::vector<unsigned char> frame;
    frame.push_back(0x81); // FIN + text
    if (text.size() < 126) {
        frame.push_back((unsigned char)text.size());
    } else if (text.size() < 65536) {
        frame.push_back(126);
        frame.push_back((text.size() >> 8) & 0xFF);
        frame.push_back(text.size() & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) frame.push_back((text.size() >> (i * 8)) & 0xFF);
    }
    frame.insert(frame.end(), text.begin(), text.end());
    return send(s, (const char*)frame.data(), (int)frame.size(), 0) == (int)frame.size();
}

struct ClientThreadCtx {
    WebSocketServer* server;
    SOCKET socket;
    std::string ip;
};

static DWORD WINAPI ClientThreadProc(LPVOID param) {
    std::unique_ptr<ClientThreadCtx> ctx((ClientThreadCtx*)param);
    if (ctx && ctx->server)
        ctx->server->HandleClient(ctx->socket, ctx->ip);
    return 0;
}

void WebSocketServer::HandleClient(SOCKET clientSocket, const std::string& clientIp) {
    std::string request;
    if (DoHandshake(clientSocket, request).empty()) {
        closesocket(clientSocket);
        return;
    }
    while (m_running) {
        std::string payload;
        if (!ReadWebSocketFrame(clientSocket, payload)) break;
        if (m_onMessage) m_onMessage(payload, clientIp);
    }
    closesocket(clientSocket);
}

void WebSocketServer::ListenLoop() {
    while (m_running && m_listenSocket != INVALID_SOCKET) {
        sockaddr_in clientAddr = {};
        int len = sizeof(clientAddr);
        SOCKET client = accept(m_listenSocket, (sockaddr*)&clientAddr, &len);
        if (client == INVALID_SOCKET) continue;
        char ipStr[64] = {};
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
        auto* ctx = new ClientThreadCtx{ this, client, ipStr };
        HANDLE h = CreateThread(nullptr, 0, ClientThreadProc, ctx, 0, nullptr);
        if (h)
            CloseHandle(h);
        else
            delete ctx;
    }
}

DWORD WINAPI WebSocketServer::ListenThreadProc(LPVOID param) {
    auto* srv = (WebSocketServer*)param;
    if (srv) srv->ListenLoop();
    return 0;
}

bool WebSocketServer::Start(unsigned short port) {
    if (m_running) return true;
    m_port = port;
    WSADATA wsa = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) return false;
    int opt = 1;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);
    if (bind(m_listenSocket, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }
    if (listen(m_listenSocket, SOMAXCONN) != 0) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }
    m_running = true;
    m_thread = CreateThread(nullptr, 0, ListenThreadProc, this, 0, nullptr);
    return m_thread != nullptr;
}

void WebSocketServer::Stop() {
    m_running = false;
    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }
    if (m_thread) {
        WaitForSingleObject(m_thread, 5000);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
}
