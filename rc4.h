// rc4.h
#ifndef RC4_H
#define RC4_H

#include <cstdint>
#include <vector>
#include <string>

class RC4 {
private:
    uint8_t S[256];
    uint8_t i = 0, j = 0;

    void init(const std::vector<uint8_t>& key);
    uint8_t generate();

public:
    RC4(const std::string& master_key, const std::string& salt);
    void crypt(void* data, size_t len);
};

#endif
