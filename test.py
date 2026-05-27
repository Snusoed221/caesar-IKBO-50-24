import sys
import ctypes

if len(sys.argv) != 5:
    print("Использование: python3 test.py <путь_к_библиотеке.so> <ключ> <input.txt> <output.txt>")
    print("Пример:        python3 test.py ./libcaesar.so 42 input.txt output.txt")
    sys.exit(1)

# Аргументы
lib_path    = sys.argv[1]
key         = int(sys.argv[2]) & 0xFF
input_file  = sys.argv[3]
output_file = sys.argv[4]

# Загружаем библиотеку
lib = ctypes.CDLL(lib_path)

# Настраиваем типы функций
lib.set_key.argtypes = [ctypes.c_char]
lib.set_key.restype  = None

lib.caesar.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
lib.caesar.restype  = None

# Читаем файл
with open(input_file, "rb") as f:
    data = f.read()

print(f"Прочитано {len(data)} байт из {input_file}")

# Подготавливаем буферы
input_buf  = bytearray(data)
output_buf = bytearray(len(data))

# Устанавливаем ключ
lib.set_key(ctypes.c_char(key))

# Создаем ctypes массивы из bytearray
input_array = (ctypes.c_char * len(data)).from_buffer(input_buf)
output_array = (ctypes.c_char * len(data)).from_buffer(output_buf)

# Шифруем
lib.caesar(ctypes.addressof(input_array),
           ctypes.addressof(output_array),
           ctypes.c_int(len(data)))

# Сохраняем результат
with open(output_file, "wb") as f:
    f.write(output_buf)

print(f"Готово → {output_file}")
