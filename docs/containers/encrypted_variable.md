# `omath::EncryptedVariable` — XOR-obfuscated value with scoped decryption

> Header: `omath/containers/encrypted_variable.hpp`
> Namespace: `omath` (helpers in `omath::detail`)
> Template: `EncryptedVariable<T, key_size, key>` and `VarAnchor<EncryptedVarType>`
> Purpose: keep a value XOR-scrambled in memory and expose it in clear only while a scope holds it open

!!! warning "Obfuscation, not encryption"
    The scheme is a byte-wise XOR with a key that is a compile-time constant stored in the binary. It makes the value harder to find by a memory scanner or to read in a debugger at a glance. It does not protect against anyone who reads your binary.

---

## API

```cpp
namespace omath {

template<class T, std::size_t key_size, std::array<std::uint8_t, key_size> key>
class EncryptedVariable final {
public:
  using value_type = std::remove_cvref_t<T>;   // private alias

  constexpr explicit EncryptedVariable(const value_type& data);   // stores data, then encrypts it
  constexpr ~EncryptedVariable();                                 // decrypts before destruction

  [[nodiscard]] constexpr bool is_encrypted() const;
  constexpr void decrypt();                      // no-op if already decrypted
  constexpr void encrypt();                      // no-op if already encrypted

  [[nodiscard]] constexpr value_type&       value();         // raw storage, whatever its state
  [[nodiscard]] constexpr const value_type& value() const;

  [[nodiscard]] constexpr auto drop_anchor();    // returns a VarAnchor bound to *this
};

// RAII guard: decrypts in the constructor, encrypts again in the destructor.
template<class EncryptedVarType>
class VarAnchor final {
public:
  constexpr VarAnchor(EncryptedVarType& var);    // implicit; decrypts var
  constexpr ~VarAnchor();                        // encrypts var
};

} // namespace omath

// Declares an EncryptedVariable type with a fresh random key of KEY_SIZE bytes.
#define OMATH_DEF_CRYPT_VAR(TYPE, KEY_SIZE) \
    omath::EncryptedVariable<TYPE, KEY_SIZE, OMATH_CT_RAND_ARRAY_BYTE(KEY_SIZE)>

// Expression yielding a std::array<std::uint8_t, N> of compile-time random bytes (uses __COUNTER__).
#define OMATH_CT_RAND_ARRAY_BYTE(N) /* ... */
```

---

## Behavior

* **State flag.** `m_is_encrypted` tracks whether the bytes are currently scrambled. The constructor stores the plain value and immediately calls `encrypt()`, so the object starts **encrypted**.
* **The cipher.** Every byte `i` of the stored object is XORed with `key[i % key_size] + i * key_size` (truncated to a byte). XOR is its own inverse, so `encrypt()` and `decrypt()` run the same loop; the flag stops a double application.
* **`value()` does not decrypt.** It returns a reference to the underlying storage. While the object is encrypted that storage holds scrambled bytes. Open a scope with `drop_anchor()` (or call `decrypt()` yourself) before reading or writing a meaningful value.
* **`VarAnchor`** decrypts on construction and encrypts on destruction. `drop_anchor()` is the shorthand for constructing one from `*this`. If the variable was already decrypted when the anchor was created, the anchor still re-encrypts it on exit.
* **Destruction.** The destructor calls `decrypt()`, so the storage is restored to the plain value before it is destroyed.
* **Key generation.** `OMATH_DEF_CRYPT_VAR` builds the key with `OMATH_CT_RAND_ARRAY_BYTE(KEY_SIZE)`, which seeds a SplitMix64 generator from `__FILE__`, `__DATE__`, `__TIME__` and `__COUNTER__`. Each use of the macro therefore gets a different key, and the keys change from build to build. The helpers live in `omath::detail` and are `consteval`.
* **Force inline.** Define `OMATH_ENABLE_FORCE_INLINE` to have the member functions marked `always_inline` / `__forceinline`; otherwise `OMATH_FORCE_INLINE` expands to nothing.

---

## Usage

```cpp
#include <omath/containers/encrypted_variable.hpp>
#include <omath/linear_algebra/vector3.hpp>

// Declare the type once with a random 128-byte key, then construct a value of it.
using SecretPos = OMATH_DEF_CRYPT_VAR(omath::Vector3<float>, 128);

SecretPos position{omath::Vector3<float>{1.f, 2.f, 3.f}};

{
    omath::VarAnchor guard = position.drop_anchor();   // decrypted inside this scope
    position.value().x += 10.f;
    use(position.value());
}                                                       // encrypted again here

// position.value() is scrambled here; do not read it without an anchor.
```

Explicit control without the guard:

```cpp
SecretPos health{omath::Vector3<float>{100.f, 0.f, 0.f}};

health.decrypt();
const float hp = health.value().x;
health.encrypt();
```

An explicit key instead of the macro:

```cpp
constexpr std::array<std::uint8_t, 4> k{0x12, 0x34, 0x56, 0x78};
omath::EncryptedVariable<int, 4, k> counter{0};
```

---

## Notes

* The XOR covers the **whole object representation**, padding included. Use trivially copyable types (numbers, vectors, plain structs). Do not wrap types that own heap memory or hold pointers.
* `OMATH_DEF_CRYPT_VAR` creates a **type**; the key is part of that type. Do not put it in a header that several translation units include, because `__COUNTER__`, `__TIME__` and `__FILE__` differ per translation unit and the type would no longer be the same in each.
* `VarAnchor` takes a reference; the encrypted variable has to outlive it.
* The object is not thread-safe: two threads holding anchors on the same variable race on the flag and the bytes.
* While decrypted, the value is in plain memory, so the protection only covers the time spent outside an anchor. Keep anchor scopes short.
* The unit test (`tests/general/unit_test_var_encryption.cpp`) checks that the value matches inside an anchor and differs after it.

---

## See also

* [Pattern scanning](../utility/pattern_scan.md) — the kind of search this obfuscation is meant to make harder
* [Hashing helpers](../utility/hashing.md)
