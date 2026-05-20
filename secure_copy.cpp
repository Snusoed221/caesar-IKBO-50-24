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
const int MAX_WORKERS = 4;

// ====================== Статистика ======================
struct Stats {
    double total_time = 0.0;
    double avg_time_per_file = 0.0;
    int files_processed = 0;
    std::string mode;
};

// ====================== Глобальные ресурсы ======================
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

std::vector<std::string> files_to_copy;
std::string output_dir;
int next_file_index = 0;
int copied_count = 0;

// ====================== Логирование ======================
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

// ====================== Обработка одного файла ======================
bool process_file(const std::string& input_path, const std::string& out_dir) {
    std::string filename = input_path.substr(input_path.find_last_of('/') + 1);
    if (filename.empty()) filename = input_path;

    std::string output_path = out_dir + "/" + filename;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    FILE* fin = fopen(input_path.c_str(), "rb");
    if (!fin) {
        log_operation(std::to_string(pthread_self()), filename, "ОШИБКА открытия входного", 0.0);
        return false;
    }

    FILE* fout = fopen(output_path.c_str(), "wb");
    if (!fout) {
        fclose(fin);
        log_operation(std::to_string(pthread_self()), filename, "ОШИБКА создания выходного", 0.0);
        return false;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes;
    bool success = true;

    while ((bytes = fread(buffer, 1, BUFFER_SIZE, fin)) > 0) {
        if (!keep_running) { 
            success = false; 
            break; 
        }
        caesar(buffer, buffer, static_cast<int>(bytes));
        if (fwrite(buffer, 1, bytes, fout) != bytes) {
            success = false;
            break;
        }
    }

    fclose(fin);
    fclose(fout);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

    if (success) {
        log_operation(std::to_string(pthread_self()), filename, "УСПЕХ", duration);
        return true;
    } else {
        log_operation(std::to_string(pthread_self()), filename, "ОШИБКА", duration);
        return false;
    }
}

// ====================== Рабочий поток ======================
void* worker_thread(void* arg) {
    (void)arg; // убираем предупреждение

    while (keep_running) {
        int file_idx = -1;

        pthread_mutex_lock(&queue_mutex);
        if (next_file_index < (int)files_to_copy.size()) {
            file_idx = next_file_index++;
        }
        pthread_mutex_unlock(&queue_mutex);

        if (file_idx == -1) break;

        const std::string& input_file = files_to_copy[file_idx];

        if (process_file(input_file, output_dir)) {
            pthread_mutex_lock(&queue_mutex);
            copied_count++;
            pthread_mutex_unlock(&queue_mutex);
        }
    }
    return nullptr;
}

// ====================== Последовательный режим ======================
Stats run_sequential() {
    Stats stats;
    stats.mode = "SEQUENTIAL";
    struct timespec start_total, end_total;
    clock_gettime(CLOCK_MONOTONIC, &start_total);

    copied_count = 0;
    for (const auto& file : files_to_copy) {
        if (process_file(file, output_dir)) {
            copied_count++;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_total);
    stats.total_time = (end_total.tv_sec - start_total.tv_sec) + 
                       (end_total.tv_nsec - start_total.tv_nsec) * 1e-9;
    stats.files_processed = copied_count;
    if (stats.files_processed > 0)
        stats.avg_time_per_file = stats.total_time / stats.files_processed;

    return stats;
}

// ====================== Параллельный режим ======================
Stats run_parallel() {
    Stats stats;
    stats.mode = "PARALLEL";
    struct timespec start_total, end_total;
    clock_gettime(CLOCK_MONOTONIC, &start_total);

    copied_count = 0;
    next_file_index = 0;

    pthread_t workers[MAX_WORKERS];
    for (int i = 0; i < MAX_WORKERS; ++i) {
        pthread_create(&workers[i], nullptr, worker_thread, nullptr);
    }

    for (int i = 0; i < MAX_WORKERS; ++i) {
        pthread_join(workers[i], nullptr);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_total);
    stats.total_time = (end_total.tv_sec - start_total.tv_sec) + 
                       (end_total.tv_nsec - start_total.tv_nsec) * 1e-9;
    stats.files_processed = copied_count;
    if (stats.files_processed > 0)
        stats.avg_time_per_file = stats.total_time / stats.files_processed;

    return stats;
}

// ====================== main ======================
int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Использование: ./secure_copy file1 [file2 ...] output_dir/ key [--mode=sequential|parallel]\n";
        return 1;
    }

    std::string mode_arg = "auto";
    int key_pos = argc - 1;

    if (argc > 1 && std::string(argv[argc-1]).rfind("--mode=", 0) == 0) {
        mode_arg = argv[argc-1];
        key_pos = argc - 2;
    }

    int key = atoi(argv[key_pos]) & 0xFF;
    set_key(static_cast<char>(key));

    output_dir = argv[key_pos - 1];
    mkdir(output_dir.c_str(), 0755);

    files_to_copy.clear();
    for (int i = 1; i < key_pos - 1; ++i) {
        files_to_copy.emplace_back(argv[i]);
    }

    if (files_to_copy.empty()) {
        std::cerr << "Нет входных файлов!\n";
        return 1;
    }

    std::cout << "Запуск: " << files_to_copy.size() << " файлов → " << output_dir 
              << " (ключ=" << key << ")" << std::endl;

    signal(SIGINT, sigint_handler);

    Stats stats;

    if (mode_arg == "auto") {
        if (files_to_copy.size() < 5) {
            mode_arg = "--mode=sequential";
            stats = run_sequential();
        } else {
            mode_arg = "--mode=parallel";
            stats = run_parallel();
        }
    } else if (mode_arg == "--mode=sequential") {
        stats = run_sequential();
    } else if (mode_arg == "--mode=parallel") {
        stats = run_parallel();
    }

    // Вывод статистики
    std::cout << "\n=== Статистика (" << mode_arg << ") ===\n";
    std::cout << "Общее время:        " << std::fixed << std::setprecision(3) 
              << stats.total_time << " сек\n";
    std::cout << "Файлов обработано:  " << stats.files_processed << " / " 
              << files_to_copy.size() << "\n";
    if (stats.files_processed > 0)
        std::cout << "Среднее время/файл: " << std::fixed << std::setprecision(3) 
                  << stats.avg_time_per_file << " сек\n";

    std::cout << "\nГотово!\n";
    return 0;
}
