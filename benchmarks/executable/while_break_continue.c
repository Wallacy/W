// Expected exit: 0
// Expected stdout:
// 0,4,8

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int64_t scan(int64_t limit) {
    int64_t index = 0;
    int64_t total = 0;
    while (index < limit) {
        index += 1;
        if (index == 2) continue;
        if (index == 5) break;
        total += index;
    }
    return total;
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    volatile int64_t limits[] = {0, 3, 9};
    printf("%" PRId64 ",%" PRId64 ",%" PRId64 "\n",
           scan(limits[0]), scan(limits[1]), scan(limits[2]));
    return 0;
}
