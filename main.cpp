#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
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
    auto it = std::copy(original.begin(), original.end(), result.begin());
    std::copy(injection.begin(), injection.end(), it);
    const uint32_t baseCrc32 = crc32(result.data(), result.size() - 4);

    /*
     * Внимание: код ниже крайне не оптимален.
     * В качестве доп. задания устраните избыточные вычисления
     */
    const size_t threadsNumber = std::max<size_t>(1, std::thread::hardware_concurrency());
    // а вдруг выдастся 0? потом делить на ноль нехорошо
    const size_t maxVal = std::numeric_limits<uint32_t>::max();
    const size_t chunkSize = maxVal / threadsNumber;
    std::vector<std::thread> threads;
    std::optional<uint32_t> found;     // сюда запишем найденное значение четырех байт
    std::atomic<bool> isFound{false};  // флаг, что решение уже найдено
    std::mutex mutex;

    for (size_t t = 0; t < threadsNumber; ++t) {
        threads.emplace_back([t, chunkSize, threadsNumber, maxVal, originalCrc32, &found, &isFound,
                              &mutex, baseCrc32]() {
            const size_t start = t * chunkSize;
            const size_t end = (t == threadsNumber - 1)
                                   ? maxVal + 1  // верхняя граница исключена, поэтому +1
                                   : start + chunkSize;

            for (size_t i = start; i < end; ++i) {
                //  Проверяем, не найдено ли решение в другой нити
                if (isFound.load(std::memory_order_relaxed)) {
                    break;
                }
                std::array<char, 4> tail{0}; // в crc32 нужно передавать char*
                std::memcpy(tail.data(), &i, 4);
                // Вычисляем CRC32 текущего вектора с учетом базового CRC32
                auto currentCrc32 = crc32(tail.data(), 4, ~baseCrc32);

                if (currentCrc32 == originalCrc32) {
                    std::cout << "Success\n";
                    if (!isFound.exchange(
                            true)) {  // гарантируем, что только один поток обновит result
                        std::lock_guard<std::mutex> lock(mutex);
                        found = static_cast<uint32_t>(i);
                    }
                    break;
                }
                // Отображаем прогресс
                if (i % 1000000 == 0) {
                    std::cout << "Thread " << t << " progress: "
                              << static_cast<double>(i - start) / static_cast<double>(end - start)
                              << std::endl;
                }
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    if (isFound && found) {
        replaceLastFourBytes(result, *found);
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

    // const char* d =
    // "01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123";
    // const uint32_t v = crc32(d, 100);
    // std::cout << "v = crc32(d, 100) - " << v << " (" << std::hex << v << std::dec << ")\n";
    // const uint32_t q1 = crc32(d, 104);
    // std::cout << "q1 = crc32(d, 104) - " << q1 << " (" << std::hex << q1 << std::dec << ")\n";
    // const uint32_t q2 = crc32(d + 100, 4, ~v);
    // std::cout << "q2 = crc32(d + 100, 4, ~v) - " << q2 << " (" << std::hex << q2 << std::dec <<
    // ")\n";

    return 0;
}
