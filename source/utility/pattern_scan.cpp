//
// Created by Vlad on 10/4/2025.
//
#include "omath/utility/pattern_scan.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdint>

// Number of positions a single SIMD step can rule out, not defined when the target has neither AVX2 nor SSE2
#if defined(OMATH_USE_AVX2) && (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86))
#define OMATH_PATTERN_SCAN_BLOCK_SIZE 32
#include <immintrin.h>
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define OMATH_PATTERN_SCAN_BLOCK_SIZE 16
#include <emmintrin.h>
#endif

namespace
{
    [[nodiscard]]
    constexpr bool is_wildcard(const std::string_view& byte_str)
    {
        return byte_str == "?" || byte_str == "??";
    }

    [[nodiscard]]
    constexpr bool invalid_byte_str_size(const std::string_view& byte_str)
    {
        return byte_str.empty() || byte_str.size() >= 3;
    }

#if defined(OMATH_PATTERN_SCAN_BLOCK_SIZE)
    // Bit N of the result is set when byte N of first_block is first_value and byte N of second_block is second_value
    [[nodiscard]]
    std::uint32_t find_value_pairs(const std::byte* first_block, const std::byte first_value,
                                   const std::byte* second_block, const std::byte second_value)
    {
#if OMATH_PATTERN_SCAN_BLOCK_SIZE == 32
        const __m256i first_equal = _mm256_cmpeq_epi8(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(first_block)),
                                                      _mm256_set1_epi8(static_cast<char>(first_value)));
        const __m256i second_equal =
                _mm256_cmpeq_epi8(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(second_block)),
                                  _mm256_set1_epi8(static_cast<char>(second_value)));

        return static_cast<std::uint32_t>(_mm256_movemask_epi8(_mm256_and_si256(first_equal, second_equal)));
#else
        const __m128i first_equal = _mm_cmpeq_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(first_block)),
                                                   _mm_set1_epi8(static_cast<char>(first_value)));
        const __m128i second_equal = _mm_cmpeq_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(second_block)),
                                                    _mm_set1_epi8(static_cast<char>(second_value)));

        return static_cast<std::uint32_t>(_mm_movemask_epi8(_mm_and_si128(first_equal, second_equal)));
#endif
    }
#endif
} // namespace

namespace omath
{

    std::span<std::byte>::iterator
    PatternScanner::scan_for_pattern(const std::span<std::byte>& range, const std::string_view& pattern)
    {
        return scan_for_pattern(range.begin(), range.end(), pattern);
    }
    const std::byte*
    PatternScanner::scan_bytes_for_parsed_pattern(const std::byte* begin, const std::byte* end,
                                                  const std::span<const std::optional<std::byte>> pattern)
    {
        if (static_cast<std::size_t>(end - begin) < pattern.size())
            return end;

        const auto is_byte = [](const std::optional<std::byte>& element)
        {
            return element.has_value();
        };
        const auto first_byte = std::ranges::find_if(pattern, is_byte);

        // Nothing but wildcards (or nothing at all) matches right away
        if (first_byte == pattern.end())
            return begin;

        // Wildcards around the pattern only limit where a match may start, so the comparison skips them and works
        // with what is left: a body that starts and ends with a byte
        const auto body_offset = static_cast<std::size_t>(first_byte - pattern.begin());
        const std::span body{first_byte, std::find_if(pattern.rbegin(), pattern.rend(), is_byte).base()};

        const auto body_is_at = [&body](const std::byte* position)
        {
            return std::equal(body.begin(), body.end(), position,
                              [](const std::optional<std::byte>& expected, const std::byte actual)
                              {
                                  return !expected || expected == actual;
                              });
        };

        const std::byte* position = begin + body_offset;
        const std::byte* const last_position = end - (pattern.size() - body_offset);

#if defined(OMATH_PATTERN_SCAN_BLOCK_SIZE)
        // Most positions are ruled out by two bytes of the body alone, a whole block of positions at a time: its first
        // byte and the last one holding another value. Were both the same value, every position inside a run of it
        // (padding, zero fill) would get past this check.
        std::size_t probe_offset = body.size() - 1;
        while (probe_offset != 0 && (!body[probe_offset] || body[probe_offset] == body.front()))
            probe_offset--;

        // Every byte of the body holds the same value
        if (probe_offset == 0)
            probe_offset = body.size() - 1;

        for (; last_position - position >= OMATH_PATTERN_SCAN_BLOCK_SIZE - 1; position += OMATH_PATTERN_SCAN_BLOCK_SIZE)
        {
            auto candidates = find_value_pairs(position, *body.front(), position + probe_offset, *body[probe_offset]);

            // The whole body is compared only where both of them match
            for (; candidates != 0; candidates &= candidates - 1)
            {
                const std::byte* candidate = position + std::countr_zero(candidates);

                if (body_is_at(candidate))
                    return candidate - body_offset;
            }
        }
#endif
        // Fewer positions than a block holds are left, reading a whole block here would run past the end
        for (; position <= last_position; position++)
            if (body_is_at(position))
                return position - body_offset;

        return end;
    }
    std::expected<std::vector<std::optional<std::byte>>, PatternScanError>
    PatternScanner::parse_pattern(const std::string_view& pattern_string)
    {
        std::vector<std::optional<std::byte>> pattern;

        auto start = pattern_string.cbegin();

        while (start != pattern_string.cend())
        {
            const auto end = std::ranges::find(start, pattern_string.cend(), ' ');

            const auto sting_view_start = std::distance(pattern_string.cbegin(), start);
            const auto sting_view_end = std::distance(start, end);

            const std::string_view byte_str = pattern_string.substr(sting_view_start, sting_view_end);

            if (invalid_byte_str_size(byte_str)) [[unlikely]]
            {
                start = end != pattern_string.end() ? std::next(end) : end;
                continue;
            }

            if (is_wildcard(byte_str))
            {
                pattern.emplace_back(std::nullopt);

                start = end != pattern_string.end() ? std::next(end) : end;
                continue;
            }

            std::uint8_t value = 0;
            // ReSharper disable once CppTooWideScopeInitStatement
            const auto [_, error_code] = std::from_chars(byte_str.data(), byte_str.data() + byte_str.size(), value, 16);

            if (error_code != std::errc{}) [[unlikely]]
                return std::unexpected(PatternScanError::INVALID_PATTERN_STRING);

            pattern.emplace_back(static_cast<std::byte>(value));

            start = end != pattern_string.end() ? std::next(end) : end;
        }
        return pattern;
    }
} // namespace omath