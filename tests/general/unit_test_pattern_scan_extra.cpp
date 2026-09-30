// Extra tests for PatternScanner behavior
#include <deque>
#include <gtest/gtest.h>
#include <omath/utility/pattern_scan.hpp>

using namespace omath;

TEST(unit_test_pattern_scan_extra, IteratorScanFound)
{
    std::vector<std::byte> buf = {static_cast<std::byte>(0xDE), static_cast<std::byte>(0xAD),
                                  static_cast<std::byte>(0xBE), static_cast<std::byte>(0xEF),
                                  static_cast<std::byte>(0x00)};
    const auto it = PatternScanner::scan_for_pattern(buf.begin(), buf.end(), "DE AD BE EF");
    EXPECT_NE(it, buf.end());
    EXPECT_EQ(std::distance(buf.begin(), it), 0);
}

TEST(unit_test_pattern_scan_extra, ConstevalIteratorScan)
{
    std::vector<std::byte> buf = {static_cast<std::byte>(0x00), static_cast<std::byte>(0xDE),
                                  static_cast<std::byte>(0xAD), static_cast<std::byte>(0xBE),
                                  static_cast<std::byte>(0xEF), static_cast<std::byte>(0x11)};
    const auto it = PatternScanner::scan_for_pattern<"DE ?? BE EF">(buf.begin(), buf.end());
    EXPECT_NE(it, buf.end());
    EXPECT_EQ(std::distance(buf.begin(), it), 1);
}

TEST(unit_test_pattern_scan_extra, IteratorScanNotFound)
{
    std::vector<std::byte> buf = {static_cast<std::byte>(0x00), static_cast<std::byte>(0x11),
                                  static_cast<std::byte>(0x22)};
    const auto it = PatternScanner::scan_for_pattern(buf.begin(), buf.end(), "FF EE DD");
    EXPECT_EQ(it, buf.end());
}

TEST(unit_test_pattern_scan_extra, ParseInvalidPattern)
{
    // invalid hex token should cause the public scan to return end (no match)
    std::vector<std::byte> buf = {static_cast<std::byte>(0x00), static_cast<std::byte>(0x11)};
    const auto it = PatternScanner::scan_for_pattern(buf.begin(), buf.end(), "GG HH");
    EXPECT_EQ(it, buf.end());
}

TEST(unit_test_pattern_scan_extra, FindsMatchAtEveryOffset)
{
    // Long enough to go through several SIMD blocks and the positions left after them
    constexpr std::size_t buffer_size = 100;
    constexpr std::array signature = {static_cast<std::byte>(0xDE), static_cast<std::byte>(0xAD),
                                      static_cast<std::byte>(0x11), static_cast<std::byte>(0xEF)};

    for (std::size_t offset = 0; offset + signature.size() <= buffer_size; offset++)
    {
        std::vector<std::byte> buf(buffer_size);
        std::ranges::copy(signature, buf.begin() + static_cast<std::ptrdiff_t>(offset));

        const auto it = PatternScanner::scan_for_pattern(buf.begin(), buf.end(), "DE AD ? EF");
        EXPECT_EQ(std::distance(buf.begin(), it), static_cast<std::ptrdiff_t>(offset));
    }
}

TEST(unit_test_pattern_scan_extra, PartialMatchesAreSkipped)
{
    // Both ends of the pattern are everywhere in the buffer, only its middle tells the match apart
    std::vector<std::byte> buf(200, static_cast<std::byte>(0xAA));
    buf[150] = static_cast<std::byte>(0xBB);

    const auto it = PatternScanner::scan_for_pattern(buf.begin(), buf.end(), "AA AA BB AA");
    EXPECT_EQ(std::distance(buf.begin(), it), 148);
}

TEST(unit_test_pattern_scan_extra, SameFirstAndLastByte)
{
    // The buffer is a run of the value the pattern starts and ends with, only its middle tells the match apart
    std::vector<std::byte> buf(160, static_cast<std::byte>(0xDE));
    buf[66] = static_cast<std::byte>(0xAD);

    const auto it = PatternScanner::scan_for_pattern(buf.begin(), buf.end(), "DE ? AD ? DE");
    EXPECT_EQ(std::distance(buf.begin(), it), 64);
}

TEST(unit_test_pattern_scan_extra, ReturnsFirstOfSeveralMatches)
{
    std::vector<std::byte> buf(128);
    for (const std::size_t offset : {70, 72, 90})
    {
        buf[offset] = static_cast<std::byte>(0xDE);
        buf[offset + 1] = static_cast<std::byte>(0xAD);
    }
    std::span<std::byte> range{buf};

    const auto it = PatternScanner::scan_for_pattern(range, "DE AD");
    EXPECT_EQ(std::distance(range.begin(), it), 70);
}

TEST(unit_test_pattern_scan_extra, SurroundingWildcardsNeedRoom)
{
    std::vector<std::byte> buf(64);
    // One byte before it instead of two
    buf[1] = static_cast<std::byte>(0xDE);
    buf[2] = static_cast<std::byte>(0xAD);
    // No byte after it
    buf[62] = static_cast<std::byte>(0xDE);
    buf[63] = static_cast<std::byte>(0xAD);

    EXPECT_EQ(PatternScanner::scan_for_pattern(buf.begin(), buf.end(), "? ? DE AD ?"), buf.end());

    buf[40] = static_cast<std::byte>(0xDE);
    buf[41] = static_cast<std::byte>(0xAD);

    const auto it = PatternScanner::scan_for_pattern(buf.begin(), buf.end(), "? ? DE AD ?");
    EXPECT_EQ(std::distance(buf.begin(), it), 38);
}

TEST(unit_test_pattern_scan_extra, OnlyWildcards)
{
    std::vector<std::byte> buf(8);

    EXPECT_EQ(PatternScanner::scan_for_pattern(buf.begin(), buf.end(), "? ?? ?"), buf.begin());
    EXPECT_EQ(PatternScanner::scan_for_pattern(buf.begin(), buf.begin() + 2, "? ?? ?"), buf.begin() + 2);
}

TEST(unit_test_pattern_scan_extra, NonContiguousRange)
{
    std::deque<std::byte> buf(100);
    buf[77] = static_cast<std::byte>(0xDE);
    buf[79] = static_cast<std::byte>(0xEF);

    const auto it = PatternScanner::scan_for_pattern(buf.begin(), buf.end(), "DE ? EF");
    EXPECT_EQ(std::distance(buf.begin(), it), 77);
}
