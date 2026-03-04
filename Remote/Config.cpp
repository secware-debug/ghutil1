#include "pch.h"
#include "Config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Remote {

static std::string Narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring GetDllDirectory(HMODULE hModule) {
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(hModule, path, MAX_PATH) == 0) return {};
    std::wstring p(path);
    size_t last = p.find_last_of(L"\\/");
    if (last != std::wstring::npos) p.resize(last + 1);
    return p;
}

// Simple JSON value extraction: find "server_url" and return quoted string value.
static std::wstring ExtractServerUrl(const std::string& json) {
    const char key[] = "\"server_url\"";
    auto pos = json.find(key);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + sizeof(key) - 1);
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos);
    if (pos == std::string::npos) return {};
    size_t start = pos + 1;
    size_t end = json.find('"', start);
    if (end == std::string::npos) return {};
    std::string value = json.substr(start, end - start);
    std::wstring result;
    result.resize(MultiByteToWideChar(CP_UTF8, 0, value.c_str(), (int)value.size(), nullptr, 0));
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), (int)value.size(), &result[0], (int)result.size());
    return result;
}

ClientConfig LoadConfig() {
    ClientConfig cfg;
    HMODULE hMod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&LoadConfig, &hMod) == 0)
        return cfg;
    std::wstring dir = GetDllDirectory(hMod);
    if (dir.empty()) return cfg;
    std::wstring path = dir + L"config.json";
    std::ifstream f(path);
    if (!f) return cfg;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();
    cfg.serverUrl = ExtractServerUrl(json);
    cfg.valid = !cfg.serverUrl.empty();
    return cfg;
}

} // namespace Remote
