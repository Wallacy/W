// C23 reference for the public process-arguments-ordering executable workload.

#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv) {
    (void)argv;

#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif

    const int count = argc - 1;
    if (count < 2) {
        if (printf("Kitchen seats %d guests\n", count) < 0) return 1;
    } else if (printf("Banquet seats %d guests\n", count) < 0) {
        return 1;
    }
    return 0;
}
