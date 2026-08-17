#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum {
  REGION_BYTES = 4096,
  STATE_READY = 0,
  STATE_COMMITTED = 1,
  STATE_ACKNOWLEDGED = 2,
};

struct region {
  char magic[8];
  uint32_t version;
  uint32_t schema_id;
  uint64_t layout_digest_tag;
  uint64_t generation;
  _Atomic uint32_t state;
  uint32_t header_validated;
  uint32_t payload;
  uint64_t child_address;
};

_Static_assert(sizeof(struct region) <= REGION_BYTES, "probe region must fit its extent");

static int timed_state(_Atomic uint32_t *state, uint32_t expected, long timeout_ms) {
  struct timespec start;
  if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) return -1;
  for (;;) {
    uint32_t observed = atomic_load_explicit(state, memory_order_acquire);
    if (observed != expected) return (int)observed;
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1;
    long elapsed = (long)((now.tv_sec - start.tv_sec) * 1000L);
    elapsed += (long)((now.tv_nsec - start.tv_nsec) / 1000000L);
    if (elapsed >= timeout_ms) return -2;
    struct timespec pause_for = { .tv_sec = 0, .tv_nsec = 1000000L };
    nanosleep(&pause_for, NULL);
  }
}

static int header_ok(const struct region *mapped, uint64_t expected_generation) {
  return memcmp(mapped->magic, "WIPC1", 6) == 0
      && mapped->version == 1
      && mapped->schema_id == 1
      && mapped->layout_digest_tag == UINT64_C(0x1355)
      && mapped->generation == expected_generation;
}

static int child_main(const char *name) {
  int fd = shm_open(name, O_RDWR, 0600);
  if (fd < 0) return 21;
  struct region *mapped = mmap(NULL, REGION_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped == MAP_FAILED) {
    close(fd);
    return 22;
  }
  if (!header_ok(mapped, 1)) {
    munmap(mapped, REGION_BYTES);
    close(fd);
    return 23;
  }
  mapped->header_validated = 1;
  mapped->child_address = (uint64_t)(uintptr_t)mapped;
  mapped->payload = 42;
  atomic_store_explicit(&mapped->state, STATE_COMMITTED, memory_order_release);
  int acknowledged = timed_state(&mapped->state, STATE_COMMITTED, 5000);
  int result = acknowledged == STATE_ACKNOWLEDGED ? 0 : 24;
  if (munmap(mapped, REGION_BYTES) != 0) result = 25;
  if (close(fd) != 0) result = 26;
  return result;
}

static int make_region(const char *name, uint64_t generation) {
  int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd < 0) return -1;
  if (ftruncate(fd, REGION_BYTES) != 0) {
    close(fd);
    shm_unlink(name);
    return -1;
  }
  struct region *mapped = mmap(NULL, REGION_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped == MAP_FAILED) {
    close(fd);
    shm_unlink(name);
    return -1;
  }
  memset(mapped, 0, REGION_BYTES);
  memcpy(mapped->magic, "WIPC1", 6);
  mapped->version = 1;
  mapped->schema_id = 1;
  mapped->layout_digest_tag = UINT64_C(0x1355);
  mapped->generation = generation;
  atomic_init(&mapped->state, STATE_READY);
  if (munmap(mapped, REGION_BYTES) != 0) {
    close(fd);
    shm_unlink(name);
    return -1;
  }
  if (close(fd) != 0) {
    shm_unlink(name);
    return -1;
  }
  return 0;
}

static int parent_main(const char *self) {
  char name[96];
  char remap_name[96];
  if (snprintf(name, sizeof(name), "/w-ipc1-%ld", (long)getpid()) >= (int)sizeof(name)) return 30;
  if (snprintf(remap_name, sizeof(remap_name), "/w-ipc1-%ld-g2", (long)getpid()) >= (int)sizeof(remap_name)) return 31;
  if (make_region(name, 1) != 0) return 32;

  int fd = shm_open(name, O_RDWR, 0600);
  if (fd < 0) {
    shm_unlink(name);
    return 33;
  }
  struct region *mapped = mmap(NULL, REGION_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped == MAP_FAILED) {
    close(fd);
    shm_unlink(name);
    return 34;
  }
  uintptr_t parent_address = (uintptr_t)mapped;
  if (!header_ok(mapped, 1)) {
    munmap(mapped, REGION_BYTES);
    close(fd);
    shm_unlink(name);
    return 35;
  }

  pid_t child = fork();
  if (child < 0) {
    munmap(mapped, REGION_BYTES);
    close(fd);
    shm_unlink(name);
    return 36;
  }
  if (child == 0) {
    execl(self, self, "child", name, (char *)NULL);
    _exit(37);
  }

  int committed = timed_state(&mapped->state, STATE_READY, 5000);
  int child_exit = 0;
  if (committed != STATE_COMMITTED || mapped->header_validated != 1 || mapped->payload != 42) {
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    munmap(mapped, REGION_BYTES);
    close(fd);
    shm_unlink(name);
    return 38;
  }
  uint64_t child_address = mapped->child_address;
  atomic_store_explicit(&mapped->state, STATE_ACKNOWLEDGED, memory_order_release);
  if (waitpid(child, &child_exit, 0) < 0 || !WIFEXITED(child_exit) || WEXITSTATUS(child_exit) != 0) {
    munmap(mapped, REGION_BYTES);
    close(fd);
    shm_unlink(name);
    return 39;
  }
  int addresses_distinct = child_address != (uint64_t)parent_address;
  if (munmap(mapped, REGION_BYTES) != 0 || close(fd) != 0 || shm_unlink(name) != 0) return 40;
  errno = 0;
  int stale_fd = shm_open(name, O_RDWR, 0600);
  int stale_name_rejected = stale_fd < 0 && errno == ENOENT;
  if (stale_fd >= 0) close(stale_fd);
  if (!stale_name_rejected || !addresses_distinct) return 41;

  if (make_region(remap_name, 2) != 0) return 42;
  int remap_fd = shm_open(remap_name, O_RDWR, 0600);
  if (remap_fd < 0) {
    shm_unlink(remap_name);
    return 43;
  }
  struct region *remapped = mmap(NULL, REGION_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, remap_fd, 0);
  if (remapped == MAP_FAILED || !header_ok(remapped, 2)) {
    if (remapped != MAP_FAILED) munmap(remapped, REGION_BYTES);
    close(remap_fd);
    shm_unlink(remap_name);
    return 44;
  }
  int remap_ok = remapped->generation == 2;
  int cleanup_ok = munmap(remapped, REGION_BYTES) == 0 && close(remap_fd) == 0 && shm_unlink(remap_name) == 0;
  if (!remap_ok || !cleanup_ok) return 45;

  printf("{\"schema\":\"w-ipc1-probe-output-1\",\"id\":\"IPC2-POSIX-two-process-map\",\"status\":\"observed-design-evidence\",\"target\":\"posix\",\"observed\":{\"twoProcess\":true,\"addressesDistinct\":true,\"headerValidated\":true,\"committedRead\":true,\"wake\":\"bounded-polling\",\"staleNameRejected\":true,\"remapGeneration\":2,\"cleanup\":\"unmap-close-unlink\"},\"providerReceipt\":false}\n");
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 3 && strcmp(argv[1], "child") == 0) return child_main(argv[2]);
  char self[PATH_MAX];
  if (realpath(argv[0], self) == NULL) return 46;
  return parent_main(self);
}
