#pragma once

#include <string>
#include <vector>

// AES-256-CBC encrypt/decrypt using Windows BCrypt. Used by Viewer encrypter dialog.
namespace ConfigCrypto {
    bool EncryptToDat(const std::string& plaintext, std::vector<unsigned char>& outCiphertext);
    bool DecryptFromDat(const std::vector<unsigned char>& ciphertext, std::string& outPlaintext);
}
