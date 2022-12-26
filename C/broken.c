#include <stdint.h>
#include <stdio.h>

static void encrypt(void) {
  uint8_t key[] = "hunter2";
  printf("encrypting with super secret key: %s\n", key);
}

static void log_completion(void) {
  /* oh no, we forgot to init the msg */
  char msg[8];
  printf("not important, just fyi: %s\n", msg);
}

int main(void) {
  encrypt();
  /* notify that we're done */
  log_completion();
  return 0;
}
