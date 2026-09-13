#include <chfloat/chfloat.h>
#ifdef CHFLOAT_HAS_REFERENCE
#include <chfloat_reference/chfloat.h>
#endif
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifdef CHFLOAT_HAS_FAST_FLOAT
#include <fast_float/fast_float.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;
volatile std::uint64_t sink = 0;
std::uint64_t pin_benchmark_thread(int cpu) {
#if defined(_WIN32)
  DWORD_PTR process_mask = 0, system_mask = 0;
  if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask)) {
    if (cpu >= int(sizeof(DWORD_PTR) * 8)) throw std::runtime_error("CPU index exceeds affinity mask width");
    const DWORD_PTR first_cpu = cpu < 0 ? process_mask & (~process_mask + 1) : DWORD_PTR(1) << cpu;
    if (!(first_cpu & process_mask)) throw std::runtime_error("requested CPU is unavailable");
    if (SetThreadAffinityMask(GetCurrentThread(), first_cpu)) return first_cpu;
  }
#endif
  if (cpu >= 0) throw std::runtime_error("could not pin the requested CPU (Windows only)");
  return 0;
}
struct dataset {
  std::vector<char> bytes;
  std::vector<std::size_t> offsets;
  std::size_t payload = 0;
  void append(std::string_view text) {
    offsets.push_back(bytes.size());
    bytes.insert(bytes.end(), text.begin(), text.end());
    bytes.push_back('\0');
    payload += text.size();
  }
  std::string_view at(std::size_t i) const {
    const auto end = i + 1 < offsets.size() ? offsets[i + 1] : bytes.size();
    return {bytes.data() + offsets[i], end - offsets[i] - 1};
  }
  std::size_t storage() const { return bytes.capacity() + offsets.capacity() * sizeof(std::size_t); }
};
struct result { std::uint64_t bits; std::size_t consumed; int ec; };
template <class T> result pack(T value, const char* end, std::string_view s, int ec) {
  std::uint64_t bits = 0;
  if (!ec) std::memcpy(&bits, &value, sizeof(T));
  return {bits, std::size_t(end - s.data()), ec};
}
int error_code(std::errc ec) {
  return ec == std::errc{} ? 0 : ec == std::errc::invalid_argument ? 1 : 2;
}
template <class T> result parse_ch(std::string_view s, int base) {
  T v = 0;
  if constexpr (std::is_integral<T>::value) {
    auto r = chfloat::from_chars(s.data(), s.data() + s.size(), v, base);
    return pack(v, r.ptr, s, int(r.ec));
  } else {
    auto r = chfloat::from_chars(s.data(), s.data() + s.size(), v);
    return pack(v, r.ptr, s, int(r.ec));
  }
}
template <class T> result parse_std(std::string_view s, int base) {
  T v = 0;
  if constexpr (std::is_integral<T>::value) {
    auto r = std::from_chars(s.data(), s.data() + s.size(), v, base);
    return pack(v, r.ptr, s, error_code(r.ec));
  } else {
    auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    return pack(v, r.ptr, s, error_code(r.ec));
  }
}
#ifdef CHFLOAT_HAS_REFERENCE
template <class T> result parse_reference(std::string_view s, int base) {
  T v = 0;
  if constexpr (std::is_integral<T>::value) {
    const auto r = chfloat_reference::from_chars(s.data(), s.data() + s.size(), v, base);
    return pack(v, r.ptr, s, int(r.ec));
  } else {
    const auto r = chfloat_reference::from_chars(s.data(), s.data() + s.size(), v);
    return pack(v, r.ptr, s, int(r.ec));
  }
}
#endif
template <class T> result parse_c(std::string_view s, int) {
  char* end = nullptr;
  T value;
  if constexpr (std::is_same<T, float>::value) value = std::strtof(s.data(), &end);
  else value = std::strtod(s.data(), &end);
  int ec = end == s.data() ? 1 : !std::isfinite(value) ? 2 : 0;
  if (!ec && value == 0) {
    // Generated datasets always have a nonzero leading digit.
    ec = 2;
  }
  return pack(value, end, s, ec);
}
struct row {
  std::string name;
  double median, minimum, maximum;
  std::size_t mismatches;
  double paired_speedup = 0;
};
using parser = result (*)(std::string_view, int);
// Keep one identical timing loop for all parsers. Separate inlined loops can
// acquire different register allocation/layout even for identical headers.
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) && !defined(__clang__)
__attribute__((noinline, noclone))
#elif defined(__clang__)
__attribute__((noinline))
#endif
double timed(const dataset& data, std::size_t iters, int base, parser fn) {
  std::uint64_t checksum = 0;
  const auto start = Clock::now();
  for (std::size_t it = 0; it < iters; ++it) {
    for (std::size_t i = 0; i < data.offsets.size(); ++i) {
      const auto r = fn(data.at(i), base);
      checksum += r.bits ^ (r.consumed + unsigned(r.ec));
    }
  }
  const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
  sink = checksum;
  return seconds;
}
row measure(const std::string& name, const dataset& data, const std::vector<result>& reference,
            std::size_t iters, std::size_t runs, int base, parser fn) {
  std::size_t mismatches = 0;
  for (std::size_t i = 0; i < reference.size(); ++i) {
    const auto r = fn(data.at(i), base);
    const auto want = reference[i];
    mismatches += r.ec != want.ec || r.consumed != want.consumed || r.bits != want.bits;
  }
  timed(data, 1, base, fn);
  std::vector<double> times;
  times.reserve(runs);
  for (std::size_t run = 0; run < runs; ++run) times.push_back(timed(data, iters, base, fn));
  std::sort(times.begin(), times.end());
  const auto mid = runs / 2;
  const auto med = runs % 2 ? times[mid] : (times[mid - 1] + times[mid]) / 2;
  return {name, med, times.front(), times.back(), mismatches};
}
#ifdef CHFLOAT_HAS_REFERENCE
void measure_pair(std::vector<row>& rows, const std::string& type, const dataset& data,
    const std::vector<result>& oracle, std::size_t iters, std::size_t runs, int base, parser fn, parser reference) {
  std::size_t errors = 0, reference_errors = 0;
  for (std::size_t i = 0; i < oracle.size(); ++i) {
    const auto want = oracle[i], current = fn(data.at(i), base), previous = reference(data.at(i), base);
    errors += current.ec != want.ec || current.consumed != want.consumed || current.bits != want.bits;
    reference_errors += previous.ec != want.ec || previous.consumed != want.consumed || previous.bits != want.bits;
  }
  timed(data, 1, base, fn);
  timed(data, 1, base, reference);
  std::vector<double> now(runs), before(runs), ratios(runs);
  for (std::size_t run = 0; run < runs; ++run) {
    if (run % 2) { before[run] = timed(data, iters, base, reference); now[run] = timed(data, iters, base, fn); }
    else { now[run] = timed(data, iters, base, fn); before[run] = timed(data, iters, base, reference); }
    ratios[run] = before[run] / now[run];
  }
  auto median = [](std::vector<double>& v) {
    std::sort(v.begin(), v.end());
    return v.size() % 2 ? v[v.size() / 2] : (v[v.size() / 2 - 1] + v[v.size() / 2]) / 2;
  };
  const auto current_median = median(now), reference_median = median(before), speedup = median(ratios);
  rows.push_back({"chfloat<" + type + ">", current_median, now.front(), now.back(), errors, speedup});
  rows.push_back({"reference_chfloat<" + type + ">", reference_median, before.front(), before.back(), reference_errors});
}
#endif
struct options {
  std::size_t n = 100000, iters = 10, runs = 7;
  std::uint64_t seed = 12345;
  int cpu = -1;
  std::string report = "report/benchmark.md", scenario;
};
enum class input_shape { normal, leading_zeroes, large_exponent, zero_exponent, integer_overflow, midpoint_tail };
struct scenario {
  const char* name;
  int int_max, frac_max, exp_min, exp_max, base;
  input_shape shape = input_shape::normal;
};
dataset generate(const scenario& sc, const options& opt) {
  std::mt19937_64 rng(opt.seed);
  dataset data;
  data.offsets.reserve(opt.n);
  data.bytes.reserve(opt.n * std::size_t(sc.int_max + sc.frac_max + 16 + (sc.shape == input_shape::normal ? 0 : 320)));
  for (std::size_t i = 0; i < opt.n; ++i) {
    std::string s;
    if (sc.base) {
      const auto value = static_cast<long long>(rng() & 0x7fffffffffffffffULL);
      char buffer[80];
      const auto r = std::to_chars(buffer, buffer + sizeof(buffer), (rng() & 1) ? -value : value, sc.base);
      s.assign(buffer, r.ptr);
    } else {
      if (rng() & 1) s += '-';
      const int ints = 1 + int(rng() % unsigned(sc.int_max));
      const int fracs = int(rng() % unsigned(sc.frac_max + 1));
      for (int j = 0; j < ints; ++j) s += char('0' + (j ? rng() % 10 : 1 + rng() % 9));
      if (fracs) {
        s += '.';
        for (int j = 0; j < fracs; ++j) s += char('0' + rng() % 10);
      }
      const int exponent = sc.exp_min + int(rng() % unsigned(sc.exp_max - sc.exp_min + 1));
      if (exponent) { s += 'e'; s += std::to_string(exponent); }
    }
    switch (sc.shape) {
      case input_shape::leading_zeroes: s.insert(s[0] == '-' ? 1 : 0, 256, '0'); break;
      case input_shape::large_exponent:
        s += rng() & 1 ? "e+" : "e-";
        s.append(256, '9');
        break;
      case input_shape::zero_exponent: s += "e-"; s.append(256, '0'); s += '2'; break;
      case input_shape::integer_overflow: s.append(256, '9'); break;
      case input_shape::midpoint_tail:
        s = rng() & 1 ? "1.000000059604644775390625" : "1.00000000000000011102230246251565404236316680908203125";
        s.append(128 + rng() % 128, '0');
        s += char('0' + rng() % 10);
        if (rng() & 1) s.insert(s.begin(), '-');
        break;
      default: break;
    }
    data.append(s);
  }
  // Storage is reported explicitly, including capacity, with no per-number allocation.
  data.bytes.shrink_to_fit();
  return data;
}
template <class T> void benchmark_type(std::vector<row>& rows, const char* type, const dataset& data, const scenario& sc, const options& opt) {
  std::vector<result> reference;
  reference.reserve(data.offsets.size());
  for (std::size_t i = 0; i < data.offsets.size(); ++i) reference.push_back(parse_std<T>(data.at(i), sc.base));
#ifdef CHFLOAT_HAS_REFERENCE
  measure_pair(rows, type, data, reference, opt.iters, opt.runs, sc.base, parse_ch<T>, parse_reference<T>);
#else
  rows.push_back(measure(std::string("chfloat<") + type + ">", data, reference, opt.iters, opt.runs, sc.base, parse_ch<T>));
#endif
  rows.push_back(measure(std::string("std::from_chars<") + type + ">", data, reference, opt.iters, opt.runs, sc.base, parse_std<T>));
  if constexpr (std::is_floating_point<T>::value) {
    rows.push_back(measure(std::is_same<T, float>::value ? "std::strtof" : "std::strtod", data, reference, opt.iters, opt.runs, sc.base, parse_c<T>));
#ifdef CHFLOAT_HAS_FAST_FLOAT
    rows.push_back(measure(std::string("fast_float<") + type + ">", data, reference, opt.iters, opt.runs, sc.base, [](std::string_view s, int) {
      T value = 0;
      const auto r = fast_float::from_chars(s.data(), s.data() + s.size(), value);
      return pack(value, r.ptr, s, error_code(r.ec));
    }));
#endif
  }
}
std::uint64_t number(std::string_view s) {
  std::uint64_t value = 0;
  const auto r = std::from_chars(s.data(), s.data() + s.size(), value);
  if (r.ec != std::errc{} || r.ptr != s.data() + s.size()) throw std::runtime_error("invalid numeric option");
  return value;
}
} // namespace
int main(int argc, char** argv) try {
  options opt;
  for (int i = 1; i < argc; ++i) {
    const std::string_view key = argv[i];
    if (key == "--help") {
      std::cout << "Usage: chfloat_benchmark [--n N] [--iters N] [--runs N] [--seed N] [--cpu N (Windows)] [--scenario NAME] [--report PATH]\n";
      return 0;
    }
    if (++i == argc) throw std::runtime_error("missing option value");
    const std::string_view value = argv[i];
    if (key == "--n") opt.n = static_cast<std::size_t>(number(value));
    else if (key == "--iters") opt.iters = static_cast<std::size_t>(number(value));
    else if (key == "--runs" || key == "--stable-runs") opt.runs = static_cast<std::size_t>(number(value));
    else if (key == "--seed") opt.seed = number(value);
    else if (key == "--cpu") {
      const auto cpu = number(value);
      if (cpu >= 64) throw std::runtime_error("CPU index must be below 64");
      opt.cpu = static_cast<int>(cpu);
    }
    else if (key == "--report") opt.report = value;
    else if (key == "--scenario") opt.scenario = value;
    else throw std::runtime_error("unknown option: " + std::string(key));
  }
  if (!opt.n || !opt.iters || !opt.runs) throw std::runtime_error("n, iters and runs must be positive");
  if (opt.n > (std::numeric_limits<std::size_t>::max)() / 512 || opt.iters > (std::numeric_limits<std::size_t>::max)() / opt.n)
    throw std::runtime_error("requested workload is too large");
  const scenario scenarios[] = {
    {"mixed", 8, 8, -30, 30, 0}, {"short_no_exp", 6, 2, 0, 0, 0}, {"long_frac", 16, 32, -30, 30, 0},
    {"wide_range", 19, 60, -350, 310, 0}, {"integer_decimal", 20, 0, 0, 0, 10}, {"integer_hex", 16, 0, 0, 0, 16},
    {"leading_zeroes", 8, 8, 0, 0, 0, input_shape::leading_zeroes},
    {"large_exponent", 8, 8, 0, 0, 0, input_shape::large_exponent},
    {"zero_exponent", 8, 8, 0, 0, 0, input_shape::zero_exponent},
    {"integer_overflow", 20, 0, 0, 0, 10, input_shape::integer_overflow},
    {"integer_zeroes", 20, 0, 0, 0, 10, input_shape::leading_zeroes},
    {"midpoint_tail", 8, 8, 0, 0, 0, input_shape::midpoint_tail}
  };
  bool selected = opt.scenario.empty();
  for (const auto& sc : scenarios) selected |= opt.scenario == sc.name;
  if (!selected) throw std::runtime_error("unknown scenario: " + opt.scenario);
  const auto affinity = pin_benchmark_thread(opt.cpu);
  const std::filesystem::path path(opt.report);
  if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
  std::ofstream report(path);
  if (!report) throw std::runtime_error("cannot open report: " + opt.report);
  report << "# chfloat benchmark report\n\n";
#ifdef CHFLOAT_BASELINE
  report << "Implementation: baseline snapshot (same benchmark harness).\n\n";
#else
  report << "Implementation: current headers.\n\n";
#endif
#ifdef CHFLOAT_COMPACT
  report << "Cached powers: compact (5,208 bytes).\n\n";
#else
  report << "Cached powers: full (10,416 bytes).\n\n";
#endif
#ifdef CHFLOAT_FORCE_PORTABLE
  report << "Arithmetic: portable scalar operations.\n\n";
#else
  report << "Arithmetic: platform default.\n\n";
#endif
  report << "Thread affinity mask: " << affinity << " (0 means not pinned).\n\n";
#ifdef CHFLOAT_HAS_REFERENCE
  report << "Current/reference timings alternate within each run; paired speedup is the median of reference/current time ratios.\n\n";
  report << "Reference float_parse.h SHA-256: `" << CHFLOAT_REFERENCE_HASH << "`.\n\n";
#endif
#if defined(__clang__)
  report << "Compiler: Clang " << __clang_version__ << "\n\n";
#elif defined(_MSC_VER)
  report << "Compiler: MSVC " << _MSC_VER << "\n\n";
#else
  report << "Compiler: " << __VERSION__ << "\n\n";
#endif
#ifdef NDEBUG
  report << "Build: Release; ";
#else
  report << "Build: Debug; ";
#endif
  report << "C++17; seed=" << opt.seed << "; n=" << opt.n << "; iters=" << opt.iters << "; runs=" << opt.runs << ".\n\n"
         << "Each parser is checked against std::from_chars before timing. Mismatches include value bits, error codes and consumed length. "
         << "Error outputs are normalized. All parsers use one non-inlined timing loop with the same function-pointer call boundary. "
         << "Timings include indirect calls and result consumption; each run has identical packed input. "
         << "Heap storage below is dataset capacity, excluding reference results and process/runtime overhead.\n\n";
  bool correct = true;
  for (const auto& sc : scenarios) {
    if (!opt.scenario.empty() && opt.scenario != sc.name) continue;
    const auto data = generate(sc, opt);
    std::vector<row> rows;
    if (sc.base) benchmark_type<long long>(rows, "int64", data, sc, opt);
    else { benchmark_type<double>(rows, "double", data, sc, opt); benchmark_type<float>(rows, "float", data, sc, opt); }
    report << "## " << sc.name << "\n\nInput bytes: " << data.payload << "; dataset allocated bytes: " << data.storage() << ".\n\n"
           << "| Parser | Median s | Min s | Max s | M items/s | MiB/s | Mismatches | Paired speedup |\n"
           << "|---|---:|---:|---:|---:|---:|---:|---:|\n";
    for (const auto& r : rows) {
      if (r.name.compare(0, 8, "chfloat<") == 0) correct &= r.mismatches == 0;
      const double rate = double(opt.n) * double(opt.iters) / r.median / 1e6;
      report << "| " << r.name << " | " << std::fixed << std::setprecision(6) << r.median << " | " << r.minimum << " | " << r.maximum
             << " | " << std::setprecision(3) << rate << " | " << double(data.payload) * double(opt.iters) / r.median / 1048576
             << " | " << r.mismatches << " | ";
      if (r.paired_speedup) report << r.paired_speedup << "x";
      else report << "-";
      report << " |\n";
      std::cout << sc.name << ": " << r.name << " " << rate << " M items/s; mismatches=" << r.mismatches << '\n';
      if (r.paired_speedup) std::cout << "  paired speedup: " << r.paired_speedup << "x\n";
    }
    report << '\n';
  }
  report << "Throughput is specific to this workload, compiler and machine.\n";
  report.flush();
  if (!report) throw std::runtime_error("failed writing report: " + opt.report);
  std::cout << "Wrote " << opt.report << '\n';
  return correct ? 0 : 2;
} catch (const std::exception& e) {
  std::cerr << "Benchmark error: " << e.what() << '\n';
  return 1;
}
