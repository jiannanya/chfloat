# chfloat benchmark report

Implementation: current headers.

Cached powers: full (10,416 bytes).

Arithmetic: platform default.

Thread affinity mask: 0 (0 means not pinned).

Current/reference timings alternate within each run; paired speedup is the median of reference/current time ratios.

Reference float_parse.h SHA-256: `a405fd5cd6141d9c289d0f4722f34098f1b529de40dfd8487a962b714c35a310`.

Compiler: Clang 17.0.0 (clang-1700.6.4.2)

Build: Release; C++17; seed=12345; n=50000; iters=20; runs=15.

Each parser is checked against std::from_chars before timing. Mismatches include value bits, error codes and consumed length. Error outputs are normalized. All parsers use one non-inlined timing loop with the same function-pointer call boundary. Timings include indirect calls and result consumption; each run has identical packed input. Heap storage below is dataset capacity, excluding reference results and process/runtime overhead.

## mixed

Input bytes: 651939; dataset allocated bytes: 1101939.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.028496 | 0.027289 | 0.029839 | 35.093 | 436.371 | 0 | 0.992x |
| reference_chfloat<double> | 0.028215 | 0.026606 | 0.029264 | 35.442 | 440.710 | 0 | - |
| std::from_chars<double> | 0.043262 | 0.042009 | 0.044036 | 23.115 | 287.431 | 0 | - |
| std::strtod | 0.035396 | 0.034027 | 0.036495 | 28.252 | 351.304 | 0 | - |
| chfloat<float> | 0.028773 | 0.027379 | 0.029574 | 34.755 | 432.164 | 0 | 0.977x |
| reference_chfloat<float> | 0.027985 | 0.027169 | 0.028655 | 35.734 | 444.341 | 0 | - |
| std::from_chars<float> | 0.052164 | 0.051269 | 0.053785 | 19.170 | 238.376 | 0 | - |
| std::strtof | 0.032718 | 0.031485 | 0.033501 | 30.564 | 380.054 | 0 | - |

## short_no_exp

Input bytes: 283824; dataset allocated bytes: 733824.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.014984 | 0.013574 | 0.016241 | 66.738 | 361.285 | 0 | 1.172x |
| reference_chfloat<double> | 0.017572 | 0.016954 | 0.018474 | 56.910 | 308.083 | 0 | - |
| std::from_chars<double> | 0.023231 | 0.022514 | 0.023544 | 43.046 | 233.031 | 0 | - |
| std::strtod | 0.023362 | 0.022483 | 0.023928 | 42.804 | 231.722 | 0 | - |
| chfloat<float> | 0.014883 | 0.013937 | 0.016311 | 67.190 | 363.733 | 0 | 1.163x |
| reference_chfloat<float> | 0.017535 | 0.016962 | 0.018330 | 57.030 | 308.733 | 0 | - |
| std::from_chars<float> | 0.024693 | 0.023976 | 0.025842 | 40.497 | 219.230 | 0 | - |
| std::strtof | 0.023015 | 0.022278 | 0.023912 | 43.450 | 235.220 | 0 | - |

## long_frac

Input bytes: 1458100; dataset allocated bytes: 1908100.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.036070 | 0.034991 | 0.038082 | 27.724 | 771.039 | 0 | 1.018x |
| reference_chfloat<double> | 0.036959 | 0.035327 | 0.038591 | 27.057 | 752.474 | 0 | - |
| std::from_chars<double> | 0.062378 | 0.060871 | 0.064280 | 16.031 | 445.848 | 0 | - |
| std::strtod | 0.049868 | 0.048634 | 0.051168 | 20.053 | 557.695 | 0 | - |
| chfloat<float> | 0.036781 | 0.035890 | 0.037590 | 27.188 | 756.130 | 0 | 1.002x |
| reference_chfloat<float> | 0.036706 | 0.035898 | 0.038078 | 27.244 | 757.674 | 0 | - |
| std::from_chars<float> | 0.099682 | 0.093899 | 0.101778 | 10.032 | 278.998 | 0 | - |
| std::strtof | 0.045226 | 0.043795 | 0.045808 | 22.111 | 614.929 | 0 | - |

## wide_range

Input bytes: 2285570; dataset allocated bytes: 2735570.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.041057 | 0.039372 | 0.042139 | 24.357 | 1061.792 | 0 | 1.016x |
| reference_chfloat<double> | 0.042082 | 0.040116 | 0.043111 | 23.763 | 1035.925 | 0 | - |
| std::from_chars<double> | 2.136308 | 2.060626 | 2.173818 | 0.468 | 20.406 | 0 | - |
| std::strtod | 0.059403 | 0.058629 | 0.060900 | 16.834 | 733.863 | 0 | - |
| chfloat<float> | 0.036926 | 0.035126 | 0.038289 | 27.081 | 1180.562 | 0 | 1.014x |
| reference_chfloat<float> | 0.037464 | 0.036589 | 0.038926 | 26.692 | 1163.606 | 0 | - |
| std::from_chars<float> | 0.135756 | 0.134064 | 0.138869 | 7.366 | 321.118 | 0 | - |
| std::strtof | 0.049912 | 0.048934 | 0.050486 | 20.035 | 873.416 | 0 | - |

## integer_decimal

Input bytes: 969083; dataset allocated bytes: 1419083.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<int64> | 0.008834 | 0.008089 | 0.009098 | 113.198 | 2092.327 | 0 | 1.021x |
| reference_chfloat<int64> | 0.008952 | 0.008564 | 0.009800 | 111.710 | 2064.824 | 0 | - |
| std::from_chars<int64> | 0.015907 | 0.015643 | 0.016525 | 62.866 | 1162.006 | 0 | - |

## integer_hex

Input bytes: 818480; dataset allocated bytes: 1268480.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<int64> | 0.008334 | 0.008018 | 0.008713 | 119.992 | 1873.221 | 0 | 1.440x |
| reference_chfloat<int64> | 0.012054 | 0.011607 | 0.012457 | 82.961 | 1295.120 | 0 | - |
| std::from_chars<int64> | 0.048058 | 0.046379 | 0.049515 | 20.808 | 324.839 | 0 | - |

## leading_zeroes

Input bytes: 13294664; dataset allocated bytes: 13744664.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.032388 | 0.031941 | 0.033465 | 30.876 | 7829.297 | 0 | 1.024x |
| reference_chfloat<double> | 0.033202 | 0.032630 | 0.033866 | 30.119 | 7637.388 | 0 | - |
| std::from_chars<double> | 0.338015 | 0.333742 | 0.342041 | 2.958 | 750.190 | 0 | - |
| std::strtod | 0.107008 | 0.105702 | 0.110357 | 9.345 | 2369.687 | 0 | - |
| chfloat<float> | 0.032369 | 0.031706 | 0.033234 | 30.894 | 7833.852 | 0 | 1.011x |
| reference_chfloat<float> | 0.032570 | 0.031986 | 0.033602 | 30.703 | 7785.657 | 0 | - |
| std::from_chars<float> | 0.289576 | 0.282507 | 0.294188 | 3.453 | 875.678 | 0 | - |
| std::strtof | 0.108047 | 0.105204 | 0.111201 | 9.255 | 2346.893 | 0 | - |

## large_exponent

Input bytes: 13392895; dataset allocated bytes: 13842895.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.044502 | 0.043316 | 0.046199 | 22.471 | 5740.222 | 0 | 1.007x |
| reference_chfloat<double> | 0.044838 | 0.044276 | 0.046464 | 22.302 | 5697.111 | 0 | - |
| std::from_chars<double> | 0.487047 | 0.474710 | 0.497806 | 2.053 | 524.485 | 0 | - |
| std::strtod | 0.221406 | 0.214002 | 0.241792 | 4.517 | 1153.759 | 0 | - |
| chfloat<float> | 0.044918 | 0.043005 | 0.047533 | 22.263 | 5686.959 | 0 | 1.025x |
| reference_chfloat<float> | 0.045389 | 0.043879 | 0.047788 | 22.032 | 5628.019 | 0 | - |
| std::from_chars<float> | 0.502114 | 0.492700 | 0.506952 | 1.992 | 508.747 | 0 | - |
| std::strtof | 0.238147 | 0.231515 | 0.239326 | 4.199 | 1072.653 | 0 | - |

## zero_exponent

Input bytes: 13444664; dataset allocated bytes: 13894664.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.036714 | 0.036055 | 0.037574 | 27.238 | 6984.765 | 0 | 1.021x |
| reference_chfloat<double> | 0.037367 | 0.036414 | 0.038081 | 26.761 | 6862.619 | 0 | - |
| std::from_chars<double> | 0.520696 | 0.501727 | 0.544244 | 1.921 | 492.488 | 0 | - |
| std::strtod | 0.339871 | 0.334010 | 0.348313 | 2.942 | 754.511 | 0 | - |
| chfloat<float> | 0.035383 | 0.034995 | 0.036769 | 28.262 | 7247.427 | 0 | 1.052x |
| reference_chfloat<float> | 0.037274 | 0.036147 | 0.038020 | 26.829 | 6879.841 | 0 | - |
| std::from_chars<float> | 0.525621 | 0.500164 | 0.548999 | 1.903 | 487.873 | 0 | - |
| std::strtof | 0.317805 | 0.308435 | 0.345826 | 3.147 | 806.900 | 0 | - |

## integer_overflow

Input bytes: 13769083; dataset allocated bytes: 14219083.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<int64> | 0.021225 | 0.020809 | 0.021800 | 47.114 | 12373.329 | 0 | 1.015x |
| reference_chfloat<int64> | 0.021562 | 0.021107 | 0.021779 | 46.378 | 12179.942 | 0 | - |
| std::from_chars<int64> | 0.091634 | 0.089443 | 0.093780 | 10.913 | 2866.014 | 0 | - |

## integer_zeroes

Input bytes: 13769083; dataset allocated bytes: 14219083.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<int64> | 0.018534 | 0.017887 | 0.019173 | 53.954 | 14169.522 | 0 | 0.989x |
| reference_chfloat<int64> | 0.018357 | 0.018027 | 0.019075 | 54.474 | 14306.110 | 0 | - |
| std::from_chars<int64> | 0.049912 | 0.047785 | 0.052631 | 20.035 | 5261.797 | 0 | - |

## midpoint_tail

Input bytes: 11682581; dataset allocated bytes: 12132581.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.078004 | 0.076443 | 0.079483 | 12.820 | 2856.615 | 0 | 1.030x |
| reference_chfloat<double> | 0.079961 | 0.078635 | 0.081790 | 12.506 | 2786.704 | 0 | - |
| std::from_chars<double> | 2.890074 | 2.868851 | 2.929380 | 0.346 | 77.101 | 0 | - |
| std::strtod | 0.446734 | 0.441198 | 0.464780 | 2.238 | 498.793 | 0 | - |
| chfloat<float> | 0.063842 | 0.062801 | 0.065095 | 15.664 | 3490.288 | 0 | 1.025x |
| reference_chfloat<float> | 0.065395 | 0.064613 | 0.066477 | 15.292 | 3407.416 | 0 | - |
| std::from_chars<float> | 3.090102 | 2.979270 | 5.525857 | 0.324 | 72.110 | 0 | - |
| std::strtof | 0.294729 | 0.290409 | 0.301519 | 3.393 | 756.043 | 0 | - |

Throughput is specific to this workload, compiler and machine.
