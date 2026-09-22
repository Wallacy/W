// Expected with 0 user arguments: exit 0, stdout "Arithmetic 0/4\n", stderr <empty>.
// Expected with 127 user arguments: exit 0, stdout "Arithmetic 127/10\n", stderr <empty>.
// Expected with 128 user arguments: exit 1, stdout <empty>, stderr <empty>.

#include <stdint.h>
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
    if (count < 0 || count > INT8_MAX) return 1;
    const int8_t narrowed = (int8_t)count;
    const int arithmetic = (((int)narrowed % 11) * 2 / 2) + 7 - 3;
    return printf("Arithmetic %d/%d\n", (int)narrowed, arithmetic) < 0 ? 1 : 0;
}
