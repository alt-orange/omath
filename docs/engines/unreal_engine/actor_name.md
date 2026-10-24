# `omath::unreal_engine::get_actor_name` — resolving FName through GNames

> Header: `omath/engines/unreal_engine/actor_name.hpp`
> Namespace: `omath::unreal_engine`
> Purpose: turn an actor (or any other `UObject`) pointer into its name, on UE 2.5, UE 3, UE 4 and UE 5

---

## Summary

```cpp
template<rev_eng::MemoryReadTrait MemoryTrait = rev_eng::InternalMemoryTrait>
std::expected<std::string, ActorNameError>
get_actor_name(std::uintptr_t actor, std::uintptr_t g_names, const NameLayout& layout);
```

The function reads `UObject::Name`, resolves that `FName` index inside the name container `g_names` points at, and
transcodes the entry to UTF-8. Just like `UObject::GetName()` it appends `_<Number - 1>` when the `FName` carries a
non zero instance number.

Every engine generation stores names differently, so the whole traversal is driven by a `NameLayout`:

| Generation | `NamePoolKind` | Lookup |
| --- | --- | --- |
| UE 2.5, UE 3 | `POINTER_ARRAY` | `TArray<FNameEntry*>`, `entry = Data[index]` |
| UE 4.0 - 4.22 | `CHUNKED_ARRAY` | `entry = Chunks[index / elements_per_chunk][index % elements_per_chunk]` |
| UE 4.23+, UE 5 | `BLOCK_POOL` | `entry = Blocks[index >> block_offset_bits] + (index & mask) * block_entry_stride` |

`POINTER_ARRAY` and `CHUNKED_ARRAY` entries are null terminated, `BLOCK_POOL` entries carry their length inside
`FNameEntryHeader`.

---

## Internal usage

```cpp
#include <omath/engines/unreal_engine/actor_name.hpp>

using omath::unreal_engine::get_actor_name;
using omath::unreal_engine::NameLayout;

const auto name = get_actor_name(actor_address, g_names_address, NameLayout::ue5());

if (name.has_value())
    std::println("{}", *name);
```

`MemoryTrait` defaults to `omath::rev_eng::InternalMemoryTrait`, which dereferences the address directly, so the call
above works from inside the game process.

---

## External usage

Any type with a `read_memory<Type>(std::uintptr_t)` static template satisfies `omath::rev_eng::MemoryReadTrait`, which
makes the same code work over `ReadProcessMemory`, `process_vm_readv`, a driver or a DMA device:

```cpp
struct RpmTrait
{
    template<class Type>
    static Type read_memory(const std::uintptr_t address)
    {
        Type value{};
        ReadProcessMemory(g_process, reinterpret_cast<LPCVOID>(address), &value, sizeof(Type), nullptr);

        return value;
    }
};

auto layout = NameLayout::ue3(); // already sets pointer_size to 4 for its x86 target
layout.pointer_size = sizeof(std::uint32_t); // spelled out: a 64 bit tool reading a 32 bit game

const auto name = get_actor_name<RpmTrait>(actor_address, g_names_address, layout);
```

---

## `NameLayout` fields

| Field | Default (`ue4()`) | Meaning |
| --- | --- | --- |
| `object_name_offset` | `0x18` | `UObject` to its `FName` |
| `name_number_offset` | `0x4` | `FName` to its instance `Number`, relative to the `FName`; `std::nullopt` when there is none (UE 2.5) |
| `pool_kind` | `BLOCK_POOL` | `NamePoolKind`, see the table above |
| `pool_data_offset` | `0x10` | `GNames` to the `TArray::Data` (`POINTER_ARRAY`), chunk table (`CHUNKED_ARRAY`) or block table (`BLOCK_POOL`) |
| `pointer_size` | `8` | Size of a pointer in the **target** process, not in the process compiling this |
| `elements_per_chunk` | `16384` | `CHUNKED_ARRAY` only |
| `block_offset_bits` | `16` | `BLOCK_POOL` only |
| `block_entry_stride` | `2` | `BLOCK_POOL` only |
| `entry_flags_offset` | `0x0` | `FNameEntry` to `Index` (`POINTER_ARRAY`, `CHUNKED_ARRAY`) or to `FNameEntryHeader` (`BLOCK_POOL`) |
| `entry_text_offset` | `0x2` | `FNameEntry` to the name characters |
| `char_kind` | `PER_ENTRY_FLAG` | `NameCharKind`: `ANSI`, `WIDE` (UTF-16) or `PER_ENTRY_FLAG` (bit 0 of the entry flags selects wide) |
| `header_length_shift` | `6` | `BLOCK_POOL` only: `FNameEntryHeader >> shift` is the length. `1` with `WITH_CASE_PRESERVING_NAME` |
| `max_name_length` | `1024` | Read guard for null terminated names (`POINTER_ARRAY`, `CHUNKED_ARRAY`); also caps `BLOCK_POOL` lengths |

---

## Presets

```cpp
NameLayout::ue2_5();      // UE 2.5, x86, UTF-16 entries, FName without Number
NameLayout::ue3();        // UE 3, x86
NameLayout::ue4_legacy(); // UE 4.0 - 4.22, x64
NameLayout::ue4();        // UE 4.23+, x64, FNamePool
NameLayout::ue5();        // same layout as ue4()
```

Presets hold the values that are typical for their generation, **they are not guaranteed for your game**. The UE 2.5
one was derived from Killing Floor 1 build 1065, see `examples/example_kf1_dumper`. Dump the target and override the
fields that differ:

```cpp
auto layout = NameLayout::ue5();
layout.object_name_offset = 0x20;   // UObject::NamePrivate moved
layout.pool_data_offset = 0x40;     // FNameEntryAllocator::Blocks on Linux, FRWLock is a pthread_rwlock_t there
layout.header_length_shift = 1;     // game built WITH_CASE_PRESERVING_NAME
```

`g_names` must be the address of the name container itself. When your signature resolves to a pointer variable holding
it, dereference that pointer before passing it in.

---

## Errors

| `ActorNameError` | Meaning |
| --- | --- |
| `NULL_POINTER` | `actor` or `g_names` is zero |
| `INVALID_NAME_INDEX` | the chunk, block or entry pointer behind the `FName` index is null |
| `EMPTY_NAME` | the entry decoded to an empty string, usually a wrong `entry_text_offset` |
