# chfloat benchmark report

Implementation: current headers.

Cached powers: full (10,416 bytes).

Thread affinity mask: 16 (0 means not pinned).

Current/reference timings alternate within each run; paired speedup is the median of reference/current time ratios.

Reference float_parse.h SHA-256: `0f645fc05300912c56b4d747e857e2dcd9f28d9b003b3a4ede99018e8ab14f81`.

Compiler: MSVC 1938

Build: Release; C++17; seed=12345; n=50000; iters=20; runs=15.

Each parser is checked against std::from_chars before timing. Mismatches include value bits, error codes and consumed length. Error outputs are normalized. All parsers use one non-inlined timing loop with the same function-pointer call boundary. Timings include indirect calls and result consumption; each run has identical packed input. Heap storage below is dataset capacity, excluding reference results and process/runtime overhead.

## mixed

Input bytes: 651939; dataset allocated bytes: 1101939.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.038167 | 0.038015 | 0.038753 | 26.201 | 325.802 | 0 | 0.983x |
| reference_chfloat<double> | 0.037573 | 0.037473 | 0.038159 | 26.615 | 330.951 | 0 | - |
| std::from_chars<double> | 0.089709 | 0.089346 | 0.090040 | 11.147 | 138.612 | 0 | - |
| std::strtod | 0.104318 | 0.101813 | 0.185952 | 9.586 | 119.200 | 0 | - |
| chfloat<float> | 0.055531 | 0.051699 | 0.071018 | 18.008 | 223.922 | 0 | 1.037x |
| reference_chfloat<float> | 0.059529 | 0.053257 | 0.065797 | 16.799 | 208.886 | 0 | - |
| std::from_chars<float> | 0.131150 | 0.121364 | 0.147956 | 7.625 | 94.813 | 0 | - |
| std::strtof | 0.135968 | 0.120870 | 0.148579 | 7.355 | 91.453 | 0 | - |

## short_no_exp

Input bytes: 283824; dataset allocated bytes: 733824.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.022130 | 0.020898 | 0.027158 | 45.188 | 244.623 | 0 | 1.266x |
| reference_chfloat<double> | 0.028515 | 0.026288 | 0.033414 | 35.069 | 189.847 | 0 | - |
| std::from_chars<double> | 0.048839 | 0.047946 | 0.059752 | 20.475 | 110.843 | 0 | - |
| std::strtod | 0.058029 | 0.056786 | 0.062373 | 17.233 | 93.290 | 0 | - |
| chfloat<float> | 0.024935 | 0.024734 | 0.026967 | 40.104 | 217.105 | 0 | 1.173x |
| reference_chfloat<float> | 0.029156 | 0.028835 | 0.032099 | 34.298 | 185.674 | 0 | - |
| std::from_chars<float> | 0.047721 | 0.047112 | 0.052509 | 20.955 | 113.442 | 0 | - |
| std::strtof | 0.056216 | 0.055386 | 0.056826 | 17.789 | 96.299 | 0 | - |

## long_frac

Input bytes: 1458100; dataset allocated bytes: 1908100.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.052305 | 0.051974 | 0.053268 | 19.119 | 531.708 | 0 | 0.997x |
| reference_chfloat<double> | 0.052137 | 0.051812 | 0.055310 | 19.180 | 533.428 | 0 | - |
| std::from_chars<double> | 0.119440 | 0.118974 | 0.142452 | 8.372 | 232.846 | 0 | - |
| std::strtod | 0.137111 | 0.135945 | 0.139033 | 7.293 | 202.835 | 0 | - |
| chfloat<float> | 0.053932 | 0.053695 | 0.055628 | 18.542 | 515.671 | 0 | 1.013x |
| reference_chfloat<float> | 0.054806 | 0.054423 | 0.056104 | 18.246 | 507.442 | 0 | - |
| std::from_chars<float> | 0.108443 | 0.107474 | 0.112002 | 9.221 | 256.458 | 0 | - |
| std::strtof | 0.124064 | 0.123093 | 0.127349 | 8.060 | 224.168 | 0 | - |

## wide_range

Input bytes: 2285570; dataset allocated bytes: 2735570.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.057862 | 0.057450 | 0.059566 | 17.283 | 753.415 | 0 | 1.264x |
| reference_chfloat<double> | 0.073183 | 0.072749 | 0.074315 | 13.664 | 595.682 | 0 | - |
| std::from_chars<double> | 0.225102 | 0.224173 | 0.230856 | 4.442 | 193.663 | 0 | - |
| std::strtod | 0.267176 | 0.263800 | 0.295326 | 3.743 | 163.165 | 0 | - |
| chfloat<float> | 0.046952 | 0.046586 | 0.048061 | 21.299 | 928.486 | 0 | 1.054x |
| reference_chfloat<float> | 0.049616 | 0.049008 | 0.050090 | 20.155 | 878.629 | 0 | - |
| std::from_chars<float> | 0.214538 | 0.210978 | 0.237148 | 4.661 | 203.198 | 0 | - |
| std::strtof | 0.251490 | 0.247650 | 0.303564 | 3.976 | 173.342 | 0 | - |

## integer_decimal

Input bytes: 969083; dataset allocated bytes: 1419083.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<int64> | 0.020098 | 0.019422 | 0.021280 | 49.755 | 919.665 | 0 | 0.980x |
| reference_chfloat<int64> | 0.019697 | 0.018598 | 0.021437 | 50.770 | 938.425 | 0 | - |
| std::from_chars<int64> | 0.018350 | 0.017584 | 0.018693 | 54.497 | 1007.313 | 0 | - |

## integer_hex

Input bytes: 818480; dataset allocated bytes: 1268480.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<int64> | 0.021518 | 0.020656 | 0.021757 | 46.473 | 725.505 | 0 | 1.010x |
| reference_chfloat<int64> | 0.021805 | 0.020812 | 0.022314 | 45.861 | 715.952 | 0 | - |
| std::from_chars<int64> | 0.015490 | 0.014985 | 0.018486 | 64.559 | 1007.855 | 0 | - |

Throughput is specific to this workload, compiler and machine.
