# `omath::ElfPatternScanner` — Scan ELF images for byte patterns

> Header: `omath/utility/elf_pattern_scan.hpp`
> Namespace: `omath`
> Platform: ELF (Linux and other ELF targets), 32 bit and 64 bit
> Purpose: Find the first occurrence of a byte pattern with wildcards inside one named section of an ELF image
> Companions: [`PatternScanner`](pattern_scan.md) (pattern grammar), [`SectionScanResult`](section_scan_result.md) (result type), [`PePatternScanner`](pe_pattern_scan.md) and [`MachOPatternScanner`](macho_pattern_scan.md) (same interface for the other formats)

`ElfPatternScanner` has the same three entry points as the other format scanners. You can scan:

* a **loaded module** in the current process,
* an **ELF file on disk**,
* an **ELF image already in memory** (a `std::span<const std::byte>` of the whole file).

The section defaults to **`.text`**. Only the section you name is searched, and only the first match is returned.

---

## Pattern grammar

Identical to [`PatternScanner`](pattern_scan.md): two hex digits per byte, `?` or `??` for a wildcard byte, whitespace ignored. An invalid pattern string gives `std::nullopt` in the runtime overloads; the `consteval` overloads reject it at compile time.

---

## API

```cpp
namespace omath
{
    class ElfPatternScanner final
    {
    public:
        // Loaded module in this process. Returns the absolute address of the first match.
        [[nodiscard]]
        static std::optional<std::uintptr_t>
        scan_for_pattern_in_loaded_module(const void* module_base_address, const std::string_view& pattern,
                                          const std::string_view& target_section_name = ".text");

        template<PatternScanner::ConstevalPattern Pattern>
        [[nodiscard]]
        static std::optional<std::uintptr_t>
        scan_for_pattern_in_loaded_module(const void* module_base_address,
                                          const std::string_view& target_section_name = ".text");

        // File on disk.
        [[nodiscard]]
        static std::optional<SectionScanResult>
        scan_for_pattern_in_file(const std::filesystem::path& path_to_file, const std::string_view& pattern,
                                 const std::string_view& target_section_name = ".text");

        template<PatternScanner::ConstevalPattern Pattern>
        [[nodiscard]]
        static std::optional<SectionScanResult>
        scan_for_pattern_in_file(const std::filesystem::path& path_to_file,
                                 const std::string_view& target_section_name = ".text");

        // ELF file already read into memory.
        [[nodiscard]]
        static std::optional<SectionScanResult>
        scan_for_pattern_in_memory_file(std::span<const std::byte> file_data, const std::string_view& pattern,
                                        const std::string_view& target_section_name = ".text");

        template<PatternScanner::ConstevalPattern Pattern>
        [[nodiscard]]
        static std::optional<SectionScanResult>
        scan_for_pattern_in_memory_file(std::span<const std::byte> file_data,
                                        const std::string_view& target_section_name = ".text");
    };
}
```

Every function is `static` and `[[nodiscard]]`. The template overloads take the pattern as a compile-time string literal (`scan_for_pattern_in_file<"48 ?? E5">(path)`), so it is parsed once at compile time. The class also has private overloads that take a section-scan function pointer; they are what the template overloads forward to and are not part of the public API.

---

## Behavior

| Entry point                          | Input                         | Result                                   |
|--------------------------------------|-------------------------------|------------------------------------------|
| `scan_for_pattern_in_loaded_module`  | base address of a mapped ELF  | `std::uintptr_t`: absolute address of the match |
| `scan_for_pattern_in_file`           | path to an ELF file           | [`SectionScanResult`](section_scan_result.md) |
| `scan_for_pattern_in_memory_file`    | bytes of an ELF file          | [`SectionScanResult`](section_scan_result.md) |

All three return `std::nullopt` when:

* the input is null, empty, unreadable or does not start with the `\x7FELF` signature,
* the class byte is neither `ELFCLASS32` nor `ELFCLASS64`,
* the section is not found,
* the pattern does not occur in the section (or is invalid, for the runtime overloads).

Details per entry point:

* **File and memory-file scans** find the section by name through the section header string table and copy its bytes (`sh_offset`, `sh_size`) before scanning. `SectionScanResult` holds `sh_addr` as `virtual_base_addr`, `sh_offset` as `raw_base_addr`, and the match offset inside the section as `target_offset`. The memory-file scan checks every offset against the buffer size and returns `nullopt` instead of reading out of bounds. The file scan and the memory-file scan produce identical results for the same bytes.
* **Loaded-module scan** reads the ELF header at `module_base_address`, walks the section header table, skips sections whose `sh_size` is zero, and scans the range `module_base_address + sh_addr` for `sh_size` bytes. The returned value is the address of the first matched byte in this process.

---

## Usage

### Scan a file on disk

```cpp
#include <omath/utility/elf_pattern_scan.hpp>

const auto result = omath::ElfPatternScanner::scan_for_pattern_in_file("/usr/lib/libfoo.so", "55 48 89 E5");

if (result)
{
    const auto virtual_hit = result->virtual_base_addr + result->target_offset; // sh_addr + offset
    const auto file_hit    = result->raw_base_addr     + result->target_offset; // sh_offset + offset
}
```

### Compile-time pattern, other section

```cpp
const auto result = omath::ElfPatternScanner::scan_for_pattern_in_file<"48 8D 05 ?? ?? ?? ??">(
        "/usr/lib/libfoo.so", ".rodata");
```

### Scan an ELF already in memory

```cpp
#include <fstream>
#include <iterator>
#include <omath/utility/elf_pattern_scan.hpp>
#include <vector>

std::ifstream in("libfoo.so", std::ios::binary);
const std::vector<char> raw{std::istreambuf_iterator<char>(in), {}};

const auto result = omath::ElfPatternScanner::scan_for_pattern_in_memory_file(
        std::as_bytes(std::span{raw}), "DE ?? BE EF");
```

### Scan a loaded module

```cpp
#include <link.h>
#include <omath/utility/elf_pattern_scan.hpp>

// base: load address of the module, e.g. dl_phdr_info::dlpi_addr
const auto hit = omath::ElfPatternScanner::scan_for_pattern_in_loaded_module(base, "55 48 89 E5");

if (hit)
{
    const auto function = reinterpret_cast<void (*)()>(*hit);
}
```

This is the same call [`InternalReverseEngineeredObject`](../rev_eng/internal_rev_object.md) makes on Linux, with `dlpi_addr` as the base.

---

## Notes

!!! warning "Loaded-module scan does no bounds checking"
    The loaded-module scan trusts the memory at `module_base_address`. It reads the section header table at `module_base_address + e_shoff` and the section names through it, and it adds the section's `sh_addr` to the base you pass. That requires the section header table to be present in the mapped image and `sh_addr` to be relative to the base. Normal loaders map only the `PT_LOAD` segments, and the section header table usually lives past them, so check this on your target before relying on it. If you can read the file, `scan_for_pattern_in_file` or `scan_for_pattern_in_memory_file` has none of these requirements.

* **Section names are matched exactly**: `.text` and `.text.startup` are different sections.
* **Only the first match** is returned. Only one section is searched per call.
* **Complexity**: the search is a linear sliding window over the section.
* **Byte order**: the file and memory-file scans read the headers as they are, with no byte swapping, so the file must have the host's endianness.
* **Tests**: `tests/general/unit_test_elf_scanner.cpp` builds a minimal ELF64 image in memory and checks the memory scan, the file scan, wildcards, the compile-time overloads, missing sections, garbage and empty input.

---

## See also

* [`PatternScanner`](pattern_scan.md) — raw buffer/iterator scanning and the pattern grammar
* [`SectionScanResult`](section_scan_result.md) — what the file scans return
* [`PePatternScanner`](pe_pattern_scan.md), [`MachOPatternScanner`](macho_pattern_scan.md)
* [`InternalReverseEngineeredObject`](../rev_eng/internal_rev_object.md) — resolves patterns in loaded modules through these scanners
