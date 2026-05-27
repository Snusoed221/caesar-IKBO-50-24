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
#include <fstream>
#include <filesystem>
#include <random>
#include <algorithm>
#include <chrono>
#include "rc4.h"

namespace fs = std::filesystem;

static void segv_handler(int sig, siginfo_t* info, void* ctx) {
    std::cerr << "\n[БЕЗОПАСНОСТЬ] Критическая ошибка: попытка записи в защищённую память (SIGSEGV)!" << std::endl;
    std::cerr << "   Адрес нарушения: " << info->si_addr << std::endl;
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
        char time_buf[26];
        ctime_r(&now, time_buf);
        time_buf[24] = '\0';
        fprintf(log, "[%s] %s | %s | %.3f сек | %s\n", 
                time_buf, thread_id.c_str(), filename.c_str(), duration_sec, status.c_str());
        fclose(log);
    }
    pthread_mutex_unlock(&log_mutex);
}

void* worker_function(void* arg) {
    int thread_id = *(int*)arg;
    std::string tid = "Thread-" + std::to_string(thread_id);

    while (true) {
        pthread_mutex_lock(&queue_mutex);
        if (next_file_index >= (int)files_to_copy.size() || !keep_running) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        std::string file = files_to_copy[next_file_index++];
        pthread_mutex_unlock(&queue_mutex);

        std::string src = file;
        std::string dst = output_dir + "/" + fs::path(file).filename().string();

        auto start = std::chrono::high_resolution_clock::now();

        std::ifstream in(src, std::ios::binary);
        std::ofstream out(dst, std::ios::binary);
        if (!in || !out) {
            log_operation(tid, file, "ERROR", 0);
            continue;
        }

        std::vector<char> buffer(BUFFER_SIZE);
        while (in.read(buffer.data(), buffer.size()) || in.gcount() > 0) {
            int bytes = in.gcount();
            caesar(buffer.data(), buffer.data(), bytes);
            out.write(buffer.data(), bytes);
        }

        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();

        log_operation(tid, file, "OK", duration);

        pthread_mutex_lock(&queue_mutex);
        copied_count++;
        pthread_mutex_unlock(&queue_mutex);
    }
    return nullptr;
}

// ====================== 6 задание ======================
struct FileEntry {
    uint32_t file_size;
    uint32_t name_len;
    uint8_t  salt[16];
};

bool add_to_image(const std::string& image_path, const std::string& master_key,
                  const std::vector<std::string>& paths) {
    std::ofstream img(image_path, std::ios::binary | std::ios::app);
    if (!img.is_open()) {
        img.open(image_path, std::ios::binary);
        if (!img.is_open()) {
            std::cerr << "Ошибка создания образа: " << image_path << std::endl;
            return false;
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (const auto& p : paths) {
        fs::path base = p;
        if (!fs::exists(base)) {
            std::cerr << "Не найден: " << p << std::endl;
            continue;
        }

        if (fs::is_directory(base)) {
            std::string dir_name = base.filename().string();  // "test_dir"

            for (auto& entry : fs::recursive_directory_iterator(base)) {
                if (!entry.is_regular_file()) continue;

                // Правильное формирование пути: test_dir/...
                fs::path rel = fs::relative(entry.path(), base);
                std::string name = dir_name + "/" + rel.string();

                std::ifstream fin(entry.path(), std::ios::binary);
                std::vector<uint8_t> content((std::istreambuf_iterator<char>(fin)),
                                             std::istreambuf_iterator<char>());

                std::string salt(16, 0);
                for (char& c : salt) c = static_cast<char>(dis(gen));

                RC4 rc4(master_key, salt);
                rc4.crypt(content.data(), content.size());

                uint32_t fsize = content.size();
                uint32_t nlen = name.size();

                img.write(reinterpret_cast<const char*>(&fsize), 4);
                img.write(reinterpret_cast<const char*>(&nlen), 4);
                img.write(salt.data(), 16);
                img.write(name.data(), nlen);
                img.write(reinterpret_cast<const char*>(content.data()), fsize);
            }
        } 
        else if (fs::is_regular_file(base)) {
            std::string name = base.filename().string();

            std::ifstream fin(base, std::ios::binary);
            std::vector<uint8_t> content((std::istreambuf_iterator<char>(fin)),
                                         std::istreambuf_iterator<char>());

            std::string salt(16, 0);
            for (char& c : salt) c = static_cast<char>(dis(gen));

            RC4 rc4(master_key, salt);
            rc4.crypt(content.data(), content.size());

            uint32_t fsize = content.size();
            uint32_t nlen = name.size();

            img.write(reinterpret_cast<const char*>(&fsize), 4);
            img.write(reinterpret_cast<const char*>(&nlen), 4);
            img.write(salt.data(), 16);
            img.write(name.data(), nlen);
            img.write(reinterpret_cast<const char*>(content.data()), fsize);
        }
    }
    return true;
}

void list_image(const std::string& image_path) {
    std::ifstream img(image_path, std::ios::binary);
    if (!img.is_open()) {
        std::cerr << "Образ не найден: " << image_path << std::endl;
        return;
    }

    std::vector<std::pair<std::string, uint32_t>> files;
    while (true) {
        FileEntry e{};
        if (!img.read(reinterpret_cast<char*>(&e), sizeof(FileEntry))) break;

        std::string name(e.name_len, '\0');
        if (!img.read(&name[0], e.name_len)) break;

        files.emplace_back(name, e.file_size);
        img.seekg(e.file_size, std::ios::cur);
    }

    std::sort(files.begin(), files.end());
    for (const auto& p : files) {
        std::cout << p.first << " (" << p.second << " байт)\n";
    }
}

bool extract_file(const std::string& image_path, const std::string& master_key,
                  const std::string& filename, const std::string& out_path) {
    std::ifstream img(image_path, std::ios::binary);
    if (!img.is_open()) {
        std::cerr << "Образ не найден\n";
        return false;
    }

    while (true) {
        FileEntry e{};
        if (!img.read(reinterpret_cast<char*>(&e), sizeof(FileEntry))) break;

        std::string name(e.name_len, '\0');
        if (!img.read(&name[0], e.name_len)) break;

        if (name == filename) {
            std::vector<uint8_t> content(e.file_size);
            if (img.read(reinterpret_cast<char*>(content.data()), e.file_size)) {
                std::string salt(reinterpret_cast<char*>(e.salt), 16);
                RC4 rc4(master_key, salt);
                rc4.crypt(content.data(), content.size());

                std::ofstream out(out_path, std::ios::binary);
                out.write(reinterpret_cast<const char*>(content.data()), content.size());
                std::cout << "Файл успешно извлечён: " << out_path << std::endl;
                return true;
            }
        } else {
            img.seekg(e.file_size, std::ios::cur);
        }
    }
    std::cerr << "Файл не найден: " << filename << std::endl;
    return false;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Использование:\n"
                  << "  ./secure_copy -add -key \"secret\" -image disk.img <files|dirs...>\n"
                  << "  ./secure_copy -list -image disk.img\n"
                  << "  ./secure_copy -get -key \"secret\" -image disk.img -out result.txt file_name\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string image, key, out_path, target_file;
    std::vector<std::string> paths;

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-image") == 0 && i+1 < argc) image = argv[++i];
        else if (strcmp(argv[i], "-key") == 0 && i+1 < argc) key = argv[++i];
        else if (strcmp(argv[i], "-out") == 0 && i+1 < argc) out_path = argv[++i];
        else if (argv[i][0] != '-') paths.push_back(argv[i]);
    }

    if (mode == "-add") {
        if (image.empty() || key.empty() || paths.empty()) {
            std::cerr << "Ошибка: укажите -image, -key и файлы/директории\n";
            return 1;
        }
        add_to_image(image, key, paths);
    }
    else if (mode == "-list") {
        if (image.empty()) {
            std::cerr << "Укажите -image\n";
            return 1;
        }
        list_image(image);
    }
    else if (mode == "-get") {
        if (image.empty() || key.empty() || out_path.empty() || paths.empty()) {
            std::cerr << "Ошибка: недостаточно параметров для -get\n";
            return 1;
        }
        target_file = paths.back();
        extract_file(image, key, target_file, out_path);
    }
    else {
        std::cout << "Неизвестный режим. Используйте -add, -list или -get.\n";
    }

    return 0;
}
