// C23 reference for the Restaurant UInt binary-bitwise workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_high = UINT64_C(9223372036854775808);
static volatile uint64_t runtime_lower = UINT64_C(9223372036854775807);

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    const uint64_t left = runtime_high;
    const uint64_t right = runtime_lower;
    const uint64_t combined = left | (right ^ (left & right));
    return printf("UInt bits %" PRIu64 "\n", combined) < 0 ? 1 : 0;
}
