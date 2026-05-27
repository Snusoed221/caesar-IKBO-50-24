// rc4.cpp
#include "rc4.h"
#include <cstring>

void RC4::init(const std::vector<uint8_t>& key) {
    for (int k = 0; k < 256; ++k) S[k] = k;
    uint8_t j = 0;
    for (int k = 0; k < 256; ++k) {
        j = (j + S[k] + key[k % key.size()]) & 255;
        std::swap(S[k], S[j]);
    }
}

uint8_t RC4::generate() {
    i = (i + 1) & 255;
    j = (j + S[i]) & 255;
    std::swap(S[i], S[j]);
    return S[(S[i] + S[j]) & 255];
}

RC4::RC4(const std::string& master_key, const std::string& salt) {
    std::vector<uint8_t> full_key;
    full_key.reserve(master_key.size() + salt.size());
    full_key.insert(full_key.end(), master_key.begin(), master_key.end());
    full_key.insert(full_key.end(), salt.begin(), salt.end());
    init(full_key);
}

void RC4::crypt(void* data, size_t len) {
    uint8_t* p = static_cast<uint8_t*>(data);
    for (size_t k = 0; k < len; ++k) {
        p[k] ^= generate();
    }
}
