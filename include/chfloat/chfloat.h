#pragma once

// Header-only, locale-independent C++17 numeric parsing.
#include <chfloat/detail/float_parse.h>
#include <limits>
#include <type_traits>

namespace chfloat {
enum class errc : int { ok = 0, invalid_argument = 1, result_out_of_range = 2 };
struct from_chars_result { const char* ptr; errc ec; };
// These are chfloat API values, not std::chars_format values.
enum class chars_format : unsigned { general = 0, scientific = 1, fixed = 2, hex = 3 };

namespace detail {
inline bool is_space_ascii(unsigned char c) noexcept {
  return c == ' ' || (c >= '\t' && c <= '\r');
}
inline const char* skip_leading_ascii_spaces(const char* first, const char* last) noexcept {
  while (first != last && is_space_ascii(static_cast<unsigned char>(*first))) ++first;
  return first;
}
struct digit_lookup {
  unsigned char values[256]{};
  constexpr digit_lookup() noexcept {
    for (unsigned c = 0; c < 256; ++c) {
      const unsigned d = c - '0', a = (c | 0x20u) - 'a';
      values[c] = static_cast<unsigned char>(d < 10 ? d : a < 26 ? a + 10 : 255);
    }
  }
};
inline constexpr digit_lookup integer_digits{};
inline int digit_value(unsigned char c) noexcept {
  return integer_digits.values[c] == 255 ? -1 : integer_digits.values[c];
}
template <class T>
using enable_integer = std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value, int>;

template <class T, bool Decimal>
inline from_chars_result parse_integer(const char* first, const char* last, T& out, int base) noexcept {
  using U = std::make_unsigned_t<T>;
  const char* p = first;
  bool neg = false;
  if constexpr (std::is_signed<T>::value) {
    if (*p == '-' || *p == '+') {
      neg = *p == '-';
      if (++p == last) return {first, errc::invalid_argument};
    }
  }
  const char* digits = p;
  if (*p == '0') p = skip_zeroes(p, last);
  const U limit = std::is_signed<T>::value
                      ? U(U((std::numeric_limits<T>::max)()) + U(neg))
                      : (std::numeric_limits<U>::max)();
  const U radix = U(Decimal ? 10 : base);
  // One division per number; decimal divisors are compile-time constants.
  const U cutoff = U(limit / radix);
  const unsigned cutlim = static_cast<unsigned>(limit % radix);
  U value = 0;
  int blocks = 0;
  // Eight decimal digits fit every 32-bit destination; sixteen fit every
  // 64-bit destination. Only subsequent digits need per-digit range checks.
  if constexpr (Decimal && sizeof(U) >= 4) {
    if (last - p >= 8) {
      const u64 word = load_u64_unaligned(p);
      if (all_8_digits(word)) {
        value = U(parse_8_digits(word));
        p += 8;
        if constexpr (sizeof(U) >= 8) {
          if (last - p >= 8) {
            const u64 next = load_u64_unaligned(p);
            if (all_8_digits(next)) {
              value = U(value * U(100000000) + U(parse_8_digits(next)));
              p += 8;
            }
          }
        }
      }
    }
  } else if constexpr (!Decimal && sizeof(U) >= 4) {
    // Hexadecimal blocks: eight digits for every 32/64-bit destination, plus a
    // second block for 64-bit ones. A block may already exceed the destination
    // range, which the post-loop check below reports.
    if (base == 16 && last - p >= 8) {
      const u64 word = load_u64_unaligned(p);
      if (all_8_hex(word)) {
        value = U(parse_8_hex(word));
        p += 8;
        blocks = 8;
        if constexpr (sizeof(U) >= 8) {
          if (last - p >= 8) {
            const u64 next = load_u64_unaligned(p);
            if (all_8_hex(next)) {
              value = U(value << 32) | U(parse_8_hex(next));
              p += 8;
              blocks = 16;
            }
          }
        }
      }
    }
  }
  while (p != last) {
    const unsigned d = Decimal ? unsigned(static_cast<unsigned char>(*p)) - '0'
                               : integer_digits.values[static_cast<unsigned char>(*p)];
    if (d >= static_cast<unsigned>(radix)) break;
    if (value > cutoff || (value == cutoff && d > cutlim)) {
      if constexpr (Decimal) {
        p = skip_decimal_digits(p + 1, last);
      } else {
        do {
          ++p;
          if (p == last) break;
          if (integer_digits.values[static_cast<unsigned char>(*p)] >= static_cast<unsigned>(radix)) break;
        } while (true);
      }
      return {p, errc::result_out_of_range};
    }
    value = U(value * radix + U(d));
    ++p;
  }
  // A full hexadecimal block can already exceed the destination range without
  // leaving another digit to trigger the loop's guard above.
  if (blocks != 0 && value > limit) return {p, errc::result_out_of_range};
  if (p == digits) return {first, errc::invalid_argument};
  if constexpr (std::is_signed<T>::value) {
    // Subtract before conversion to represent the signed minimum portably.
    out = neg ? T(-static_cast<T>(value - U(value != 0)) - T(value != 0)) : static_cast<T>(value);
  } else {
    out = value;
  }
  return {p, errc::ok};
}
} // namespace detail

inline from_chars_result from_chars(const char* first, const char* last, double& value,
                                    chars_format fmt = chars_format::general) noexcept {
  const auto r = detail::dispatch_fp(first, last, value, static_cast<unsigned>(fmt));
  return {r.ptr, static_cast<errc>(r.ec)};
}
inline from_chars_result from_chars(const char* first, const char* last, float& value,
                                    chars_format fmt = chars_format::general) noexcept {
  const auto r = detail::dispatch_fp(first, last, value, static_cast<unsigned>(fmt));
  return {r.ptr, static_cast<errc>(r.ec)};
}
template <class T, detail::enable_integer<T> = 0>
inline from_chars_result from_chars(const char* first, const char* last, T& value, int base = 10) noexcept {
  if (first == last || base < 2 || base > 36) return {first, errc::invalid_argument};
  return base == 10 ? detail::parse_integer<T, true>(first, last, value, 10)
                    : detail::parse_integer<T, false>(first, last, value, base);
}
template <class T, std::enable_if_t<std::is_same<T, float>::value || std::is_same<T, double>::value, int> = 0>
inline from_chars_result from_chars_ws(const char* first, const char* last, T& value,
                                       chars_format fmt = chars_format::general) noexcept {
  return from_chars(detail::skip_leading_ascii_spaces(first, last), last, value, fmt);
}
template <class T, detail::enable_integer<T> = 0>
inline from_chars_result from_chars_ws(const char* first, const char* last, T& value, int base = 10) noexcept {
  return from_chars(detail::skip_leading_ascii_spaces(first, last), last, value, base);
}
inline bool parse_digit(char c, unsigned& out) noexcept {
  const unsigned d = unsigned(static_cast<unsigned char>(c)) - '0';
  if (d > 9) return false;
  out = d;
  return true;
}
} // namespace chfloat
