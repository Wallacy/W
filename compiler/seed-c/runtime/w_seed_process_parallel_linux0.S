/* w-seed-process-parallel-link0-linux-1
 * Bounded CRT-free Linux x86-64 adapter for the W-1598 process/parallel
 * composition. The fixed child stack and four-slot frame are private provider
 * evidence, not W Task storage, a scheduler, or a public runtime ABI. */

  .text
  .globl w_seed_process_arguments_is_empty
  .type w_seed_process_arguments_is_empty,@function
w_seed_process_arguments_is_empty:
  movzbl (%rdi), %eax
  testl %eax, %eax
  setne %al
  ret
  .size w_seed_process_arguments_is_empty,.-w_seed_process_arguments_is_empty

  .globl w_seed_parallel_launch_task_0
  .type w_seed_parallel_launch_task_0,@function
w_seed_parallel_launch_task_0:
  cmpb $0, w_seed_parallel_force_failure(%rip)
  jne .Llaunch_failure
  movq %rdi, w_seed_parallel_frame_pointer(%rip)
  movq $0, 0(%rdi)
  movq %rsi, 8(%rdi)
  movq $0, 16(%rdi)
  movl $1, 24(%rdi)

  /* clone(CLONE_VM | SIGCHLD, child_stack, 0, 0, 0). The child is a
   * separately waitable process sharing only this bounded provider memory. */
  movq $56, %rax
  movq $273, %rdi
  leaq w_seed_parallel_child_stack_end(%rip), %rsi
  xorq %rdx, %rdx
  xorq %r10, %r10
  xorq %r8, %r8
  syscall
  testq %rax, %rax
  js .Llaunch_failure
  jz .Llaunch_child
  movq w_seed_parallel_frame_pointer(%rip), %rcx
  movq %rax, 0(%rcx)
  movl $1, %eax
  ret

.Llaunch_child:
  movq w_seed_parallel_frame_pointer(%rip), %rcx
  xorq %rdi, %rdi
  xorq %rsi, %rsi
  movq 8(%rcx), %rdx
  call w_seed_parallel_task_0
  movq w_seed_parallel_frame_pointer(%rip), %rcx
  movq %rax, 16(%rcx)
  movl $0, 24(%rcx)
  movq $60, %rax
  xorq %rdi, %rdi
  syscall
  ud2

.Llaunch_failure:
  xorl %eax, %eax
  ret
  .size w_seed_parallel_launch_task_0,.-w_seed_parallel_launch_task_0

  .globl w_seed_parallel_join_task_0
  .type w_seed_parallel_join_task_0,@function
w_seed_parallel_join_task_0:
  pushq %r12
  pushq %r13
  subq $16, %rsp
  movq %rdi, %r12
  movq %rsi, %r13
  movl $0, (%rsp)
  movq 0(%r12), %rdi
  testq %rdi, %rdi
  jz .Ljoin_failure
.Ljoin_wait:
  movq $61, %rax
  movq %rsp, %rsi
  xorq %rdx, %rdx
  xorq %r10, %r10
  syscall
  cmpq $-4, %rax
  je .Ljoin_wait
  cmpq 0(%r12), %rax
  jne .Ljoin_failure
  cmpl $0, (%rsp)
  jne .Ljoin_failure
  cmpl $0, 24(%r12)
  jne .Ljoin_failure
  movq 8(%r12), %rcx
  addq $1, %rcx
  movq 16(%r12), %rdx
  cmpq %rcx, %rdx
  jne .Ljoin_failure
  movq %rdx, 0(%r13)
  movl $1, %eax
  jmp .Ljoin_done

.Ljoin_failure:
  xorl %eax, %eax
.Ljoin_done:
  addq $16, %rsp
  popq %r13
  popq %r12
  ret
  .size w_seed_parallel_join_task_0,.-w_seed_parallel_join_task_0

  .globl _start
  .type _start,@function
_start:
  movq (%rsp), %rax
  subq $16, %rsp
  cmpq $1, %rax
  jg .Lstart_payload
  movb $1, (%rsp)
  movb $0, w_seed_parallel_force_failure(%rip)
  jmp .Lstart_run

.Lstart_payload:
  movb $0, (%rsp)
  movq 32(%rsp), %rcx
  cmpb $33, (%rcx)
  sete w_seed_parallel_force_failure(%rip)

.Lstart_run:
  movq %rsp, %rdi
  xorq %rsi, %rsi
  call w_seed_process_parallel_entry
  movl %eax, %edi
  movq $60, %rax
  syscall
  ud2
  .size _start,.-_start

  .section .bss
  .p2align 4
w_seed_parallel_frame_pointer:
  .zero 8
w_seed_parallel_force_failure:
  .zero 1
  .p2align 4
w_seed_parallel_child_stack:
  .zero 65536
w_seed_parallel_child_stack_end:

  .section .note.GNU-stack,"",@progbits
