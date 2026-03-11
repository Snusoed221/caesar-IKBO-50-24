CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -fPIC -O2 -pthread

all: libcaesar.so secure_copy

# Библиотека из Задания 1
libcaesar.so: caesar.o
	$(CXX) -shared -o $@ $^

caesar.o: caesar.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Новая программа Задания 2
secure_copy: secure_copy.o
	$(CXX) -o $@ $< -L. -lcaesar -Wl,-rpath=. -pthread

secure_copy.o: secure_copy.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

install:
	sudo cp libcaesar.so /usr/local/lib/
	sudo ldconfig

test: all
	python3 test.py ./libcaesar.so 42 input.txt output.txt
	@echo "Проверка двойного XOR"
	python3 test.py ./libcaesar.so 42 output.txt output2.txt
	diff -q input.txt output2.txt && echo "УСПЕШНО!" || echo "ОШИБКА"

clean:
	rm -f *.o *.so secure_copy output*.txt big.bin

.PHONY: all install test clean
