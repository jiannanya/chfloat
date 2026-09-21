// Concurrency verification for chfloat.
//
// The parser is documented as having no mutable global state, so threads may
// parse separate inputs and destinations concurrently. This executable checks
// that claim directly:
//
//   1. every input in a corpus covering all parse paths is evaluated once on the
//      main thread to produce a reference result set;
//   2. several threads then re-evaluate the whole corpus, each starting at a
//      different rotation and writing into its own destination, and every result
//      must match the reference bit for bit;
//   3. the corpus is repeated under contention to give interleaved execution a
//      chance to expose shared state or data races;
//   4. aggregate throughput is reported against a single-threaded run.
#include <chfloat/chfloat.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#if defined(__has_include)
#if __has_include(<midpoint_cases.h>)
#include "midpoint_cases.h"
#define CHFLOAT_HAVE_MIDPOINTS 1
#endif
#endif

namespace {
int failures = 0;
#define CHECK(expr) do { if (!(expr)) { if (failures < 20) std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); ++failures; } } while (false)

using Clock = std::chrono::steady_clock;

enum class kind { fp_double, fp_float, fp_hex, fp_fixed, fp_scientific, integer_decimal, integer_binary, integer_base };

struct entry {
  std::string text;
  kind k;
  int base = 10;
};

// A single parse outcome, normalized so two runs can be compared exactly.
struct outcome {
  std::uint64_t bits = 0;
  std::size_t consumed = 0;
  int ec = 0;
  bool operator==(const outcome& o) const {
    return bits == o.bits && consumed == o.consumed && ec == o.ec;
  }
};

template <class T> std::uint64_t bit_pattern(T value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(T));
  return bits;
}

outcome evaluate(const entry& e) {
  const char* first = e.text.data();
  const char* last = first + e.text.size();
  outcome r;
  switch (e.k) {
    case kind::fp_double: {
      double v = 0;
      const auto p = chfloat::from_chars(first, last, v, chfloat::chars_format::general);
      r = {bit_pattern(v), std::size_t(p.ptr - first), int(p.ec)};
      break;
    }
    case kind::fp_float: {
      float v = 0;
      const auto p = chfloat::from_chars(first, last, v, chfloat::chars_format::general);
      r = {bit_pattern(v), std::size_t(p.ptr - first), int(p.ec)};
      break;
    }
    case kind::fp_hex: {
      double v = 0;
      const auto p = chfloat::from_chars(first, last, v, chfloat::chars_format::hex);
      r = {bit_pattern(v), std::size_t(p.ptr - first), int(p.ec)};
      break;
    }
    case kind::fp_fixed: {
      double v = 0;
      const auto p = chfloat::from_chars(first, last, v, chfloat::chars_format::fixed);
      r = {bit_pattern(v), std::size_t(p.ptr - first), int(p.ec)};
      break;
    }
    case kind::fp_scientific: {
      double v = 0;
      const auto p = chfloat::from_chars(first, last, v, chfloat::chars_format::scientific);
      r = {bit_pattern(v), std::size_t(p.ptr - first), int(p.ec)};
      break;
    }
    case kind::integer_decimal: {
      long long v = 0;
      const auto p = chfloat::from_chars(first, last, v, 10);
      r = {std::uint64_t(v), std::size_t(p.ptr - first), int(p.ec)};
      break;
    }
    case kind::integer_binary: {
      unsigned long long v = 0;
      const auto p = chfloat::from_chars(first, last, v, 2);
      r = {v, std::size_t(p.ptr - first), int(p.ec)};
      break;
    }
    default: {
      unsigned long long v = 0;
      const auto p = chfloat::from_chars(first, last, v, e.base);
      r = {v, std::size_t(p.ptr - first), int(p.ec)};
      break;
    }
  }
  return r;
}

std::vector<entry> build_corpus() {
  std::vector<entry> corpus;
  const char* literals[] = {
    "0", "-0", "+0", "1", "-1", "0.5", "-0.5", "1.5e10", "12.5e-3",
    "0.1", "0.2", "0.3", "1e-308", "1e308", "1e309", "1e-400", "1e-323",
    "4.9406564584124654e-324", "2.2250738585072014e-308", "1.7976931348623157e308",
    "1.000000059604644775390625", "1.00000000000000011102230246251565404236316680908203125",
    "3.4028234663852886e38", "3.4028235677973366e38", "1.1754943508222875e-38",
    "9007199254740993", "18446744073709551615", "99999999999999999999999999",
    "1234567890123456789012345678901234567890.1234567890123456789e-40",
    "inf", "-inf", "INFINITY", "nan", "-nan", "NaN(payload_1)", "nan(", "nan()",
    "nan(a-b)", "1.8p+2", "0x1p-1074", "1p-149", "1.fffffep127", ".5", "5.", "+.", "-.",
    "", " ", "abc", "e5", ".e5", "1e", "1e+", "1e-", "--1", "+-1", "1..0", "\xff", "1\xff"
  };
  for (const char* s : literals) {
    corpus.push_back({s, kind::fp_double});
    corpus.push_back({s, kind::fp_float});
    corpus.push_back({s, kind::fp_hex});
    corpus.push_back({s, kind::fp_fixed});
    corpus.push_back({s, kind::fp_scientific});
  }
  // Long inputs exercise the bounded scratch buffer and the sticky-digit path.
  for (std::size_t n : {7u, 8u, 9u, 19u, 20u, 31u, 64u, 255u, 256u, 1000u, 5000u}) {
    corpus.push_back({"0." + std::string(n, '0') + "1e" + std::to_string(n), kind::fp_double});
    corpus.push_back({"1" + std::string(n, '0') + "e-" + std::to_string(n), kind::fp_double});
    corpus.push_back({std::string(n, '0') + "123.25", kind::fp_float});
    corpus.push_back({"1.000000059604644775390625" + std::string(n, '0') + "1", kind::fp_float});
    corpus.push_back({std::string(n, '9') + "1", kind::integer_decimal});
  }
  // Randomised coverage, deterministic across runs.
  std::mt19937_64 rng(0x5eed1234);
  for (unsigned i = 0; i < 4000; ++i) {
    std::string s;
    if (rng() & 1) s += '-';
    const std::size_t n = 1 + rng() % 40;
    const std::size_t point = rng() % (n + 1);
    for (std::size_t j = 0; j < n; ++j) {
      if (j == point) s += '.';
      s += char('0' + rng() % 10);
    }
    if (rng() % 3) { s += 'e'; s += std::to_string(int(rng() % 900) - 450); }
    corpus.push_back({s, kind::fp_double});
    corpus.push_back({s, kind::fp_float});
    corpus.push_back({s, kind::fp_scientific});
    if (i % 4 == 0) corpus.push_back({s, kind::fp_hex});
  }
  for (unsigned i = 0; i < 1500; ++i) {
    const auto value = static_cast<long long>(rng() & 0x7fffffffffffffffULL);
    char buffer[80];
    const auto r = std::to_chars(buffer, buffer + sizeof(buffer), (rng() & 1) ? -value : value, 10);
    corpus.push_back({std::string(buffer, r.ptr), kind::integer_decimal});
  }
  for (unsigned i = 0; i < 1000; ++i) {
    const auto value = rng();
    char buffer[80];
    const auto r = std::to_chars(buffer, buffer + sizeof(buffer), value, 2);
    corpus.push_back({std::string(buffer, r.ptr), kind::integer_binary});
  }
  // Every base 2..36 is represented in the corpus itself, so all threads parse
  // the same text through the same base-specific code path.
  for (unsigned i = 0; i < 200; ++i) {
    const auto value = rng();
    for (int base = 2; base <= 36; ++base) {
      char buffer[80];
      const auto r = std::to_chars(buffer, buffer + sizeof(buffer), value, base);
      corpus.push_back({std::string(buffer, r.ptr), kind::integer_base, base});
    }
  }
#ifdef CHFLOAT_HAVE_MIDPOINTS
  for (const auto& c : midpoint_cases) {
    corpus.push_back({c.text, c.is_double ? kind::fp_double : kind::fp_float});
    corpus.push_back({"-" + std::string(c.text), c.is_double ? kind::fp_double : kind::fp_float});
  }
#endif
  return corpus;
}

template <class Fn> double time_call(Fn&& fn) {
  const auto start = Clock::now();
  fn();
  return std::chrono::duration<double>(Clock::now() - start).count();
}

struct workload {
  const std::vector<entry>* corpus;
  const std::vector<outcome>* expected;
  std::size_t rotation;
  std::size_t repeats;
  std::uint64_t local_checksum = 0;
  std::size_t local_mismatches = 0;
  void operator()() {
    const auto& c = *corpus;
    const auto& want = *expected;
    std::uint64_t sum = 0;
    std::size_t bad = 0;
    for (std::size_t repeat = 0; repeat < repeats; ++repeat) {
      for (std::size_t i = 0; i < c.size(); ++i) {
        // Every thread walks the corpus from a different offset, so the same
        // entry is normally being parsed by several threads at once.
        const std::size_t index = (i + rotation) % c.size();
        const outcome got = evaluate(c[index]);
        if (!(got == want[index])) ++bad;
        sum += got.bits ^ (got.consumed + unsigned(got.ec));
      }
    }
    local_checksum = sum;
    local_mismatches = bad;
  }
};
} // namespace

int main(int argc, char** argv) {
  std::size_t repeats = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 20;
  const std::vector<entry> corpus = build_corpus();
  if (corpus.empty()) { std::fprintf(stderr, "empty corpus\n"); return 1; }

  // 1. Single-threaded reference.
  std::vector<outcome> expected(corpus.size());
  for (std::size_t i = 0; i < corpus.size(); ++i) expected[i] = evaluate(corpus[i]);

  // 2. Sanity: the reference must be reproducible before threads run.
  for (std::size_t i = 0; i < corpus.size(); ++i) CHECK(expected[i] == evaluate(corpus[i]));

  const unsigned hardware = std::thread::hardware_concurrency();
  const std::size_t threads = std::max<std::size_t>(2, hardware ? hardware : 2);
  std::printf("corpus: %zu entries; threads: %zu; repeats: %zu\n", corpus.size(), threads, repeats);

  // 3. Single-threaded throughput baseline.
  workload single{&corpus, &expected, 0, repeats};
  const double single_seconds = time_call(single);

  // 4. Concurrent run. Threads start from different rotations and are given
  //    disjoint destinations, so any shared mutable state would show up either
  //    as a wrong result or as a torn comparison.
  std::vector<workload> work;
  work.reserve(threads);
  for (std::size_t t = 0; t < threads; ++t) work.push_back({&corpus, &expected, t, repeats});
  const double parallel_seconds = time_call([&] {
    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (std::size_t t = 0; t < threads; ++t) pool.emplace_back(std::ref(work[t]));
    for (auto& thread : pool) thread.join();
  });

  std::uint64_t checksum = 0;
  std::size_t total_bad = 0;
  for (const auto& w : work) {
    checksum ^= w.local_checksum;
    total_bad += w.local_mismatches;
  }
  const std::size_t operations = corpus.size() * repeats;
  std::printf("single-threaded: %.4f s (%.2f M items/s)\n", single_seconds,
              double(operations) / single_seconds / 1e6);
  // The concurrent run parses the corpus once per thread, so compare per-thread
  // wall time against the single-threaded run.
  const double speedup = single_seconds * double(threads) / parallel_seconds;
  std::printf("%zu threads: %.4f s (%.2f M items/s aggregate, %.2fx)\n", threads,
              parallel_seconds, double(operations) * double(threads) / parallel_seconds / 1e6,
              speedup);
  std::printf("cross-thread mismatches: %zu; checksum: %llx\n", total_bad,
              (unsigned long long)checksum);

  const bool ok = failures == 0 && total_bad == 0;
  std::printf("%s\n", ok ? "concurrency test passed" : "concurrency test FAILED");
  return ok ? 0 : 1;
}
