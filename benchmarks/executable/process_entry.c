// C23 reference for the public process-entry executable workload.

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

    if (argc == 1) {
        fputs("missing\n", stdout);
        return 2;
    }

    fputs("received\n", stdout);
    return 0;
}
