#include <chfloat/chfloat.h>
#include <cstdio>
#include <cstring>
int main(int argc, char** argv) {
  const char* s = argc > 1 ? argv[1] : "123.456";
  const char* end = s + std::strlen(s);
  double d = 0; float f = 0; long long i = 0;
  const auto rd = chfloat::from_chars(s, end, d);
  const auto rf = chfloat::from_chars(s, end, f);
  const auto ri = chfloat::from_chars(s, end, i);
  std::printf("%g %g %lld (%d,%d,%d)\n", d, double(f), i, int(rd.ec), int(rf.ec), int(ri.ec));
}
