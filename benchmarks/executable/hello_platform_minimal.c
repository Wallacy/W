// Expected exit: 0
// Expected stdout:
// Hello, world!

#if defined(_WIN32)
typedef unsigned long w_dword;
typedef void *w_handle;

__declspec(dllimport) w_handle __stdcall GetStdHandle(w_dword standard_handle);
__declspec(dllimport) int __stdcall WriteFile(
    w_handle file,
    const void *buffer,
    w_dword bytes_to_write,
    w_dword *bytes_written,
    void *overlapped);
__declspec(dllimport) __declspec(noreturn) void __stdcall ExitProcess(w_dword exit_code);

__declspec(noreturn) void w_entry(void) {
    static const char greeting[] = "Hello, world!\n";
    w_dword bytes_written = 0;
    w_handle output = GetStdHandle((w_dword)-11);
    if (WriteFile(output, greeting, (w_dword)(sizeof(greeting) - 1),
                  &bytes_written, 0) && bytes_written == sizeof(greeting) - 1) {
        ExitProcess(0);
    }
    ExitProcess(1);
}
#elif defined(__linux__) && defined(__x86_64__)
__attribute__((noreturn)) static void linux_exit(unsigned long exit_code) {
    __asm__ __volatile__(
        "syscall"
        :
        : "a"(60UL), "D"(exit_code)
        : "rcx", "r11", "memory");
    __builtin_unreachable();
}

__attribute__((noreturn, used)) void _start(void) {
    static const char greeting[] = "Hello, world!\n";
    long bytes_written;
    __asm__ __volatile__(
        "syscall"
        : "=a"(bytes_written)
        : "0"(1L), "D"(1L), "S"(greeting), "d"(sizeof(greeting) - 1)
        : "rcx", "r11", "memory");
    linux_exit(bytes_written == sizeof(greeting) - 1 ? 0UL : 1UL);
}
#else
#error "hello-platform-minimal supports Windows x64 and Linux x64 only"
#endif
