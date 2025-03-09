#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>

typedef union vec {
        struct {
           _Atomic int acnt, cnt;
        };
        struct {
           int _acnt, _cnt;
        };
} vec;

vec res = {};

void *adding_a(void *input)
{
    for(int i=0; i<10000; i++)
    {
        // Posso escolher se vou incrementar atomic ou não!
        if (res.acnt++ == 8754){
            printf("(A) acnt is 8754\n");
        }
        if (res._cnt++ == 8754){
            printf("(A)  _cnt is 8754\n");
        }
        // res._acnt++;
        // res.cnt++;
        
    }
    pthread_exit(NULL);
}
void *adding_b(void *input)
{
    for(int i=0; i<10000; i++)
    {
        if (res.acnt++ == 8754){
            printf("(B) acnt is 8754\n");
        }
        if (res._cnt++ == 8754){
            printf("(B)  _cnt is 8754\n");
        }
    }
    pthread_exit(NULL);
}
int main()
{
    pthread_t tid[200];
    for(int i=0; i<200; i=i+2){
        pthread_create(&tid[i],NULL,adding_a,NULL);
        pthread_create(&tid[i+1],NULL,adding_b,NULL);
    }
    for(int i=0; i<200; i++)
        pthread_join(tid[i],NULL);

    printf("sizeof rs is %lu\n", sizeof(res));
    printf("the value of acnt is %d\n", res.acnt);
    printf("the value of cnt is %d\n", res.cnt);
    // printf("the value of res._acnt is %d\n", res._acnt);
    // printf("the value of res._cnt is %d\n", res._cnt);
    return 0;
}

//
// O importante aqui é saber que posso usar a mesma estrutura e decidir dependendo do contexto quando chamar as variaveis atomicas ou não.