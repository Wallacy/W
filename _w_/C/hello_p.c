#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    void* p = malloc(10);
    write(1, "hello world\n", 12);
    printf(">> %p", p);
    free(p);
  return 0;
}