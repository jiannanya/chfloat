#include <chfloat/chfloat.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
  #define NOMINMAX
  #include <windows.h>
#endif

#include <fast_float/fast_float.h>

namespace {

using clock_type = std::chrono::steady_clock;

struct bench_result {
  std::string name;
  double seconds = 0.0;
  size_t items = 0;
  double items_per_sec = 0.0;
  double mb_per_sec = 0.0;
};

static double now_seconds() {
  return std::chrono::duration<double>(clock_type::now().time_since_epoch()).count();
}

static void warm_cpu_seconds(double seconds) {
  const double start = now_seconds();
  // Busy loop to reduce cold-frequency / power-state ramp impacting the first timed run.
  volatile std::uint64_t x = 0x123456789abcdef0ULL;
  while ((now_seconds() - start) < seconds) {
    x ^= (x << 7);
    x ^= (x >> 9);
    x *= 0x9e3779b97f4a7c15ULL;
  }
  if (x == 0) std::cerr << "";
}

static void setup_benchmark_process() {
#if defined(_WIN32)
  // Best-effort: reduce scheduler jitter for single-threaded microbenchmarks.
  // If these calls fail, benchmark still works.
  (void)SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  (void)SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
  // Pin to one CPU to avoid cross-core migration.
  (void)SetThreadAffinityMask(GetCurrentThread(), 1ULL);
#endif
}

static std::vector<std::string> make_random_decimal_strings(size_t n, uint32_t seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> sign_dist(0, 1);
  std::uniform_int_distribution<int> int_digits_dist(1, 8);
  std::uniform_int_distribution<int> frac_digits_dist(0, 8);
  std::uniform_int_distribution<int> exp_dist(-30, 30);
  std::uniform_int_distribution<int> digit_dist(0, 9);

  std::vector<std::string> out;
  out.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    std::string s;
    s.reserve(32);

    if (sign_dist(rng)) s.push_back('-');

    int int_digits = int_digits_dist(rng);
    for (int d = 0; d < int_digits; ++d) {
      char c = static_cast<char>('0' + digit_dist(rng));
      // avoid leading zeros too often
      if (d == 0 && c == '0') c = '1';
      s.push_back(c);
    }

    int frac_digits = frac_digits_dist(rng);
    if (frac_digits > 0) {
      s.push_back('.');
      for (int d = 0; d < frac_digits; ++d) {
        s.push_back(static_cast<char>('0' + digit_dist(rng)));
      }
    }

    int exp = exp_dist(rng);
    if (exp != 0) {
      s.push_back('e');
      s += std::to_string(exp);
    }

    out.push_back(std::move(s));
  }

  return out;
}

static std::vector<std::string> make_random_decimal_strings_ex(size_t n, uint32_t seed, int int_digits_min,
                                                               int int_digits_max, int frac_digits_min,
                                                               int frac_digits_max, int exp_min, int exp_max,
                                                               bool force_exp) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> sign_dist(0, 1);
  std::uniform_int_distribution<int> int_digits_dist(int_digits_min, int_digits_max);
  std::uniform_int_distribution<int> frac_digits_dist(frac_digits_min, frac_digits_max);
  std::uniform_int_distribution<int> exp_dist(exp_min, exp_max);
  std::uniform_int_distribution<int> digit_dist(0, 9);

  std::vector<std::string> out;
  out.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    std::string s;
    s.reserve(64);

    if (sign_dist(rng)) s.push_back('-');

    const int int_digits = int_digits_dist(rng);
    for (int d = 0; d < int_digits; ++d) {
      char c = static_cast<char>('0' + digit_dist(rng));
      if (d == 0 && c == '0') c = '1';
      s.push_back(c);
    }

    const int frac_digits = frac_digits_dist(rng);
    if (frac_digits > 0) {
      s.push_back('.');
      for (int d = 0; d < frac_digits; ++d) {
        s.push_back(static_cast<char>('0' + digit_dist(rng)));
      }
    }

    const int exp = exp_dist(rng);
    if (force_exp || exp != 0) {
      s.push_back('e');
      s += std::to_string(exp);
    }

    out.push_back(std::move(s));
  }

  return out;
}

static size_t total_bytes(const std::vector<std::string>& v) {
  size_t b = 0;
  for (auto& s : v) b += s.size();
  return b;
}

template <class Fn>
static bench_result run_bench(const std::string& name, const std::vector<std::string>& inputs, Fn&& fn, size_t iters) {
  // Warmup
  {
    double sink = 0;
    for (size_t i = 0; i < inputs.size(); ++i) {
      sink += fn(inputs[i]);
    }
    if (sink == 1234567.0) std::cerr << "";
  }

  double start = now_seconds();
  double sink = 0;
  for (size_t it = 0; it < iters; ++it) {
    for (size_t i = 0; i < inputs.size(); ++i) {
      sink += fn(inputs[i]);
    }
  }
  double end = now_seconds();
  if (sink == 1234567.0) std::cerr << "";

  const double sec = end - start;
  const size_t items = inputs.size() * iters;
  const double ips = (sec > 0) ? (static_cast<double>(items) / sec) : 0.0;
  const double mbps = (sec > 0) ? (static_cast<double>(total_bytes(inputs) * iters) / (1024.0 * 1024.0) / sec) : 0.0;

  bench_result r;
  r.name = name;
  r.seconds = sec;
  r.items = items;
  r.items_per_sec = ips;
  r.mb_per_sec = mbps;
  return r;
}

static std::string pad_right(std::string s, size_t n) {
  if (s.size() < n) s.append(n - s.size(), ' ');
  return s;
}

static std::string fmt_double(double v, int precision = 2) {
  std::ostringstream oss;
  oss.setf(std::ios::fixed);
  oss << std::setprecision(precision) << v;
  return oss.str();
}

static double median_inplace(std::vector<double>& v) {
  if (v.empty()) return 0.0;
  const size_t n = v.size();
  const size_t mid = n / 2;
  std::nth_element(v.begin(), v.begin() + mid, v.end());
  double m = v[mid];
  if ((n & 1) == 0) {
    std::nth_element(v.begin(), v.begin() + (mid - 1), v.end());
    m = 0.5 * (m + v[mid - 1]);
  }
  return m;
}

template <class Fn>
static bench_result run_bench_stable(const std::string& name, const std::vector<std::string>& inputs, Fn&& fn,
                                     size_t iters, size_t runs) {
  std::vector<double> seconds;
  seconds.reserve(runs);
  for (size_t i = 0; i < runs; ++i) {
    auto r = run_bench(name, inputs, fn, iters);
    seconds.push_back(r.seconds);
  }
  const double med_sec = median_inplace(seconds);
  const size_t items = inputs.size() * iters;
  const double ips = (med_sec > 0) ? (static_cast<double>(items) / med_sec) : 0.0;
  const double mbps = (med_sec > 0) ? (static_cast<double>(total_bytes(inputs) * iters) / (1024.0 * 1024.0) / med_sec)
                                    : 0.0;

  bench_result out;
  out.name = name;
  out.seconds = med_sec;
  out.items = items;
  out.items_per_sec = ips;
  out.mb_per_sec = mbps;
  return out;
}

static void write_markdown_table(std::ofstream& out, const std::vector<bench_result>& rows) {
  // Determine column widths
  size_t w_name = std::strlen("Name");
  size_t w_sec = std::strlen("Seconds");
  size_t w_ips = std::strlen("Items/s");
  size_t w_mbps = std::strlen("MB/s");

  std::vector<std::array<std::string, 4>> cells;
  cells.reserve(rows.size());

  for (auto& r : rows) {
    std::array<std::string, 4> c = {
        r.name,
        fmt_double(r.seconds, 6),
        fmt_double(r.items_per_sec, 0),
        fmt_double(r.mb_per_sec, 2),
    };
    w_name = std::max(w_name, c[0].size());
    w_sec = std::max(w_sec, c[1].size());
    w_ips = std::max(w_ips, c[2].size());
    w_mbps = std::max(w_mbps, c[3].size());
    cells.push_back(std::move(c));
  }

  auto line = [&](const std::string& a, const std::string& b, const std::string& c, const std::string& d) {
    out << "| " << pad_right(a, w_name) << " | " << pad_right(b, w_sec) << " | " << pad_right(c, w_ips)
        << " | " << pad_right(d, w_mbps) << " |\n";
  };

  line("Name", "Seconds", "Items/s", "MB/s");
  line(std::string(w_name, '-'), std::string(w_sec, '-'), std::string(w_ips, '-'), std::string(w_mbps, '-'));
  for (auto& c : cells) {
    line(c[0], c[1], c[2], c[3]);
  }
}

struct scenario_report {
  std::string name;
  size_t n = 0;
  size_t iters = 0;
  std::vector<bench_result> one_shot;
  std::vector<bench_result> stable;
};

static void write_markdown_report(const std::string& path, const std::vector<scenario_report>& scenarios,
                                  size_t stable_runs) {
  std::filesystem::path p(path);
  if (p.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
  }

  std::ofstream out(path, std::ios::binary);
  out << "# chfloat benchmark report\n\n";
  out << "Environment:\n\n";
  out << "- C++: C++17\n";
  out << "- Build: "
#if defined(NDEBUG)
         "Release"
#else
         "Debug"
#endif
      << "\n";

#if defined(_MSC_VER)
  out << "- Compiler: MSVC _MSC_VER=" << _MSC_VER << "\n";
#elif defined(__clang__)
  out << "- Compiler: Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__ << "\n";
#elif defined(__GNUC__)
  out << "- Compiler: GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__ << "\n";
#else
  out << "- Compiler: (unknown)\n";
#endif
  out << "- Baselines: chfloat + std::strtod/strtof\n";
  out << "- Comparison: fast_float\n";
  out << "\n";

  for (const auto& sc : scenarios) {
    out << "## Scenario: " << sc.name << "\n\n";
    out << "- Inputs: n=" << sc.n << ", iters=" << sc.iters << "\n\n";

    out << "### One-shot\n\n";
    write_markdown_table(out, sc.one_shot);

    out << "\n\n### Stable (median)\n\n";
    out << "- Runs: " << stable_runs << " (median seconds)\n\n";
    write_markdown_table(out, sc.stable);
    out << "\n\n";
  }

  out << "\nNotes:\n\n";
  out << "- Items/s counts parsed numbers; MB/s counts input bytes processed.\n";
  out << "- This benchmark is single-threaded and measures throughput on this machine.\n";
  out << "- The 'Stable' table reports median seconds across multiple runs.\n";
}

} // namespace

int main(int argc, char** argv) {
  setup_benchmark_process();

  size_t n = 1'000'00;      // number of distinct strings
  size_t iters = 10;        // repeat count
  uint32_t seed = 12345;
  size_t stable_runs = 7;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--n") == 0 && i + 1 < argc) {
      n = static_cast<size_t>(std::stoull(argv[++i]));
    } else if (std::strcmp(argv[i], "--iters") == 0 && i + 1 < argc) {
      iters = static_cast<size_t>(std::stoull(argv[++i]));
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (std::strcmp(argv[i], "--stable-runs") == 0 && i + 1 < argc) {
      stable_runs = static_cast<size_t>(std::stoull(argv[++i]));
    }
  }

  struct scenario_def {
    const char* name;
    int int_min, int_max;
    int frac_min, frac_max;
    int exp_min, exp_max;
    bool force_exp;
    uint32_t seed_salt;
  };

  // Keep exponent within [-30, 30] so float and double both stay mostly in-range.
  const scenario_def defs[] = {
      {"mixed", 1, 8, 0, 8, -30, 30, false, 0x11111111u},
      {"short_no_exp", 1, 6, 0, 2, 0, 0, false, 0x22222222u},
      {"long_frac", 1, 16, 0, 16, -30, 30, true, 0x33333333u},
  };

  std::vector<scenario_report> reports;
  reports.reserve(std::size(defs));

  for (const auto& def : defs) {
    scenario_report sc;
    sc.name = def.name;
    sc.n = n;
    sc.iters = iters;

    auto inputs = make_random_decimal_strings_ex(n, seed ^ def.seed_salt, def.int_min, def.int_max, def.frac_min,
                                                def.frac_max, def.exp_min, def.exp_max, def.force_exp);

    // Warm CPU to reduce first-timed-run volatility.
    warm_cpu_seconds(0.15);

    sc.one_shot.push_back(run_bench("chfloat::from_chars<double>", inputs,
                                   [](const std::string& s) {
                                     double v = 0;
                                     auto r = chfloat::from_chars(s.data(), s.data() + s.size(), v);
                                     return (r.ec == chfloat::errc::ok) ? v : 0.0;
                                   },
                                   iters));
    sc.stable.push_back(run_bench_stable("chfloat::from_chars<double>", inputs,
                                        [](const std::string& s) {
                                          double v = 0;
                                          auto r = chfloat::from_chars(s.data(), s.data() + s.size(), v);
                                          return (r.ec == chfloat::errc::ok) ? v : 0.0;
                                        },
                                        iters, stable_runs));

    sc.one_shot.push_back(run_bench("fast_float::from_chars<double>", inputs,
                                   [](const std::string& s) {
                                     double v = 0;
                                     auto r = fast_float::from_chars(s.data(), s.data() + s.size(), v);
                                     return (r.ec == std::errc{}) ? v : 0.0;
                                   },
                                   iters));
    sc.stable.push_back(run_bench_stable("fast_float::from_chars<double>", inputs,
                                        [](const std::string& s) {
                                          double v = 0;
                                          auto r = fast_float::from_chars(s.data(), s.data() + s.size(), v);
                                          return (r.ec == std::errc{}) ? v : 0.0;
                                        },
                                        iters, stable_runs));

    sc.one_shot.push_back(run_bench("std::strtod", inputs,
                                   [](const std::string& s) {
                                     char* end = nullptr;
                                     double v = std::strtod(s.c_str(), &end);
                                     return (end != s.c_str()) ? v : 0.0;
                                   },
                                   iters));
    sc.stable.push_back(run_bench_stable("std::strtod", inputs,
                                        [](const std::string& s) {
                                          char* end = nullptr;
                                          double v = std::strtod(s.c_str(), &end);
                                          return (end != s.c_str()) ? v : 0.0;
                                        },
                                        iters, stable_runs));

    sc.one_shot.push_back(run_bench("chfloat::from_chars<float>", inputs,
                                   [](const std::string& s) {
                                     float v = 0;
                                     auto r = chfloat::from_chars(s.data(), s.data() + s.size(), v);
                                     return (r.ec == chfloat::errc::ok) ? static_cast<double>(v) : 0.0;
                                   },
                                   iters));
    sc.stable.push_back(run_bench_stable("chfloat::from_chars<float>", inputs,
                                        [](const std::string& s) {
                                          float v = 0;
                                          auto r = chfloat::from_chars(s.data(), s.data() + s.size(), v);
                                          return (r.ec == chfloat::errc::ok) ? static_cast<double>(v) : 0.0;
                                        },
                                        iters, stable_runs));

    sc.one_shot.push_back(run_bench("fast_float::from_chars<float>", inputs,
                                   [](const std::string& s) {
                                     float v = 0;
                                     auto r = fast_float::from_chars(s.data(), s.data() + s.size(), v);
                                     return (r.ec == std::errc{}) ? static_cast<double>(v) : 0.0;
                                   },
                                   iters));
    sc.stable.push_back(run_bench_stable("fast_float::from_chars<float>", inputs,
                                        [](const std::string& s) {
                                          float v = 0;
                                          auto r = fast_float::from_chars(s.data(), s.data() + s.size(), v);
                                          return (r.ec == std::errc{}) ? static_cast<double>(v) : 0.0;
                                        },
                                        iters, stable_runs));

    sc.one_shot.push_back(run_bench("std::strtof", inputs,
                                   [](const std::string& s) {
                                     char* end = nullptr;
                                     float v = std::strtof(s.c_str(), &end);
                                     return (end != s.c_str()) ? static_cast<double>(v) : 0.0;
                                   },
                                   iters));
    sc.stable.push_back(run_bench_stable("std::strtof", inputs,
                                        [](const std::string& s) {
                                          char* end = nullptr;
                                          float v = std::strtof(s.c_str(), &end);
                                          return (end != s.c_str()) ? static_cast<double>(v) : 0.0;
                                        },
                                        iters, stable_runs));

    reports.push_back(std::move(sc));
  }

  // Write report
  std::string report_path;
#if defined(CHFLOAT_PROJECT_DIR)
  report_path = (std::filesystem::path(CHFLOAT_PROJECT_DIR) / "report" / "benchmark.md").string();
#else
  report_path = std::string("report/benchmark.md");
#endif
  write_markdown_report(report_path, reports, stable_runs);

  // Also print a short summary to stdout.
  std::cout << "Wrote " << report_path << "\n";
  for (const auto& sc : reports) {
    std::cout << "Scenario: " << sc.name << "\n";
    std::cout << "One-shot:\n";
    for (const auto& r : sc.one_shot) {
      std::cout << r.name << ": " << r.items_per_sec << " items/s, " << r.mb_per_sec << " MB/s\n";
    }
    std::cout << "Stable (median, runs=" << stable_runs << "):\n";
    for (const auto& r : sc.stable) {
      std::cout << r.name << ": " << r.items_per_sec << " items/s, " << r.mb_per_sec << " MB/s\n";
    }
  }

  return 0;
}
