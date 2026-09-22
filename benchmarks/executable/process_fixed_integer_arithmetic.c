// Expected with 0 user arguments: exit 0, stdout "Arithmetic 0/4/1\n", stderr <empty>.
// Expected with 126 user arguments: exit 0, stdout "Arithmetic 126/9/127\n", stderr <empty>.
// Expected with 127 user arguments: exit 2, stdout <empty>, stderr <empty>.
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
    if (narrowed == INT8_MAX) return 2;
    const int arithmetic = (((int)narrowed % 11) * 2 / 2) + 7 - 3;
    const int incremented = (int)narrowed + 1;
    return printf("Arithmetic %d/%d/%d\n", (int)narrowed, arithmetic,
                  incremented) < 0 ? 1 : 0;
}
