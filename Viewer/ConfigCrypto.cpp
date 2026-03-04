#include "pch.h"
#include "ConfigCrypto.h"
#include "..\ConfigCryptoKey.h"
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace ConfigCrypto {

bool EncryptToDat(const std::string& plaintext, std::vector<unsigned char>& outCiphertext) {
    outCiphertext.clear();
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return false;
    if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)AES_KEY, sizeof(AES_KEY), 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    ULONG cipherLen = 0;
    UCHAR ivBuf[16];
    memcpy(ivBuf, AES_IV, 16);
    NTSTATUS st = BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), (ULONG)plaintext.size(), nullptr, ivBuf, 16, nullptr, 0, &cipherLen, BCRYPT_BLOCK_PADDING);
    if (st != 0 || cipherLen == 0) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    outCiphertext.resize(cipherLen);
    memcpy(ivBuf, AES_IV, 16);
    st = BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), (ULONG)plaintext.size(), nullptr, ivBuf, 16, outCiphertext.data(), cipherLen, &cipherLen, BCRYPT_BLOCK_PADDING);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return (st == 0);
}

bool DecryptFromDat(const std::vector<unsigned char>& ciphertext, std::string& outPlaintext) {
    outPlaintext.clear();
    if (ciphertext.empty()) return false;
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return false;
    if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)AES_KEY, sizeof(AES_KEY), 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    ULONG plainLen = 0;
    UCHAR ivBuf[16];
    memcpy(ivBuf, AES_IV, 16);
    NTSTATUS st = BCryptDecrypt(hKey, (PUCHAR)ciphertext.data(), (ULONG)ciphertext.size(), nullptr, ivBuf, 16, nullptr, 0, &plainLen, BCRYPT_BLOCK_PADDING);
    if (st != 0 || plainLen == 0) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    outPlaintext.resize(plainLen);
    memcpy(ivBuf, AES_IV, 16);
    st = BCryptDecrypt(hKey, (PUCHAR)ciphertext.data(), (ULONG)ciphertext.size(), nullptr, ivBuf, 16, (PUCHAR)&outPlaintext[0], plainLen, &plainLen, BCRYPT_BLOCK_PADDING);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return (st == 0);
}

} // namespace ConfigCrypto
