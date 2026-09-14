#include "w_seed_wrt0.h"

#include <stdio.h>
#include <string.h>

static bool contains(const w_seed_wrt0_artifact *artifact,
                     const char *needle) {
  if (artifact == NULL || needle == NULL) return false;
  const size_t length = strlen(needle);
  if (length == 0u || length > artifact->llvm_ir_length) return false;
  for (size_t offset = 0u; offset <= artifact->llvm_ir_length - length;
       offset += 1u)
    if (memcmp(artifact->llvm_ir + offset, needle, length) == 0) return true;
  return false;
}

int main(int argc, char **argv) {
  w_seed_wrt0_artifact artifact = {NULL, 0u};
  if (!w_seed_wrt0_get(W_SEED_WRT0_TARGET_LINUX_X86_64, &artifact) ||
      artifact.llvm_ir == NULL || artifact.llvm_ir_length == 0u)
    return 1;

  if (argc == 2 && strcmp(argv[1], "--emit-linux-x86-64-llvm") == 0)
    return fwrite(artifact.llvm_ir, 1u, artifact.llvm_ir_length, stdout) ==
                   artifact.llvm_ir_length
               ? 0
               : 1;
  if (argc != 1) return 2;

  if (!contains(&artifact, "target triple = \"x86_64-unknown-linux-gnu\"") ||
      !contains(&artifact, "declare i32 @main()") ||
      !contains(&artifact, ".globl _start") ||
      !contains(&artifact, "syscall") || contains(&artifact, "@printf") ||
      contains(&artifact, "@malloc"))
    return 1;

  const w_seed_wrt0_artifact sentinel = {
      (const uint8_t *)(uintptr_t)1u, 17u};
  w_seed_wrt0_artifact rejected = sentinel;
  if (w_seed_wrt0_get((w_seed_wrt0_target)99, &rejected) ||
      rejected.llvm_ir != sentinel.llvm_ir ||
      rejected.llvm_ir_length != sentinel.llvm_ir_length ||
      w_seed_wrt0_get(W_SEED_WRT0_TARGET_LINUX_X86_64, NULL))
    return 1;

  puts("wrt0 tests: ok");
  return 0;
}
