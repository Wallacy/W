#include "w_seed_wrt0.h"

/* WRT0 is the complete runtime closure of the bounded Linux seed executable.
 * It owns process startup, stdout and exit through the x86_64 Linux syscall
 * ABI. The final artifact therefore needs neither a C entry point nor libc,
 * CRT objects, a dynamic loader, or generated C source. */
static const uint8_t LINUX_X86_64_LLVM_IR[] =
    "target triple = \"x86_64-unknown-linux-gnu\"\n"
    "\n"
    "@w_seed_linux_initial_argc = global i64 0, align 8\n"
    "@w_seed_linux_initial_argv = global ptr null, align 8\n"
    "\n"
    "define i64 @w_seed_process_argc() nounwind {\n"
    "entry:\n"
    "  %value = load i64, ptr @w_seed_linux_initial_argc, align 8\n"
    "  ret i64 %value\n"
    "}\n"
    "\n"
    "define ptr @w_seed_process_argv() nounwind {\n"
    "entry:\n"
    "  %value = load ptr, ptr @w_seed_linux_initial_argv, align 8\n"
    "  ret ptr %value\n"
    "}\n"
    "\n"
    "declare i32 @main()\n"
    "\n"
    "define i32 @w_seed_linux_start(ptr %stack) nounwind {\n"
    "entry:\n"
    "  %argc = load i64, ptr %stack, align 8\n"
    "  %argv = getelementptr i8, ptr %stack, i64 8\n"
    "  store i64 %argc, ptr @w_seed_linux_initial_argc, align 8\n"
    "  store ptr %argv, ptr @w_seed_linux_initial_argv, align 8\n"
    "  %status = call i32 @main()\n"
    "  ret i32 %status\n"
    "}\n"
    "\n"
    "define i64 @write(i32 %fd, ptr %buffer, i64 %count) nounwind {\n"
    "entry:\n"
    "  %result = call i64 asm sideeffect \"syscall\", \"={rax},{rax},{rdi},{rsi},{rdx},~{rcx},~{r11},~{memory}\"(i64 1, i32 %fd, ptr %buffer, i64 %count)\n"
    "  ret i64 %result\n"
    "}\n"
    "\n"
    "module asm \".text\\0A.globl _start\\0A.type _start,@function\\0A_start:\\0A  movq %rsp, %rdi\\0A  callq w_seed_linux_start\\0A  movl %eax, %edi\\0A  movl $60, %eax\\0A  syscall\\0A  ud2\\0A.size _start, .-_start\\0A\"\n";

/* Count-only process products receive argc directly from the kernel-created
 * initial stack. They need neither an argv accessor nor process-global startup
 * storage. */
static const uint8_t LINUX_X86_64_ARGUMENT_COUNT_LLVM_IR[] =
    "target triple = \"x86_64-unknown-linux-gnu\"\n"
    "\n"
    "declare i32 @main(i64)\n"
    "\n"
    "define i32 @w_seed_linux_count_only_start(ptr %stack) nounwind {\n"
    "entry:\n"
    "  %argc = load i64, ptr %stack, align 8\n"
    "  %status = call i32 @main(i64 %argc)\n"
    "  ret i32 %status\n"
    "}\n"
    "\n"
    "define i64 @write(i32 %fd, ptr %buffer, i64 %count) nounwind {\n"
    "entry:\n"
    "  %result = call i64 asm sideeffect \"syscall\", \"={rax},{rax},{rdi},{rsi},{rdx},~{rcx},~{r11},~{memory}\"(i64 1, i32 %fd, ptr %buffer, i64 %count)\n"
    "  ret i64 %result\n"
    "}\n"
    "\n"
    "module asm \".text\\0A.globl _start\\0A.type _start,@function\\0A_start:\\0A  movq %rsp, %rdi\\0A  callq w_seed_linux_count_only_start\\0A  movl %eax, %edi\\0A  movl $60, %eax\\0A  syscall\\0A  ud2\\0A.size _start, .-_start\\0A\"\n";

/* Programs proven not to require process arguments still use the same
 * syscall-based entry/write/exit contract, but do not read or publish the
 * kernel argc/argv vector. */
static const uint8_t LINUX_X86_64_NO_ARGUMENTS_LLVM_IR[] =
    "target triple = \"x86_64-unknown-linux-gnu\"\n"
    "\n"
    "declare i32 @main()\n"
    "\n"
    "define i64 @write(i32 %fd, ptr %buffer, i64 %count) nounwind {\n"
    "entry:\n"
    "  %result = call i64 asm sideeffect \"syscall\", \"={rax},{rax},{rdi},{rsi},{rdx},~{rcx},~{r11},~{memory}\"(i64 1, i32 %fd, ptr %buffer, i64 %count)\n"
    "  ret i64 %result\n"
    "}\n"
    "\n"
    "module asm \".text\\0A.globl _start\\0A.type _start,@function\\0A_start:\\0A  callq main\\0A  movl %eax, %edi\\0A  movl $60, %eax\\0A  syscall\\0A  ud2\\0A.size _start, .-_start\\0A\"\n";

bool w_seed_wrt0_get(w_seed_wrt0_target target,
                     w_seed_runtime_requirements requirements,
                     w_seed_wrt0_artifact *artifact) {
  if (artifact == NULL || target != W_SEED_WRT0_TARGET_LINUX_X86_64)
    return false;
  const uint8_t *llvm_ir = NULL;
  size_t llvm_ir_length = 0u;
  switch (requirements) {
    case W_SEED_RUNTIME_REQUIREMENTS_NONE:
      llvm_ir = LINUX_X86_64_NO_ARGUMENTS_LLVM_IR;
      llvm_ir_length = sizeof(LINUX_X86_64_NO_ARGUMENTS_LLVM_IR) - 1u;
      break;
    case W_SEED_RUNTIME_REQUIREMENTS_PROCESS_ARGUMENTS:
      llvm_ir = LINUX_X86_64_LLVM_IR;
      llvm_ir_length = sizeof(LINUX_X86_64_LLVM_IR) - 1u;
      break;
    case W_SEED_RUNTIME_REQUIREMENTS_PROCESS_ARGUMENT_COUNT:
      llvm_ir = LINUX_X86_64_ARGUMENT_COUNT_LLVM_IR;
      llvm_ir_length = sizeof(LINUX_X86_64_ARGUMENT_COUNT_LLVM_IR) - 1u;
      break;
    case W_SEED_RUNTIME_REQUIREMENTS_UNKNOWN:
    default:
      return false;
  }
  const w_seed_wrt0_artifact candidate = {llvm_ir, llvm_ir_length};
  *artifact = candidate;
  return true;
}
