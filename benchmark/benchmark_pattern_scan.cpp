#include <algorithm>
#include <benchmark/benchmark.h>
#include <omath/utility/pattern_scan.hpp>
#include <random>
#include <vector>
using namespace omath;

namespace
{
    constexpr std::size_t buffer_size = 16 * 1024 * 1024;

    // None of the signatures below is in its buffer, so every iteration goes through all of it
    void scan_whole_buffer(benchmark::State& state, const std::vector<std::byte>& buffer,
                           const std::string_view& pattern)
    {
        for ([[maybe_unused]] const auto _ : state)
            benchmark::DoNotOptimize(PatternScanner::scan_for_pattern(buffer.begin(), buffer.end(), pattern));

        state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(buffer.size()));
    }

    // The two bytes of the signature the scanner looks for first rarely line up, which rules out nearly every position
    void pattern_scan_rare_candidates(benchmark::State& state)
    {
        std::vector<std::byte> buffer(buffer_size);
        std::mt19937 generator{1337}; // NOLINT(*-msc51-cpp)
        std::ranges::generate(buffer,
                              [&generator]
                              {
                                  return static_cast<std::byte>(generator() & 0xFF);
                              });

        scan_whole_buffer(state, buffer, "48 8B 05 ? ? ? ? 48 85 C0 74 ? DE AD BE EF");
    }

    // A signature made of one repeated value in a buffer nearly full of it: those two bytes line up almost
    // everywhere, so the whole signature is compared at almost every position. This is the slowest a scan can get.
    void pattern_scan_dense_candidates(benchmark::State& state)
    {
        std::vector buffer(buffer_size, static_cast<std::byte>(0xCC));
        for (std::size_t i = 7; i < buffer.size(); i += 8)
            buffer[i] = static_cast<std::byte>(0x00);

        scan_whole_buffer(state, buffer, "CC CC CC CC CC CC CC CC CC");
    }
} // namespace

BENCHMARK(pattern_scan_rare_candidates);
BENCHMARK(pattern_scan_dense_candidates);
