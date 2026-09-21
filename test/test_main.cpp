#include <chfloat/chfloat.h>
#include <array>
#include <cfenv>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>

namespace {
#include "midpoint_cases.h"
int failures = 0;
std::uint64_t comparisons = 0;
std::uint64_t oracle_skips = 0;
#define CHECK(expr) do { if (!(expr)) { if (failures < 30) std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); ++failures; } } while (false)
template <class T> std::uint64_t bits(T value) {
  std::uint64_t result = 0;
  std::memcpy(&result, &value, sizeof(T));
  return result;
}
std::chars_format std_format(chfloat::chars_format fmt) {
  switch (fmt) {
    case chfloat::chars_format::fixed: return std::chars_format::fixed;
    case chfloat::chars_format::scientific: return std::chars_format::scientific;
    case chfloat::chars_format::hex: return std::chars_format::hex;
    default: return std::chars_format::general;
  }
}
template <class T> void compare(std::string_view s, chfloat::chars_format fmt = chfloat::chars_format::general) {
  T actual = T(123), expected = T(123);
  const char* first = s.data();
  const char* last = first + s.size();
  // chfloat intentionally retains its leading-plus extension.
  const char* oracle = first != last && *first == '+' ? first + 1 : first;
  const char* oracle_last = last;
  if (fmt == chfloat::chars_format::fixed) {
    // `fixed` must not consume an exponent part. Some standard libraries do so
    // for long inputs, so restrict the oracle to the plain-decimal prefix:
    // that prefix is exactly what a conforming fixed parse must consume.
    const char* q = oracle;
    bool seen_digit = false;
    if (q != last && (*q == '+' || *q == '-')) ++q;
    for (; q != last; ++q) {
      if (*q >= '0' && *q <= '9') { seen_digit = true; continue; }
      if (*q == '.') continue;
      if ((*q == 'e' || *q == 'E') && seen_digit) oracle_last = q;
      break;
    }
  }
  auto want = std::from_chars(oracle, oracle_last, expected, std_format(fmt));
  if (oracle != first && oracle != last && (*oracle == '+' || *oracle == '-')) want.ec = std::errc::invalid_argument;
  if (want.ec == std::errc::invalid_argument) want.ptr = first;
  // Reject unusable oracle results before comparing. A conforming parse never
  // stops in the middle of a digit run, and never reports success with a
  // non-finite value for a finite decimal input; some standard libraries do
  // both. Specials such as "inf"/"nan" are excluded from the second test.
  const char* probe = oracle;
  if (probe != last && (*probe == '+' || *probe == '-')) ++probe;
  const bool decimal_input = probe != last && ((*probe >= '0' && *probe <= '9') || *probe == '.');
  if ((decimal_input && want.ec == std::errc{} && !std::isfinite(expected)) ||
      (want.ptr != last && *want.ptr >= '0' && *want.ptr <= '9')) {
    ++oracle_skips;
    return;
  }
  // Older MSVC libraries overwrite the destination on range errors.
  if (want.ec != std::errc{}) expected = T(123);
  auto got = chfloat::from_chars(first, last, actual, fmt);
  const auto ec = want.ec == std::errc{} ? chfloat::errc::ok :
      want.ec == std::errc::invalid_argument ? chfloat::errc::invalid_argument : chfloat::errc::result_out_of_range;
  const bool equal = (std::isnan(actual) && std::isnan(expected) && std::signbit(actual) == std::signbit(expected)) || bits(actual) == bits(expected);
  ++comparisons;
  if (got.ec != ec || got.ptr != want.ptr || !equal) {
    if (failures < 30) std::fprintf(stderr, "Mismatch %s fmt=%u input=%.160s size=%zu ec=%d/%d ptr=%td/%td bits=%llx/%llx\n",
        sizeof(T) == 4 ? "float" : "double", unsigned(fmt), std::string(s).c_str(), s.size(), int(got.ec), int(ec),
        got.ptr - first, want.ptr - first, (unsigned long long)bits(actual), (unsigned long long)bits(expected));
    ++failures;
  }
}
template <class T> void regression() {
  constexpr std::string_view cases[] = {
    "", " ", "abc", "+", "-", ".", "+.", "--1", "+-1", "1..0", "1e", "1e+", "1e-", ".5tail", "1.e2!",
    "0", "-0", "+0", "-0.000e999999", "0000000000000000000000000001", "0.000000000000000000000001",
    "12345678901234567890.12345678901234567890", "1.23456789012345678901234567890123456789",
    "3.14159265358979323846264338327950288419716939937510", "9007199254740993", "18446744073709551615",
    "99999999999999999999999999999999", "1.000000059604644775390625", "1.00000005960464477539062500000001",
    "1.00000005960464477539062499999999", "1.000000178813934326171875", "16777217", "16777219",
    "1.00000000000000011102230246251565404236316680908203125",
    "1.000000000000000111022302462515654042363166809082031250000001",
    "1.000000000000000111022302462515654042363166809082031249999999",
    "1.7976931348623157e308", "1.7976931348623158e308", "1.7976931348623159e308", "1e309", "1e-400",
    "2.2250738585072014e-308", "2.2250738585072011e-308", "4.9406564584124654e-324", "2.4703282292062327e-324",
    "2.4703282292062328e-324", "3.4028234663852886e38", "3.4028235677973366e38", "3.4028235677973367e38",
    "1.1754943508222875e-38", "1.1754942106924411e-38", "1.401298464324817e-45", "7.006492321624085e-46",
    "1e99999999999999999999999999999999", "1e-9999999999999999999999999999999",
    "nan", "NaN", "-nan", "+nan", "nan()", "NaN(payload_123)", "nan(unclosed", "nan(a-b)", "nan(\xff)",
    "inf", "-INF", "infinity", "+InFiNiTy", "infinite", "Infinity!", "nanometer", "\xff", "1\xff"
  };
  for (auto s : cases) {
    compare<T>(s);
    compare<T>(s, chfloat::chars_format::fixed);
    compare<T>(s, chfloat::chars_format::scientific);
  }
  for (auto s : {"0", "-0", "1p0", "1.8p1", "0x1p2", "1p", "1p+", ".fp-1", "f.fp10!", "1.fffffffffffff8p1023",
                 "1p-1075", "1.00000000000000000000000001p-1075", "1p-1074", "1p-149", "1p-150", "1.000001p-150",
                 "1.fffffep127", "1.ffffffp127", "1p999999999999999999999", "0p999999999999999999999", "@", "`", "f@"})
    compare<T>(s, chfloat::chars_format::hex);
  for (std::size_t length : {20u, 100u, 1000u, 100000u}) {
    compare<T>(std::string(length, '0') + "1");
    compare<T>("0." + std::string(length, '0') + "1e" + std::to_string(length));
    compare<T>("1" + std::string(length, '0') + "e-" + std::to_string(length));
    compare<T>("1.00000000000000011102230246251565404236316680908203125" + std::string(length, '0') + "1");
  }
  const char raw[] = {'1', '.', '2', '5', '\0', '9'};
  for (std::size_t length = 0; length <= sizeof(raw); ++length) compare<T>(std::string_view(raw, length));
  T value = T(99);
  auto r = chfloat::from_chars(nullptr, nullptr, value);
  CHECK(r.ptr == nullptr && r.ec == chfloat::errc::invalid_argument && value == T(99));
  const char* invalid_format = "123";
  r = chfloat::from_chars(invalid_format, invalid_format + 3, value, static_cast<chfloat::chars_format>(99));
  CHECK(r.ptr == invalid_format && r.ec == chfloat::errc::invalid_argument && value == T(99));
  const char* ws = " \t\n\r\f\v-1.25e2!";
  r = chfloat::from_chars_ws(ws, ws + std::strlen(ws), value, chfloat::chars_format::scientific);
  CHECK(r.ec == chfloat::errc::ok && *r.ptr == '!' && value == T(-125));
}
template <class T> void integer_tests() {
  std::mt19937_64 rng(901);
  for (int base = 2; base <= 36; ++base) {
    for (int i = 0; i < 200; ++i) {
      T input = static_cast<T>(rng());
      if (i == 0) input = (std::numeric_limits<T>::min)();
      if (i == 1) input = (std::numeric_limits<T>::max)();
      char data[160];
      auto written = std::to_chars(data, data + sizeof(data) - 1, input, base);
      *written.ptr = '!';
      T actual = 0;
      auto got = chfloat::from_chars(data, written.ptr + 1, actual, base);
      CHECK(got.ec == chfloat::errc::ok && got.ptr == written.ptr && actual == input);
      ++comparisons;
    }
    std::string overflow(200, '1');
    T value = T(42);
    auto got = chfloat::from_chars(overflow.data(), overflow.data() + overflow.size(), value, base);
    CHECK(got.ec == chfloat::errc::result_out_of_range && got.ptr == overflow.data() + overflow.size() && value == T(42));
  }
  T value = T(42);
  const char* invalid = "+";
  auto got = chfloat::from_chars(invalid, invalid + 1, value);
  CHECK(got.ec == chfloat::errc::invalid_argument && got.ptr == invalid && value == T(42));
  for (int base : {-1, 0, 1, 37, 100}) {
    got = chfloat::from_chars(invalid, invalid + 1, value, base);
    CHECK(got.ec == chfloat::errc::invalid_argument && got.ptr == invalid && value == T(42));
  }
  const char* ws = " \t42!";
  got = chfloat::from_chars_ws(ws, ws + 5, value);
  CHECK(got.ec == chfloat::errc::ok && value == T(42) && *got.ptr == '!');
  if constexpr (std::is_signed<T>::value) {
    const char* plus = "+42";
    got = chfloat::from_chars(plus, plus + 3, value);
    CHECK(got.ec == chfloat::errc::ok && value == T(42));
  } else {
    for (auto s : {"+42", "-1"}) {
      got = chfloat::from_chars(s, s + std::strlen(s), value);
      CHECK(got.ec == chfloat::errc::invalid_argument && got.ptr == s);
    }
  }
}
void random_decimal(std::size_t count) {
  std::mt19937_64 rng(0xcef123);
  std::string s;
  s.reserve(600);
  for (std::size_t i = 0; i < count; ++i) {
    s.clear();
    if (rng() & 1) s += '-';
    const std::size_t n = 1 + rng() % (i % 10 == 0 ? 500 : 40);
    const std::size_t point = rng() % (n + 1);
    for (std::size_t j = 0; j < n; ++j) { if (j == point) s += '.'; s += char('0' + rng() % 10); }
    if (rng() % 4) { s += 'e'; s += std::to_string(int(rng() % 800) - 450); }
    if (rng() % 5 == 0) s += '!';
    compare<float>(s);
    compare<double>(s);
    if (i % 10 == 0) {
      compare<float>(s, chfloat::chars_format::fixed);
      compare<double>(s, chfloat::chars_format::scientific);
    }
  }
}
template <class T> void roundtrips(std::size_t count) {
  std::mt19937_64 rng(0xabbacddc);
  for (std::size_t i = 0; i < count; ++i) {
    std::uint64_t raw = rng();
    T value;
    std::memcpy(&value, &raw, sizeof(T));
    if (!std::isfinite(value)) continue;
    char buffer[128];
    for (auto fmt : {chfloat::chars_format::general, chfloat::chars_format::scientific, chfloat::chars_format::hex}) {
      const auto r = std::to_chars(buffer, buffer + sizeof(buffer), value, std_format(fmt));
      CHECK(r.ec == std::errc{});
      compare<T>(std::string_view(buffer, std::size_t(r.ptr - buffer)), fmt);
      T actual = 0;
      const auto parsed = chfloat::from_chars(buffer, r.ptr, actual, fmt);
      CHECK(parsed.ec == chfloat::errc::ok && bits(actual) == bits(value));
    }
  }
}
void rounding_modes() {
  const int original = std::fegetround();
  for (int mode : {FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}) {
    if (std::fesetround(mode)) continue;
    double d = 0;
    float f = 0;
    const char* text = "0.1";
    auto rd = chfloat::from_chars(text, text + 3, d);
    auto rf = chfloat::from_chars(text, text + 3, f);
    CHECK(rd.ec == chfloat::errc::ok && bits(d) == 0x3fb999999999999aULL);
    CHECK(rf.ec == chfloat::errc::ok && bits(f) == 0x3dcccccdULL);
    CHECK(std::fegetround() == mode);
  }
  std::fesetround(original);
}
void short_decimal_rounding() {
  const int original = std::fegetround();
  std::fesetround(FE_TONEAREST);
  std::mt19937 rng(0x98512);
  for (unsigned i = 0; i < 100000; ++i) {
    const unsigned mantissa = i < 50000 ? i : rng() % 100000000;
    for (unsigned scale = 0; scale <= 2; ++scale) {
      char buffer[32];
      char* end = std::to_chars(buffer, buffer + 24, mantissa).ptr;
      *end++ = 'e'; *end++ = '-'; *end++ = char('0' + scale);
      compare<float>({buffer, std::size_t(end - buffer)});
      compare<double>({buffer, std::size_t(end - buffer)});
      if (i % 100 == 0) {
        float want_f = 0;
        double want_d = 0;
        std::from_chars(buffer, end, want_f);
        std::from_chars(buffer, end, want_d);
        for (int mode : {FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}) {
          std::fesetround(mode);
          float f = 0;
          double d = 0;
          const auto rf = chfloat::from_chars(buffer, end, f);
          const auto rd = chfloat::from_chars(buffer, end, d);
          CHECK(rf.ec == chfloat::errc::ok && bits(f) == bits(want_f));
          CHECK(rd.ec == chfloat::errc::ok && bits(d) == bits(want_d));
          comparisons += 2;
        }
        std::fesetround(FE_TONEAREST);
      }
    }
  }
#if (defined(_M_X64) || defined(__x86_64__)) && !defined(CHFLOAT_FORCE_PORTABLE)
  const unsigned previous_csr = _mm_getcsr();
  // Unmask the inexact exception. The parser must use its integer path and
  // must not raise a floating-point exception for decimal 0.1.
  _mm_setcsr(previous_csr & ~(0x1000u | 0x3fu));
  const char text[] = "0.1";
  double d = 0;
  const auto result = chfloat::from_chars(text, text + 3, d);
  const unsigned resulting_csr = _mm_getcsr();
  _mm_setcsr(previous_csr);
  CHECK(result.ec == chfloat::errc::ok && bits(d) == 0x3fb999999999999aULL);
  CHECK((resulting_csr & 0x3fu) == 0);
#endif
  std::fesetround(original);
}
template <class T> void exact_midpoints() {
  for (const auto& c : midpoint_cases) {
    if (c.is_double != std::is_same<T, double>::value) continue;
    for (bool negative : {false, true}) {
      const std::string text = std::string(negative ? "-" : "") + c.text;
      T value = T(123);
      const auto r = chfloat::from_chars(text.data(), text.data() + text.size(), value);
      const auto expected = c.expected | (negative ? std::uint64_t(1) << (sizeof(T) * 8 - 1) : 0);
      CHECK(r.ptr == text.data() + text.size());
      CHECK(r.ec == (c.range_error ? chfloat::errc::result_out_of_range : chfloat::errc::ok));
      CHECK(bits(value) == (c.range_error ? bits(T(123)) : expected));
      ++comparisons;
    }
  }
}
void arbitrary_buffers() {
  std::mt19937_64 rng(984102);
  for (unsigned i = 0; i < 5000; ++i) {
    const std::size_t length = 1 + rng() % 48;
    // An exact-sized allocation lets ASan detect reads past the supplied range.
    char* raw = new char[length];
    for (std::size_t j = 0; j < length; ++j) raw[j] = static_cast<char>(rng());
    if (i % 2 == 0) raw[0] = '1';
    for (auto fmt : {chfloat::chars_format::general, chfloat::chars_format::fixed,
                     chfloat::chars_format::scientific, chfloat::chars_format::hex}) {
      compare<float>({raw, length}, fmt);
      compare<double>({raw, length}, fmt);
    }
    delete[] raw;
  }
}
void long_run_boundaries() {
  for (std::size_t n : {0u, 1u, 7u, 8u, 9u, 15u, 16u, 17u, 18u, 19u, 20u, 31u, 32u, 33u,
                        63u, 64u, 65u, 255u, 256u, 257u, 4096u}) {
    for (auto text : {std::string(n, '0') + "123.25!", "0." + std::string(n, '0') + "125e" + std::to_string(n),
                      "1.25e+" + std::string(n, '0') + "2!", "1.25e-" + std::string(n, '0') + "2!",
                      "1e" + std::string(n, '0') + "!", "1e-" + std::string(n, '9') + "!"}) {
      compare<float>(text); compare<double>(text);
    }
    // Exercise every possible terminating byte at each block boundary, with
    // exact-sized buffers so sanitizers can catch speculative overreads.
    for (unsigned c = 0; c < 256; ++c) {
      const std::string prefix = "99999999999999999999" + std::string(n, '9');
      const auto size = prefix.size() + 1;
      char* text = new char[size];
      std::memcpy(text, prefix.data(), prefix.size());
      text[size - 1] = static_cast<char>(c);
      long long value = 123, expected = 123;
      const auto want = std::from_chars(text, text + size, expected);
      const auto got = chfloat::from_chars(text, text + size, value);
      CHECK(want.ec == std::errc::result_out_of_range && got.ec == chfloat::errc::result_out_of_range);
      CHECK(got.ptr == want.ptr && value == 123);
      ++comparisons;
      delete[] text;
    }
  }
  for (unsigned bit = 0; bit < 64; ++bit) {
    const auto v = std::uint64_t(1) << bit;
    CHECK(chfloat::detail::lz64(v) == int(63 - bit));
    CHECK(chfloat::detail::lz64(v | (v - 1)) == int(63 - bit));
  }
  CHECK(chfloat::detail::lz64(0) == 64);
}
template <class T> void compare_integer_buffer(std::string_view s, int base = 10) {
  const char* first = s.data();
  const char* last = first + s.size();
  const char* oracle = first;
  if constexpr (std::is_signed<T>::value) {
    if (first != last && *first == '+') ++oracle;
  }
  T expected = 123, value = 123;
  auto want = std::from_chars(oracle, last, expected, base);
  if (oracle != first && oracle != last && (*oracle == '+' || *oracle == '-')) want.ec = std::errc::invalid_argument;
  if (want.ec == std::errc::invalid_argument) want.ptr = first;
  const auto got = chfloat::from_chars(first, last, value, base);
  const auto ec = want.ec == std::errc{} ? chfloat::errc::ok :
      want.ec == std::errc::invalid_argument ? chfloat::errc::invalid_argument : chfloat::errc::result_out_of_range;
  CHECK(got.ec == ec && got.ptr == want.ptr && value == (want.ec == std::errc{} ? expected : T(123)));
  ++comparisons;
}
void integer_byte_boundaries() {
  // Cover every byte value throughout bounded integer inputs, including
  // digits around 32/64-bit limits and signs with no following digit.
  constexpr char number[] = "1844674407370955161500";
  for (std::size_t length : {1u, 7u, 8u, 9u, 15u, 16u, 17u, 19u, 20u, 21u}) {
    char* raw = new char[length];
    std::memcpy(raw, number, length);
    for (std::size_t pos = 0; pos < length; ++pos) {
      for (unsigned c = 0; c < 256; ++c) {
        raw[pos] = static_cast<char>(c);
        const std::string_view text(raw, length);
        compare_integer_buffer<std::int32_t>(text);
        compare_integer_buffer<std::uint32_t>(text);
        compare_integer_buffer<std::int64_t>(text);
        compare_integer_buffer<std::uint64_t>(text);
      }
      raw[pos] = number[pos];
    }
    delete[] raw;
  }
}
void integer_hex_boundaries() {
  // The hexadecimal block parser reads eight bytes at a time; cover every byte
  // value at every position around the 8/16-digit block boundaries.
  constexpr char number[] = "1234567890abcdef0011";
  for (std::size_t length : {1u, 7u, 8u, 9u, 15u, 16u, 17u, 19u, 20u}) {
    char* raw = new char[length];
    std::memcpy(raw, number, length);
    for (std::size_t pos = 0; pos < length; ++pos) {
      for (unsigned c = 0; c < 256; ++c) {
        raw[pos] = static_cast<char>(c);
        const std::string_view text(raw, length);
        compare_integer_buffer<std::int32_t>(text, 16);
        compare_integer_buffer<std::uint32_t>(text, 16);
        compare_integer_buffer<std::int64_t>(text, 16);
        compare_integer_buffer<std::uint64_t>(text, 16);
      }
      raw[pos] = number[pos];
    }
    delete[] raw;
  }
}
void hex_block_helpers() {
  // Exhaustively check the eight-digit probe and value conversion for every
  // byte in every lane against a scalar reference.
  for (unsigned c = 0; c < 256; ++c) {
    for (int lane = 0; lane < 8; ++lane) {
      char raw[8];
      std::memcpy(raw, "12345678", sizeof(raw));
      raw[lane] = static_cast<char>(c);
      std::uint64_t word = 0;
      std::memcpy(&word, raw, sizeof(word));
      bool valid = true;
      unsigned expected = 0;
      for (int i = 0; i < 8; ++i) {
        const unsigned d = static_cast<unsigned char>(raw[i]);
        if (d >= '0' && d <= '9') expected = expected * 16 + (d - '0');
        else if (d >= 'a' && d <= 'f') expected = expected * 16 + (d - 'a' + 10);
        else if (d >= 'A' && d <= 'F') expected = expected * 16 + (d - 'A' + 10);
        else valid = false;
      }
      CHECK(chfloat::detail::all_8_hex(word) == valid);
      if (valid) CHECK(chfloat::detail::parse_8_hex(word) == expected);
    }
  }
}
} // namespace
int main(int argc, char** argv) {
  std::size_t count = 50000;
  if (argc == 2) count = std::strtoull(argv[1], nullptr, 10);
  regression<float>(); regression<double>();
  integer_tests<signed char>(); integer_tests<unsigned char>();
  integer_tests<short>(); integer_tests<unsigned short>();
  integer_tests<int>(); integer_tests<unsigned>();
  integer_tests<long>(); integer_tests<unsigned long>();
  integer_tests<long long>(); integer_tests<unsigned long long>();
  random_decimal(count);
  roundtrips<float>(count / 2); roundtrips<double>(count / 2);
  rounding_modes();
  short_decimal_rounding();
  exact_midpoints<float>(); exact_midpoints<double>();
  arbitrary_buffers();
  long_run_boundaries();
  integer_byte_boundaries();
  integer_hex_boundaries();
  hex_block_helpers();
  for (unsigned i = 0; i < 256; ++i) {
    unsigned value = 99;
    const bool valid = chfloat::parse_digit(static_cast<char>(i), value);
    CHECK(valid == (i >= '0' && i <= '9'));
    CHECK(value == (valid ? i - '0' : 99));
  }
  std::printf("%llu comparisons, %d failures, %llu oracle comparisons skipped\n",
              (unsigned long long)comparisons, failures, (unsigned long long)oracle_skips);
  return failures ? 1 : 0;
}
