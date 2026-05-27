#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>

static void* key_mem = nullptr;
static const size_t KEY_SIZE = 16;

// Обработчик SIGSEGV
static void segv_handler(int sig, siginfo_t* info, void* ctx) {
    std::cerr << "\n[БЕЗОПАСНОСТЬ] Обнаружена попытка записи в защищённую память (SIGSEGV)!" << std::endl;
    std::cerr << "Адрес нарушения: " << info->si_addr << std::endl;
    _exit(1);  // Немедленный выход с ошибкой
}

extern "C" {

    void set_key(const char* key_str) {
        if (key_mem) {
            munmap(key_mem, KEY_SIZE);
        }

        // Выделяем защищённую память
        key_mem = mmap(NULL, KEY_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (key_mem == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }

        // Записываем ключ (16 байт)
        memset(key_mem, 0, KEY_SIZE);
        size_t len = strlen(key_str);
        if (len > KEY_SIZE) len = KEY_SIZE;
        memcpy(key_mem, key_str, len);

        // Устанавливаем защиту только на чтение
        if (mprotect(key_mem, KEY_SIZE, PROT_READ) == -1) {
            perror("mprotect");
            munmap(key_mem, KEY_SIZE);
            exit(1);
        }
    }

    void caesar(void* src, void* dst, int len) {
        if (!key_mem) return;

        uint8_t* source = static_cast<uint8_t*>(src);
        uint8_t* dest   = static_cast<uint8_t*>(dst);

        // Временно снимаем защиту для копирования ключа
        if (mprotect(key_mem, KEY_SIZE, PROT_READ | PROT_WRITE) == -1) {
            perror("mprotect temporary");
            return;
        }

        uint8_t local_key = *(static_cast<uint8_t*>(key_mem));

        // Возвращаем защиту
        mprotect(key_mem, KEY_SIZE, PROT_READ);

        // Шифрование
        for (int i = 0; i < len; ++i) {
            dest[i] = source[i] ^ local_key;
        }
    }

    // Функция очистки (вызывается при завершении)
    void cleanup_key() {
        if (key_mem) {
            mprotect(key_mem, KEY_SIZE, PROT_READ | PROT_WRITE);
            memset(key_mem, 0, KEY_SIZE);
            munmap(key_mem, KEY_SIZE);
            key_mem = nullptr;
        }
    }
}
