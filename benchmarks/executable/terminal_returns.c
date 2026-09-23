// Expected exit: 0
// Expected stdout:
// -1,0,1

#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int sign(int value) {
    if (value < 0) {
        return -1;
    } else if (value == 0) {
        return 0;
    }
    return 1;
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif

    volatile int inputs[] = {-5, 0, 7};
    const int negative = sign(inputs[0]);
    const int zero = sign(inputs[1]);
    const int positive = sign(inputs[2]);
    printf("%d,%d,%d\n", negative, zero, positive);
    return 0;
}
