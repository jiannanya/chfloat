#include <chfloat/chfloat.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
static int g_failures = 0;

#define CHECK(expr)                                                                 \
  do {                                                                              \
    if (!(expr)) {                                                                 \
      std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
      std::fflush(stderr);                                                         \
      ++g_failures;                                                                \
    }                                                                              \
  } while (0)

template <class T>
static T make_nan() {
  return std::numeric_limits<T>::quiet_NaN();
}

template <class T>
static bool is_nan(T v) {
  return std::isnan(v);
}

template <class T>
static bool is_inf(T v) {
  return std::isinf(v);
}

template <class T>
static uint64_t bitcast_u64(T v) {
  static_assert(sizeof(T) <= sizeof(uint64_t), "unexpected size");
  uint64_t out = 0;
  std::memcpy(&out, &v, sizeof(T));
  return out;
}

template <class T>
static bool equal_or_both_nan(T a, T b) {
  if (is_nan(a) && is_nan(b)) return true;
  // Treat +0 and -0 as equal in numeric sense
  if (a == T(0) && b == T(0)) return true;
  return a == b;
}

template <class T>
static void test_parse_ok(std::string_view s, T expected, chfloat::chars_format fmt = chfloat::chars_format::general) {
  T out = T(0);
  auto r = chfloat::from_chars(s.data(), s.data() + s.size(), out, fmt);
  CHECK(r.ec == chfloat::errc::ok);
  CHECK(r.ptr == s.data() + s.size());
  if (!equal_or_both_nan(out, expected)) {
    // If it's a special value, validate category.
    if (is_nan(expected)) {
      CHECK(is_nan(out));
    } else if (is_inf(expected)) {
      CHECK(is_inf(out));
      CHECK(std::signbit(out) == std::signbit(expected));
    } else {
      // Last-resort: compare bitwise equality for exact strings.
      CHECK(bitcast_u64(out) == bitcast_u64(expected));
    }
  }
}

template <class T>
static void test_parse_err(std::string_view s) {
  T out = T(123);
  auto r = chfloat::from_chars(s.data(), s.data() + s.size(), out);
  CHECK(r.ec != chfloat::errc::ok);
}

template <class T>
static void test_parse_partial_ok(std::string_view s) {
  T out = T(0);
  auto r = chfloat::from_chars(s.data(), s.data() + s.size(), out);
  CHECK(r.ec == chfloat::errc::ok);
  // Must consume at least 1 char, but not necessarily the whole string.
  CHECK(r.ptr > s.data());
  CHECK(r.ptr < s.data() + s.size());
}

static void test_float_double_basic() {
  test_parse_ok<float>("0", 0.0f);
  test_parse_ok<float>("-0", -0.0f);
  test_parse_ok<float>("1", 1.0f);
  test_parse_ok<float>("-1", -1.0f);
  test_parse_ok<float>("3.1415926", 3.1415926f);
  test_parse_ok<float>("1e10", 1e10f);
  test_parse_ok<float>("1E-10", 1e-10f);

  test_parse_ok<double>("0", 0.0);
  test_parse_ok<double>("-0", -0.0);
  test_parse_ok<double>("1", 1.0);
  test_parse_ok<double>("-1", -1.0);
  test_parse_ok<double>("3.141592653589793", 3.141592653589793);
  test_parse_ok<double>("1e308", 1e308);
  test_parse_ok<double>("1e-308", 1e-308);
}

static void test_float_specials_if_supported() {
  // std::from_chars floating parsing support varies across standard libraries.
  // We only assert that these don't crash; result may be invalid_argument.
  const std::string_view cases[] = {"nan", "NaN", "inf", "-inf", "infinity"};
  for (auto s : cases) {
    double out = 0;
    (void)chfloat::from_chars(s.data(), s.data() + s.size(), out);
  }
}

static void test_float_errors() {
  test_parse_err<float>("");
  test_parse_err<float>(" ");
  test_parse_err<float>("abc");
  test_parse_err<float>("--1");
  test_parse_partial_ok<float>("1..0");

  test_parse_err<double>("");
  test_parse_err<double>("abc");
  test_parse_err<double>("1e9999"); // out of range
}

static void test_ws_variant() {
  float out = 0;
  const char* first = "  \t\n-12.5";
  const char* last = first + std::strlen(first);
  auto r = chfloat::from_chars_ws(first, last, out);
  CHECK(r.ec == chfloat::errc::ok);
  CHECK(out == -12.5f);
}

static void test_int_basic() {
  int64_t v = 0;
  {
    const char* first = "-123";
    const char* last = first + 4;
    auto r = chfloat::from_chars(first, last, v);
    if (r.ec != chfloat::errc::ok) {
      std::fprintf(stderr, "int64 parse failed: ec=%d, consumed=%td, v=%lld\n",
                   (int)r.ec, r.ptr - first, (long long)v);
      std::fflush(stderr);
    }
    CHECK(r.ec == chfloat::errc::ok);
    CHECK(v == -123);
  }
  {
    uint32_t u = 0;
    const char* first = "ff";
    const char* last = first + 2;
    auto r = chfloat::from_chars(first, last, u, 16);
    if (r.ec != chfloat::errc::ok) {
      std::fprintf(stderr, "u32 hex parse failed: ec=%d, consumed=%td, u=%u\n",
                   (int)r.ec, r.ptr - first, (unsigned)u);
      std::fflush(stderr);
    }
    CHECK(r.ec == chfloat::errc::ok);
    CHECK(u == 255u);
  }
  {
    int x = 0;
    // INT_MAX + 1
    const char* first = "2147483648";
    const char* last = first + 10;
    auto r = chfloat::from_chars(first, last, x);
    if (r.ec != chfloat::errc::result_out_of_range) {
      std::fprintf(stderr, "int overflow parse unexpected: ec=%d, consumed=%td, x=%d\n",
                   (int)r.ec, r.ptr - first, x);
      std::fflush(stderr);
    }
    CHECK(r.ec == chfloat::errc::result_out_of_range);
  }
}


static void test_parse_digit() {
  unsigned d = 999;
  CHECK(chfloat::parse_digit('0', d) && d == 0u);
  CHECK(chfloat::parse_digit('9', d) && d == 9u);
  CHECK(!chfloat::parse_digit('a', d));
}

} // namespace

int main() {
  test_float_double_basic();
  test_float_specials_if_supported();
  test_float_errors();
  test_ws_variant();
  test_int_basic();
  test_parse_digit();
  return g_failures == 0 ? 0 : 1;
}
