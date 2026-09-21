# chfloat

A header-only **C++17** parser for IEEE-754 `float`/`double` and integral types. Parsing uses bounded byte ranges, ASCII syntax, constant auxiliary storage, and no heap allocations or locale-dependent conversion functions.

## API

```cpp
#include <chfloat/chfloat.h>

const char text[] = "-12.5e2,rest";
double value = 0;
auto r = chfloat::from_chars(text, text + sizeof(text) - 1, value);
// value == -1250; r.ec == chfloat::errc::ok; *r.ptr == ','

const char hex[] = "1.8p+2";
float f = 0;
chfloat::from_chars(hex, hex + sizeof(hex) - 1, f, chfloat::chars_format::hex);
// f == 6

const char integer[] = " \tff!";
unsigned short number = 0;
chfloat::from_chars_ws(integer, integer + sizeof(integer) - 1, number, 16);
// number == 255
```

- `from_chars(first, last, value[, format/base])` returns `{ptr, ec}`.
- Floating formats: `general` (default), `fixed`, `scientific`, and `hex`.
- Integers: integral types except `bool`, bases 2 through 36, with destination-specific range checks. This includes `char`, `short`, `int`, `long`, and `long long`, signed and unsigned.
- `from_chars_ws` supports the same types and options, skipping ASCII space, tab, LF, CR, FF, and VT.
- `parse_digit(character, unsigned_value)` recognizes ASCII digits and preserves the output on failure.

### Parsing contract

| Case | Behavior |
|---|---|
| Success | `ec == errc::ok`; `ptr` points past the consumed prefix |
| Invalid syntax / base / format | `invalid_argument`; output unchanged; `ptr == first` |
| Overflow or nonzero input rounding to zero | `result_out_of_range`; output unchanged; entire valid numeric prefix consumed |
| Representable subnormal | Success |
| Floating rounding | Nearest representable value, ties to even; independent of the active floating-point rounding mode |
| Whitespace | Rejected by `from_chars`; skipped by `from_chars_ws` |
| Signs | Floating and signed integer parsers accept `-` and `+`; unsigned parsers reject both |
| Specials | Case-insensitive `inf`, `infinity`, `nan`, and complete `nan(payload)` with ASCII letters, digits, or `_`; sign preserved; payload bits are not interpreted |
| Decimal exponent | Optional in `general`, required in `scientific`, not consumed in `fixed` |
| Hexadecimal | No `0x` prefix; hexadecimal significand with optional binary `p` exponent |
| Incomplete exponent | Left unconsumed in `general`/`hex`; invalid in `scientific` |

`first` and `last` must describe a valid forward range in one buffer; null termination is unnecessary. Empty ranges, including `(nullptr, nullptr)`, are invalid input. For whitespace variants, failure positions are relative to the range after whitespace was skipped. Trailing input is intentionally allowed: check `r.ptr == last` when complete consumption is required.

The `errc` and `chars_format` enum values belong to chfloat and must not be cast from the corresponding standard-library enums. Parsing preserves the active floating-point rounding mode and exception masks, but may set masked status flags such as inexact.

## Build and test

From this directory, with MSVC:

```sh
cmake -S . -B build -DCHFLOAT_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
build/Release/chfloat_tests 1000000
```

With Ninja and GCC/Clang:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
```

Tests cover differential parsing against `std::from_chars`, random binary round trips, independently generated exact decimal midpoints, all integer bases, malformed byte buffers, signed zero, long inputs, rounding modes, portable arithmetic, compact tables, heap allocation counting, and concurrent parsing against a single-threaded reference. A comparison against `std::from_chars` is skipped when the standard library returns a demonstrably non-conforming result, such as stopping in the middle of a digit run or reporting a non-finite value for finite input; the skip count is printed so the exclusions stay visible. The test executables require a standard library with C++17 floating `from_chars` and `to_chars`; the library itself does not depend on these functions.

For address and undefined-behavior sanitizers on Linux:

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCHFLOAT_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

On Windows this option enables ASan only. The installed sanitizer runtime must support the host Windows version. The allocation-counting executable runs separately without sanitizer interposition.

## Configuration

| Option | Description |
|---|---|
| `CHFLOAT_COMPACT=1` | Select the compact cached-power table |
| `CHFLOAT_FORCE_PORTABLE=1` | Use portable scalar operations instead of platform intrinsics |

Define these macros before including the header. Compact mode can also be enabled with `-DCHFLOAT_COMPACT=ON` in CMake. **Use the same macro configuration in every translation unit.**

The parser has no mutable global state; threads may parse separate inputs and destinations concurrently. It reads the hardware floating-point control register (MXCSR on x86-64, FPCR on AArch64) only to decide whether short decimals may take a guarded fast path; every other target, and `CHFLOAT_FORCE_PORTABLE=1`, uses the integer path for all inputs.

### Footprint

The cached-power table is the only static data: 10,416 bytes by default, 5,208 bytes with `CHFLOAT_COMPACT=1`. Parsing allocates nothing from the heap and consumes a constant amount of stack; the widest path, exact decimal/midpoint comparison, uses a fixed 344-byte scratch buffer for `double` (52 bytes for `float`), independent of input length.

## Integration

Include the `include/` directory directly, or use `add_subdirectory` and link `chfloat::chfloat`. An installable CMake package is also provided:

```sh
cmake --install build --config Release --prefix /path/to/prefix
```

```cmake
find_package(chfloat 0.2 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE chfloat::chfloat)
```

## Generated data

```sh
python script/gen_pow5_table.py --check
python script/gen_pow5_table.py            # deterministic regeneration
python script/gen_midpoint_tests.py        # regenerate exact midpoint test vectors
```

Neither Python nor external libraries are required to consume the header or build the checked-in tests.
