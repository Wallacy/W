// C23 reference for the public process-arguments-ordering executable workload.
// Expected output cases (argv => exit; stdout):
// [] => 0; "Argument mode compact: count=0\n"
// [""] => 0; "Argument mode compact: count=1\n"
// ["alpha", "beta"] => 0; "Argument mode extended: count=2\n"
// ["alpha", "beta", "gamma"] => 0; "Argument mode extended: count=3\n"

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
        if (printf("Argument mode compact: count=%d\n", count) < 0) return 1;
    } else if (printf("Argument mode extended: count=%d\n", count) < 0) {
        return 1;
    }
    return 0;
}
