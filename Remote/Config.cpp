#include "pch.h"
#include "Config.h"
#include "..\ConfigCryptoKey.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace Remote {

static void LogDbg(const char* msg) {
    OutputDebugStringA("[Remote] ");
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}

static std::wstring GetDllDirectory(HMODULE hModule) {
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(hModule, path, MAX_PATH) == 0) return {};
    std::wstring p(path);
    size_t last = p.find_last_of(L"\\/");
    if (last != std::wstring::npos) p.resize(last + 1);
    return p;
}

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

static bool DecryptConfigDat(const unsigned char* data, size_t size, std::string& outPlain) {
    outPlain.clear();
    if (!data || size == 0) return false;
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return false;
    if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)ConfigCrypto::AES_KEY, sizeof(ConfigCrypto::AES_KEY), 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    ULONG plainLen = 0;
    UCHAR ivBuf[16];
    memcpy(ivBuf, ConfigCrypto::AES_IV, 16);
    NTSTATUS st = BCryptDecrypt(hKey, (PUCHAR)data, (ULONG)size, nullptr, ivBuf, 16, nullptr, 0, &plainLen, BCRYPT_BLOCK_PADDING);
    if (st != 0 || plainLen == 0) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    outPlain.resize(plainLen);
    memcpy(ivBuf, ConfigCrypto::AES_IV, 16);
    st = BCryptDecrypt(hKey, (PUCHAR)data, (ULONG)size, nullptr, ivBuf, 16, (PUCHAR)&outPlain[0], plainLen, &plainLen, BCRYPT_BLOCK_PADDING);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return (st == 0);
}

ClientConfig LoadConfig() {
    ClientConfig cfg;
    try {
        HMODULE hMod = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&LoadConfig, &hMod) == 0) {
            LogDbg("LoadConfig: GetModuleHandleEx failed");
            return cfg;
        }
        std::wstring dir = GetDllDirectory(hMod);
        if (dir.empty()) {
            LogDbg("LoadConfig: GetDllDirectory failed");
            return cfg;
        }
        std::string json;
        std::wstring pathDat = dir + L"config.dat";
        std::ifstream fDat(pathDat, std::ios::binary);
        if (fDat) {
            fDat.seekg(0, std::ios::end);
            size_t size = (size_t)fDat.tellg();
            fDat.seekg(0);
            if (size > 0 && size < 1024 * 1024) {
                std::vector<unsigned char> buf(size);
                fDat.read((char*)buf.data(), size);
                fDat.close();
                if (DecryptConfigDat(buf.data(), size, json) && !json.empty()) {
                    cfg.serverUrl = ExtractServerUrl(json);
                    cfg.valid = !cfg.serverUrl.empty();
                    return cfg;
                }
            }
            fDat.close();
        }
        std::wstring pathJson = dir + L"config.json";
        std::ifstream fJson(pathJson);
        if (!fJson) {
            LogDbg("LoadConfig: no config.dat or config.json found");
            return cfg;
        }
        std::stringstream ss;
        ss << fJson.rdbuf();
        json = ss.str();
        cfg.serverUrl = ExtractServerUrl(json);
        cfg.valid = !cfg.serverUrl.empty();
    } catch (const std::exception& e) {
        OutputDebugStringA("[Remote] LoadConfig exception: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
    } catch (...) {
        LogDbg("LoadConfig: unknown exception");
    }
    return cfg;
}

} // namespace Remote
