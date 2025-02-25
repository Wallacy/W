#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Template para função W
#define W_FUNCTION(nome, retorno, args, corpo) \
    retorno nome(args) { \
        corpo \
    }

// Template para loop for
#define W_FOR(variavel, inicio, fim, passo, corpo) \
    for (int variavel = inicio; variavel < fim; variavel += passo) { \
        corpo \
    }

// Template para print
#define W_PRINT(mensagem) \
    printf("%s",mensagem);

// Função W convertida para C
W_FUNCTION(imprime_ola_mundo, void, int n, {
    W_FOR(i, 0, n, 1, {
        W_PRINT("Hello World!\n")
    })
})


// Template para interpolação de string
#define W_STRING_INTERPOLATION(str, ...) \
    ({ \
        char *temp = malloc(strlen(str) + 50); /* Alocar espaço suficiente */ \
        sprintf(temp, str, __VA_ARGS__); \
        temp; \
    })

// Conversão da função W para C
W_FUNCTION(cumprimentar, void, char *nome, {
    char *mensagem = W_STRING_INTERPOLATION("Olá, %s!", nome);
    W_PRINT(mensagem);
    free(mensagem);
})

int main() {
    imprime_ola_mundo(5);
    cumprimentar("Wall");
    return 0;
}