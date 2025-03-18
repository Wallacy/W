#include <stdio.h>
#include <stdint.h>

void print_float128(__float128 x) {
    // Parte inteira
    __uint128_t inteiro = (__uint128_t)x;
    printf("%llu%019llu.", 
           (unsigned long long)(inteiro / 1000000000000000000ULL),
           (unsigned long long)(inteiro % 1000000000000000000ULL));
    
    // Parte decimal (40 dígitos após o ponto)
    x = x - (__float128)inteiro;
    for(int i = 0; i < 40; i++) {
        x *= 10;
        printf("%d", (int)x);
        x -= (int)x;
    }
    printf("\n");
}

int main() {
    uint64_t a = UINT64_MAX;
    uint64_t b = a-1;
    printf(">> 0x%016lx >> 0x%016lx, %lu - %lu\n",a, b, a,b);

    // Exemplo com uint128_t (usando valor grande)
    __uint128_t u128_a = ((__uint128_t)UINT64_MAX + 1) * 2;
    __uint128_t u128_b = u128_a + 1000000;
    
    // Impressão dividindo em parte alta e baixa (64 bits cada)
    printf("u128_a (hex): %016lx%016lx\n", 
           (unsigned long)(u128_a >> 64),
           (unsigned long)(u128_a & UINT64_MAX));
    
    // Exemplo com int128_t
    __int128_t i128 = ((__int128_t)INT64_MAX) * 2;
    printf("i128 value: %lld%019lld\n",
           (long long)(i128 / 1000000000000000000LL),
           (long long)(i128 % 1000000000000000000LL));

    // Float128 (simplificado para long double)
    __float128 f128 = 123456789012345.123456789;
    printf("f128 aproximado: %.15Lf\n", (long double)f128);

    // Float128 com mais precisão
    __float128 f128_preciso = 123456789012345.123456789123456789Q;
    printf("f128 com mais precisão:\n");
    print_float128(f128_preciso);

    return 0;
}