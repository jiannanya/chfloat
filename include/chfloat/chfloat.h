
#pragma once

// chfloat: header-only numeric parsing helpers.
// Public API is pointer-based (first,last) to avoid std::string_view.

#include <chfloat/detail/float_parse.h>

namespace chfloat {

// Minimal error codes (do NOT match std::errc values).
enum class errc : int {
  ok = 0,
  invalid_argument = 1,
  result_out_of_range = 2,
};

struct from_chars_result {
  const char* ptr;
  errc ec;
};

enum class chars_format : unsigned {
  general = 0,
  scientific = 1,
  fixed = 2,
  hex = 3,
};

namespace detail {

// NOTE: Avoid locale; ASCII only.
inline bool is_space_ascii(unsigned char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

inline const char* skip_leading_ascii_spaces(const char* first, const char* last) noexcept {
  while (first < last && is_space_ascii(static_cast<unsigned char>(*first))) {
    ++first;
  }
  return first;
}

inline int digit_value(unsigned char c) noexcept {
  if (c >= static_cast<unsigned char>('0') && c <= static_cast<unsigned char>('9')) {
    return int(c - static_cast<unsigned char>('0'));
  }
  if (c >= static_cast<unsigned char>('a') && c <= static_cast<unsigned char>('z')) {
    return 10 + int(c - static_cast<unsigned char>('a'));
  }
  if (c >= static_cast<unsigned char>('A') && c <= static_cast<unsigned char>('Z')) {
    return 10 + int(c - static_cast<unsigned char>('A'));
  }
  return -1;
}

// Unsigned parser with overflow detection, for unsigned long long.
inline from_chars_result parse_ull_any_base(const char* first, const char* last,
                                           unsigned long long& out, int base) noexcept {
  if (base < 2 || base > 36) return {first, errc::invalid_argument};
  if (first == last) return {first, errc::invalid_argument};

  unsigned long long value = 0;
  const char* p = first;
  bool any = false;

  const unsigned long long umax = ~0ULL;
  const unsigned long long ub = static_cast<unsigned long long>(base);

  for (; p < last; ++p) {
    int dv = digit_value(static_cast<unsigned char>(*p));
    if (dv < 0 || dv >= base) break;
    any = true;

    const unsigned long long ud = static_cast<unsigned long long>(dv);
    if (value > (umax - ud) / ub) {
      // overflow: consume remaining digits
      ++p;
      for (; p < last; ++p) {
        int dv2 = digit_value(static_cast<unsigned char>(*p));
        if (dv2 < 0 || dv2 >= base) break;
      }
      return {p, errc::result_out_of_range};
    }
    value = value * ub + ud;
  }

  if (!any) return {first, errc::invalid_argument};
  out = value;
  return {p, errc::ok};
}

} // namespace detail

// Strict parsing (no whitespace skipping).

inline from_chars_result from_chars(const char* first, const char* last, double& value,
                                    chars_format fmt = chars_format::general) noexcept {
  if (fmt != chars_format::general) {
    // Non-general formats are not supported in the nostd build.
    return {first, errc::invalid_argument};
  }
  detail::fp_chars_result r = detail::parse_fp_double(first, last, value);
  return {r.ptr, static_cast<errc>(r.ec)};
}

inline from_chars_result from_chars(const char* first, const char* last, float& value,
                                    chars_format fmt = chars_format::general) noexcept {
  if (fmt != chars_format::general) {
    return {first, errc::invalid_argument};
  }
  detail::fp_chars_result r = detail::parse_fp_float(first, last, value);
  return {r.ptr, static_cast<errc>(r.ec)};
}

inline from_chars_result from_chars(const char* first, const char* last, long long& value,
                                    int base = 10) noexcept {
  if (first == last) return {first, errc::invalid_argument};

  const char* p = first;
  bool negative = false;
  if (*p == '-') {
    negative = true;
    ++p;
  } else if (*p == '+') {
    ++p;
  }

  unsigned long long mag = 0;
  auto r = detail::parse_ull_any_base(p, last, mag, base);
  if (r.ec != errc::ok) {
    if (r.ec == errc::invalid_argument) return {first, errc::invalid_argument};
    return r;
  }

  // signed range checks
  const unsigned long long pos_max = static_cast<unsigned long long>(0x7fffffffffffffffULL);
  const unsigned long long neg_max = pos_max + 1ULL;
  if (!negative) {
    if (mag > pos_max) return {r.ptr, errc::result_out_of_range};
    value = static_cast<long long>(mag);
    return {r.ptr, errc::ok};
  }
  if (mag > neg_max) return {r.ptr, errc::result_out_of_range};
  value = (mag == neg_max) ? static_cast<long long>(0x8000000000000000ULL)
                           : -static_cast<long long>(mag);
  return {r.ptr, errc::ok};
}

inline from_chars_result from_chars(const char* first, const char* last, unsigned long long& value,
                                    int base = 10) noexcept {
  if (first == last) return {first, errc::invalid_argument};
  if (*first == '-' || *first == '+') return {first, errc::invalid_argument};
  return detail::parse_ull_any_base(first, last, value, base);
}

inline from_chars_result from_chars(const char* first, const char* last, int& value,
                                    int base = 10) noexcept {
  long long v = 0;
  auto r = from_chars(first, last, v, base);
  if (r.ec != errc::ok) return r;
  // assume 32-bit int on supported targets
  if (v < -2147483648LL || v > 2147483647LL) return {r.ptr, errc::result_out_of_range};
  value = static_cast<int>(v);
  return r;
}

inline from_chars_result from_chars(const char* first, const char* last, unsigned& value,
                                    int base = 10) noexcept {
  unsigned long long v = 0;
  auto r = from_chars(first, last, v, base);
  if (r.ec != errc::ok) return r;
  if (v > 0xffffffffULL) return {r.ptr, errc::result_out_of_range};
  value = static_cast<unsigned>(v);
  return r;
}

// Whitespace-skipping variants (ASCII only).

inline from_chars_result from_chars_ws(const char* first, const char* last, double& value) noexcept {
  first = detail::skip_leading_ascii_spaces(first, last);
  return from_chars(first, last, value, chars_format::general);
}

inline from_chars_result from_chars_ws(const char* first, const char* last, float& value) noexcept {
  first = detail::skip_leading_ascii_spaces(first, last);
  return from_chars(first, last, value, chars_format::general);
}

inline from_chars_result from_chars_ws(const char* first, const char* last, long long& value,
                                       int base = 10) noexcept {
  first = detail::skip_leading_ascii_spaces(first, last);
  return from_chars(first, last, value, base);
}

inline from_chars_result from_chars_ws(const char* first, const char* last, unsigned long long& value,
                                       int base = 10) noexcept {
  first = detail::skip_leading_ascii_spaces(first, last);
  return from_chars(first, last, value, base);
}

// Small convenience: parse a single digit char into [0,9].
inline bool parse_digit(char c, unsigned& out) noexcept {
  unsigned v = static_cast<unsigned>(static_cast<unsigned char>(c) - static_cast<unsigned char>('0'));
  if (v <= 9u) {
    out = v;
    return true;
  }
  return false;
}

} // namespace chfloat
