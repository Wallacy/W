// C23 reference for the Restaurant UInt bitwise-complement workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_zero = UINT64_C(0);

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    const uint64_t inverted = ~runtime_zero;
    return printf("UInt not %" PRIu64 "\n", inverted) < 0 ? 1 : 0;
}
