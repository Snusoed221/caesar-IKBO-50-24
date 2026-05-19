// secure_copy.cpp
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctime>
#include <vector>
#include <string>

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

pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int copied_count = 0;
std::vector<std::string> files_to_copy;
std::string output_dir;
int next_file_index = 0;

void log_operation(const std::string& thread_id, const std::string& filename,
                   const std::string& status, double duration_sec) {
    pthread_mutex_lock(&log_mutex);
    FILE* log = fopen("log.txt", "a");
    if (log) {
        time_t now = time(nullptr);
        char time_buf[80];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

        fprintf(log, "[%s] Поток %s | Файл: %s | Результат: %s | Время: %.3f сек\n",
                time_buf, thread_id.c_str(), filename.c_str(), status.c_str(), duration_sec);
        fclose(log);
    }
    pthread_mutex_unlock(&log_mutex);
}

bool process_file(const std::string& input_path, const std::string& out_dir) {
    std::string filename = input_path.substr(input_path.find_last_of('/') + 1);
    if (filename.empty()) filename = input_path;

    std::string output_path = out_dir + "/" + filename;

    clock_t start = clock();

    FILE* fin = fopen(input_path.c_str(), "rb");
    if (!fin) {
        log_operation(std::to_string(pthread_self()), filename,
                     "ОШИБКА: входной файл не найден", 0.0);
        return false;
    }

    FILE* fout = fopen(output_path.c_str(), "wb");
    if (!fout) {
        fclose(fin);
        log_operation(std::to_string(pthread_self()), filename 
                     "ОШИБКА: не могу создать выходной файл", 0.0);
        return false;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    bool success = true;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fin)) > 0) {
        if (!keep_running) {
            success = false;
            break;
        }
        caesar(buffer, buffer, static_cast<int>(bytes_read));
        if (fwrite(buffer, 1, bytes_read, fout) != bytes_read) {
            success = false;
            break;
        }
    }

    if (ferror(fin)) {
        success = false;
    }

    fclose(fin);
    fclose(fout);

    double duration = static_cast<double>(clock() - start) / CLOCKS_PER_SEC;

    if (success) {
        log_operation(std::to_string(pthread_self()), filename, "УСПЕХ", duration);
        return true;
    } else {
        log_operation(std::to_string(pthread_self()), filename, "ОШИБКА чтения/записи", duration);
        return false;
    }
}

void* worker_thread(void* arg) {
    int thread_id = *(int*)arg;
    delete (int*)arg;

    while (keep_running) {
        int file_idx = -1;

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 5;

        if (pthread_mutex_timedlock(&counter_mutex, &ts) != 0) {
            std::cerr << "Возможная взаимоблокировка: поток " << thread_id
                      << " ожидает мьютекс более 5 секунд" << std::endl;
            continue;
        }

        if (next_file_index < (int)files_to_copy.size()) {
            file_idx = next_file_index++;
        }

        pthread_mutex_unlock(&counter_mutex);

        if (file_idx == -1) break;

        const std::string& input_file = files_to_copy[file_idx];

        if (process_file(input_file, output_dir)) {
            pthread_mutex_lock(&counter_mutex);
            copied_count++;
            pthread_mutex_unlock(&counter_mutex);
        }
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Использование: ./secure_copy file1 [file2 ...] output_dir/ key\n";
        return 1;
    }

    int key = atoi(argv[argc-1]) & 0xFF;
    set_key(static_cast<char>(key));

    output_dir = argv[argc-2];

    mkdir(output_dir.c_str(), 0755);

    for (int i = 1; i < argc - 2; ++i) {
        files_to_copy.push_back(argv[i]);
    }

    if (files_to_copy.empty()) {
        std::cerr << "Нет входных файлов!\n";
        return 1;
    }

    std::cout << "Запуск: " << files_to_copy.size() << " файлов → " << output_dir 
              << " (ключ=" << key << ")" << std::endl;

    signal(SIGINT, sigint_handler);

    const int NUM_THREADS = 3;
    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; ++i) {
        int* tid = new int(i + 1);
        pthread_create(&threads[i], nullptr, worker_thread, tid);
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], nullptr);
    }

    std::cout << "\nГотово! Успешно обработано: " << copied_count 
              << " из " << files_to_copy.size() << " файлов." << std::endl;

    pthread_mutex_destroy(&counter_mutex);
    pthread_mutex_destroy(&log_mutex);

    return 0;
}        size_t bytes = fread(temp, 1, BUFFER_SIZE, sh->in);
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
