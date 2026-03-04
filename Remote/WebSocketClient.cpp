#include "pch.h"
#include "WebSocketClient.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <sstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

namespace Remote {

namespace {

bool ParseUrl(const std::wstring& url, std::wstring& host, int& port, std::wstring& path, bool& useSsl) {
    useSsl = (url.find(L"wss://") == 0 || url.find(L"https://") == 0);
    std::wstring rest = (url.find(L"://") != std::wstring::npos) ? url.substr(url.find(L"://") + 3) : url;
    size_t firstSlash = rest.find(L'/');
    host = (firstSlash != std::wstring::npos) ? rest.substr(0, firstSlash) : rest;
    path = (firstSlash != std::wstring::npos) ? rest.substr(firstSlash) : L"/";
    size_t colon = host.find(L':');
    if (colon != std::wstring::npos) {
        port = _wtoi(host.substr(colon + 1).c_str());
        host = host.substr(0, colon);
    } else {
        port = useSsl ? 443 : 80;
    }
    return !host.empty();
}

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::string WideToNarrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

// Base64 encode (for Sec-WebSocket-Key)
std::string Base64Encode(const unsigned char* data, size_t len) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = (unsigned int)data[i] << 16;
        if (i + 1 < len) n |= (unsigned int)data[i + 1] << 8;
        if (i + 2 < len) n |= (unsigned int)data[i + 2];
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += (i + 1 < len) ? (char)tbl[(n >> 6) & 63] : '=';
        out += (i + 2 < len) ? (char)tbl[n & 63] : '=';
    }
    return out;
}

// Connect via raw TCP + HTTP WebSocket handshake for ws:// (no WinHTTP proxy/SSL issues)
bool ConnectRawSocket(const std::wstring& host, int port, const std::wstring& path, SOCKET& outSocket) {
    WSADATA wsa = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    std::string hostA = WideToNarrow(host);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, hostA.c_str(), &addr.sin_addr) != 1) {
        addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostA.c_str(), nullptr, &hints, &res) != 0 || !res) {
            closesocket(s);
            return false;
        }
        addr.sin_addr = ((sockaddr_in*)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(s);
        return false;
    }

    unsigned char keyBytes[16];
    for (int i = 0; i < 16; i++) keyBytes[i] = (unsigned char)(rand() % 256);
    std::string key = Base64Encode(keyBytes, 16);

    std::string pathA = WideToUtf8(path);
    if (pathA.empty()) pathA = "/";
    std::ostringstream req;
    req << "GET " << pathA << " HTTP/1.1\r\n"
        << "Host: " << hostA << ":" << port << "\r\n"
        << "Upgrade: websocket\r\n"
        << "Connection: Upgrade\r\n"
        << "Sec-WebSocket-Key: " << key << "\r\n"
        << "Sec-WebSocket-Version: 13\r\n"
        << "\r\n";
    std::string reqStr = req.str();
    if (send(s, reqStr.data(), (int)reqStr.size(), 0) != (int)reqStr.size()) {
        closesocket(s);
        return false;
    }

    char resp[2048];
    int total = 0;
    while (total < (int)sizeof(resp) - 1) {
        int n = recv(s, resp + total, (int)(sizeof(resp) - 1 - total), 0);
        if (n <= 0) { closesocket(s); return false; }
        total += n;
        resp[total] = '\0';
        if (strstr(resp, "\r\n\r\n")) break;
    }
    if (strstr(resp, "101") == nullptr) {
        closesocket(s);
        return false;
    }
    outSocket = s;
    return true;
}

} // namespace

WebSocketClient::WebSocketClient() = default;

WebSocketClient::~WebSocketClient() {
    Disconnect();
}

bool WebSocketClient::Connect(const std::wstring& serverUrl) {
    if (m_connected) return true;
    m_serverUrl = serverUrl;
    std::wstring host;
    int port = 443;
    std::wstring path;
    bool useSsl = true;
    if (!ParseUrl(serverUrl, host, port, path, useSsl)) return false;

    // ws:// : use raw socket to avoid WinHTTP proxy / localhost issues
    if (!useSsl) {
        m_rawSocket = nullptr;
        SOCKET s = INVALID_SOCKET;
        if (ConnectRawSocket(host, port, path, s) && s != INVALID_SOCKET) {
            m_rawSocket = (void*)(UINT_PTR)s;
            m_connected = true;
            return true;
        }
        return false;
    }

    m_rawSocket = nullptr;
    m_hSession = WinHttpOpen(L"GhUtilRemote/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!m_hSession) return false;

    m_hConnect = WinHttpConnect(m_hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!m_hConnect) {
        WinHttpCloseHandle(m_hSession);
        m_hSession = nullptr;
        return false;
    }

    DWORD flags = useSsl ? WINHTTP_FLAG_SECURE : 0;
    m_hRequest = WinHttpOpenRequest(m_hConnect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!m_hRequest) {
        WinHttpCloseHandle(m_hConnect);
        WinHttpCloseHandle(m_hSession);
        m_hConnect = m_hSession = nullptr;
        return false;
    }

    if (useSsl) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(m_hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    DWORD timeout = 10000; // 10 s for handshake
    WinHttpSetOption(m_hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    // WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET takes no arguments (NULL, 0) per MSDN/samples.
    if (!WinHttpSetOption(m_hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0)) {
        WinHttpCloseHandle(m_hRequest);
        WinHttpCloseHandle(m_hConnect);
        WinHttpCloseHandle(m_hSession);
        m_hRequest = m_hConnect = m_hSession = nullptr;
        return false;
    }

    if (!WinHttpSendRequest(m_hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(m_hRequest);
        WinHttpCloseHandle(m_hConnect);
        WinHttpCloseHandle(m_hSession);
        m_hRequest = m_hConnect = m_hSession = nullptr;
        return false;
    }

    if (!WinHttpReceiveResponse(m_hRequest, nullptr)) {
        WinHttpCloseHandle(m_hRequest);
        WinHttpCloseHandle(m_hConnect);
        WinHttpCloseHandle(m_hSession);
        m_hRequest = m_hConnect = m_hSession = nullptr;
        return false;
    }

    DWORD status = 0;
    DWORD size = sizeof(status);
    if (!WinHttpQueryHeaders(m_hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
        WinHttpCloseHandle(m_hRequest);
        WinHttpCloseHandle(m_hConnect);
        WinHttpCloseHandle(m_hSession);
        m_hRequest = m_hConnect = m_hSession = nullptr;
        return false;
    }
    if (status != 101) {
        WinHttpCloseHandle(m_hRequest);
        WinHttpCloseHandle(m_hConnect);
        WinHttpCloseHandle(m_hSession);
        m_hRequest = m_hConnect = m_hSession = nullptr;
        return false;
    }

    m_hWebSocket = WinHttpWebSocketCompleteUpgrade(m_hRequest, 0);
    WinHttpCloseHandle(m_hRequest);
    m_hRequest = nullptr;
    if (!m_hWebSocket) {
        WinHttpCloseHandle(m_hConnect);
        WinHttpCloseHandle(m_hSession);
        m_hConnect = m_hSession = nullptr;
        return false;
    }

    m_connected = true;
    return true;
}

bool WebSocketClient::SendText(const std::string& text) {
    if (!m_connected) return false;
    if (m_rawSocket != nullptr) {
        SOCKET s = (SOCKET)(UINT_PTR)m_rawSocket;
        size_t len = text.size();
        std::vector<unsigned char> frame;
        frame.push_back(0x81);
        if (len < 126) {
            frame.push_back(0x80 | (unsigned char)len);
        } else if (len < 65536) {
            frame.push_back(0x80 | 126);
            frame.push_back((len >> 8) & 0xFF);
            frame.push_back(len & 0xFF);
        } else {
            frame.push_back(0x80 | 127);
            for (int i = 7; i >= 0; i--) frame.push_back((len >> (i * 8)) & 0xFF);
        }
        unsigned char mask[4];
        for (int i = 0; i < 4; i++) mask[i] = (unsigned char)(rand() % 256);
        for (int i = 0; i < 4; i++) frame.push_back(mask[i]);
        for (size_t i = 0; i < len; i++) frame.push_back((unsigned char)text[i] ^ mask[i % 4]);
        return send(s, (const char*)frame.data(), (int)frame.size(), 0) == (int)frame.size();
    }
    if (!m_hWebSocket) return false;
    DWORD err = WinHttpWebSocketSend(m_hWebSocket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (PVOID)text.data(), (DWORD)text.size());
    return (err == ERROR_SUCCESS);
}

void WebSocketClient::Disconnect() {
    m_stop = true;
    m_connected = false;
    if (m_rawSocket != nullptr) {
        closesocket((SOCKET)(UINT_PTR)m_rawSocket);
        m_rawSocket = nullptr;
    }
    if (m_hWebSocket) {
        WinHttpWebSocketClose(m_hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
        WinHttpCloseHandle(m_hWebSocket);
        m_hWebSocket = nullptr;
    }
    if (m_hConnect) { WinHttpCloseHandle(m_hConnect); m_hConnect = nullptr; }
    if (m_hSession) { WinHttpCloseHandle(m_hSession); m_hSession = nullptr; }
    if (m_thread) {
        WaitForSingleObject(m_thread, 5000);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
    m_stop = false;
}

void WebSocketClient::RunLoop() {
    // Optional: receive loop could be added here; for now we only send.
    (void)this;
}

DWORD WINAPI WebSocketClient::ThreadProc(LPVOID param) {
    auto* self = (WebSocketClient*)param;
    if (self) self->RunLoop();
    return 0;
}

} // namespace Remote
