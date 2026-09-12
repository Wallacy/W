// C23 reference for the public process-arguments-count executable workload.

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

    if (printf("Argument count %d\n", argc - 1) < 0) return 1;
    return 0;
}
