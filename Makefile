# Makefile для задания 1

CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -fPIC -O2

all: libcaesar.so

libcaesar.so: caesar.o
	$(CXX) -shared -o $@ $^

caesar.o: caesar.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

install:
	sudo cp libcaesar.so /usr/local/lib/
	sudo ldconfig

test: all
	python3 test.py ./libcaesar.so 42 input.txt output.txt
	@echo "=== Проверка двойного XOR (должен вернуться оригинал) ==="
	python3 test.py ./libcaesar.so 42 output.txt output2.txt
	diff -q input.txt output2.txt && echo "УСПЕШНО: оригинал восстановлен" || echo "ОШИБКА!"

clean:
	rm -f *.o *.so output.txt output2.txt
