__attribute__((naked)) void _start() {
    __asm__(
        "mov $1, %rax;"        // syscall número 1, sys_write
        "mov $1, %rdi;"        // arquivo 1, stdout
        "lea msg(%rip), %rsi;" // carregar endereço da mensagem relativo ao ponteiro de instrução (RIP)
        "mov $13, %rdx;"       // tamanho da mensagem
        "syscall;"             // chamada do sistema

        "mov $60, %rax;"       // syscall número 60, sys_exit
        "xor %rdi, %rdi;"      // código de saída 0
        "syscall;"             // chamada do sistema
    );
}

const char msg[] = "Hello, World!\n";
