#!/bin/bash
# Скрипт создает входной файл и запускает программу

cd build
echo "Test data" > input.txt
./crc32_crack input.txt output.txt
cat output.txt
cd ..