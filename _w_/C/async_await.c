#include <stdatomic.h>
#include <stdlib.h>
#include <setjmp.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

// Estrutura de uma tarefa
typedef struct Task
{
    void (*function)(void *); // Função da tarefa
    void *args;               // Argumentos
    struct Task *next;        // Próxima tarefa
} Task;

// Fila de tarefas lock-free
typedef struct
{
    _Atomic(Task *) head;  // Cabeça da fila
    _Atomic(Task *) tail;  // Cauda da fila
    atomic_flag has_tasks; // Sinalização de novas tarefas
} TaskQueue;

// Estrutura do módulo
typedef struct
{
    TaskQueue *queue;    // Fila de tarefas do módulo
    pthread_t thread;    // Thread do módulo
    _Atomic int running; // Estado de execução
} Module;

// Estrutura da corrotina (para Módulo A)
typedef struct
{
    jmp_buf context;      // Contexto da corrotina
    int state;            // 0: inicial, 1: rodando, 2: pausado, 3: concluído
    void (*func)(void *); // Função da corrotina
    void *args;           // Argumentos
} Coroutine;

// Inicializa a fila de tarefas
void init_queue(TaskQueue *queue)
{
    atomic_store(&queue->head, NULL);
    atomic_store(&queue->tail, NULL);
    atomic_flag_clear(&queue->has_tasks);
}

// Adiciona uma tarefa à fila (lock-free)
void add_task(TaskQueue *queue, Task *task)
{
    task->next = NULL;
    Task *tail = atomic_load(&queue->tail);
    if (!tail)
    {
        atomic_store(&queue->head, task);
        atomic_store(&queue->tail, task);
    }
    else
    {
        tail->next = task;
        atomic_store(&queue->tail, task);
    }
    atomic_flag_test_and_set(&queue->has_tasks);
}

// Remove uma tarefa da fila
Task *get_task(TaskQueue *queue)
{
    Task *head = atomic_load(&queue->head);
    if (!head)
        return NULL;
    atomic_store(&queue->head, head->next);
    if (!head->next)
        atomic_store(&queue->tail, NULL);
    return head;
}

// Função de yield para corrotinas (Módulo A)
void yield(Coroutine *co)
{
    if (co->state == 1)
    {
        co->state = 2;
        if (setjmp(co->context) == 0)
            longjmp(((Coroutine *)co->args)->context, 1);
    }
}

// Executa uma corrotina (Módulo A)
void run_coroutine(Coroutine *co)
{
    if (co->state == 0)
    {
        co->state = 1;
        co->func(co);
        co->state = 3;
    }
    else if (co->state == 2)
    {
        co->state = 1;
        longjmp(co->context, 1);
    }
}

// Tarefa async para Módulo A
void async_task(void *args)
{
    Coroutine *co = (Coroutine *)args;
    printf("Módulo A: Iniciando tarefa async\n");
    yield(co);
    printf("Módulo A: Tarefa async concluída\n");
}

// Tarefa spawn para Módulo B
void spawn_task(void *args)
{
    printf("Módulo B: Executando tarefa spawnada\n");
    sleep(1); // Simula trabalho paralelo
    printf("Módulo B: Tarefa spawnada concluída\n");
}

// Thread do Módulo A (com corrotinas)
void *module_a_thread(void *arg)
{
    Module *mod = (Module *)arg;
    TaskQueue *queue = mod->queue;
    Coroutine coroutines[10];
    int co_count = 0;

    while (atomic_load(&mod->running))
    {
        Task *task = get_task(queue);
        if (task)
        {
            Coroutine *co = &coroutines[co_count++];
            co->func = task->function;
            co->args = co;
            co->state = 0;
            run_coroutine(co);
            free(task);
        }
        int active = 0;
        for (int i = 0; i < co_count; i++)
        {
            if (coroutines[i].state == 2)
            {
                run_coroutine(&coroutines[i]);
                active++;
            }
        }
        if (!active && !atomic_flag_test_and_set(&queue->has_tasks))
        {
            sched_yield();
        }
    }
    return NULL;
}

// Thread do Módulo B (sem corrotinas)
void *module_b_thread(void *arg)
{
    Module *mod = (Module *)arg;
    TaskQueue *queue = mod->queue;

    while (atomic_load(&mod->running))
    {
        Task *task = get_task(queue);
        if (task)
        {
            task->function(task->args);
            free(task);
        }
        else if (!atomic_flag_test_and_set(&queue->has_tasks))
        {
            sched_yield();
        }
    }
    return NULL;
}

// Função para spawnar uma tarefa em outro módulo
void spawn_task_to_module(TaskQueue *target_queue, void (*func)(void *), void *args)
{
    Task *task = malloc(sizeof(Task));
    task->function = func;
    task->args = args;
    add_task(target_queue, task);
}

int main()
{
    // Inicializa os módulos
    TaskQueue queue_a, queue_b;
    init_queue(&queue_a);
    init_queue(&queue_b);
    Module mod_a = {&queue_a, 0, ATOMIC_VAR_INIT(1)};
    Module mod_b = {&queue_b, 0, ATOMIC_VAR_INIT(1)};

    // Cria as threads dos módulos
    pthread_create(&mod_a.thread, NULL, module_a_thread, &mod_a);
    pthread_create(&mod_b.thread, NULL, module_b_thread, &mod_b);

    // Adiciona uma tarefa async ao Módulo A
    Task *task_async = malloc(sizeof(Task));
    task_async->function = async_task;
    task_async->args = NULL;
    add_task(&queue_a, task_async);

    // Spawna uma tarefa do Módulo A para o Módulo B
    spawn_task_to_module(&queue_b, spawn_task, NULL);

    // Aguarda um pouco para visualização (em produção, usar lógica de término)
    sleep(2);

    // Finaliza os módulos
    atomic_store(&mod_a.running, 0);
    atomic_store(&mod_b.running, 0);
    pthread_join(mod_a.thread, NULL);
    pthread_join(mod_b.thread, NULL);

    printf("Execução concluída.\n");
    return 0;
}