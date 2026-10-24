# `omath::rev_eng::InternalReverseEngineeredObject` — raw in-process offset/VTABLE access

> Header: `omath/rev_eng/internal_rev_object.hpp`
> Namespace: `omath::rev_eng`
> Purpose: Convenience base for **internal** (same-process) RE wrappers that:
>
> * read/write fields by **byte offset** from `this`
> * call **virtual methods** by **vtable index**
> * call **non-virtual functions** by address, or by **module + byte pattern** (resolved with the platform's pattern scanner)

---

## At a glance

```cpp
class InternalReverseEngineeredObject {
protected:
  // Field access (non-const returns T&, const returns const T&); noexcept
  template<class Type> [[nodiscard]] Type&       get_by_offset(std::ptrdiff_t offset) noexcept;
  template<class Type> [[nodiscard]] const Type& get_by_offset(std::ptrdiff_t offset) const noexcept;

  // Call a function at a known address with `this` as the first argument
  template<class ReturnType> ReturnType call_method(const void* ptr, auto... arg_list);
  template<class ReturnType> ReturnType call_method(const void* ptr, auto... arg_list) const;

  // Same, resolving the address once from a module name and a pattern (compile-time strings)
  template<PatternScanner::ConstevalPattern ModuleName, PatternScanner::ConstevalPattern Pattern, class ReturnType>
  ReturnType call_method(auto... arg_list);        // and a const overload

  // Same with runtime strings
  template<class ReturnType>
  ReturnType call_method(const std::string_view& module_name, const std::string_view& pattern, auto... arg_list);  // and const

  // Virtual calls by vtable index
  template<std::size_t Id, class ReturnType>
  ReturnType call_virtual_method(auto... arg_list);                       // and const overload

  // Virtual call through a secondary vtable located TableOffset bytes into the object
  template<std::ptrdiff_t TableOffset, std::size_t Id, class ReturnType>
  ReturnType call_virtual_method(auto... arg_list);                       // and const overload
};
```

* `get_by_offset<T>(off)` — returns a **reference** to `T` located at `reinterpret_cast<uintptr_t>(this) + off`.
* `call_method<Ret>(ptr, args...)` — casts `ptr` to a function taking `this` plus the argument types and calls it.
* `call_method<ModuleName, Pattern, Ret>(args...)` / `call_method<Ret>(module_name, pattern, args...)` — scans the module for `pattern` (see below), caches the address in a function-local `static`, then behaves like the address overload.
* `call_virtual_method<Id, Ret>(args...)` — fetches the function pointer from `(*reinterpret_cast<void***>(this))[Id]` and invokes it through `call_method` with `this` passed explicitly.
* `call_virtual_method<TableOffset, Id, Ret>(args...)` — same, but `this` is first shifted by `TableOffset` bytes (for objects with multiple inheritance, where the wanted vtable pointer is not at offset 0); the shifted pointer is passed as `this`.

On MSVC builds the function pointer type uses `__thiscall`; on non-MSVC it uses a plain function pointer taking `void*` as the first parameter (the typical Itanium ABI shape).

---

## Example: wrapping a reverse-engineered class

```cpp
struct Player : omath::rev_eng::InternalReverseEngineeredObject {
  // Field offsets (document game/app version!)
  static constexpr std::ptrdiff_t kHealth   = 0x100;
  static constexpr std::ptrdiff_t kPosition = 0x30;

  // Accessors
  float&             health()             { return get_by_offset<float>(kHealth); }
  const float&       health()       const { return get_by_offset<float>(kHealth); }

  Vector3<float>&    position()           { return get_by_offset<Vector3<float>>(kPosition); }
  const Vector3<float>& position() const  { return get_by_offset<Vector3<float>>(kPosition); }

  // Virtuals (vtable indices discovered via RE)
  int  getTeam()            { return call_virtual_method<27, int>(); }
  void setArmor(float val)  { call_virtual_method<42, void>(val); } // signature must match!

  // Non-virtual function found by pattern in a loaded module (resolved once, then cached)
  void respawn()            { call_method<"client.dll", "48 89 5C 24 ?? 57 48 83 EC 20", void>(); }
};
```

Usage:

```cpp
auto* p = /* pointer to live Player instance within the same process */;
p->health() = 100.f;
int team = p->getTeam();
```

---

## How `call_virtual_method` resolves the signature

```cpp
template<std::size_t Id, class ReturnType>
ReturnType call_virtual_method(auto... arg_list) {
  const auto vtable = *reinterpret_cast<void***>(this);
  return call_method<ReturnType>(vtable[Id], arg_list...);   // -> Fn(this, arg_list...)
}

// inside call_method<ReturnType>(ptr, arg_list...):
#ifdef _MSC_VER
  using Fn = ReturnType(__thiscall*)(void*, decltype(arg_list)...);
#else
  using Fn = ReturnType(*)(void*, decltype(arg_list)...);
#endif
```

* The **first parameter** is always `this` (`void*`).
* Remaining parameter types are deduced from the **actual arguments** (`decltype(arg_list)...`).
  Ensure you pass arguments with the correct types (e.g., `int32_t` vs `int`, pointer/ref qualifiers), or define thin wrappers that cast to the exact signature you recovered.

> On 32-bit MSVC the `__thiscall` distinction matters; on 64-bit MSVC it’s ignored (all member funcs use the common x64 calling convention).

---

## Pattern-resolved calls

`call_method<ModuleName, Pattern, Ret>` and `call_method<Ret>(module_name, pattern, ...)` locate the function at runtime:

1. The module base is looked up by name: `GetModuleHandleA` on Windows; on Linux (`dl_iterate_phdr`) and macOS (dyld image list) the first loaded image whose path **contains** `module_name` is used.
2. The pattern is scanned with `PePatternScanner`, `ElfPatternScanner` or `MachOPatternScanner::scan_for_pattern_in_loaded_module` (see [pattern scanning](../utility/pe_pattern_scan.md)).
3. The resulting address is stored in a function-local `static`, so the scan runs once per template instantiation.

Caveats:

* A missing module or pattern is reported only by `assert`; in builds without assertions the call proceeds with a null or invalid address.
* The cache is per **instantiation**. For the runtime-string overload the module and pattern values are not part of the instantiation, so two calls with the same `ReturnType` and argument types share one cached address regardless of the strings passed. Prefer the compile-time overload (strings are template arguments) when you call several different patterns, or wrap each in its own member function.

---

## Safety notes (read before using!)

Working at this level is inherently unsafe; be deliberate:

1. **Correct offsets & alignment**

    * `get_by_offset<T>` assumes `this + offset` is **properly aligned** for `T` and points to an object of type `T`.
    * Wrong offsets or misalignment ⇒ **undefined behavior** (UB), crashes, silent corruption.

2. **Object layout assumptions**

    * The vtable pointer is assumed to be at the **start of the most-derived subobject at `this`**.
    * With **multiple/virtual inheritance**, the desired subobject’s vptr may be at a non-zero offset. If so, adjust `this` to that subobject before calling, e.g.:

      ```cpp
      auto* sub = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(this) + kSubobjectOffset);
      // … then reinterpret sub instead of this inside a custom helper
      ```

3. **ABI & calling convention**

    * Indices and signatures are **compiler/ABI-specific**. Recheck after updates or different builds (MSVC vs Clang/LLVM-MSVC vs MinGW).

4. **Strict aliasing**

    * Reinterpreting memory as unrelated `T` can violate aliasing rules. Prefer **trivially copyable** PODs and exact original types where possible.

5. **Const-correctness**

    * The `const` overload returns `const T&` (it reinterprets `this + offset` as `const Type*`); do not write through it. Use the non-const overload to mutate.

6. **Thread safety**

    * No synchronization is provided. Ensure the underlying object isn’t concurrently mutated in incompatible ways.

---

## Tips & patterns

* **Centralize offsets** in `constexpr` with comments (`// game v1.2.3, sig XYZ`).
* **Guard reads**: if you have a canary or RTTI/vtable hash, check it before relying on offsets.
* **Prefer accessors** returning references**:** lets you both read and write with natural syntax.
* **Wrap tricky virtuals**: if a method takes complex/reference params, wrap `call_virtual_method` in a strongly typed member that casts exactly as needed.

---

## Troubleshooting

* **Crash on virtual call** → wrong index or wrong `this` (subobject), or mismatched signature (args/ret or calling conv).
* **Weird field values** → wrong offset, wrong type size/packing, stale layout after an update.
* **Only in 32-bit** → double-check `__thiscall` and parameter passing (register vs stack).

---

*Last updated: 1 Nov 2025*
