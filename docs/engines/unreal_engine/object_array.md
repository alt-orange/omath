# `omath::unreal_engine` object array — walking GObjects

> Header: `omath/engines/unreal_engine/object_array.hpp`
> Namespace: `omath::unreal_engine`
> Purpose: enumerate the global `UObject` array on UE 2.5, UE 3, UE 4 and UE 5

---

## Summary

```cpp
template<rev_eng::MemoryReadTrait MemoryTrait = rev_eng::InternalMemoryTrait>
std::expected<std::uint32_t, ObjectArrayError>
get_object_count(std::uintptr_t g_objects, const ObjectArrayLayout& layout);

template<rev_eng::MemoryReadTrait MemoryTrait = rev_eng::InternalMemoryTrait>
std::expected<std::uintptr_t, ObjectArrayError>
get_object_by_index(std::uintptr_t g_objects, std::uint32_t index, const ObjectArrayLayout& layout);
```

`get_object_by_index` bounds checks the index against the stored count, walks the array and returns the `UObject`
address, which is exactly what [`get_actor_name`](actor_name.md) expects.

`g_objects` is the address of `UObject::GObjObjects` on UE 2.5 and UE 3, and the address of `GUObjectArray` on UE 4
and UE 5. `get_object_count` returns the number of slots, freed slots included.

| Generation | `ObjectArrayKind` | Container |
| --- | --- | --- |
| UE 2.5, UE 3 | `FLAT` | `TArray<UObject*>` |
| UE 4 with a fixed array | `FLAT` | `FFixedUObjectArray`, elements are `FUObjectItem` |
| UE 4.11+, UE 5 | `CHUNKED` | `FChunkedFixedUObjectArray`, 64K `FUObjectItem` per chunk |
| UE 4.0 - 4.10 | `CHUNKED` with `indirect_chunk_table = false` | chunk table inlined into the array object |

## `ObjectArrayLayout` fields

| Field | Default (`ue4()`) | Meaning |
| --- | --- | --- |
| `kind` | `CHUNKED` | `ObjectArrayKind`, see the table above |
| `objects_offset` | `0x10` | `GObjects` to `TArray::Data` / `FUObjectArray::ObjObjects::Objects` |
| `count_offset` | `0x24` | `GObjects` to `TArray::ArrayNum` / `FUObjectArray::ObjObjects::NumElements` |
| `elements_per_chunk` | `64 * 1024` | `CHUNKED` only |
| `indirect_chunk_table` | `true` | `CHUNKED` only: `objects_offset` holds a pointer to the chunk table (UE 4.11+, UE 5); `false` when the table is inlined (UE 4.0 - 4.10) |
| `item_stride` | `0x18` | Size of one element: `sizeof(FUObjectItem)` on UE 4 / UE 5, a bare pointer on UE 2.5 / UE 3 |
| `item_object_offset` | `0x0` | Element to its `UObject*` |
| `pointer_size` | `8` | Size of a pointer in the **target** process, not in the process compiling this |

---

## Iterating

```cpp
#include <omath/engines/unreal_engine/actor_name.hpp>
#include <omath/engines/unreal_engine/object_array.hpp>

using namespace omath::unreal_engine;

void dump_objects(const std::uintptr_t g_objects, const std::uintptr_t g_names)
{
    const auto count = get_object_count(g_objects, ObjectArrayLayout::ue5());

    if (!count.has_value())
        return;

    for (std::uint32_t i = 0; i < *count; ++i)
    {
        const auto object = get_object_by_index(g_objects, i, ObjectArrayLayout::ue5());

        // Slots of destroyed objects are empty, skipping them is normal
        if (!object.has_value())
            continue;

        const auto name = get_actor_name(*object, g_names, NameLayout::ue5());

        if (name.has_value())
            std::println("[{}] {:#x} {}", i, *object, *name);
    }
}
```

---

## Presets

```cpp
ObjectArrayLayout::ue2_5();     // UE 2.5, x86, TArray<UObject*>
ObjectArrayLayout::ue3();       // UE 3, x86, same TArray<UObject*>
ObjectArrayLayout::ue4_fixed(); // UE 4 with FFixedUObjectArray, x64
ObjectArrayLayout::ue4();       // UE 4.11+, x64, FChunkedFixedUObjectArray
ObjectArrayLayout::ue5();       // same layout as ue4()
```

Presets hold the values that are typical for their generation, **they are not guaranteed for your game**. Common
overrides:

```cpp
auto layout = ObjectArrayLayout::ue5();
layout.item_stride = 0x20;             // FUObjectItem grew in this build
layout.count_offset = 0x2C;            // FUObjectArray header differs

auto early = ObjectArrayLayout::ue4(); // UE 4.0 - 4.10
early.indirect_chunk_table = false;    // chunk table is inlined, not behind a pointer
early.elements_per_chunk = 16384;
early.item_stride = sizeof(std::uintptr_t); // chunks hold bare UObject*, no FUObjectItem

auto ue3 = ObjectArrayLayout::ue3();   // 64 bit tool reading a 32 bit game
ue3.pointer_size = sizeof(std::uint32_t);
ue3.item_stride = sizeof(std::uint32_t);
```

---

## Errors

| `ObjectArrayError` | Meaning |
| --- | --- |
| `NULL_POINTER` | `g_objects` is zero, or the array/chunk pointer behind it is zero |
| `INDEX_OUT_OF_RANGE` | index is not below the stored element count |
| `NULL_OBJECT` | the slot is empty, its object was destroyed |
