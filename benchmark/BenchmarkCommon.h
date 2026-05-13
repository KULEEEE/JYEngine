#pragma once
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>

using uint32 = uint32_t;
using uint8  = uint8_t;

static constexpr int    REPEAT_COUNT   = 20;
static constexpr uint32 OBJECT_COUNTS[] = { 1000, 10000, 50000, 100000 };

struct GatherResult
{
    float  x, y, z;
    uint32 materialID;
    uint32 meshID;
};

struct BenchmarkRecord
{
    std::string label;
    uint32      objectCount;
    double      avgMs;
};

template<typename Fn>
double MeasureMs(Fn&& fn)
{
    fn(); // warm-up

    double total = 0.0;
    for (int i = 0; i < REPEAT_COUNT; ++i)
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        fn();
        auto t1 = std::chrono::high_resolution_clock::now();
        total += std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
    return total / REPEAT_COUNT;
}

inline void PrintResults(const std::vector<BenchmarkRecord>& records)
{
    std::cout << "\n"
              << std::left << std::setw(28) << "benchmark"
              << std::setw(12) << "objects"
              << "avg_ms\n"
              << std::string(52, '-') << "\n";

    for (const auto& r : records)
    {
        std::cout << std::left << std::setw(28) << r.label
                  << std::setw(12) << r.objectCount
                  << std::fixed << std::setprecision(4) << r.avgMs << "\n";
    }
}

inline void WriteCSV(const std::string& path, const std::vector<BenchmarkRecord>& records)
{
    std::ofstream f(path);
    f << "benchmark,object_count,avg_ms\n";
    for (const auto& r : records)
        f << r.label << "," << r.objectCount << ","
          << std::fixed << std::setprecision(4) << r.avgMs << "\n";

    std::cout << "\nCSV -> " << path << "\n";
}
