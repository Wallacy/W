// Expected with 0 user arguments: exit 0, stdout "Exact 0\n", stderr <empty>.
// Expected with 127 user arguments: exit 0, stdout "Exact 127\n", stderr <empty>.
// Expected with 128 user arguments: exit 1, stdout <empty>, stderr <empty>.

#include <stdint.h>
#include <stdio.h>

int main(int argc, char **argv) {
    (void)argv;
    const int count = argc - 1;
    if (count < 0 || count > INT8_MAX) return 1;
    const int8_t narrowed = (int8_t)count;
    return printf("Exact %d\n", (int)narrowed) < 0 ? 1 : 0;
}
