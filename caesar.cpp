#include <cstdint>

// Глобальная статическая переменная — ключ хранится внутри библиотеки
static char encryption_key = 0;

extern "C" {

    void set_key(char key) {
        encryption_key = key;
    }

    void caesar(void* src, void* dst, int len) {
        // Приводим void* к указателю на байты
        uint8_t* source = static_cast<uint8_t*>(src);
        uint8_t* dest   = static_cast<uint8_t*>(dst);

        // Побайтовый XOR с сохранённым ключом
        for (int i = 0; i < len; ++i) {
            dest[i] = source[i] ^ static_cast<uint8_t>(encryption_key);
        }
    }

}
