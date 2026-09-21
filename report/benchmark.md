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
| chfloat<double> | 0.027861 | 0.027214 | 0.028344 | 35.893 | 446.319 | 0 | 0.994x |
| reference_chfloat<double> | 0.027623 | 0.027086 | 0.028513 | 36.202 | 450.164 | 0 | - |
| std::from_chars<double> | 0.043315 | 0.042667 | 0.044150 | 23.087 | 287.076 | 0 | - |
| std::strtod | 0.035360 | 0.034260 | 0.036269 | 28.280 | 351.657 | 0 | - |
| chfloat<float> | 0.028077 | 0.026958 | 0.028477 | 35.616 | 442.879 | 0 | 0.989x |
| reference_chfloat<float> | 0.027767 | 0.026760 | 0.028676 | 36.014 | 447.829 | 0 | - |
| std::from_chars<float> | 0.052883 | 0.051348 | 0.053709 | 18.910 | 235.136 | 0 | - |
| std::strtof | 0.032685 | 0.032063 | 0.033414 | 30.595 | 380.440 | 0 | - |

## short_no_exp

Input bytes: 283824; dataset allocated bytes: 733824.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.018694 | 0.017746 | 0.018978 | 53.492 | 289.578 | 0 | 0.999x |
| reference_chfloat<double> | 0.018612 | 0.018219 | 0.019150 | 53.730 | 290.867 | 0 | - |
| std::from_chars<double> | 0.023492 | 0.022917 | 0.023952 | 42.568 | 230.441 | 0 | - |
| std::strtod | 0.024424 | 0.024027 | 0.025019 | 40.943 | 221.644 | 0 | - |
| chfloat<float> | 0.018409 | 0.017684 | 0.019307 | 54.322 | 294.071 | 0 | 1.014x |
| reference_chfloat<float> | 0.018813 | 0.018150 | 0.019814 | 53.156 | 287.760 | 0 | - |
| std::from_chars<float> | 0.025877 | 0.024894 | 0.027123 | 38.644 | 209.200 | 0 | - |
| std::strtof | 0.024164 | 0.023321 | 0.027005 | 41.383 | 224.028 | 0 | - |

## long_frac

Input bytes: 1458100; dataset allocated bytes: 1908100.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.036757 | 0.035752 | 0.039369 | 27.205 | 756.611 | 0 | 1.021x |
| reference_chfloat<double> | 0.037369 | 0.036258 | 0.038748 | 26.760 | 744.227 | 0 | - |
| std::from_chars<double> | 0.062000 | 0.061121 | 0.063496 | 16.129 | 448.564 | 0 | - |
| std::strtod | 0.049291 | 0.048882 | 0.050341 | 20.288 | 564.225 | 0 | - |
| chfloat<float> | 0.036033 | 0.034773 | 0.036882 | 27.753 | 771.832 | 0 | 1.020x |
| reference_chfloat<float> | 0.037044 | 0.035996 | 0.038520 | 26.995 | 750.763 | 0 | - |
| std::from_chars<float> | 0.099305 | 0.098580 | 0.101532 | 10.070 | 280.057 | 0 | - |
| std::strtof | 0.044934 | 0.044014 | 0.046411 | 22.255 | 618.928 | 0 | - |

## wide_range

Input bytes: 2285570; dataset allocated bytes: 2735570.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.041002 | 0.039540 | 0.042057 | 24.389 | 1063.213 | 0 | 1.033x |
| reference_chfloat<double> | 0.042203 | 0.041656 | 0.043148 | 23.695 | 1032.953 | 0 | - |
| std::from_chars<double> | 2.188265 | 2.133302 | 3.432211 | 0.457 | 19.922 | 0 | - |
| std::strtod | 0.060399 | 0.059350 | 0.061866 | 16.557 | 721.762 | 0 | - |
| chfloat<float> | 0.037410 | 0.036706 | 0.038171 | 26.731 | 1165.294 | 0 | 1.025x |
| reference_chfloat<float> | 0.038585 | 0.037001 | 0.040099 | 25.917 | 1129.811 | 0 | - |
| std::from_chars<float> | 0.137675 | 0.134782 | 0.139859 | 7.263 | 316.642 | 0 | - |
| std::strtof | 0.050703 | 0.049964 | 0.051522 | 19.723 | 859.789 | 0 | - |

## integer_decimal

Input bytes: 969083; dataset allocated bytes: 1419083.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<int64> | 0.008941 | 0.008580 | 0.009228 | 111.839 | 2067.211 | 0 | 1.015x |
| reference_chfloat<int64> | 0.009005 | 0.008772 | 0.009182 | 111.045 | 2052.538 | 0 | - |
| std::from_chars<int64> | 0.016267 | 0.015981 | 0.016678 | 61.474 | 1136.264 | 0 | - |

## integer_hex

Input bytes: 818480; dataset allocated bytes: 1268480.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<int64> | 0.008483 | 0.008206 | 0.008727 | 117.880 | 1840.255 | 0 | 1.439x |
| reference_chfloat<int64> | 0.012045 | 0.011811 | 0.013770 | 83.021 | 1296.070 | 0 | - |
| std::from_chars<int64> | 0.048248 | 0.045872 | 0.050032 | 20.726 | 323.561 | 0 | - |

## leading_zeroes

Input bytes: 13294664; dataset allocated bytes: 13744664.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.033093 | 0.032481 | 0.033962 | 30.218 | 7662.524 | 0 | 1.007x |
| reference_chfloat<double> | 0.033247 | 0.032714 | 0.033979 | 30.078 | 7626.955 | 0 | - |
| std::from_chars<double> | 0.351462 | 0.340676 | 0.484113 | 2.845 | 721.487 | 0 | - |
| std::strtod | 0.111343 | 0.108284 | 0.114960 | 8.981 | 2277.428 | 0 | - |
| chfloat<float> | 0.034106 | 0.033816 | 0.034464 | 29.320 | 7434.899 | 0 | 0.998x |
| reference_chfloat<float> | 0.033993 | 0.033738 | 0.034550 | 29.417 | 7459.559 | 0 | - |
| std::from_chars<float> | 0.300621 | 0.291990 | 0.322088 | 3.326 | 843.505 | 0 | - |
| std::strtof | 0.108535 | 0.105974 | 0.110938 | 9.214 | 2336.347 | 0 | - |

## large_exponent

Input bytes: 13392895; dataset allocated bytes: 13842895.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.043629 | 0.042250 | 0.052848 | 22.921 | 5855.043 | 0 | 1.024x |
| reference_chfloat<double> | 0.044643 | 0.043512 | 0.046337 | 22.400 | 5722.049 | 0 | - |
| std::from_chars<double> | 0.500625 | 0.497760 | 0.508061 | 1.998 | 510.261 | 0 | - |
| std::strtod | 0.238666 | 0.232762 | 0.241179 | 4.190 | 1070.322 | 0 | - |
| chfloat<float> | 0.043759 | 0.042859 | 0.044514 | 22.852 | 5837.605 | 0 | 1.017x |
| reference_chfloat<float> | 0.044419 | 0.043594 | 0.045685 | 22.513 | 5750.851 | 0 | - |
| std::from_chars<float> | 0.504739 | 0.501638 | 0.507888 | 1.981 | 506.102 | 0 | - |
| std::strtof | 0.235345 | 0.232282 | 0.237735 | 4.249 | 1085.425 | 0 | - |

## zero_exponent

Input bytes: 13444664; dataset allocated bytes: 13894664.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.036185 | 0.035676 | 0.037182 | 27.636 | 7086.862 | 0 | 1.044x |
| reference_chfloat<double> | 0.037654 | 0.036996 | 0.038850 | 26.558 | 6810.387 | 0 | - |
| std::from_chars<double> | 0.546431 | 0.525667 | 0.560133 | 1.830 | 469.294 | 0 | - |
| std::strtod | 0.345473 | 0.339503 | 0.352328 | 2.895 | 742.276 | 0 | - |
| chfloat<float> | 0.036597 | 0.035103 | 0.037665 | 27.325 | 7007.071 | 0 | 1.022x |
| reference_chfloat<float> | 0.037486 | 0.036693 | 0.038549 | 26.677 | 6840.902 | 0 | - |
| std::from_chars<float> | 0.554468 | 0.545451 | 0.574554 | 1.804 | 462.491 | 0 | - |
| std::strtof | 0.344892 | 0.333431 | 0.352279 | 2.899 | 743.528 | 0 | - |

## integer_overflow

Input bytes: 13769083; dataset allocated bytes: 14219083.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<int64> | 0.021666 | 0.021333 | 0.022133 | 46.154 | 12121.267 | 0 | 1.003x |
| reference_chfloat<int64> | 0.021711 | 0.021242 | 0.022155 | 46.060 | 12096.608 | 0 | - |
| std::from_chars<int64> | 0.094727 | 0.093473 | 0.098107 | 10.557 | 2772.440 | 0 | - |

## integer_zeroes

Input bytes: 13769083; dataset allocated bytes: 14219083.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<int64> | 0.018883 | 0.018339 | 0.019577 | 52.959 | 13908.320 | 0 | 1.000x |
| reference_chfloat<int64> | 0.019107 | 0.018265 | 0.019740 | 52.336 | 13744.602 | 0 | - |
| std::from_chars<int64> | 0.051710 | 0.050277 | 0.054397 | 19.339 | 5078.785 | 0 | - |

## midpoint_tail

Input bytes: 11682581; dataset allocated bytes: 12132581.

| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| chfloat<double> | 0.078803 | 0.077895 | 0.080921 | 12.690 | 2827.643 | 0 | 1.029x |
| reference_chfloat<double> | 0.081160 | 0.079601 | 0.083166 | 12.321 | 2745.538 | 0 | - |
| std::from_chars<double> | 2.949197 | 2.937434 | 2.971556 | 0.339 | 75.555 | 0 | - |
| std::strtod | 0.469274 | 0.464783 | 0.471977 | 2.131 | 474.835 | 0 | - |
| chfloat<float> | 0.063838 | 0.063071 | 0.065302 | 15.665 | 3490.516 | 0 | 1.034x |
| reference_chfloat<float> | 0.066013 | 0.065386 | 0.067045 | 15.149 | 3375.523 | 0 | - |
| std::from_chars<float> | 3.257591 | 3.231353 | 5.992830 | 0.307 | 68.403 | 0 | - |
| std::strtof | 0.318096 | 0.312994 | 0.323751 | 3.144 | 700.503 | 0 | - |

Throughput is specific to this workload, compiler and machine.
