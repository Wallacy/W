#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <PID> <valor_inteiro> <mensagem>\n", argv[0]);
        return 1;
    }

    pid_t target_pid = atoi(argv[1]);
    int signal_number = SIGUSR2;

    union sigval value;
    value.sival_int = atoi(argv[2]);
    value.sival_ptr = (void*) strdup(argv[3]);

    if (sigqueue(target_pid, signal_number, value) == 0) {
        printf("Sinal enviado com sucesso para o processo com PID %d!\n", target_pid);
    } else {
        perror("Erro ao enviar sinal:");
    }

    return 0;
}

// Não funciona no MacOS o sigqueue na libc padrão...
