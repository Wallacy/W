// C23/c2x source variant for the Restaurant branch workload.

#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static void serve(int is_open) {
    fputs(is_open ? "Kitchen open\n" : "Kitchen closed\n", stdout);
    fputs("After service\n", stdout);
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    serve(1);
    serve(0);
    return 0;
}
