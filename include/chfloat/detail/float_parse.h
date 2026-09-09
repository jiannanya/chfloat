#pragma once

#include <chfloat/detail/pow5_table.h>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#if (defined(_M_X64) || defined(__x86_64__)) && !defined(CHFLOAT_FORCE_PORTABLE)
#include <xmmintrin.h>
#endif
#if defined(_MSC_VER) && defined(_M_X64) && !defined(CHFLOAT_FORCE_PORTABLE)
#include <intrin.h>
#endif

namespace chfloat { namespace detail {
#if defined(_MSC_VER)
#define CHFLOAT_FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CHFLOAT_FORCEINLINE inline __attribute__((always_inline))
#else
#define CHFLOAT_FORCEINLINE inline
#endif
#if defined(_MSC_VER)
#define CHFLOAT_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define CHFLOAT_NOINLINE __attribute__((noinline))
#else
#define CHFLOAT_NOINLINE
#endif
using u64 = std::uint64_t;
using u32 = std::uint32_t;
using i64 = std::int64_t;
struct fp_chars_result { const char* ptr; int ec; };
struct u128 { u64 hi, lo; };

inline int lz64(u64 x) noexcept {
#if defined(_MSC_VER) && defined(_M_X64) && !defined(CHFLOAT_FORCE_PORTABLE)
  unsigned long index;
  if (!_BitScanReverse64(&index, x)) return 64;
  return 63 - static_cast<int>(index);
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(CHFLOAT_FORCE_PORTABLE)
  return x ? __builtin_clzll(x) : 64;
#else
  if (!x) return 64;
  int n = 0;
  while (!(x & (u64(1) << 63))) { ++n; x <<= 1; }
  return n;
#endif
}
inline u128 mul_64x64_to_128(u64 a, u64 b) noexcept {
#if defined(_MSC_VER) && defined(_M_X64) && !defined(CHFLOAT_FORCE_PORTABLE)
  u64 hi;
  const auto lo = _umul128(a, b, &hi);
  return {hi, lo};
#elif defined(__SIZEOF_INT128__) && !defined(CHFLOAT_FORCE_PORTABLE)
  const __uint128_t p = static_cast<__uint128_t>(a) * b;
  return {static_cast<u64>(p >> 64), static_cast<u64>(p)};
#else
  const u64 p0 = u64(u32(a)) * u32(b), p1 = u64(u32(a)) * (b >> 32);
  const u64 p2 = (a >> 32) * u32(b), p3 = (a >> 32) * (b >> 32);
  const u64 mid = (p0 >> 32) + u32(p1) + u32(p2);
  return {p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32), (mid << 32) | u32(p0)};
#endif
}
inline u64 load_u64_unaligned(const char* p) noexcept {
#if defined(CHFLOAT_FORCE_PORTABLE)
  u64 v = 0;
  for (int i = 0; i < 8; ++i) v |= u64(static_cast<unsigned char>(p[i])) << (i * 8);
  return v;
#else
  u64 v;
  std::memcpy(&v, p, sizeof(v));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  v = __builtin_bswap64(v);
#endif
  return v;
#endif
}
inline bool all_8_digits(u64 v) noexcept {
  return (((v + 0x4646464646464646ULL) | (v - 0x3030303030303030ULL)) & 0x8080808080808080ULL) == 0;
}
inline u32 parse_8_digits(u64 v) noexcept {
  v -= 0x3030303030303030ULL;
  v = (v * 10 + (v >> 8)) & 0x00ff00ff00ff00ffULL;
  v = (v * 100 + (v >> 16)) & 0x0000ffff0000ffffULL;
  return u32(v * 10000 + (v >> 32));
}
inline unsigned digit(char c) noexcept { return unsigned(static_cast<unsigned char>(c)) - '0'; }
inline bool is_digit(char c) noexcept { return digit(c) < 10; }
inline i64 saturated_add(i64 a, i64 b) noexcept {
  constexpr i64 hi = (std::numeric_limits<i64>::max)(), lo = (std::numeric_limits<i64>::min)();
  if (b > 0 && a > hi - b) return hi;
  if (b < 0 && a < lo - b) return lo;
  return a + b;
}
inline const char* read_exponent(const char* p, const char* last, i64& exponent) noexcept {
  bool neg = false;
  if (p != last && (*p == '+' || *p == '-')) { neg = *p == '-'; ++p; }
  if (p == last || !is_digit(*p)) return nullptr;
  constexpr i64 cap = (std::numeric_limits<i64>::max)();
  // The first two digits cannot overflow, and cover common scientific input.
  i64 value = digit(*p++);
  if (p != last && is_digit(*p)) {
    value = value * 10 + digit(*p++);
    while (p != last && is_digit(*p)) {
      const unsigned d = digit(*p++);
      value = value < cap / 10 || (value == cap / 10 && d <= unsigned(cap % 10))
                  ? value * 10 + d : cap;
    }
  }
  exponent = saturated_add(exponent, neg ? -value : value);
  return p;
}

struct decimal {
  u64 mant = 0;
  i64 exp10 = 0;
  const char* ptr;
  int digits = 0;
  bool truncated = false;
  bool valid = false;
};
CHFLOAT_FORCEINLINE decimal finish_decimal(decimal d, const char* p, const char* last, unsigned fmt) noexcept {
  bool has_exponent = false;
  if (d.valid && fmt != 2 && p != last && (*p == 'e' || *p == 'E')) {
    if (const char* end = read_exponent(p + 1, last, d.exp10)) {
      p = end;
      has_exponent = true;
    }
  }
  if (fmt == 1 && !has_exponent) d.valid = false;
  d.ptr = p;
  return d;
}
CHFLOAT_FORCEINLINE decimal scan_decimal(const char* p, const char* last, unsigned fmt) noexcept {
  decimal d{};
  const char* integer = p;
  // Leading zeroes never consume the significant-digit budget.
  while (p != last && *p == '0') ++p;
  if (last - p <= 19) {
    // The bounded range cannot overflow the significand; omit per-digit budget
    // checks and tail scanning for short numbers.
    const char* significant = p;
    while (p != last && is_digit(*p)) d.mant = d.mant * 10 + digit(*p++);
    d.digits = int(p - significant);
    d.valid = p != integer;
    if (p != last && *p == '.') {
      const char* fraction = ++p;
      if (!d.mant) {
        while (p != last && *p == '0') ++p;
      }
      const char* significant_fraction = p;
      while (p != last && is_digit(*p)) d.mant = d.mant * 10 + digit(*p++);
      d.digits += int(p - significant_fraction);
      d.exp10 = fraction - p;
      d.valid |= p != fraction;
    }
    return finish_decimal(d, p, last, fmt);
  }
  while (last - p >= 8 && d.digits <= 11) {
    const u64 word = load_u64_unaligned(p);
    if (!all_8_digits(word)) break;
    d.mant = d.mant * 100000000 + parse_8_digits(word);
    d.digits += 8;
    p += 8;
  }
  while (p != last && is_digit(*p) && d.digits < 19) {
    d.mant = d.mant * 10 + digit(*p++);
    ++d.digits;
  }
  const char* dropped = p;
  while (last - p >= 8 && all_8_digits(load_u64_unaligned(p))) {
    d.truncated |= load_u64_unaligned(p) != 0x3030303030303030ULL;
    p += 8;
  }
  while (p != last && is_digit(*p)) d.truncated |= *p++ != '0';
  d.exp10 = p - dropped;
  bool any = p != integer;
  if (p != last && *p == '.') {
    const char* fraction = ++p;
    if (d.mant == 0) {
      const char* zeroes = p;
      while (p != last && *p == '0') ++p;
      d.exp10 -= p - zeroes;
    }
    while (last - p >= 8 && d.digits <= 11) {
      const u64 word = load_u64_unaligned(p);
      if (!all_8_digits(word)) break;
      d.mant = d.mant * 100000000 + parse_8_digits(word);
      d.digits += 8;
      d.exp10 -= 8;
      p += 8;
    }
    while (p != last && is_digit(*p) && d.digits < 19) {
      d.mant = d.mant * 10 + digit(*p++);
      ++d.digits;
      --d.exp10;
    }
    // Discarded fractional digits do not change the prefix's exponent.
    while (last - p >= 8 && all_8_digits(load_u64_unaligned(p))) {
      d.truncated |= load_u64_unaligned(p) != 0x3030303030303030ULL;
      p += 8;
    }
    while (p != last && is_digit(*p)) d.truncated |= *p++ != '0';
    any |= p != fraction;
  }
  d.valid = any;
  return finish_decimal(d, p, last, fmt);
}

template <class T> struct binary_traits {
  static_assert(std::numeric_limits<T>::is_iec559 && std::numeric_limits<T>::radix == 2,
                "chfloat requires IEEE-754 binary32/binary64");
  static_assert((sizeof(T) == 4 && std::numeric_limits<T>::digits == 24) ||
                (sizeof(T) == 8 && std::numeric_limits<T>::digits == 53),
                "chfloat supports only binary32 and binary64 storage");
  using uint = std::conditional_t<std::is_same<T, double>::value, u64, u32>;
  static constexpr int precision = std::numeric_limits<T>::digits;
  static constexpr int fraction = precision - 1;
  static constexpr int bias = std::numeric_limits<T>::max_exponent - 1;
  static constexpr u64 hidden = u64(1) << fraction;
  static constexpr u64 infinity = u64(2 * bias + 1) << fraction;
  static constexpr u64 sign = u64(1) << (sizeof(T) * 8 - 1);
  static constexpr int min_q = std::is_same<T, double>::value ? -342 : -64;
  static constexpr int max_q = std::is_same<T, double>::value ? 308 : 38;
};
template <class T> inline T from_bits(u64 bits) noexcept {
  const typename binary_traits<T>::uint u = static_cast<typename binary_traits<T>::uint>(bits);
  T value;
  std::memcpy(&value, &u, sizeof(value));
  return value;
}
inline bool nearest_rounding_without_traps() noexcept {
#if (defined(_M_X64) || (defined(__x86_64__) && defined(__SSE2_MATH__))) && !defined(CHFLOAT_FORCE_PORTABLE) && !defined(__FAST_MATH__) && !defined(_M_FP_FAST)
  // Check the actual SSE rounding mode and exception masks on every call.
  // Directed rounding or unmasked exceptions use the integer conversion path.
  return (_mm_getcsr() & 0x7f80u) == 0x1f80u;
#else
  return false;
#endif
}
struct binary { u64 bits; bool uncertain; bool tail_ambiguous = false; };
template <class T, bool NonzeroExponent = false>
CHFLOAT_FORCEINLINE binary compute_binary(int q, u64 w, bool truncated = false) noexcept {
  using B = binary_traits<T>;
  const int z = lz64(w);
  if constexpr (!NonzeroExponent) {
    if (q == 0) {
      const int top = 63 - z;
      const int shift = top - B::fraction;
      u64 m = shift <= 0 ? w << -shift : w >> shift;
      if (shift > 0) {
        const u64 remainder = w & ((u64(1) << shift) - 1);
        const u64 halfway = u64(1) << (shift - 1);
        m += remainder > halfway || (remainder == halfway && (truncated || (m & 1)));
      }
      return {(u64(top + B::bias) << B::fraction) + m - B::hidden, false};
    }
  }
  const u64 normalized = w << z;
  const auto& c = pow5_table[q - pow5_smallest_q];
  auto p = mul_64x64_to_128(normalized, c.hi);
  constexpr u64 mask = ~u64(0) >> (B::fraction + 3);
  bool uncertain = false;
  if ((p.hi & mask) == mask) {
#if defined(CHFLOAT_COMPACT)
    uncertain = true;
#else
    const auto tail = mul_64x64_to_128(normalized, c.lo);
    const u64 low = p.lo + tail.hi;
    p.hi += low < p.lo;
    p.lo = low;
    uncertain = low == ~u64(0);
#endif
  }
  const int upper = int(p.hi >> 63);
  const int shift = upper + 64 - B::fraction - 3;
  u64 m = p.hi >> shift;
  // Floor division is explicit: right-shifting negative signed integers is
  // implementation-defined in C++17.
  const int product = 217706 * q;
  int e = (product >= 0 ? product / 65536 : -((-product + 65535) / 65536)) + 63 + upper - z + B::bias;
  // A discarded decimal tail is strictly between 0 and 1 mantissa unit.
  // In this normalized product it can advance the high word by at most 2^z;
  // one extra unit covers the cached-product approximation. A 19-digit
  // prefix has z <= 4, much smaller than a binary32/64 rounding interval.
  const u64 tail_margin = truncated ? (u64(1) << z) + 1 : 0;
  if (e <= 0) {
    const int total_shift = shift - e + 2;
    if (total_shift > 64) return {0, false, total_shift == 65 && truncated && ~p.hi < tail_margin};
    const u64 remainder = total_shift == 64 ? p.hi : p.hi & ((u64(1) << total_shift) - 1);
    const u64 halfway = u64(1) << (total_shift - 1);
    m = total_shift == 64 ? 0 : p.hi >> total_shift;
    m += remainder >= halfway;
    // Only a product close to a midpoint needs the full decimal comparison.
    return {m, uncertain || remainder == halfway,
        (truncated && remainder < halfway && halfway - remainder <= tail_margin)};
  }
  // A prefix already at or above 2^max_exponent cannot return to the finite
  // range when its positive decimal tail is restored.
  if (e >= 2 * B::bias + 1) return {B::infinity, false};
  bool tail_ambiguous = false;
  if (truncated) {
    const u64 remainder = p.hi & ((u64(1) << (shift + 1)) - 1);
    const u64 halfway = u64(1) << shift;
    tail_ambiguous = remainder <= halfway && halfway - remainder <= tail_margin;
  }
  constexpr int min_tie = B::fraction == 52 ? -4 : -17;
  constexpr int max_tie = B::fraction == 52 ? 23 : 10;
  if ((m & 3) == 1 && q >= min_tie && q <= max_tie && p.lo <= 1 && (m << shift) == p.hi) m &= ~u64(1);
  m = (m + (m & 1)) >> 1;
  if (m >= 2 * B::hidden) { m = B::hidden; ++e; }
  if (e >= 2 * B::bias + 1) return {B::infinity, uncertain, tail_ambiguous};
  return {(u64(e) << B::fraction) | (m & (B::hidden - 1)), uncertain, tail_ambiguous};
}

// Compare the entire decimal input with the exact midpoint above `lower`.
// Binary64 midpoints fit 768 decimal digits; binary32 fits 113. Scratch storage
// is 86 base-1e9 limbs (344 bytes) or 13 limbs (52 bytes), independent of input.
CHFLOAT_NOINLINE inline int compare_decimal_midpoint(const decimal& d, const char* first,
    const char* end, u64 coefficient, int power, u32* limbs) noexcept {
  const int scale = power < 0 ? power : 0;
  int used = 0;
  for (u64 n = coefficient; n; n /= 1000000000) limbs[used++] = u32(n % 1000000000);
  auto multiply = [&](u32 factor) {
    u64 carry = 0;
    for (int i = 0; i < used; ++i) {
      const u64 n = u64(limbs[i]) * factor + carry;
      limbs[i] = u32(n % 1000000000);
      carry = n / 1000000000;
    }
    if (carry) limbs[used++] = u32(carry);
  };
  if (power < 0) {
    power = -power;
    while (power >= 12) { multiply(244140625); power -= 12; }
    u32 factor = 1;
    while (power--) factor *= 5;
    multiply(factor);
  } else {
    while (power >= 29) { multiply(u32(1) << 29); power -= 29; }
    multiply(u32(1) << power);
  }
  u32 divisor = 1;
  int top_digits = 1;
  while (limbs[used - 1] / divisor >= 10) { divisor *= 10; ++top_digits; }
  const i64 order = (used - 1) * 9 + top_digits + scale;
  const i64 input_order = d.exp10 + d.digits; // Called only for bounded exp10.
  if (input_order != order) return input_order < order ? -1 : 1;
  const char* p = first;
  while (p != end && (*p == '0' || *p == '.')) ++p;
  for (int i = used - 1; i >= 0; --i) {
    u32 n = limbs[i];
    do {
      if (p != end && *p == '.') ++p;
      const unsigned a = p == end ? 0 : digit(*p++);
      const unsigned b = n / divisor;
      if (a != b) return a < b ? -1 : 1;
      n %= divisor;
      divisor /= 10;
    } while (divisor);
    divisor = 100000000;
  }
  while (p != end) { if (*p != '.' && *p != '0') return 1; ++p; }
  return 0;
}
template <class T> inline int compare_midpoint(const decimal& d, const char* first, const char* end, u64 lower) noexcept {
  using B = binary_traits<T>;
  const int field = int(lower >> B::fraction);
  const u64 significand = (lower & (B::hidden - 1)) | (field ? B::hidden : 0);
  const u64 coefficient = 2 * significand + 1;
  int power = (field ? field : 1) - B::bias - B::fraction - 1;
  u32 limbs[B::precision == 24 ? 13 : 86];
  return compare_decimal_midpoint(d, first, end, coefficient, power, limbs);
}

// Keep the bounded scratch buffer out of the common path's stack frame.
template <class T> CHFLOAT_NOINLINE inline u64 correct_rounding(const decimal& d, const char* first, u64 bits,
                                                              bool check_lower) noexcept {
  using B = binary_traits<T>;
  const char* end = first;
  while (end != d.ptr && (is_digit(*end) || *end == '.')) ++end;
  if (check_lower && bits != 0) {
    const int below = compare_midpoint<T>(d, first, end, bits - 1);
    if (below < 0 || (below == 0 && (bits & 1))) return bits - 1;
  }
  if (bits != B::infinity) {
    const int above = compare_midpoint<T>(d, first, end, bits);
    if (above > 0 || (above == 0 && (bits & 1))) return bits + 1;
  }
  return bits;
}
template <class T> CHFLOAT_NOINLINE inline bool tail_changes_rounding(int q, u64 w, u64 bits) noexcept {
  // q == 0 is handled exactly using the decimal sticky bit in the caller.
  const auto upper = compute_binary<T, true>(q, w + 1);
  return upper.uncertain || upper.bits != bits;
}

inline unsigned hex_digit(char c) noexcept {
  const unsigned d = digit(c);
  if (d < 10) return d;
  const unsigned a = (unsigned(static_cast<unsigned char>(c)) | 0x20u) - 'a';
  return a < 6 ? a + 10 : 16;
}
template <class T> inline fp_chars_result parse_hex(const char* first, const char* p, const char* last,
                                                   bool neg, T& value) noexcept {
  using B = binary_traits<T>;
  u64 mant = 0;
  i64 exponent = 0;
  int digits = 0;
  bool any = false, point = false, sticky = false;
  while (p != last) {
    const unsigned d = hex_digit(*p);
    if (d >= 16) {
      if (*p == '.' && !point) { point = true; ++p; continue; }
      break;
    }
    any = true;
    if (digits < 16) {
      mant = (mant << 4) | d;
      if (mant) ++digits;
      if (point) exponent = saturated_add(exponent, -4);
    } else {
      sticky |= d != 0;
      if (!point) exponent = saturated_add(exponent, 4);
    }
    ++p;
  }
  if (!any) return {first, 1};
  if (p != last && (*p == 'p' || *p == 'P')) {
    if (const char* end = read_exponent(p + 1, last, exponent)) p = end;
  }
  u64 bits = 0;
  if (mant) {
    const int length = 64 - lz64(mant);
    const i64 top = saturated_add(exponent, length - 1);
    if (top > B::bias) return {p, 2};
    if (top < 1 - B::bias - B::precision) return {p, 2};
    const bool subnormal = top < 1 - B::bias;
    const int shift = subnormal ? int((1 - B::bias - B::fraction) - exponent) : length - B::precision;
    u64 rounded;
    if (shift <= 0) {
      rounded = mant << -shift;
    } else {
      rounded = shift == 64 ? 0 : mant >> shift;
      const u64 remainder = shift == 64 ? mant : mant & ((u64(1) << shift) - 1);
      const u64 half = u64(1) << (shift - 1);
      rounded += remainder > half || (remainder == half && (sticky || (rounded & 1)));
    }
    bits = subnormal ? rounded : (u64(top + B::bias) << B::fraction) + rounded - B::hidden;
    if (!bits || bits >= B::infinity) return {p, 2};
  }
  value = from_bits<T>(bits | (neg ? B::sign : 0));
  return {p, 0};
}
inline bool iequal(const char* p, const char* literal, int n) noexcept {
  for (int i = 0; i < n; ++i) if ((static_cast<unsigned char>(p[i]) | 0x20) != literal[i]) return false;
  return true;
}
template <class T> CHFLOAT_NOINLINE inline fp_chars_result parse_special(const char* first, const char* p,
    const char* last, bool neg, T& value) noexcept {
  using B = binary_traits<T>;
  if (last - p >= 3) {
    u64 special = 0;
    if (iequal(p, "inf", 3)) {
      special = B::infinity;
      p += last - p >= 8 && iequal(p, "infinity", 8) ? 8 : 3;
    } else if (iequal(p, "nan", 3)) {
      special = B::infinity | (B::hidden >> 1);
      p += 3;
      if (p != last && *p == '(') {
        const char* tail = p + 1;
        while (tail != last && (is_digit(*tail) || *tail == '_' ||
               (unsigned(static_cast<unsigned char>(*tail) | 0x20u) - 'a') < 26)) ++tail;
        if (tail != last && *tail == ')') p = tail + 1;
      }
    }
    if (special) {
      value = from_bits<T>(special | (neg ? B::sign : 0));
      return {p, 0};
    }
  }
  return {first, 1};
}
template <class T, unsigned Fmt> inline fp_chars_result parse_fp(const char* first, const char* last, T& value) noexcept {
  using B = binary_traits<T>;
  if (first == last) return {first, 1};
  const char* p = first;
  bool neg = false;
  if (*p == '-' || *p == '+') { neg = *p == '-'; if (++p == last) return {first, 1}; }
  const unsigned initial = static_cast<unsigned char>(*p) | 0x20u;
  if (initial == 'i' || initial == 'n') return parse_special(first, p, last, neg, value);
  if constexpr (Fmt == 3) {
    return parse_hex(first, p, last, neg, value);
  } else {
    const decimal d = scan_decimal(p, last, Fmt);
    if (!d.valid) return {first, 1};
    if (d.mant <= 99999999 && u64(d.exp10) + 2 <= 2 && nearest_rounding_without_traps()) {
      // w/10 and w/100 with w < 1e8 are separated from a binary32 midpoint
      // by far more than a binary64 rounding error, unless exactly equal.
      // Exact midpoint values here are also exactly representable in binary64.
      double v = static_cast<double>(d.mant);
      if (d.exp10 == -1) v /= 10.0;
      else if (d.exp10 == -2) v /= 100.0;
      const T rounded = static_cast<T>(v);
      value = neg ? -rounded : rounded;
      return {d.ptr, 0};
    }
    u64 bits = 0;
    if (d.mant) {
      if (d.exp10 < B::min_q || d.exp10 > B::max_q) return {d.ptr, 2};
      const auto b = compute_binary<T>(int(d.exp10), d.mant, d.truncated);
      bits = b.bits;
      if (b.uncertain || (b.tail_ambiguous && tail_changes_rounding<T>(int(d.exp10), d.mant, bits)))
        bits = correct_rounding<T>(d, p, bits, b.uncertain);
      if (bits == 0 || bits == B::infinity) return {d.ptr, 2};
    }
    value = from_bits<T>(bits | (neg ? B::sign : 0));
    return {d.ptr, 0};
  }
}
template <class T> CHFLOAT_FORCEINLINE fp_chars_result dispatch_fp(const char* first, const char* last, T& value, unsigned fmt) noexcept {
  switch (fmt) {
    case 0: return parse_fp<T, 0>(first, last, value);
    case 1: return parse_fp<T, 1>(first, last, value);
    case 2: return parse_fp<T, 2>(first, last, value);
    case 3: return parse_fp<T, 3>(first, last, value);
    default: return {first, 1};
  }
}
#undef CHFLOAT_FORCEINLINE
#undef CHFLOAT_NOINLINE
} } // namespace chfloat::detail
