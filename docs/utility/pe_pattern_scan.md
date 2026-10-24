# `omath::PePatternScanner` — Scan PE images for byte patterns

> Header: `omath/utility/pe_pattern_scan.hpp`
> Namespace: `omath`
> Platform: **PE (Portable Executable) images**. The loaded-module scan needs a PE image mapped in this process; the file and memory-file scans parse the PE headers themselves and use no Windows API
> Depends on: `omath/utility/pattern_scan.hpp`, `omath/utility/section_scan_result.hpp`, `<filesystem>`, `<optional>`, `<cstdint>`, `<span>`
> Companion: works well with `omath::PatternScanner` (same pattern grammar)

`PePatternScanner` searches **Portable Executable (PE)** binaries for a hex pattern (with wildcards). You can scan:

* a **loaded module** in the current process, or
* a **PE file on disk**, or
* a **PE file already read into memory** (`std::span<const std::byte>`),

each by section name (defaults to **`.text`**).

---

## Pattern string grammar (same as `PatternScanner`)

* **Hex byte**: two hex digits → one byte (`90`, `4F`, `00`, `ff`).
* **Wildcard byte**: `?` or `??` matches **any byte**.
* **Whitespace**: tokens are separated by spaces. See [`PatternScanner`](pattern_scan.md) for the exact rules (runtime string versus compile-time pattern).

Valid: `"48 8B ?? ?? 89"`, `"55 8B EC"`, `"??"`

---

## API

```cpp
namespace omath {

// omath/utility/section_scan_result.hpp
struct SectionScanResult {
  std::uintptr_t virtual_base_addr;  // ImageBase + SectionRVA of the scanned section
  std::uintptr_t raw_base_addr;      // file offset of the start of the section
  std::ptrdiff_t target_offset;      // offset from section base to the first matched byte
};

class PePatternScanner final {
public:
  // Scan a module already loaded in *this* process.
  // module_base_address: HMODULE / ImageBase (e.g., from GetModuleHandle)
  // Returns absolute address (process VA) of the first match, or nullopt.
  [[nodiscard]] static std::optional<std::uintptr_t>
  scan_for_pattern_in_loaded_module(const void* module_base_address,
                                    const std::string_view& pattern,
                                    const std::string_view& target_section_name = ".text");

  // Scan a PE file on disk, by section name (default ".text").
  [[nodiscard]] static std::optional<SectionScanResult>
  scan_for_pattern_in_file(const std::filesystem::path& path_to_file,
                           const std::string_view& pattern,
                           const std::string_view& target_section_name = ".text");

  // Scan a PE file that is already in memory (raw file layout, not a mapped image).
  [[nodiscard]] static std::optional<SectionScanResult>
  scan_for_pattern_in_memory_file(std::span<const std::byte> file_data,
                                  const std::string_view& pattern,
                                  const std::string_view& target_section_name = ".text");

  // Compile-time pattern variants of all three (pattern given as a template argument)
  template<PatternScanner::ConstevalPattern Pattern>
  [[nodiscard]] static std::optional<std::uintptr_t>
  scan_for_pattern_in_loaded_module(const void* module_base_address,
                                    const std::string_view& target_section_name = ".text");

  template<PatternScanner::ConstevalPattern Pattern>
  [[nodiscard]] static std::optional<SectionScanResult>
  scan_for_pattern_in_file(const std::filesystem::path& path_to_file,
                           const std::string_view& target_section_name = ".text");

  template<PatternScanner::ConstevalPattern Pattern>
  [[nodiscard]] static std::optional<SectionScanResult>
  scan_for_pattern_in_memory_file(std::span<const std::byte> file_data,
                                  const std::string_view& target_section_name = ".text");
};

} // namespace omath
```

The `SectionScanResult` type is shared with the ELF and Mach-O scanners; see [SectionScanResult](section_scan_result.md).

---

## Return values

* **Loaded module**: `std::optional<std::uintptr_t>`

    * `value()` = **process virtual address** of the first match (the section's address in memory plus the match offset). Every section whose name equals `target_section_name` (and has raw data) is scanned in header order.
    * `nullopt` = no match or parse/PE error.

* **File / memory-file scan**: `std::optional<SectionScanResult>`

    * `virtual_base_addr` = **ImageBase + SectionRVA** of the scanned section (its address if mapped at the preferred base).
    * `raw_base_addr` = **file offset** of section start.
    * `target_offset` = offset from the section base to the **first matched byte**.
    * To get addresses:

        * **Virtual address (at the preferred ImageBase)** of hit = `virtual_base_addr + target_offset`
        * **Raw file offset** of hit = `raw_base_addr + target_offset`

---

## Usage examples

### Scan a loaded module (current process)

```cpp
#include <windows.h>
#include <omath/utility/pe_pattern_scan.hpp>

using omath::PePatternScanner;

auto hMod = ::GetModuleHandleW(L"kernel32.dll");
if (hMod) {
  auto addr = PePatternScanner::scan_for_pattern_in_loaded_module(
      hMod, "48 8B ?? ?? 89" // hex + wildcards
  );
  if (addr) {
    // Use the absolute process VA:
    std::uintptr_t hit_va = *addr;
    // ...
  }
}
```

### Scan a PE file on disk (default section “.text”)

```cpp
#include <omath/utility/pe_pattern_scan.hpp>
using omath::PePatternScanner;

auto res = PePatternScanner::scan_for_pattern_in_file(
    R"(C:\Windows\System32\kernel32.dll)", "55 8B EC"
);
if (res) {
  auto va_hit   = res->virtual_base_addr + res->target_offset;
  auto raw_hit  = res->raw_base_addr     + res->target_offset;
  // va_hit: where it would be in memory at the preferred base; raw_hit: file offset
}
```

### Scan another section (e.g., “.rdata”)

```cpp
auto res = PePatternScanner::scan_for_pattern_in_file(
  "foo.dll", "48 8D 0D ?? ?? ?? ??", ".rdata"
);
```

### Compile-time pattern, file already in memory

```cpp
std::vector<std::byte> file_bytes = /* read the whole PE file */;

auto res = PePatternScanner::scan_for_pattern_in_memory_file<"48 8D 0D ?? ?? ?? ??">(file_bytes, ".rdata");
```

---

## Notes & edge cases

* **PE only**: these functions assume a valid **PE/COFF** layout. Non-PE files or corrupted headers yield `nullopt`. The memory-file variant additionally returns `nullopt` if a header or section lies outside the supplied span.
* **Section name**: `scan_for_pattern_in_file` defaults to **`.text`**; pass a different name to target other sections.
* **Addresses**: `virtual_base_addr` is `ImageBase` (from the optional header) plus the section's RVA, so it is a VA at the preferred base, not a bare RVA. The **process VA** returned by the “loaded module” API reflects where the module is actually mapped.
* **Architecture**: works for 32-bit and 64-bit PEs; `std::uintptr_t` size matches the build architecture.
* **Performance**: Matching is done by [`PatternScanner`](pattern_scan.md) (worst case **O(N × M)**, close to O(N) in practice with SIMD pre-filtering). File and memory-file scans copy the target section into a temporary buffer first. For large images, prefer scanning only necessary sections.
* **Wildcards**: Each `?` matches **one byte** (no nibble masks). If you need `A?`-style nibble wildcards, extend the parser (see `PatternScanner`).
* **Safety**: For loaded modules, you must have access to the memory; scanning read-only sections is fine, but never write. For file scans, ensure the file path is accessible.

---

## Troubleshooting

* **`nullopt`**: Verify the pattern (valid tokens), correct **section**, and that you’re scanning the intended module/file (check bitness and version).
* **“Loaded module” address math**: If you need an **offset from the module base**, compute `offset = hit_va - reinterpret_cast<std::uintptr_t>(module_base_address)`.
* **Multiple matches**: Only the **first** match is returned. If you need all matches, extend the implementation to continue scanning from `target_offset + 1`.

---

## See also

* [`omath::PatternScanner`](pattern_scan.md) — raw buffer/iterator scanning with the same pattern grammar.
* [`omath::SectionScanResult`](section_scan_result.md) — result type shared with the ELF and Mach-O scanners.

---

*Last updated: 31 Oct 2025*
