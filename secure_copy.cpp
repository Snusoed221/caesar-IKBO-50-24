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
#include <iomanip>
#include <sys/mman.h>
#include <cstdint>

static void segv_handler(int sig, siginfo_t* info, void* ctx) {
    std::cerr << "\n[БЕЗОПАСНОСТЬ] Критическая ошибка: попытка записи в защищённую память (SIGSEGV)!" << std::endl;
    std::cerr << "   Адрес нарушения: " << info->si_addr << std::endl;
    std::cerr << "   Доступ к ключу шифрования запрещён!" << std::endl;
    _exit(1);
}

extern "C" {
    void set_key(const char* key_str);
    void caesar(void* src, void* dst, int len);
    void cleanup_key();
}

volatile sig_atomic_t keep_running = 1;

void sigint_handler(int) {
    keep_running = 0;
    std::cout << "\nОперация прервана пользователем" << std::endl;
}

const int BUFFER_SIZE = 8192;
const int MAX_WORKERS = 4;

struct Stats {
    double total_time = 0.0;
    double avg_time_per_file = 0.0;
    int files_processed = 0;
    std::string mode;
};

std::vector<std::string> files_to_copy;
std::string output_dir;
int next_file_index = 0;
int copied_count = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void process_file(const std::string& input_path, const std::string& output_path) {
    FILE* in = fopen(input_path.c_str(), "rb");
    if (!in) {
        std::cerr << "Ошибка открытия: " << input_path << std::endl;
        return;
    }

    FILE* out = fopen(output_path.c_str(), "wb");
    if (!out) {
        std::cerr << "Ошибка создания: " << output_path << std::endl;
        fclose(in);
        return;
    }

    char buffer[BUFFER_SIZE];
    char out_buffer[BUFFER_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, in)) > 0) {
        caesar(buffer, out_buffer, bytes_read);
        fwrite(out_buffer, 1, bytes_read, out);
    }

    fclose(in);
    fclose(out);
}

void* worker_thread(void* arg) {
    std::string thread_id = "T" + std::to_string(reinterpret_cast<uintptr_t>(arg));

    while (keep_running) {
        std::string file_path;
        bool has_file = false;

        pthread_mutex_lock(&queue_mutex);
        if (next_file_index < static_cast<int>(files_to_copy.size())) {
            file_path = files_to_copy[next_file_index];
            next_file_index++;
            has_file = true;
        }
        pthread_mutex_unlock(&queue_mutex);

        if (!has_file) break;

        std::string output_path = output_dir + "/" + 
            file_path.substr(file_path.find_last_of('/') + 1);

        clock_t start = clock();
        process_file(file_path, output_path);
        double duration = (clock() - start) / (double)CLOCKS_PER_SEC;

        log_operation(thread_id, file_path, "OK", duration);

        pthread_mutex_lock(&queue_mutex);
        copied_count++;
        pthread_mutex_unlock(&queue_mutex);
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Использование: " << argv[0] 
                  << " <input_file1> [file2 ...] <output_dir> <key>" << std::endl;
        return 1;
    }

    // === Задание 5: Установка обработчика SIGSEGV ===
    struct sigaction sa = {};
    sa.sa_sigaction = segv_handler;
    sa.sa_flags = SA_SIGINFO;
    if (sigaction(SIGSEGV, &sa, nullptr) == -1) {
        perror("sigaction");
        return 1;
    }

    signal(SIGINT, sigint_handler);

    // Парсинг аргументов
    int key_pos = argc - 1;
    output_dir = argv[argc - 2];

    // Установка защищённого ключа (Задание 5)
    set_key(argv[key_pos]);

    // Сбор файлов
    files_to_copy.clear();
    for (int i = 1; i < argc - 2; ++i) {
        files_to_copy.push_back(argv[i]);
    }

    mkdir(output_dir.c_str(), 0755);

    std::cout << "Запущено " << files_to_copy.size() 
              << " файлов с защищённым ключом.\n";

    // Запуск обработки
    pthread_t threads[MAX_WORKERS];
    for (int i = 0; i < MAX_WORKERS; ++i) {
        pthread_create(&threads[i], nullptr, worker_thread, (void*)(intptr_t)i);
    }

    for (int i = 0; i < MAX_WORKERS; ++i) {
        pthread_join(threads[i], nullptr);
    }

    cleanup_key();  // Важно! Затираем ключ

    std::cout << "\nГотово! Обработано файлов: " << copied_count << std::endl;
    std::cout << "Ключ успешно очищен из защищённой памяти.\n";

    return 0;
}
