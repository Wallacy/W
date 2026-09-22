#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static long long bill(long long total) { return total + 2; }

long long unused_receipt(long long total) { return total / 2; }

static void hidden_menu(void) { (void)printf("Never served\n"); }

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 2;
#endif
  return printf("Bill %lld\n", bill(40)) < 0 ? 1 : 0;
}
