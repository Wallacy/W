// Expected with 0 user arguments: exit 0, stdout <empty>, stderr <empty>.
// Expected with 128 user arguments: exit 1, stdout <empty>, stderr <empty>.

#include <stdint.h>

int main(int argc, char **argv) {
    (void)argv;
    const int count = argc - 1;
    return count >= 0 && count <= INT8_MAX ? 0 : 1;
}
