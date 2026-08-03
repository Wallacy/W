#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>




// Definição do TaggedPointer
typedef union TaggedPointer {
    uint64_t raw;
    struct {
        uint64_t value : 62; // 62 bits para o valor
        uint8_t tag : 2;     // 2 bits para a tag
    };
} TaggedPointer;

#define red "\e[91m" //Red
#define grn "\e[92m" //Green
#define blu "\e[94m" //Blue
#define DEF "\e[0m"  //Default color and effects 
// Função para imprimir os bits de um double
void print_bits(void* x) {
    uint64_t bits;
    memcpy(&bits, x, sizeof(double));
    for (int i = 63; i >= 0; i--) {
        uint8_t b = (bits >> i) & 1;
        b ? printf(grn) : printf(DEF);
        printf("%u", b);
        if (i == 63 || i == 52) printf("    "); // Separa sinal e expoente
    }
    printf(DEF"\n");
}

// Armazena o float62 no TaggedPointer
void store_float62_in_tagged(TaggedPointer* tp, double float64) {
    uint64_t bits;
    memcpy(&bits, &float64, sizeof(double));
    tp->value = bits >> 2; // Pega os 62 bits mais significativos
    tp->tag = 0;           // Tag 0 para indicar float62
}

// Recupera o float62 do TaggedPointer
double retrieve_float62_from_tagged(TaggedPointer* tp) {
    uint64_t bits = tp->value << 2; // Reposiciona os 62 bits, preenchendo os 2 últimos com zeros
    double float62;
    memcpy(&float62, &bits, sizeof(double));
    return float62;
}

// Teste
int main(int argc, char *argv[]) {
    double x = 1.0e-45;
    if (argc >= 2) {
        x = atof(argv[1]);
    }

    printf("Valor original         : %.15f - %.50f\n", x, x);
    printf("Bits  original         : ");
    print_bits(&x);

    TaggedPointer tp;
    store_float62_in_tagged(&tp, x);
    double recovered = retrieve_float62_from_tagged(&tp);
    printf("Valor recuperado       : %.15f - %.50f\n", recovered, recovered);
    printf("Bits  recuperados      : ");
    print_bits(&recovered);

    if (argc >= 3) {
        printf("\n************************************\n");
        // Other tests

        union {
            uint64_t i;
            double d;
        } y,z;
    
        y.i = z.i = 1;
        printf("F64   smallest non-zero: %.15f - %.50f\n", y.d, y.d);
        printf("Bits                   : ");
        print_bits((double *)&y);
        printf("F64        using .1075f: %.1075f\n", y.d);
        z.i <<= 2;
        printf("F62   smallest non-zero: %.15f - %.50f\n", z.d, z.d);
        printf("Bits                   : ");
        print_bits((double *)&z);
        printf("F62        using .1075f: %.1075f\n", z.d);
    }


    return 0;
}