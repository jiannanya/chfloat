#pragma once
#ifndef CHFLOAT_FLOAT_PARSE_HPP
#define CHFLOAT_FLOAT_PARSE_HPP
// Internal floating-point parsing implementation.
//
// Exposes:
//   chfloat::detail::fp_chars_result
//   chfloat::detail::parse_fp_double
//   chfloat::detail::parse_fp_float
//
// Error codes match chfloat::errc ordinal values:
//   0 = ok, 1 = invalid_argument, 2 = result_out_of_range

#include <chfloat/detail/pow5_table.h>

#if defined(_MSC_VER)
  #include <intrin.h>
#endif

namespace chfloat {
namespace detail {

using u64 = unsigned long long;
using u32 = unsigned int;
using i32 = int;
using i64 = long long;

enum fp_ec : int {
  fp_ok = 0,
  fp_invalid_argument = 1,
  fp_result_out_of_range = 2,
};

struct fp_chars_result {
  const char* ptr;
  int ec;
};

struct u128 {
  u64 hi;
  u64 lo;
};

static inline u64 load_u64_unaligned(const char* p) noexcept {
#if defined(_MSC_VER)
  return *reinterpret_cast<const unsigned __int64 __unaligned*>(p);
#elif defined(__GNUC__) || defined(__clang__)
  u64 v;
  __builtin_memcpy(&v, p, 8);
  return v;
#else
  u64 v = 0;
  const unsigned char* s = reinterpret_cast<const unsigned char*>(p);
  for (int i = 0; i < 8; ++i) v |= (u64(s[i]) << (8 * i));
  return v;
#endif
}

static inline bool all_8_digits(u64 x) noexcept {
  // Each byte must be in ['0','9'].
  // For each byte b:
  //   (b - '0') underflows => high bit set
  //   (b + 0x46) overflows past 0x7f for b > '9' => high bit set
  const u64 a = x + 0x4646464646464646ULL;
  const u64 b = x - 0x3030303030303030ULL;
  return ((a | b) & 0x8080808080808080ULL) == 0;
}

static inline bool any_nonzero_digit_8(u64 x) noexcept {
  // Assumes all_8_digits(x) is true.
  return x != 0x3030303030303030ULL;
}

static inline int lz64(u64 x) noexcept {
  if (x == 0) return 64;
#if defined(_MSC_VER) && defined(_M_X64)
  unsigned long idx;
  _BitScanReverse64(&idx, x);
  return int(63 - idx);
#elif defined(__GNUC__) || defined(__clang__)
  return __builtin_clzll(x);
#else
  int n = 0;
  while ((x & (u64(1) << 63)) == 0) {
    ++n;
    x <<= 1;
  }
  return n;
#endif
}

static inline u128 mul_64x64_to_128(u64 a, u64 b) noexcept {
#if defined(_MSC_VER) && defined(_M_X64)
  u64 hi;
  u64 lo = _umul128(a, b, &hi);
  return {hi, lo};
#elif defined(__SIZEOF_INT128__)
  __uint128_t p = static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b);
  return {static_cast<u64>(p >> 64), static_cast<u64>(p)};
#else
  const u64 a_lo = static_cast<u32>(a);
  const u64 a_hi = a >> 32;
  const u64 b_lo = static_cast<u32>(b);
  const u64 b_hi = b >> 32;

  const u64 p0 = a_lo * b_lo;
  const u64 p1 = a_lo * b_hi;
  const u64 p2 = a_hi * b_lo;
  const u64 p3 = a_hi * b_hi;

  const u64 mid = (p0 >> 32) + (p1 & 0xffffffffULL) + (p2 & 0xffffffffULL);
  const u64 hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
  const u64 lo = (mid << 32) | (p0 & 0xffffffffULL);
  return {hi, lo};
#endif
}

// Fixed-point approximation for log2(5^q) + q.
static inline i32 approx_log2_pow5(i32 q) noexcept {
  // Uses a 16.16 fixed-point approximation to log2(5) (same numeric constant as other parsers,
  // but kept as an internal helper with different naming/structure).
  return (((152170 + 65536) * q) >> 16) + 63;
}

static inline double bits_to_double(u64 bits) noexcept {
  union {
    u64 u;
    double d;
  } v;
  v.u = bits;
  return v.d;
}

static inline float bits_to_float(u32 bits) noexcept {
  union {
    u32 u;
    float f;
  } v;
  v.u = bits;
  return v.f;
}

static inline double pow10d_exact_upto15(i32 e) noexcept {
  // Exact powers of 10 as integers are representable in binary64 up to 10^15 (since 10^15 < 2^53).
  // Preconditions: 0 <= e <= 15.
  static constexpr double tbl[16] = {
      1.0,
      10.0,
      100.0,
      1000.0,
      10000.0,
      100000.0,
      1000000.0,
      10000000.0,
      100000000.0,
      1000000000.0,
      10000000000.0,
      100000000000.0,
      1000000000000.0,
      10000000000000.0,
      100000000000000.0,
      1000000000000000.0,
  };
  return tbl[static_cast<u32>(e)];
}

static inline double pow10d_i32(i32 e) noexcept {
  // Small table for float-path scaling: compute in double, then cast to float.
  // This keeps correct rounding for typical "mixed" inputs, and avoids the expensive pow5-table path.
  // Valid for e in [-38, 38]. Call sites ensure bounds.
  static constexpr double tbl[77] = {
      1e-38, 1e-37, 1e-36, 1e-35, 1e-34, 1e-33, 1e-32, 1e-31, 1e-30, 1e-29, 1e-28,
      1e-27, 1e-26, 1e-25, 1e-24, 1e-23, 1e-22, 1e-21, 1e-20, 1e-19, 1e-18, 1e-17,
      1e-16, 1e-15, 1e-14, 1e-13, 1e-12, 1e-11, 1e-10, 1e-9,  1e-8,  1e-7,  1e-6,
      1e-5,  1e-4,  1e-3,  1e-2,  1e-1,  1e0,   1e1,   1e2,   1e3,   1e4,   1e5,
      1e6,   1e7,   1e8,   1e9,   1e10,  1e11,  1e12,  1e13,  1e14,  1e15,  1e16,
      1e17,  1e18,  1e19,  1e20,  1e21,  1e22,  1e23,  1e24,  1e25,  1e26,  1e27,
      1e28,  1e29,  1e30,  1e31,  1e32,  1e33,  1e34,  1e35,  1e36,  1e37,  1e38,
  };
  return tbl[static_cast<u32>(e + 38)];
}

static inline bool ascii_ieq3(const char* p, const char* lit3) noexcept {
  // case-insensitive compare for 3 chars
  for (int i = 0; i < 3; ++i) {
    unsigned char a = static_cast<unsigned char>(p[i]);
    unsigned char b = static_cast<unsigned char>(lit3[i]);
    if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b - 'A' + 'a');
    if (a != b) return false;
  }
  return true;
}

static inline bool ascii_ieq8(const char* p, const char* lit8) noexcept {
  for (int i = 0; i < 8; ++i) {
    unsigned char a = static_cast<unsigned char>(p[i]);
    unsigned char b = static_cast<unsigned char>(lit8[i]);
    if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b - 'A' + 'a');
    if (a != b) return false;
  }
  return true;
}

struct dec64 {
  u64 mant;
  i32 exp10;
  bool neg;
  bool exact;
  const char* ptr;
  int ec;
};

static inline bool is_digit(char c) noexcept {
  return static_cast<unsigned char>(c) >= static_cast<unsigned char>('0') &&
         static_cast<unsigned char>(c) <= static_cast<unsigned char>('9');
}

static inline unsigned digit_u8(unsigned char c) noexcept {
  return static_cast<unsigned>(c) - static_cast<unsigned>('0');
}

static inline const char* scan_digits_fast(const char* p, const char* last, i32& count, bool& any_nonzero) noexcept {
  // Scan a run of digits, counting how many and whether any digit is non-zero.
  count = 0;
  any_nonzero = false;

  while ((last - p) >= 8) {
    const u64 w = load_u64_unaligned(p);
    if (!all_8_digits(w)) break;
    count += 8;
    any_nonzero |= any_nonzero_digit_8(w);
    p += 8;
  }
  while (p < last) {
    const unsigned d = digit_u8(static_cast<unsigned char>(*p));
    if (d > 9) break;
    ++count;
    any_nonzero |= (d != 0);
    ++p;
  }
  return p;
}

static inline dec64 parse_decimal_19_impl(const char* p, const char* last, bool neg) noexcept {
  dec64 r{};
  r.ptr = p;
  r.ec = fp_invalid_argument;
  r.exact = false;

  if (p == last) return r;

  u64 mant = 0;
  i32 exp10 = 0;
  bool any = false;
  int sig = 0;

  bool dropped = false;
  unsigned dropped_first = 0;
  bool dropped_tail = false;

  while (p < last) {
    unsigned d = digit_u8(static_cast<unsigned char>(*p));
    if (d > 9) break;
    any = true;
    if (sig < 19) {
      mant = mant * 10ULL + static_cast<u64>(d);
      ++sig;
      ++p;
      continue;
    }
    // first dropped digit
    dropped = true;
    dropped_first = d;
    ++exp10;
    ++p;
    // bulk-scan remaining integer digits
    i32 n = 0;
    bool nz = false;
    const char* np = scan_digits_fast(p, last, n, nz);
    exp10 += n;
    dropped_tail |= nz;
    p = np;
    break;
  }

  if (p < last && *p == '.') {
    ++p;
    while (p < last) {
      unsigned d = digit_u8(static_cast<unsigned char>(*p));
      if (d > 9) break;
      any = true;
      if (sig < 19) {
        mant = mant * 10ULL + static_cast<u64>(d);
        ++sig;
        --exp10;
        ++p;
        continue;
      }
      if (!dropped) {
        dropped = true;
        dropped_first = d;
      } else {
        dropped_tail |= (d != 0);
      }
      --exp10;
      ++p;
      // bulk-scan remaining fractional digits
      i32 n = 0;
      bool nz = false;
      const char* np = scan_digits_fast(p, last, n, nz);
      exp10 -= n;
      dropped_tail |= nz;
      p = np;
      break;
    }
  }

  if (!any) {
    r.ec = fp_invalid_argument;
    return r;
  }

  if (p < last && (*p == 'e' || *p == 'E')) {
    const char* epos = p;
    ++p;
    bool eneg = false;
    if (p < last && (*p == '-' || *p == '+')) {
      eneg = (*p == '-');
      ++p;
    }
    if (p == last || !is_digit(*p)) {
      p = epos;
    } else {
      // Fast path: exponent is almost always 1–2 digits in our benchmarks.
      i32 e = static_cast<i32>(digit_u8(static_cast<unsigned char>(*p++)));
      if (p < last) {
        unsigned d1 = digit_u8(static_cast<unsigned char>(*p));
        if (d1 <= 9) {
          e = e * 10 + static_cast<i32>(d1);
          ++p;
          // Rare fallback: 3+ digits.
          while (p < last) {
            unsigned d = digit_u8(static_cast<unsigned char>(*p));
            if (d > 9) break;
            if (e < 10000) e = e * 10 + static_cast<i32>(d);
            ++p;
          }
        }
      }
      if (eneg) e = -e;
      exp10 = static_cast<i32>(exp10 + e);
    }
  }

  if (dropped) {
    const bool round_up = (dropped_first > 5) || (dropped_first == 5 && (dropped_tail || (mant & 1ULL)));
    if (round_up) {
      ++mant;
      constexpr u64 p10 = 10000000000000000000ULL; // 10^19
      if (mant == p10) {
        mant = 1000000000000000000ULL; // 10^18
        ++exp10;
      }
    }
  }

  r.mant = mant;
  r.exp10 = exp10;
  r.neg = neg;
  r.exact = !dropped;
  r.ptr = p;
  r.ec = fp_ok;
  return r;
}

static inline dec64 parse_decimal_19(const char* first, const char* last) noexcept {
  dec64 r{};
  r.ptr = first;
  r.ec = fp_invalid_argument;

  const char* p = first;
  if (p == last) return r;

  bool neg = false;
  if (*p == '-') {
    neg = true;
    ++p;
  } else if (*p == '+') {
    ++p;
  }

  r = parse_decimal_19_impl(p, last, neg);
  if (r.ec != fp_ok) {
    r.ptr = first;
  }
  return r;
}

static inline dec64 parse_decimal_10_impl(const char* p, const char* last, bool neg) noexcept {
  // Same parser but capped at 10 significant digits (float-friendly).
  dec64 r{};
  r.ptr = p;
  r.ec = fp_invalid_argument;
  r.exact = false;

  if (p == last) return r;

  u64 mant = 0;
  i32 exp10 = 0;
  bool any = false;
  int sig = 0;

  bool dropped = false;
  unsigned dropped_first = 0;
  bool dropped_tail = false;

  while (p < last) {
    unsigned d = digit_u8(static_cast<unsigned char>(*p));
    if (d > 9) break;
    any = true;
    if (sig < 10) {
      mant = mant * 10ULL + static_cast<u64>(d);
      ++sig;
      ++p;
      continue;
    }
    dropped = true;
    dropped_first = d;
    ++exp10;
    ++p;
    i32 n = 0;
    bool nz = false;
    const char* np = scan_digits_fast(p, last, n, nz);
    exp10 += n;
    dropped_tail |= nz;
    p = np;
    break;
  }

  if (p < last && *p == '.') {
    ++p;
    while (p < last) {
      unsigned d = digit_u8(static_cast<unsigned char>(*p));
      if (d > 9) break;
      any = true;
      if (sig < 10) {
        mant = mant * 10ULL + static_cast<u64>(d);
        ++sig;
        --exp10;
        ++p;
        continue;
      }
      if (!dropped) {
        dropped = true;
        dropped_first = d;
      } else {
        dropped_tail |= (d != 0);
      }
      --exp10;
      ++p;

      i32 n = 0;
      bool nz = false;
      const char* np = scan_digits_fast(p, last, n, nz);
      exp10 -= n;
      dropped_tail |= nz;
      p = np;
      break;
    }
  }

  if (!any) {
    r.ec = fp_invalid_argument;
    return r;
  }

  if (p < last && (*p == 'e' || *p == 'E')) {
    const char* epos = p;
    ++p;
    bool eneg = false;
    if (p < last && (*p == '-' || *p == '+')) {
      eneg = (*p == '-');
      ++p;
    }
    if (p == last || !is_digit(*p)) {
      p = epos;
    } else {
      i32 e = static_cast<i32>(digit_u8(static_cast<unsigned char>(*p++)));
      if (p < last) {
        unsigned d1 = digit_u8(static_cast<unsigned char>(*p));
        if (d1 <= 9) {
          e = e * 10 + static_cast<i32>(d1);
          ++p;
          while (p < last) {
            unsigned d = digit_u8(static_cast<unsigned char>(*p));
            if (d > 9) break;
            if (e < 10000) e = e * 10 + static_cast<i32>(d);
            ++p;
          }
        }
      }
      if (eneg) e = -e;
      exp10 = static_cast<i32>(exp10 + e);
    }
  }

  if (dropped) {
    const bool round_up = (dropped_first > 5) || (dropped_first == 5 && (dropped_tail || (mant & 1ULL)));
    if (round_up) {
      ++mant;
      constexpr u64 p10 = 10000000000ULL; // 10^10
      if (mant == p10) {
        mant = 1000000000ULL; // 10^9
        ++exp10;
      }
    }
  }

  r.mant = mant;
  r.exp10 = exp10;
  r.neg = neg;
  r.exact = !dropped;
  r.ptr = p;
  r.ec = fp_ok;
  return r;
}

static inline dec64 parse_decimal_10(const char* first, const char* last) noexcept {
  dec64 r{};
  r.ptr = first;
  r.ec = fp_invalid_argument;

  const char* p = first;
  if (p == last) return r;

  bool neg = false;
  if (*p == '-') {
    neg = true;
    ++p;
  } else if (*p == '+') {
    ++p;
  }

  r = parse_decimal_10_impl(p, last, neg);
  if (r.ec != fp_ok) {
    r.ptr = first;
  }
  return r;
}

struct bin64 {
  u64 mant;
  i32 exp;
};

struct bin32 {
  u32 mant;
  i32 exp;
};

static inline bin64 build_binary64_q0(u64 w) noexcept {
  // Exact integer -> binary64 conversion with round-to-nearest-even.
  // Preconditions: w != 0.
  i32 e2 = static_cast<i32>(63 - lz64(w));
  if (e2 <= 52) {
    const u64 m = w << (52 - e2);
    return {m & ((1ULL << 52) - 1ULL), e2 + 1023};
  }

  const int shift = int(e2 - 52);
  u64 m = w >> shift;
  const u64 rem = w & ((1ULL << shift) - 1ULL);
  const u64 halfway = 1ULL << (shift - 1);
  if (rem > halfway || (rem == halfway && (m & 1ULL))) {
    ++m;
    if (m == (1ULL << 53)) {
      m >>= 1;
      ++e2;
    }
  }

  return {m & ((1ULL << 52) - 1ULL), e2 + 1023};
}

static inline bin32 build_binary32_q0(u64 w) noexcept {
  // Exact integer -> binary32 conversion with round-to-nearest-even.
  // Preconditions: w != 0.
  i32 e2 = static_cast<i32>(63 - lz64(w));
  if (e2 <= 23) {
    const u64 m = w << (23 - e2);
    return {static_cast<u32>(m & ((1ULL << 23) - 1ULL)), e2 + 127};
  }

  const int shift = int(e2 - 23);
  u64 m = w >> shift;
  const u64 rem = w & ((1ULL << shift) - 1ULL);
  const u64 halfway = 1ULL << (shift - 1);
  if (rem > halfway || (rem == halfway && (m & 1ULL))) {
    ++m;
    if (m == (1ULL << 24)) {
      m >>= 1;
      ++e2;
    }
  }

  return {static_cast<u32>(m & ((1ULL << 23) - 1ULL)), e2 + 127};
}

static inline bin64 build_binary64(i32 q10, u64 w) noexcept {
  // Preconditions: w != 0, q10 in [-342, 308]
  if (q10 == 0) {
    return build_binary64_q0(w);
  }
  const int z = lz64(w);
  const u64 wnorm = w << z;

  const pow5_128& c = pow5_table[q10 - pow5_smallest_q];

  // 52-bit mantissa => need 55 bits of precision before rounding
  const u64 mask = (0xffffffffffffffffULL >> 55);

  u128 p = mul_64x64_to_128(wnorm, c.hi);
  if ((p.hi & mask) == mask) {
    u128 p2 = mul_64x64_to_128(wnorm, c.lo);
    const u64 new_lo = p.lo + p2.hi;
    const u64 carry = (new_lo < p.lo) ? 1ULL : 0ULL;
    p.lo = new_lo;
    p.hi = p.hi + carry;
  }

  const int upper = int(p.hi >> 63);
  const int shift = upper + 64 - 52 - 3;

  u64 m = p.hi >> shift;
  i32 e2 = approx_log2_pow5(q10) + upper - z - (-1023);

  if (e2 <= 0) {
    const int rshift = -e2 + 1;
    if (rshift >= 64) return {0ULL, 0};
    m >>= rshift;
    m += (m & 1ULL);
    m >>= 1;
    const i32 be = (m < (1ULL << 52)) ? 0 : 1;
    const u64 mb = (m & ((1ULL << 52) - 1ULL));
    return {mb, be};
  }

  if ((m & 3ULL) == 1ULL) {
    if ((q10 >= -4) && (q10 <= 23) && (p.lo <= 1ULL)) {
      if ((m << shift) == p.hi) m &= ~1ULL;
    }
  }

  m += (m & 1ULL);
  m >>= 1;

  if (m >= (2ULL << 52)) {
    m = (1ULL << 52);
    ++e2;
  }

  m &= ~(1ULL << 52);
  if (e2 >= 0x7FF) return {0ULL, 0x7FF};

  return {m, e2};
}

static inline bin32 build_binary32(i32 q10, u64 w) noexcept {
  // Preconditions: w != 0, q10 in [-64, 38]
  if (q10 == 0) {
    return build_binary32_q0(w);
  }
  const int z = lz64(w);
  const u64 wnorm = w << z;

  const pow5_128& c = pow5_table[q10 - pow5_smallest_q];

  const u64 mask = (0xffffffffffffffffULL >> 26);

  u128 p = mul_64x64_to_128(wnorm, c.hi);
  if ((p.hi & mask) == mask) {
    u128 p2 = mul_64x64_to_128(wnorm, c.lo);
    const u64 new_lo = p.lo + p2.hi;
    const u64 carry = (new_lo < p.lo) ? 1ULL : 0ULL;
    p.lo = new_lo;
    p.hi = p.hi + carry;
  }

  const int upper = int(p.hi >> 63);
  const int shift = upper + 64 - 23 - 3;

  u64 m = p.hi >> shift;
  i32 e2 = approx_log2_pow5(q10) + upper - z - (-127);

  if (e2 <= 0) {
    const int rshift = -e2 + 1;
    if (rshift >= 64) return {0u, 0};
    m >>= rshift;
    m += (m & 1ULL);
    m >>= 1;
    const i32 be = (m < (1ULL << 23)) ? 0 : 1;
    const u32 mb = static_cast<u32>(m & ((1ULL << 23) - 1ULL));
    return {mb, be};
  }

  if ((m & 3ULL) == 1ULL) {
    if ((q10 >= -17) && (q10 <= 10) && (p.lo <= 1ULL)) {
      if ((m << shift) == p.hi) m &= ~1ULL;
    }
  }

  m += (m & 1ULL);
  m >>= 1;

  if (m >= (2ULL << 23)) {
    m = (1ULL << 23);
    ++e2;
  }

  m &= ~(1ULL << 23);
  if (e2 >= 0xFF) return {0u, 0xFF};

  return {static_cast<u32>(m), e2};
}

fp_chars_result parse_fp_double(const char* first, const char* last, double& value) noexcept {
  // Handle optional leading sign for special tokens.
  const char* p = first;
  bool neg = false;
  if (p < last && (*p == '-' || *p == '+')) {
    neg = (*p == '-');
    ++p;
  }

  // Specials: nan/inf/infinity (ASCII, case-insensitive).
  if (p < last) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c == 'n' || c == 'N') {
      if ((last - p) >= 3 && ascii_ieq3(p, "nan")) {
        u64 bits = 0x7ff8000000000000ULL;
        if (neg) bits |= (1ULL << 63);
        value = bits_to_double(bits);
        return {p + 3, fp_ok};
      }
    } else if (c == 'i' || c == 'I') {
      if ((last - p) >= 3 && ascii_ieq3(p, "inf")) {
        u64 bits = 0x7ff0000000000000ULL;
        if (neg) bits |= (1ULL << 63);
        value = bits_to_double(bits);
        return {p + 3, fp_ok};
      }
      if ((last - p) >= 8 && ascii_ieq8(p, "infinity")) {
        u64 bits = 0x7ff0000000000000ULL;
        if (neg) bits |= (1ULL << 63);
        value = bits_to_double(bits);
        return {p + 8, fp_ok};
      }
    }
  }

  // Parse decimal number (sign already handled) using the 19-digit bounded parser.
  dec64 d = parse_decimal_19_impl(p, last, neg);
  if (d.ec != fp_ok) return {first, d.ec};

  // Fast path for common exact inputs: do an IEEE-754 multiply/divide by an *exact* power of 10.
  // For |exp10|<=15, 10^|exp10| is an exactly representable integer in binary64, so the operation
  // rounds exactly as required for decimal->binary64.
  if (d.exact && d.mant <= 9007199254740991ULL) { // 2^53-1
    const i32 e = d.exp10;

    // Special-case the short_no_exp distribution (<=6 integer digits, <=2 fractional digits):
    // avoid expensive floating-point division by 10/100.
    if (d.mant <= 99999999ULL) {
      if (e == -1) {
        const u64 q = d.mant / 10ULL;
        const u32 r = static_cast<u32>(d.mant - q * 10ULL);
        static constexpr double frac10[10] = {
            0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9,
        };
        double v = static_cast<double>(q) + frac10[r];
        if (d.neg) v = -v;
        value = v;
        return {d.ptr, fp_ok};
      }
      if (e == -2) {
        const u64 q = d.mant / 100ULL;
        const u32 r = static_cast<u32>(d.mant - q * 100ULL);
        static constexpr double frac100[100] = {
            0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08, 0.09,
            0.10, 0.11, 0.12, 0.13, 0.14, 0.15, 0.16, 0.17, 0.18, 0.19,
            0.20, 0.21, 0.22, 0.23, 0.24, 0.25, 0.26, 0.27, 0.28, 0.29,
            0.30, 0.31, 0.32, 0.33, 0.34, 0.35, 0.36, 0.37, 0.38, 0.39,
            0.40, 0.41, 0.42, 0.43, 0.44, 0.45, 0.46, 0.47, 0.48, 0.49,
            0.50, 0.51, 0.52, 0.53, 0.54, 0.55, 0.56, 0.57, 0.58, 0.59,
            0.60, 0.61, 0.62, 0.63, 0.64, 0.65, 0.66, 0.67, 0.68, 0.69,
            0.70, 0.71, 0.72, 0.73, 0.74, 0.75, 0.76, 0.77, 0.78, 0.79,
            0.80, 0.81, 0.82, 0.83, 0.84, 0.85, 0.86, 0.87, 0.88, 0.89,
            0.90, 0.91, 0.92, 0.93, 0.94, 0.95, 0.96, 0.97, 0.98, 0.99,
        };
        double v = static_cast<double>(q) + frac100[r];
        if (d.neg) v = -v;
        value = v;
        return {d.ptr, fp_ok};
      }
    }

    if (static_cast<u32>(e + 15) <= 30u) { // e in [-15, 15]
      double v = static_cast<double>(d.mant);
      if (e > 0) {
        v = v * pow10d_exact_upto15(e);
      } else if (e < 0) {
        v = v / pow10d_exact_upto15(-e);
      }
      if (d.neg) v = -v;
      value = v;
      return {d.ptr, fp_ok};
    }
  }

  if (d.mant == 0) {
    u64 bits = 0; // +0
    if (d.neg) bits |= (1ULL << 63);
    value = bits_to_double(bits);
    return {d.ptr, fp_ok};
  }

  // Range guard. Single-compare form helps the hot path a bit.
  // Valid: [-342, 308] => after biasing by +342, valid is [0, 650].
  if (static_cast<u32>(d.exp10 + 342) > 650u) {
    if (d.exp10 > 308) {
      u64 bits = 0x7ff0000000000000ULL;
      if (d.neg) bits |= (1ULL << 63);
      value = bits_to_double(bits);
    } else {
      u64 bits = 0;
      if (d.neg) bits |= (1ULL << 63);
      value = bits_to_double(bits);
    }
    return {d.ptr, fp_result_out_of_range};
  }

  bin64 b = build_binary64(d.exp10, d.mant);
  u64 bits = (static_cast<u64>(b.exp) << 52) | (b.mant & ((1ULL << 52) - 1ULL));
  if (d.neg) bits |= (1ULL << 63);
  value = bits_to_double(bits);
  return {d.ptr, fp_ok};
}

fp_chars_result parse_fp_float(const char* first, const char* last, float& value) noexcept {
  const char* p = first;
  bool neg = false;
  if (p < last && (*p == '-' || *p == '+')) {
    neg = (*p == '-');
    ++p;
  }

  // Specials: nan/inf/infinity (ASCII, case-insensitive).
  if (p < last) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c == 'n' || c == 'N') {
      if ((last - p) >= 3 && ascii_ieq3(p, "nan")) {
        u32 bits = 0x7fc00000u;
        if (neg) bits |= (1u << 31);
        value = bits_to_float(bits);
        return {p + 3, fp_ok};
      }
    } else if (c == 'i' || c == 'I') {
      if ((last - p) >= 3 && ascii_ieq3(p, "inf")) {
        u32 bits = 0x7f800000u;
        if (neg) bits |= (1u << 31);
        value = bits_to_float(bits);
        return {p + 3, fp_ok};
      }
      if ((last - p) >= 8 && ascii_ieq8(p, "infinity")) {
        u32 bits = 0x7f800000u;
        if (neg) bits |= (1u << 31);
        value = bits_to_float(bits);
        return {p + 8, fp_ok};
      }
    }
  }

  dec64 d = parse_decimal_10_impl(p, last, neg);
  if (d.ec != fp_ok) return {first, d.ec};

  // Very common fast paths: exact values with tiny decimal exponent.
  // This targets short_no_exp (0..2 fractional digits, no exponent) and avoids a pow10-table load.
  if (d.exact) {
    const i32 e = d.exp10;
    // Fast reject for mixed: most exponents are not in [-2,0].
    if (static_cast<u32>(e + 2) <= 2u) {
      if (e == 0) {
        float vf = static_cast<float>(d.mant);
        if (d.neg) vf = -vf;
        value = vf;
        return {d.ptr, fp_ok};
      }
      if (e == -1) {
        double vd = static_cast<double>(d.mant) / 10.0;
        float vf = static_cast<float>(vd);
        if (d.neg) vf = -vf;
        value = vf;
        return {d.ptr, fp_ok};
      }
      // e == -2
      double vd = static_cast<double>(d.mant) / 100.0;
      float vf = static_cast<float>(vd);
      if (d.neg) vf = -vf;
      value = vf;
      return {d.ptr, fp_ok};
    }
  }

  // Fast path for exact bounded-decimal inputs.
  // For mixed: exp10 is typically within [-30,30] and mant has <=10 digits, so this avoids
  // the expensive pow5-table conversion most of the time.
  if (d.exact && d.exp10 >= -38 && d.exp10 <= 38) {
    double vd = static_cast<double>(d.mant) * pow10d_i32(d.exp10);
    float vf = static_cast<float>(vd);
    if (d.neg) vf = -vf;
    value = vf;
    return {d.ptr, fp_ok};
  }

  if (d.mant == 0) {
    u32 bits = 0;
    if (d.neg) bits |= (1u << 31);
    value = bits_to_float(bits);
    return {d.ptr, fp_ok};
  }

  // Range guard. Valid: [-64, 38] => after biasing by +64, valid is [0, 102].
  if (static_cast<u32>(d.exp10 + 64) > 102u) {
    if (d.exp10 > 38) {
      u32 bits = 0x7f800000u;
      if (d.neg) bits |= (1u << 31);
      value = bits_to_float(bits);
    } else {
      u32 bits = 0;
      if (d.neg) bits |= (1u << 31);
      value = bits_to_float(bits);
    }
    return {d.ptr, fp_result_out_of_range};
  }

  bin32 b = build_binary32(d.exp10, d.mant);
  u32 bits = (static_cast<u32>(b.exp) << 23) | (b.mant & ((1u << 23) - 1u));
  if (d.neg) bits |= (1u << 31);
  value = bits_to_float(bits);
  return {d.ptr, fp_ok};
}

} // namespace detail
} // namespace chfloat

#endif 