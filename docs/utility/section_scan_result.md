# `omath::SectionScanResult` — Where a file scan found its match

> Header: `omath/utility/section_scan_result.hpp`
> Namespace: `omath`
> Purpose: Shared result type of the file and in-memory-file scans of [`PePatternScanner`](pe_pattern_scan.md), [`ElfPatternScanner`](elf_pattern_scan.md) and [`MachOPatternScanner`](macho_pattern_scan.md)

---

## API

```cpp
namespace omath
{
    struct SectionScanResult final
    {
        std::uintptr_t virtual_base_addr;
        std::uintptr_t raw_base_addr;
        std::ptrdiff_t target_offset;
    };
}
```

It is a plain aggregate with no constructors and no members other than the three fields.

---

## Fields

| Field                 | Meaning                                                                                  |
|-----------------------|------------------------------------------------------------------------------------------|
| `virtual_base_addr`   | Address the scanned section is mapped at, taken from the section header of the file      |
| `raw_base_addr`       | Offset of the section's first byte inside the file (or inside the supplied buffer)       |
| `target_offset`       | Offset of the first matched byte, counted from the start of the section                  |

What goes into `virtual_base_addr` depends on the format:

| Format  | `virtual_base_addr`                                   | `raw_base_addr`      |
|---------|-------------------------------------------------------|----------------------|
| PE      | section `virtual_address` + optional header `image_base` | section `ptr_raw_data` |
| ELF     | section `sh_addr`                                     | section `sh_offset`  |
| Mach-O  | section `addr`                                        | section `offset`     |

For PE the image base is already added, so the value is an absolute preferred address. For ELF and Mach-O it is the value stored in the section header, which is a link-time address (for a position independent image it is relative to the load address).

---

## Usage

```cpp
#include <omath/utility/elf_pattern_scan.hpp>

const auto result = omath::ElfPatternScanner::scan_for_pattern_in_file("/usr/bin/true", "55 48 89 E5");

if (result)
{
    // Address of the hit once the image is mapped (link-time address)
    const auto virtual_hit = result->virtual_base_addr + result->target_offset;

    // Position of the hit inside the file, e.g. for a hex editor or a patcher
    const auto file_hit = result->raw_base_addr + result->target_offset;
}
```

`example_signature_scan` prints exactly these two sums for a PE file.

---

## Notes

!!! note "Not returned by the loaded-module scans"
    `scan_for_pattern_in_loaded_module` returns a plain `std::optional<std::uintptr_t>` (the absolute address of the hit). `SectionScanResult` is only produced by `scan_for_pattern_in_file` and `scan_for_pattern_in_memory_file`.

* `target_offset` is signed (`std::ptrdiff_t`) but is never negative in a result that a scanner returns.
* `virtual_base_addr` and `raw_base_addr` are `std::uintptr_t`, so on a 32 bit build they are 32 bit wide.
* The result type is the same for all three formats, so code that consumes a hit does not need to know which scanner produced it.

---

## See also

* [`PePatternScanner`](pe_pattern_scan.md)
* [`ElfPatternScanner`](elf_pattern_scan.md)
* [`MachOPatternScanner`](macho_pattern_scan.md)
* [`PatternScanner`](pattern_scan.md) — the pattern grammar shared by all of them
