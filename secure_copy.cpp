// secure_copy.cpp
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

extern "C" {
    void set_key(char key);
    void caesar(void* src, void* dst, int len);
}

volatile sig_atomic_t keep_running = 1;

void sigint_handler(int) {
    keep_running = 0;
    std::cout << "\nОперация прервана пользователем" << std::endl;
}

const int BUFFER_SIZE = 8192;

struct SharedData {
    FILE* in;
    FILE* out;
    char buffer[BUFFER_SIZE];
    int buf_len;
    bool eof;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};

void* producer(void* arg) {
    auto* sh = static_cast<SharedData*>(arg);
    char temp[BUFFER_SIZE];

    while (keep_running) {
        pthread_mutex_lock(&sh->mutex);
        while (sh->buf_len > 0 && keep_running) {
            pthread_cond_wait(&sh->not_full, &sh->mutex);
        }
        if (!keep_running) {
            pthread_mutex_unlock(&sh->mutex);
            return nullptr;
        }
        pthread_mutex_unlock(&sh->mutex);

        size_t bytes = fread(temp, 1, BUFFER_SIZE, sh->in);
        if (bytes == 0) {
            pthread_mutex_lock(&sh->mutex);
            sh->eof = true;
            pthread_cond_signal(&sh->not_empty);
            pthread_mutex_unlock(&sh->mutex);
            return nullptr;
        }

        caesar(temp, temp, static_cast<int>(bytes));  // шифруем

        pthread_mutex_lock(&sh->mutex);
        memcpy(sh->buffer, temp, bytes);
        sh->buf_len = static_cast<int>(bytes);
        pthread_cond_signal(&sh->not_empty);
        pthread_mutex_unlock(&sh->mutex);
    }
    return nullptr;
}

void* consumer(void* arg) {
    auto* sh = static_cast<SharedData*>(arg);
    char temp[BUFFER_SIZE];

    while (keep_running) {
        pthread_mutex_lock(&sh->mutex);
        while (sh->buf_len == 0 && !sh->eof && keep_running) {
            pthread_cond_wait(&sh->not_empty, &sh->mutex);
        }
        if (sh->buf_len == 0 && sh->eof) {
            pthread_mutex_unlock(&sh->mutex);
            return nullptr;
        }
        if (!keep_running) {
            pthread_mutex_unlock(&sh->mutex);
            return nullptr;
        }

        int len = sh->buf_len;
        memcpy(temp, sh->buffer, len);
        sh->buf_len = 0;
        pthread_cond_signal(&sh->not_full);
        pthread_mutex_unlock(&sh->mutex);

        fwrite(temp, 1, len, sh->out);
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Использование: ./secure_copy <input> <output> <key>\n";
        return 1;
    }

    const char* input  = argv[1];
    const char* output = argv[2];
    int key = atoi(argv[3]) & 0xFF;

    if (access(input, R_OK) != 0) {
        std::cerr << "Ошибка: входной файл не найден!\n";
        return 1;
    }

    set_key(static_cast<char>(key));

    FILE* fin = fopen(input, "rb");
    FILE* fout = fopen(output, "wb");
    if (!fin || !fout) {
        std::cerr << "Ошибка открытия файлов!\n";
        return 1;
    }

    SharedData sh{};
    sh.in = fin;
    sh.out = fout;
    sh.buf_len = 0;
    sh.eof = false;

    pthread_mutex_init(&sh.mutex, nullptr);
    pthread_cond_init(&sh.not_empty, nullptr);
    pthread_cond_init(&sh.not_full, nullptr);

    signal(SIGINT, sigint_handler);

    pthread_t prod, cons;
    pthread_create(&prod, nullptr, producer, &sh);
    pthread_create(&cons, nullptr, consumer, &sh);

    pthread_join(prod, nullptr);
    pthread_join(cons, nullptr);

    pthread_mutex_destroy(&sh.mutex);
    pthread_cond_destroy(&sh.not_empty);
    pthread_cond_destroy(&sh.not_full);

    fclose(fin);
    fclose(fout);

    if (keep_running) {
        std::cout << "Готово! Файл успешно зашифрован/скопирован.\n";
    }
    return 0;
}
