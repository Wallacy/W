// C23 sequential reference for the Restaurant static-yield lifecycle workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int64_t stage(int64_t value) {
    return value + 1;
}

static int64_t prepare(int64_t value) {
    // Immediate continuation is a legal schedule for W's non-barrier yield.
    return stage(value) * 2;
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    const int64_t first = prepare(20);
    const int64_t second = prepare(22);
    if (printf("Prepared %" PRId64 "\n", first + second) < 0) return 1;
    return 0;
}
