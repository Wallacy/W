#include <stdatomic.h>
#include <stdlib.h>
#include <setjmp.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

// Configurações padrão
#ifndef MODEL
#define MODEL 1 // 1: Corrotinas, 2: Thread Pool
#endif
#ifndef USE_COROUTINES
#define USE_COROUTINES 1 // Apenas para Modelo 1 (1: corrotinas, 0: enfileiramento)
#endif

// Estruturas
typedef struct Task
{
    void (*function)(void *, void (*)(void *)); // Função da tarefa
    void *args;                                 // Argumentos
    struct Task *next;                          // Próxima tarefa
} Task;

typedef struct
{
    Task *head;                // Cabeça da fila
    Task *tail;                // Cauda da fila
    pthread_mutex_t mutex;     // Proteção da fila
    _Atomic int tasks_pending; // Contador de tarefas pendentes
} TaskQueue;

typedef struct
{
    jmp_buf context;                        // Contexto da corrotina
    int state;                              // 0: inicial, 1: rodando, 2: pausado, 3: concluído
    void (*func)(void *, void (*)(void *)); // Função da corrotina
    void *args;                             // Argumentos
    void (*handler)(void *);                // Completion handler
} Coroutine;

typedef struct
{
    TaskQueue *queue;    // Fila de tarefas
    _Atomic int running; // Estado de execução
} Module;

// Funções da fila
void init_queue(TaskQueue *queue)
{
    queue->head = NULL;
    queue->tail = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
    atomic_store(&queue->tasks_pending, 0);
}

void add_task(TaskQueue *queue, Task *task)
{
    task->next = NULL;
    pthread_mutex_lock(&queue->mutex);
    if (!queue->head)
    {
        queue->head = task;
        queue->tail = task;
    }
    else
    {
        queue->tail->next = task;
        queue->tail = task;
    }
    atomic_fetch_add(&queue->tasks_pending, 1);
    pthread_mutex_unlock(&queue->mutex);
}

Task *get_task(TaskQueue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    if (!queue->head)
    {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    Task *task = queue->head;
    queue->head = task->next;
    if (!queue->head)
        queue->tail = NULL;
    atomic_fetch_sub(&queue->tasks_pending, 1);
    pthread_mutex_unlock(&queue->mutex);
    return task;
}
void yield(Coroutine *co);
void socket_task_step2(void *args, void (*handler)(void *));
void socket_task_step3(void *args, void (*handler)(void *));
void socket_task_step4(void *args, void (*handler)(void *));
void async_task(void *args, void (*handler)(void *));
void compute_task(void *args, void (*handler)(void *));
void socket_task(void *args, void (*handler)(void *));

// Tarefas
void socket_task(void *args, void (*handler)(void *))
{
#if MODEL == 1 && USE_COROUTINES
    Coroutine *co = (Coroutine *)args;
    printf("Abrindo socket...\n");
    yield(co);
    printf("Enviando mensagem...\n");
    yield(co);
    printf("Recebendo resposta...\n");
    yield(co);
    printf("Socket fechado\n");
#else
    Module *mod = (Module *)args;
    printf("Abrindo socket...\n");
    Task *step2 = malloc(sizeof(Task));
    step2->function = socket_task_step2;
    step2->args = mod;
    add_task(mod->queue, step2);
    return;
#endif
    if (handler)
        handler("resposta recebida");
}

#if MODEL == 1 && !USE_COROUTINES || MODEL == 2
void socket_task_step2(void *args, void (*handler)(void *))
{
    Module *mod = (Module *)args;
    printf("Enviando mensagem...\n");
    Task *step3 = malloc(sizeof(Task));
    step3->function = socket_task_step3;
    step3->args = mod;
    add_task(mod->queue, step3);
}

void socket_task_step3(void *args, void (*handler)(void *))
{
    Module *mod = (Module *)args;
    printf("Recebendo resposta...\n");
    Task *step4 = malloc(sizeof(Task));
    step4->function = socket_task_step4;
    step4->args = mod;
    add_task(mod->queue, step4);
}

void socket_task_step4(void *args, void (*handler)(void *))
{
    printf("Socket fechado\n");
    if (handler)
        handler("resposta recebida");
}
#endif

void async_task(void *args, void (*handler)(void *))
{
#if MODEL == 1 && USE_COROUTINES
    Coroutine *co = (Coroutine *)args;
    printf("Iniciando tarefa assíncrona\n");
    yield(co);
    printf("Tarefa assíncrona concluída\n");
#else
    printf("Iniciando tarefa assíncrona\n");
    sleep(1); // Simulação de trabalho
    printf("Tarefa assíncrona concluída\n");
#endif
    if (handler)
        handler("resultado simples");
}

void compute_task(void *args, void (*handler)(void *))
{
#if MODEL == 1 && USE_COROUTINES
    Coroutine *co = (Coroutine *)args;
    printf("Iniciando computação pesada...\n");
    yield(co);
    printf("Processando dados...\n");
    yield(co);
    printf("Computação concluída\n");
#else
    printf("Iniciando computação pesada...\n");
    sleep(1); // Simulação de trabalho
    printf("Processando dados...\n");
    sleep(1); // Simulação de trabalho
    printf("Computação concluída\n");
#endif
    if (handler)
        handler("resultado computado");
}

// Corrotinas
#if MODEL == 1 && USE_COROUTINES
void yield(Coroutine *co)
{
    if (co->state == 1)
    {
        co->state = 2;
        if (setjmp(co->context) == 0)
            longjmp(((Coroutine *)co->args)->context, 1);
    }
}

void run_coroutine(Coroutine *co)
{
    if (co->state == 0)
    {
        co->state = 1;
        co->func(co, co->handler);
        co->state = 3;
    }
    else if (co->state == 2)
    {
        co->state = 1;
        longjmp(co->context, 1);
    }
}
#endif

// Modelo 1: Thread principal com corrotinas ou enfileiramento
#if MODEL == 1
void *module_thread(void *arg)
{
    Module *mod = (Module *)arg;
    TaskQueue *queue = mod->queue;
#if USE_COROUTINES
    Coroutine coroutines[10];
    int co_count = 0;
#endif
    while (atomic_load(&mod->running) || atomic_load(&queue->tasks_pending) > 0)
    {
        Task *task = get_task(queue);
        if (task)
        {
#if USE_COROUTINES
            Coroutine *co = &coroutines[co_count++];
            co->func = task->function;
            co->args = co;
            co->handler = NULL;
            co->state = 0;
            run_coroutine(co);
#else
            task->function(mod, NULL);
#endif
            free(task);
        }
#if USE_COROUTINES
        int active = 0;
        for (int i = 0; i < co_count; i++)
        {
            if (coroutines[i].state == 2)
            {
                run_coroutine(&coroutines[i]);
                active++;
            }
        }
        if (!active && !atomic_load(&queue->tasks_pending))
            break;
#endif
        sched_yield();
    }
    return NULL;
}
#endif

// Modelo 2: Thread pool
#if MODEL == 2
#define MAX_THREADS 4
void *worker_thread(void *arg)
{
    Module *mod = (Module *)arg;
    TaskQueue *queue = mod->queue;
    while (atomic_load(&mod->running) || atomic_load(&queue->tasks_pending) > 0)
    {
        Task *task = get_task(queue);
        if (task)
        {
            task->function(mod, NULL);
            free(task);
        }
        sched_yield();
    }
    return NULL;
}

void start_module(Module *mod)
{
    atomic_store(&mod->running, 1);
    pthread_t threads[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++)
    {
        pthread_create(&threads[i], NULL, worker_thread, mod);
    }
    for (int i = 0; i < MAX_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }
}
#endif

int main()
{
    TaskQueue queue;
    init_queue(&queue);
    Module mod = {&queue, 0};

#if MODEL == 1
    atomic_store(&mod.running, 1);
    pthread_t thread;
    pthread_create(&thread, NULL, module_thread, &mod);
#else
    start_module(&mod);
#endif

    // Enfileira tarefas
    Task *task1 = malloc(sizeof(Task));
    task1->function = socket_task;
    task1->args = &mod;
    add_task(&queue, task1);

    Task *task2 = malloc(sizeof(Task));
    task2->function = async_task;
    task2->args = &mod;
    add_task(&queue, task2);

    Task *task3 = malloc(sizeof(Task));
    task3->function = compute_task;
    task3->args = &mod;
    add_task(&queue, task3);

#if MODEL == 1
    pthread_join(thread, NULL);
#endif

    // Limpeza
    pthread_mutex_destroy(&queue.mutex);
    return 0;
}