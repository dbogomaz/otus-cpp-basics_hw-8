#include <algorithm>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "CRC32.hpp"
#include "IO.hpp"
#include "timer.h"

/// @brief Переписывает последние 4 байта значением value
void replaceLastFourBytes(std::vector<char>& data, uint32_t value) {
    std::copy_n(reinterpret_cast<const char*>(&value), 4, data.end() - 4);
}

void printBytes(const std::vector<char>& data, const char* name) {
    std::cout << name << ": \n";
    for (char c : data) {
        std::cout << '\t' << std::hex << (static_cast<uint32_t>(c) & 0xFF) << '\n';
    }
    std::cout << std::dec << '\n';
}

/**
 * @brief Формирует новый вектор с тем же CRC32, добавляя в конец оригинального
 * строку injection и дополнительные 4 байта
 * @details При формировании нового вектора последние 4 байта не несут полезной
 * нагрузки и подбираются таким образом, чтобы CRC32 нового и оригинального
 * вектора совпадали
 * @param original оригинальный вектор
 * @param injection произвольная строка, которая будет добавлена после данных
 * оригинального вектора
 * @return новый вектор
 */
std::vector<char> hack(const std::vector<char>& original, const std::string& injection) {
    const uint32_t originalCrc32 = crc32(original.data(), original.size());
    std::vector<char> result(original.size() + injection.size() + 4, 0);
    printBytes(original, "original");
    auto it = std::copy(original.begin(), original.end(), result.begin());
    std::copy(injection.begin(), injection.end(), it);
    const uint32_t baseCrc32 = crc32(result.data(), result.size() - 4);
    std::vector<char> tail(4, 0);
    std::copy_n(result.end() - 4, 4, tail.begin());

    /*
     * Внимание: код ниже крайне не оптимален.
     * В качестве доп. задания устраните избыточные вычисления
     */
    const size_t threadsNumber = std::max<size_t>(1, std::thread::hardware_concurrency());
    // а вдруг выдастся 0? потом делить на ноль нехорошо
    const size_t maxVal = std::numeric_limits<uint32_t>::max();
    const size_t chunkSize = maxVal / threadsNumber;
    std::vector<std::thread> threads;
    std::optional<std::vector<char>> found;  // сюда запишем найденный вектор
    bool isFound{false};                     // флаг, что решение уже найдено
    std::mutex mutex;
    // std::mutex isFoundMutex;

    for (size_t t = 0; t < threadsNumber; ++t) {
        threads.emplace_back([t, chunkSize, threadsNumber, maxVal, &result, originalCrc32, &found,
                              &isFound, &mutex, baseCrc32, &tail]() {
            std::vector<char> local = tail; // каждый поток работает с собственной копией
            const size_t start = t * chunkSize;
            const size_t end = (t == threadsNumber - 1)
                                   ? maxVal + 1  // верхняя граница исключена, поэтому +1
                                   : start + chunkSize;

            for (size_t i = start; i < end; ++i) {
                {
                    // std::lock_guard<std::mutex> lock(isFoundMutex);
                    //  тормозит, превращая в один поток
                    // узкое место, хотя вряд ли потоки одновременно найдут решение
                    //  Проверяем, не найдено ли решение в другой нити
                    if (isFound) {
                        break;
                    }
                }
                // Заменяем последние четыре байта на значение i
                replaceLastFourBytes(local, uint32_t(i));
                // Вычисляем CRC32 текущего вектора result
                auto currentCrc32 = crc32(local.data(), 4, baseCrc32);

                if (currentCrc32 == originalCrc32) {
                    std::cout << "Success\n";
                    std::lock_guard<std::mutex> lock(mutex);  // защита записи в found
                    // Запоминаем найденный вектор
                    found = local;
                    isFound = true;
                    break;
                }
                // Отображаем прогресс
                //if (i % 1000000 == 0) {
                //    std::cout << "Thread " << t << " progress: "
                //              << static_cast<double>(i - start) / static_cast<double>(end - start)
                //              << std::endl;
                //}
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    if (isFound && found.has_value()) {
        replaceLastFourBytes(result, *found->data() | (*(found->data() + 1) << 8) |
                                              (*(found->data() + 2) << 16) |
                                              (*(found->data() + 3) << 24));
        return result;
    } else {
        throw std::logic_error("Can't hack");
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Call with two args: " << argv[0] << " <input file> <output file>\n";
        return 1;
    }

    try {
        Timer timer;
        const std::vector<char> data = readFromFile(argv[1]);
        const std::vector<char> badData = hack(data, "He-he-he");
        writeToFile(argv[2], badData);
    } catch (std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 2;
    }
    return 0;
}
