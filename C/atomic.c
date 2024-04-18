#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>
// #include <execinfo.h>
// #include <stdlib.h>

_Atomic int acnt;
int cnt;
void *adding(void *input)
{
    for(int i=0; i<10000; i++)
    {
        acnt++;
        cnt++;
    }
    pthread_exit(NULL);
}
int main()
{
    pthread_t tid[10];
    for(int i=0; i<10; i++)
        pthread_create(&tid[i],NULL,adding,NULL);
    for(int i=0; i<10; i++)
        pthread_join(tid[i],NULL);
        
    // void* callstack[128];
    // int i, frames = backtrace(callstack, 128);
    // char** strs = backtrace_symbols(callstack, frames);
    // for (i = 0; i < frames; ++i) {
    //     printf("%s\n", strs[i]);
    // }
    // free(strs);

    printf("the value of acnt is %d\n", acnt);
    printf("the value of cnt is %d\n", cnt);
    return 0;
}