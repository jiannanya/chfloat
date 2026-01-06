# chfloat

`chfloat` is a lightweight and superfast **C++17** numeric parsing library.

Primary goal: provide a convenient, zero-allocation API for parsing **float/double** and **integers** from byte buffers.

Note: **integers** api is no optimized so far.

## Features

- Float/double parsing: `chfloat::from_chars` (pointer-range API)

  - Supports `general` format only
  - Supports specials: `nan`, `inf`, `infinity` (ASCII, case-insensitive)
- Integer parsing: `chfloat::from_chars` (base 2..36)
- Whitespace skipping variants: `chfloat::from_chars_ws` (ASCII-only leading whitespace)
- Small utility: `chfloat::parse_digit`

## Files

- Public API: `include/chfloat/chfloat.h`
- Tests: `test/test_main.cpp`
- Benchmarks: `benchmark/benchmark_main.cpp`
- Benchmark report output: `report/benchmark.md`

## Build & run

From repository root:

```bash
cmake -S chfloat -B chfloat/build -DCHFLOAT_BUILD_TESTS=ON -DCHFLOAT_BUILD_BENCHMARKS=ON
# For performance numbers, build Release:
cmake --build chfloat/build -j --config Release
ctest --test-dir chfloat/build

# Benchmark (writes report/benchmark.md relative to the current working dir)
cd chfloat
../chfloat/build/Release/chfloat_benchmark --n 100000 --iters 10
```

If you use Ninja (single-config):

```bash
cmake -S chfloat -B chfloat/buildn -G Ninja -DCHFLOAT_BUILD_TESTS=ON -DCHFLOAT_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build chfloat/buildn
cd chfloat
../chfloat/buildn/chfloat_benchmark --n 100000 --iters 10
```

## API sketch

```cpp
#include <chfloat/chfloat.h>

double x = 0;
auto r = chfloat::from_chars("3.14", "3.14" + 4, x);

// r.ptr points one-past-the-last consumed character.
// r.ec is chfloat::errc::{ok, invalid_argument, result_out_of_range}.

int v = 0;
auto ri = chfloat::from_chars("42", "42" + 2, v);
```

## Notes

- `from_chars` is strict (no leading whitespace). Use `from_chars_ws` if you want ASCII whitespace skipping.
- `chars_format` is currently accepted for API shape, but only `chars_format::general` is supported for floats.
- Locale is not used (ASCII only).


# chfloat benchmark report

Environment:

- os: Windows 10.0.26100
- cpu: 12th Gen Intel(R) Core(TM) i9-12900K
- logical_cores: 24
- ram_total_gib: 31.7474

- C++: C++17
- Build: Release
- Compiler: MSVC _MSC_VER=1938
- Baselines: chfloat + std::strtod/strtof
- Comparison: fast_float

## Scenario: mixed

- Inputs: n=100000, iters=10

### One-shot

| Name                           | Seconds  | Items/s  | MB/s   |
| ------------------------------ | -------- | -------- | ------ |
| chfloat::from_chars<double>    | 0.041219 | 24260362 | 301.88 |
| fast_float::from_chars<double> | 0.045157 | 22144961 | 275.56 |
| std::strtod                    | 0.109733 | 9113012  | 113.40 |
| chfloat::from_chars<float>     | 0.042506 | 23526256 | 292.74 |
| fast_float::from_chars<float>  | 0.044511 | 22466407 | 279.56 |
| std::strtof                    | 0.101200 | 9881394  | 122.96 |


### Stable (median)

- Runs: 7 (median seconds)

| Name                           | Seconds  | Items/s  | MB/s   |
| ------------------------------ | -------- | -------- | ------ |
| chfloat::from_chars<double>    | 0.043332 | 23077473 | 287.16 |
| fast_float::from_chars<double> | 0.044973 | 22235514 | 276.68 |
| std::strtod                    | 0.109155 | 9161318  | 114.00 |
| chfloat::from_chars<float>     | 0.039408 | 25375880 | 315.76 |
| fast_float::from_chars<float>  | 0.044134 | 22658217 | 281.94 |
| std::strtof                    | 0.101789 | 9824273  | 122.25 |


## Scenario: short_no_exp

- Inputs: n=100000, iters=10

### One-shot

| Name                           | Seconds  | Items/s  | MB/s   |
| ------------------------------ | -------- | -------- | ------ |
| chfloat::from_chars<double>    | 0.021758 | 45960529 | 248.16 |
| fast_float::from_chars<double> | 0.027960 | 35764867 | 193.11 |
| std::strtod                    | 0.062113 | 16099689 | 86.93  |
| chfloat::from_chars<float>     | 0.020416 | 48980951 | 264.47 |
| fast_float::from_chars<float>  | 0.029505 | 33892790 | 183.00 |
| std::strtof                    | 0.060510 | 16526085 | 89.23  |


### Stable (median)

- Runs: 7 (median seconds)

| Name                           | Seconds  | Items/s  | MB/s   |
| ------------------------------ | -------- | -------- | ------ |
| chfloat::from_chars<double>    | 0.020735 | 48228565 | 260.41 |
| fast_float::from_chars<double> | 0.028000 | 35714031 | 192.84 |
| std::strtod                    | 0.062791 | 15925748 | 85.99  |
| chfloat::from_chars<float>     | 0.020386 | 49053272 | 264.86 |
| fast_float::from_chars<float>  | 0.028636 | 34921078 | 188.56 |
| std::strtof                    | 0.060731 | 16465974 | 88.91  |


## Scenario: long_frac

- Inputs: n=100000, iters=10

### One-shot

| Name                           | Seconds  | Items/s  | MB/s   |
| ------------------------------ | -------- | -------- | ------ |
| chfloat::from_chars<double>    | 0.050735 | 19710453 | 397.37 |
| fast_float::from_chars<double> | 0.052813 | 18934732 | 381.73 |
| std::strtod                    | 0.124360 | 8041184  | 162.11 |
| chfloat::from_chars<float>     | 0.050101 | 19959761 | 402.40 |
| fast_float::from_chars<float>  | 0.054490 | 18352025 | 369.98 |
| std::strtof                    | 0.114427 | 8739173  | 176.19 |


### Stable (median)

- Runs: 7 (median seconds)

| Name                           | Seconds  | Items/s  | MB/s   |
| ------------------------------ | -------- | -------- | ------ |
| chfloat::from_chars<double>    | 0.051676 | 19351493 | 390.13 |
| fast_float::from_chars<double> | 0.053283 | 18767571 | 378.36 |
| std::strtod                    | 0.124231 | 8049547  | 162.28 |
| chfloat::from_chars<float>     | 0.049899 | 20040361 | 404.02 |
| fast_float::from_chars<float>  | 0.053638 | 18643673 | 375.86 |
| std::strtof                    | 0.114725 | 8716458  | 175.73 |



Notes:

- Items/s counts parsed numbers; MB/s counts input bytes processed.
- This benchmark is single-threaded and measures throughput on this machine.
- The 'Stable' table reports median seconds across multiple runs.

