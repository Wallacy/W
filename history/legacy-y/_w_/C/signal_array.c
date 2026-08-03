#include <stdatomic.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <pthread.h>

#define ARRAY_SIZE 1024 // 8 KB para ponteiros de 8 bytes (1024 elementos)

// Tipo para as tarefas (funções a serem executadas)
typedef void (*FuncPtr)();

// Estrutura de cada array de tarefas
typedef struct {
    FuncPtr tasks[ARRAY_SIZE];
    _Atomic int count; // Número de tarefas no array
} TaskArray;

// Estrutura da fila de tarefas
typedef struct {
    TaskArray **arrays; // Lista de arrays
    _Atomic int current_index; // Índice do array atual para adição
    _Atomic int io_index; // Índice do array que a thread de I/O está consumindo
    int num_arrays; // Total de arrays alocados
} TaskQueue;

// Inicializa a fila
void init_queue(TaskQueue *q) {
    q->arrays = malloc(sizeof(TaskArray*));
    q->arrays[0] = malloc(sizeof(TaskArray));
    atomic_init(&q->arrays[0]->count, 0);
    atomic_init(&q->current_index, 0);
    atomic_init(&q->io_index, 0);
    q->num_arrays = 1;
}

// Adiciona uma tarefa à fila
bool enqueue(TaskQueue *q, FuncPtr func) {
    int idx = atomic_load(&q->current_index);
    TaskArray *array = q->arrays[idx];
    int count = atomic_load(&array->count);

    if (count < ARRAY_SIZE) {
        array->tasks[count] = func;
        atomic_store(&array->count, count + 1);
        return true;
    } else {
        // Array cheio, cria um novo
        TaskArray *new_array = malloc(sizeof(TaskArray));
        atomic_init(&new_array->count, 0);
        q->num_arrays++;
        q->arrays = realloc(q->arrays, q->num_arrays * sizeof(TaskArray*));
        q->arrays[q->num_arrays - 1] = new_array;
        atomic_store(&q->current_index, q->num_arrays - 1);
        new_array->tasks[0] = func;
        atomic_store(&new_array->count, 1);
        return true;
    }
}

// Função da thread de I/O (simulada aqui)
void *io_thread(void *arg) {
    TaskQueue *q = (TaskQueue *)arg;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    int sig;

    while (true) {
        // Aguarda o sinal SIGUSR1
        sigwait(&set, &sig);

        int io_idx = atomic_load(&q->io_index);
        if (io_idx >= q->num_arrays) {
            printf("Nenhum array novo para processar.\n");
            continue;
        }

        TaskArray *array = q->arrays[io_idx];
        int count = atomic_load(&array->count);
        for (int i = 0; i < count; i++) {
            array->tasks[i](); // Executa a tarefa
        }
        atomic_store(&q->io_index, io_idx + 1); // Move para o próximo array
    }
    return NULL;
}

// Exemplo de tarefa
void example_task() {
    printf("Tarefa executada!\n");
}

// Teste básico
int main() {
    TaskQueue q;
    init_queue(&q);

    // Adiciona algumas tarefas
    enqueue(&q, example_task);
    enqueue(&q, example_task);

    // Bloqueia SIGUSR1 na thread principal
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // Cria a thread de I/O
    pthread_t tid;
    pthread_create(&tid, NULL, io_thread, &q);

    // Envia o sinal SIGUSR1 para a thread de I/O
    pthread_kill(tid, SIGUSR1);

    pthread_join(tid, NULL);

    for (int i = 0; i < q.num_arrays; i++) {
        free(q.arrays[i]);
    }
    free(q.arrays);

    return 0;
}