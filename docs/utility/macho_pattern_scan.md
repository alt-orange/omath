# `omath::MachOPatternScanner` — Scan Mach-O images for byte patterns

> Header: `omath/utility/macho_pattern_scan.hpp`
> Namespace: `omath`
> Platform: Mach-O (macOS / iOS), 32 bit and 64 bit thin images
> Purpose: Find the first occurrence of a byte pattern with wildcards inside one named section of a Mach-O image
> Companions: [`PatternScanner`](pattern_scan.md) (pattern grammar), [`SectionScanResult`](section_scan_result.md) (result type), [`PePatternScanner`](pe_pattern_scan.md) and [`ElfPatternScanner`](elf_pattern_scan.md) (same interface for the other formats)

`MachOPatternScanner` mirrors [`ElfPatternScanner`](elf_pattern_scan.md) for Mach-O. You can scan:

* a **loaded module** in the current process,
* a **Mach-O file on disk**,
* a **Mach-O image already in memory** (a `std::span<const std::byte>` of the whole file).

The section defaults to **`__text`**. Only the section you name is searched, and only the first match is returned.

---

## Pattern grammar

Identical to [`PatternScanner`](pattern_scan.md): two hex digits per byte, `?` or `??` for a wildcard byte, whitespace ignored. An invalid pattern string gives `std::nullopt` in the runtime overloads; the `consteval` overloads reject it at compile time.

---

## API

```cpp
namespace omath
{
    class MachOPatternScanner final
    {
    public:
        // Loaded module in this process. Returns the absolute address of the first match.
        [[nodiscard]]
        static std::optional<std::uintptr_t>
        scan_for_pattern_in_loaded_module(const void* module_base_address, const std::string_view& pattern,
                                          const std::string_view& target_section_name = "__text");

        template<PatternScanner::ConstevalPattern Pattern>
        [[nodiscard]]
        static std::optional<std::uintptr_t>
        scan_for_pattern_in_loaded_module(const void* module_base_address,
                                          const std::string_view& target_section_name = "__text");

        // File on disk.
        [[nodiscard]]
        static std::optional<SectionScanResult>
        scan_for_pattern_in_file(const std::filesystem::path& path_to_file, const std::string_view& pattern,
                                 const std::string_view& target_section_name = "__text");

        template<PatternScanner::ConstevalPattern Pattern>
        [[nodiscard]]
        static std::optional<SectionScanResult>
        scan_for_pattern_in_file(const std::filesystem::path& path_to_file,
                                 const std::string_view& target_section_name = "__text");

        // Mach-O file already read into memory.
        [[nodiscard]]
        static std::optional<SectionScanResult>
        scan_for_pattern_in_memory_file(std::span<const std::byte> file_data, const std::string_view& pattern,
                                        const std::string_view& target_section_name = "__text");

        template<PatternScanner::ConstevalPattern Pattern>
        [[nodiscard]]
        static std::optional<SectionScanResult>
        scan_for_pattern_in_memory_file(std::span<const std::byte> file_data,
                                        const std::string_view& target_section_name = "__text");
    };
}
```

Every function is `static` and `[[nodiscard]]`. The template overloads take the pattern as a compile-time string literal (`scan_for_pattern_in_file<"48 ?? E5">(path)`). The class also has private overloads that take a section-scan function pointer; the template overloads forward to them and they are not part of the public API.

---

## Behavior

| Entry point                          | Input                              | Result                                   |
|--------------------------------------|------------------------------------|------------------------------------------|
| `scan_for_pattern_in_loaded_module`  | base address (Mach-O header) of a mapped image | `std::uintptr_t`: absolute address of the match |
| `scan_for_pattern_in_file`           | path to a Mach-O file              | [`SectionScanResult`](section_scan_result.md) |
| `scan_for_pattern_in_memory_file`    | bytes of a Mach-O file             | [`SectionScanResult`](section_scan_result.md) |

The image is recognised by its magic number: `0xFEEDFACF` (64 bit) or `0xFEEDFACE` (32 bit), plus their byte-swapped forms. The scanner then walks the load commands (`LC_SEGMENT_64` for 64 bit, `LC_SEGMENT` for 32 bit) and compares the 16 byte `sectname` of every section to the name you passed.

All three return `std::nullopt` when:

* the input is null, empty, unreadable or has an unknown magic number,
* the section is not found,
* the pattern does not occur in the section (or is invalid, for the runtime overloads).

For the file and memory-file scans `SectionScanResult` holds the section's `addr` as `virtual_base_addr`, its `offset` as `raw_base_addr`, and the match offset inside the section as `target_offset`. The memory-file scan checks load commands, section headers and the section data against the buffer size and returns `nullopt` instead of reading out of bounds. The loaded-module scan scans `module_base_address + section.addr` for `section.size` bytes and returns the address of the first matched byte.

---

## Usage

### Scan a file on disk

```cpp
#include <omath/utility/macho_pattern_scan.hpp>

const auto result = omath::MachOPatternScanner::scan_for_pattern_in_file("libfoo.dylib", "55 48 89 E5");

if (result)
{
    const auto virtual_hit = result->virtual_base_addr + result->target_offset; // section addr + offset
    const auto file_hit    = result->raw_base_addr     + result->target_offset; // section offset + offset
}
```

### Compile-time pattern

```cpp
const auto result = omath::MachOPatternScanner::scan_for_pattern_in_memory_file<"DE ?? BE EF">(
        std::span<const std::byte>{file_bytes});
```

### Scan a loaded module

```cpp
#include <mach-o/dyld.h>
#include <omath/utility/macho_pattern_scan.hpp>

const auto* header = _dyld_get_image_header(0);
const auto hit = omath::MachOPatternScanner::scan_for_pattern_in_loaded_module(header, "55 48 89 E5");
```

This is the same call [`InternalReverseEngineeredObject`](../rev_eng/internal_rev_object.md) makes on macOS, with the image header from `_dyld_get_image_header` as the base.

---

## Notes

!!! warning "Loaded-module scan: link-time addresses"
    The loaded-module scan adds the section's `addr` field to the base you pass, and does no bounds checking on the mapped memory. `addr` is a link-time virtual address; it only equals an offset from the Mach-O header when the image's `__TEXT` segment is linked at address 0 (typical for dylibs, not for main executables, which are linked at a non-zero `vmaddr`). Check the result on your target before relying on it.

!!! warning "File scan reads only the first segment that has sections"
    In the current implementation the file scan (`scan_for_pattern_in_file`) does not advance to the next load command after it has searched a segment's sections, so a section that lives in a later segment than the first one with sections is not found. `__text` in `__TEXT` is not affected. The memory-file scan advances correctly and can be used as a workaround: read the file into a buffer and call `scan_for_pattern_in_memory_file`.

* **Section names are matched by `sectname` only.** The segment name is not compared, so `__text` matches the section of that name in any segment.
* **Fat (universal) binaries are not supported.** A `0xCAFEBABE` header is an unknown magic and gives `nullopt`; extract one architecture slice first.
* **No byte swapping.** The byte-swapped magic numbers are accepted, but the header fields are read as they are, so the file must have the host's endianness.
* **Only the first match** is returned. The search is a linear sliding window over the section.
* **Tests**: `tests/general/unit_test_macho_scanner.cpp` (file and loaded-module scans, 32 and 64 bit) and `tests/general/unit_test_macho_memory_file_scan.cpp` (in-memory scan, compile-time overloads, raw and virtual base addresses).

---

## See also

* [`PatternScanner`](pattern_scan.md) — raw buffer/iterator scanning and the pattern grammar
* [`SectionScanResult`](section_scan_result.md) — what the file scans return
* [`PePatternScanner`](pe_pattern_scan.md), [`ElfPatternScanner`](elf_pattern_scan.md)
* [`InternalReverseEngineeredObject`](../rev_eng/internal_rev_object.md) — resolves patterns in loaded modules through these scanners
