# `omath::hashing` — FNV-1a, CRC-32 and Base64, usable at compile time

> Header: `omath/hashing.hpp`
> Namespace: `omath::hashing`
> Depends on: standard library only (`<string_view>`, `<string>`, `<expected>`)
> Purpose: small `constexpr` hash and encoding helpers, for example to turn names into integer keys at compile time

---

## API

```cpp
namespace omath::hashing {

enum class Base64Error { INVALID_LENGTH, INVALID_CHARACTER, INVALID_PADDING };

[[nodiscard]] constexpr std::uint64_t fnv1a(std::string_view value) noexcept;
[[nodiscard]] constexpr std::uint32_t crc32(std::string_view value) noexcept;

[[nodiscard]] constexpr std::string base64_encode(std::string_view value);
[[nodiscard]] constexpr std::expected<std::string, Base64Error> base64_decode(std::string_view value);

} // namespace omath::hashing
```

---

## Behavior

### `fnv1a`

64-bit FNV-1a: starts from the offset basis `14695981039346656037`, then for each byte XORs it in and multiplies by the prime `1099511628211`. Bytes are read as `unsigned char`, so the result does not depend on whether `char` is signed. The empty string hashes to the offset basis.

Known value, verified by the unit tests: `fnv1a("hello") == 0xA430D84680AABD0B`.

### `crc32`

Standard CRC-32 (IEEE 802.3, reflected polynomial `0xEDB88320`, initial value and final XOR `0xFFFFFFFF`), computed bit by bit with no lookup table.

Known value: `crc32("123456789") == 0xCBF43926`.

### `base64_encode`

RFC 4648 standard alphabet (`A-Z a-z 0-9 + /`) with `=` padding. No line wrapping and no URL-safe variant. Input is treated as raw bytes, so any `std::string_view` works, including ones with embedded zero bytes.

### `base64_decode`

Returns the decoded bytes as a `std::string`, or a `Base64Error`:

| Error | Raised when |
|---|---|
| `INVALID_LENGTH` | The input length is not a multiple of 4 (padding is required). |
| `INVALID_CHARACTER` | A character outside the alphabet appears, or `=` appears in one of the first two positions of a group. |
| `INVALID_PADDING` | `=` appears anywhere but the end of the last group, or the unused trailing bits are not zero (non-canonical encoding such as `Zm9=`). |

The empty string decodes to the empty string. Decoding is strict: whitespace and newlines are rejected, not skipped.

---

## Usage

Compile-time string hashing:

```cpp
#include <omath/hashing.hpp>

constexpr auto k_weapon_hash = omath::hashing::fnv1a("weapon_ak47");

void on_item(std::string_view name)
{
    switch (omath::hashing::fnv1a(name))
    {
        case k_weapon_hash: /* ... */ break;
        case omath::hashing::fnv1a("weapon_awp"): /* ... */ break;
        default: break;
    }
}

static_assert(omath::hashing::fnv1a("hello") == 0xA430D84680AABD0BULL);
static_assert(omath::hashing::crc32("123456789") == 0xCBF43926U);
```

Round-trip Base64 with error handling:

```cpp
const std::string encoded = omath::hashing::base64_encode("foobar");   // "Zm9vYmFy"

const auto decoded = omath::hashing::base64_decode(encoded);
if (decoded)
    use(*decoded);                                   // "foobar"
else if (decoded.error() == omath::hashing::Base64Error::INVALID_LENGTH)
    report("truncated input");
```

---

## Notes

* The functions are `constexpr`, so they run at compile time when the argument is a constant expression and as normal functions otherwise. `base64_encode` and `base64_decode` return `std::string`, so using them in a constant expression needs a toolchain with `constexpr` `std::string`; the unit tests do this with `static_assert`.
* FNV-1a and CRC-32 are not cryptographic hashes. Do not use them for passwords or integrity against an attacker.
* All functions take a `std::string_view`. A string literal converts to a view that stops at its first `\0`; build the view with an explicit length (`std::string_view{data, size}`) to hash bytes past a zero.
* The same FNV-1a constants are used by the key generator in [`EncryptedVariable`](../containers/encrypted_variable.md) (`omath::detail::fnv1a_64`), but that is a separate `consteval` helper.

---

## See also

* [`EncryptedVariable`](../containers/encrypted_variable.md)
* [Pattern scanning](pattern_scan.md)
