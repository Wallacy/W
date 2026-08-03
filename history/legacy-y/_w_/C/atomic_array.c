#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>

#define sleep useconds_t sleepTime = (useconds_t)(((rand() % 7) + 1) * 11);usleep(sleepTime);
#define threads 0

_Atomic int acnt[100];
int cnt;
void *adding(void *input)
{
    for(int i=0; i<10000; i++)
    {
    //   sleep
      for(int z=0; z<96; z++)
      {
            acnt[z+0]++;
            cnt++;
            acnt[z+1]++;
            cnt++;
            acnt[z+2]++;
            cnt++;
            acnt[z+3]++;
            cnt++;
            acnt[z+4]++;
       } 
    }
    
    (threads != 0) ? ({pthread_exit(NULL);}) : ({});
    return NULL;
}
int main()
{
    pthread_t tid[10];
    switch (threads)
    {
    case 0:
        for(int i=0; i<10; i++)
        {
            (*adding)(NULL);
        }
        break;
    default:
        for(int i=0; i<10; i++)
        {
            pthread_create(&tid[i],NULL,*adding,NULL);
        }
        for(int i=0; i<10; i++)
        {
            pthread_join(tid[i],NULL);
        }
    }

    printf("the value of acnt[1-5] is [%d,%d,%d,%d,%d]\n", acnt[0],acnt[1],acnt[2],acnt[3],acnt[4]);
    printf("the value of acnt[95-100] is [%d,%d,%d,%d,%d]\n", acnt[95],acnt[96],acnt[97],acnt[98],acnt[99]);
    printf("the value of cnt is %d\n", cnt);
    return 0;
}