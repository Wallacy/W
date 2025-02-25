#include <unistd.h>
#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdlib.h>

__thread const char* thread_name;
void *hello() {
    thread_name = __FUNCTION__;
    printf("thread name = %s\n", thread_name); 
    void* p = malloc(10);
    write(1, "hello world\n", 12);
    printf(">> %p\n", p);
    free(p);
    pthread_exit(NULL);
}

int main(){
  thread_name = __FUNCTION__;
  pthread_t tid;
  pthread_create(&tid,NULL,hello,NULL);
  pthread_join(tid,NULL);
  printf("Original thread name: %s\n", thread_name);
  return 0;
}