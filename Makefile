CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++17 -pthread -O2 -fPIC

all: libcaesar.so secure_copy

# ==================== Старый Caesar (нужен для совместимости) ====================
libcaesar.so: caesar.o
	$(CXX) -shared -o $@ $^

caesar.o: caesar.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ==================== RC4 + Новый secure_copy ====================
rc4.o: rc4.cpp rc4.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

secure_copy.o: secure_copy.cpp rc4.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

secure_copy: secure_copy.o rc4.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -L. -lcaesar -Wl,-rpath=.

clean:
	rm -f *.o *.so secure_copy disk.img log.txt *.txt output* 2>/dev/null || true

.PHONY: all clean
