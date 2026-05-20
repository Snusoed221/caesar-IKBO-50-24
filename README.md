Задание 4

make clean
make
or i in {1..10}; do      echo "Тестовый файл номер $i для демонстрации Задания 4" > file$i.txt; done
./secure_copy file*.txt my_output_par/ 44 --mode=parallel
./secure_copy file*.txt my_output_seq/ 44 --mode=sequential
