# `omath::PatternScanner` — Fast byte-pattern search with wildcards

> Header: `omath/utility/pattern_scan.hpp`
> Namespace: `omath`
> Core API: `scan_for_pattern(...)` (span or iterators)
> Errors: `PatternScanError::INVALID_PATTERN_STRING` (from `parse_pattern`)
> C++: uses `std::span`, `std::expected`, `std::byte`; the compile-time overload needs C++20 class-type NTTPs

`PatternScanner` scans a contiguous byte range for a **hex pattern** that may include **wildcards**. It returns an iterator to the **first match** or the end iterator if not found (or if the pattern string is invalid).

---

## Quick start

```cpp
#include <omath/utility/pattern_scan.hpp>
using omath::PatternScanner;

std::vector<std::byte> buf = /* ... bytes ... */;
std::span<std::byte>    s{buf.data(), buf.size()};

// Example pattern: "48 8B ?? ?? 89" (hex bytes with '?' as any byte)
auto it = PatternScanner::scan_for_pattern(s, "48 8B ?? ?? 89");
if (it != s.end()) {
  // Found at offset:
  auto offset = static_cast<std::size_t>(it - s.begin());
}
```

Or with iterators:

```cpp
auto it = PatternScanner::scan_for_pattern(buf.begin(), buf.end(), "DE AD BE EF");
if (it != buf.end()) { /* ... */ }
```

Or with a pattern checked and parsed at compile time:

```cpp
auto it = PatternScanner::scan_for_pattern<"48 8B ?? ?? 89">(buf.begin(), buf.end());
```

---

## Pattern string grammar

The pattern string is parsed into a sequence of byte **tokens**:

* **Hex byte**: hexadecimal digits form one byte.
  Examples: `90`, `4F`, `00`, `ff`
* **Wildcard byte**: `?` or `??` matches **any single byte**.
* **Separators**: the runtime parser splits on the **space** character only. The compile-time overload also accepts tab, newline and carriage return.

> Valid: `"48 8B ?? 05 00"`, `"90 90 90"`, `"??"`

Runtime string (`std::string_view`) behavior:

* A token that is not a valid hex number (1 or 2 characters) makes `parse_pattern()` return `std::unexpected(PatternScanError::INVALID_PATTERN_STRING)`, and the public scan function returns **end**.
* Empty tokens (repeated spaces) and tokens of 3 or more characters are **silently skipped**, not rejected. A single hex digit is accepted as one byte (`"9"` is `0x09`).

Compile-time overload (`scan_for_pattern<"...">`): every token must be exactly two hex digits, `?` or `??`; anything else is a compile-time error.

---

## API

```cpp
namespace omath {

enum class PatternScanError { INVALID_PATTERN_STRING };

class PatternScanner final {
public:
  // Contiguous range (span) overload
  [[nodiscard]]
  static std::span<std::byte>::iterator
  scan_for_pattern(const std::span<std::byte>& range,
                   const std::string_view& pattern);

  // Deleted rvalue-span overload (prevents dangling)
  static std::span<std::byte>::iterator
  scan_for_pattern(std::span<std::byte>&&,
                   const std::string_view&) = delete;

  // Iterator overload
  template<class IteratorType>
  requires std::input_or_output_iterator<std::remove_cvref_t<IteratorType>>
  static IteratorType
  scan_for_pattern(const IteratorType& begin,
                   const IteratorType& end,
                   const std::string_view& pattern);

  // Compile-time pattern overload: the pattern is parsed (and validated) at compile time
  template<ConstevalPattern Pattern, class IteratorType>
  requires std::input_or_output_iterator<std::remove_cvref_t<IteratorType>>
  static IteratorType
  scan_for_pattern(const IteratorType& begin, const IteratorType& end);

  // Wrapper that lets a string literal be a template argument
  template<std::size_t Length>
  struct ConstevalPattern {
    char value[Length]{};
    constexpr ConstevalPattern(const char (&text)[Length]);
    constexpr operator std::string_view() const noexcept;
  };
private:
  [[nodiscard]]
  static std::expected<std::vector<std::optional<std::byte>>, PatternScanError>
  parse_pattern(const std::string_view& pattern_string);
};

} // namespace omath
```

### Return value

* On success: iterator to the **first** matching position.
* On failure / not found / invalid pattern: **`end`**.

---

## Complexity

Let `N` be the number of bytes in the range and `M` the number of **pattern tokens**.

* Time: **O(N × M)** in the worst case. Contiguous byte ranges are filtered 32 positions at a time with AVX2 (`OMATH_USE_AVX2`) or 16 at a time with SSE2, and the whole pattern is compared only where two of its bytes both match, so a typical scan is close to **O(N)**. Other iterators use a simple sliding window with early break.
* Space: **O(M)** for the parsed pattern vector.

---

## Examples

### Find a function prologue

```cpp
// x86-64: push rbp; mov rbp, rsp
auto it = PatternScanner::scan_for_pattern(s, "55 48 89 E5");
if (it != s.end()) {
  // ... process
}
```

### Skip variable bytes with wildcards

```cpp
// mov rax, <imm32>; call <rel32>
auto it = PatternScanner::scan_for_pattern(s, "48 B8 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ??");
```

### Iterator-based scan (subrange)

```cpp
auto sub_begin = buf.begin() + 1024;
auto sub_end   = buf.begin() + 4096;
auto it = PatternScanner::scan_for_pattern(sub_begin, sub_end, "DE AD ?? BE EF");
```

---

## Behavior & edge cases

* **Too-short range**: If the parsed pattern has more tokens than the range has bytes (`M > N`), the function returns `end`.
* **Empty / all-wildcard pattern**: A pattern with no hex byte (empty, or only wildcards) matches immediately at `begin` when the range is at least `M` bytes long.
* **Wildcards**: Each `?`/`??` token matches **exactly one** byte.
* **No exceptions**: Invalid pattern → parsed as `unexpected`, public API returns `end` (the compile-time overload rejects bad patterns at compile time instead).
* **No ownership**: The span overload takes `const std::span<std::byte>&` and returns a **mutable iterator** into that span. You must ensure the underlying memory stays alive. The rvalue-span overload is **deleted** to prevent dangling.

---

## Notes & caveats (implementation-sensitive)

* Although the iterator overloads are constrained with `std::input_or_output_iterator`, the implementation uses `*(begin + i + j)` and arithmetic on iterators. In practice this means you should pass **random-access / contiguous iterators** (e.g., from `std::vector<std::byte>` or `std::span<std::byte>`). Using non-random-access iterators would be ill-formed.
* Contiguous iterators over `std::byte` take a vectorized path (AVX2 when built with `OMATH_USE_AVX2`, otherwise SSE2 where available); other iterator types use the plain sliding window.
* The matching compares a parsed token (`std::optional<std::byte>`) to the candidate byte; `std::nullopt` is treated as a **wildcard** (always matches).
* The scan bound is derived from the number of **parsed tokens**, not from the length of the pattern string.

---

## See also

* [`PePatternScanner`](pe_pattern_scan.md) — applies the same patterns to PE sections

---

## Testing hooks

The class befriends several unit tests:

```
unit_test_pattern_scan_read_test_Test
unit_test_pattern_scan_corner_case_1_Test
unit_test_pattern_scan_corner_case_2_Test
unit_test_pattern_scan_corner_case_3_Test
unit_test_pattern_scan_corner_case_4_Test
unit_test_pattern_scan_consteval_read_test_Test
unit_test_pattern_scan_consteval_spacing_and_case_Test
```

Use these to validate parsing correctness, wildcard handling, and boundary conditions.

---

## Troubleshooting

* **Always returns end**: verify the pattern string is valid hex/wildcards (remember that only the space character separates tokens at runtime) and that you’re scanning the intended subrange. Try a simpler pattern (e.g., a single known byte) to sanity-check.
* **Crashes or compile errors with iterator overload**: use iterators that support random access (e.g., from `std::vector`), or prefer the `std::span` overload.
* **Ambiguous wildcards**: this scanner treats `?` and `??` as **byte-wide** wildcards (not per-nibble). If you need nibble-level masks, extend `parse_pattern` to support patterns like `A?`/`?F` with bitmask matching.

---

## Minimal unit test sketch

```cpp
TEST(pattern, basic) {
  std::array<std::byte, 8> data{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x01}, std::byte{0x02},
    std::byte{0x89}, std::byte{0x50}, std::byte{0x90}, std::byte{0xCC}
  };
  std::span<std::byte> s{data.data(), data.size()};
  auto it = omath::PatternScanner::scan_for_pattern(s, "48 8B ?? ?? 89");
  ASSERT_NE(it, s.end());
  EXPECT_EQ(static_cast<size_t>(it - s.begin()), 0U);
}
```

---

*Last updated: 31 Oct 2025*
