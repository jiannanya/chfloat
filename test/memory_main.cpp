#include <chfloat/chfloat.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#if defined(_WIN32)
#include <malloc.h>
#endif
namespace {
std::atomic<std::size_t> allocations{0};
bool tracking = false;
volatile double sink = 0;
}
void* operator new(std::size_t size) {
  if (tracking) ++allocations;
  if (void* p = std::malloc(size ? size : 1)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t size, std::align_val_t alignment) {
  if (tracking) ++allocations;
  const auto a = static_cast<std::size_t>(alignment);
#if defined(_WIN32)
  void* p = _aligned_malloc(size ? size : 1, a);
#else
  void* p = std::aligned_alloc(a, ((size ? size : 1) + a - 1) / a * a);
#endif
  if (!p) throw std::bad_alloc();
  return p;
}
void* operator new[](std::size_t size, std::align_val_t a) { return ::operator new(size, a); }
void operator delete(void* p, std::align_val_t) noexcept {
#if defined(_WIN32)
  _aligned_free(p);
#else
  std::free(p);
#endif
}
void operator delete[](void* p, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete[](void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
int main() {
  // Allocate the adversarial inputs before measuring parser allocations.
  const std::string long_input = "0." + std::string(2 * 1024 * 1024, '0') + "1e2097153";
  const std::string halfway = "1.00000000000000011102230246251565404236316680908203125" + std::string(100000, '0') + "1";
  const std::string long_integer = std::string(2 * 1024 * 1024, '0') + "9223372036854775807";
  const std::string overflowing_integer(200000, '9');
  const char* inputs[] = {"12.5", "1e308", "1e-308", "4.9406564584124654e-324", "1e99999", "nan(payload)", "infinity", "abc",
                          "1.00000005960464477539062500001", long_input.c_str(), halfway.c_str(),
                          long_integer.c_str(), overflowing_integer.c_str()};
  tracking = true;
  bool valid = true;
  for (const char* text : inputs) {
    const char* end = text + std::strlen(text);
    double d = 0; float f = 0;
    const auto rd = chfloat::from_chars(text, end, d);
    chfloat::from_chars(text, end, f);
    sink = d;
    if (text == long_input.c_str()) valid &= rd.ec == chfloat::errc::ok && d == 1;
    chfloat::from_chars(text, end, d, chfloat::chars_format::hex);
    long long integer = 37;
    const auto ri = chfloat::from_chars(text, end, integer);
    if (text == long_integer.c_str()) valid &= ri.ec == chfloat::errc::ok && ri.ptr == end && integer == 9223372036854775807LL;
    if (text == overflowing_integer.c_str()) valid &= ri.ec == chfloat::errc::result_out_of_range && ri.ptr == end && integer == 37;
    for (int base : {2, 16, 36}) {
      unsigned long long u = 0;
      chfloat::from_chars(text, end, u, base);
    }
  }
  tracking = false;
  std::printf("parser heap allocations: %zu; long-input result: %s\n", allocations.load(), valid ? "correct" : "incorrect");
  return allocations != 0 || !valid ? 1 : 0;
}
