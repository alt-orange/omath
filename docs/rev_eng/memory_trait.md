# `omath::rev_eng::MemoryReadTrait` — How omath reads a target's memory

> Header: `omath/rev_eng/memory_trait.hpp`
> Namespace: `omath::rev_eng`
> Purpose: The read-memory contract used by the Unreal Engine helpers, the in-process implementation of it, and two small address helpers

A memory trait is a type with a static `read_memory<T>(address)`. Code that has to follow pointers in a target process (the Unreal Engine [object array](../engines/unreal_engine/object_array.md) and [actor name](../engines/unreal_engine/actor_name.md) helpers) takes the trait as a template parameter, so the same code works inside the target (`InternalMemoryTrait`) or from outside it (your own trait around `ReadProcessMemory`, `process_vm_readv`, a driver, a dump file, ...).

---

## API

```cpp
namespace omath::rev_eng
{
    template<class Type>
    concept MemoryReadTrait =
            requires(const std::uintptr_t address) { Type::template read_memory<std::uint32_t>(address); };

    // Reads memory of the process omath is running in.
    struct InternalMemoryTrait final
    {
        template<class Type>
        [[nodiscard("You must use read value")]]
        static Type read_memory(const std::uintptr_t address) noexcept;
    };

    [[nodiscard("You must use shifted address")]]
    constexpr std::uintptr_t shift_address(const std::uintptr_t address, const std::ptrdiff_t offset) noexcept;

    template<MemoryReadTrait MemoryTrait>
    [[nodiscard("You must use read pointer")]]
    std::uintptr_t read_pointer(const std::uintptr_t address, const std::size_t pointer_size);
}
```

---

## `MemoryReadTrait`

A type satisfies the concept when `Type::template read_memory<std::uint32_t>(address)` compiles for a `std::uintptr_t` address. In practice the trait must provide:

```cpp
template<class Type>
static Type read_memory(std::uintptr_t address);
```

The concept only checks the `std::uint32_t` instantiation, so the function must work for every `Type` the caller asks for (integers, pointers, floats, trivially copyable structs). A trait does not have to be `noexcept` or provide `write_memory` for the concept; the write side is separate (see [`ExternalReverseEngineeredObject`](external_rev_object.md)).

---

## `InternalMemoryTrait`

```cpp
return *reinterpret_cast<const Type*>(address);
```

A plain dereference of the address in the current process. There is no validity check: an unmapped address crashes the process. Use it when your code is injected into the target; for an out-of-process target write your own trait. The Unreal Engine helpers use it as their default (`MemoryTrait = rev_eng::InternalMemoryTrait`).

---

## `shift_address`

`shift_address(address, offset)` returns `address + offset` for a signed `std::ptrdiff_t` offset, as a `constexpr` `noexcept` function. It exists so that field offsets (which may be negative) are applied to a `std::uintptr_t` in one place.

---

## `read_pointer`

```cpp
template<MemoryReadTrait MemoryTrait>
std::uintptr_t read_pointer(std::uintptr_t address, std::size_t pointer_size);
```

Reads a pointer stored at `address` in the target, where the target's pointer is `pointer_size` bytes wide:

* `pointer_size == 4` reads a `std::uint32_t`.
* any other value reads a `std::uint64_t`.

The 8 byte case reads a fixed `std::uint64_t` rather than the host's `std::uintptr_t`, so a 32 bit host does not silently truncate a 64 bit target pointer while reading it. The result is still narrowed to the host's `std::uintptr_t` on return: a 32 bit host cannot represent a target address above 4 GB.

This is what lets a 64 bit tool read a 32 bit game: pass `sizeof(std::uint32_t)` and the tool follows 4 byte pointers.

---

## Usage

### An out-of-process trait

This is the trait of the `example_kf1_dumper` example (Windows, `ReadProcessMemory`), shortened:

```cpp
struct RpmTrait final
{
    template<class Type>
    [[nodiscard]]
    static Type read_memory(const std::uintptr_t address) noexcept
    {
        Type value{};
        SIZE_T read{};

        if (!ReadProcessMemory(g_process, reinterpret_cast<LPCVOID>(address), &value, sizeof(Type), &read)
            || read != sizeof(Type))
            g_read_failed = true; // a failed read yields a zeroed value, remember that it happened

        return value;
    }
};

static_assert(omath::rev_eng::MemoryReadTrait<RpmTrait>);
```

### Following pointers of a 32 bit target

```cpp
using omath::rev_eng::read_pointer;
using omath::rev_eng::shift_address;

// UObject::Class lives at +0x28 in the target, and the target uses 4 byte pointers
const auto object_class = read_pointer<RpmTrait>(shift_address(object, 0x28), sizeof(std::uint32_t));
```

### Passing the trait to the engine helpers

```cpp
#include <omath/engines/unreal_engine/object_array.hpp>

const auto count = omath::unreal_engine::get_object_count<RpmTrait>(g_objects, layout);
```

See [object array](../engines/unreal_engine/object_array.md) and [actor name](../engines/unreal_engine/actor_name.md) for the full calls.

---

## Notes

* **Reads are by value.** `read_memory<Type>` returns a `Type`; reading a struct means reading `sizeof(Type)` bytes from the address.
* **Failure handling is up to the trait.** The concept has no error channel; the `RpmTrait` above sets a flag and returns a zeroed value, which a caller can check after reading a group of fields.
* **`InternalReverseEngineeredObject` does not use this trait.** It dereferences `this` directly; see [`InternalReverseEngineeredObject`](internal_rev_object.md).
* **`ExternalReverseEngineeredObject` is not constrained by the concept.** It calls `ExternalMemoryManagementTrait::read_memory<Type>(address)` and `write_memory(address, value)` by name; a trait written for it is expected to provide both. See [`ExternalReverseEngineeredObject`](external_rev_object.md).

---

## See also

* [`ExternalReverseEngineeredObject`](external_rev_object.md) — typed offsets over external memory
* [`InternalReverseEngineeredObject`](internal_rev_object.md) — typed offsets and calls inside the target
* [Unreal Engine object array](../engines/unreal_engine/object_array.md), [actor name](../engines/unreal_engine/actor_name.md) — consumers of `MemoryReadTrait`
