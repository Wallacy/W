#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <cpuid.h>
#include <string.h>

#ifdef USE_MIMALLOC
#include <mimalloc.h>
#define MALLOC(size) mi_malloc(size)
#define FREE(ptr) mi_free(ptr)
#else
#define MALLOC(size) malloc(size)
#define FREE(ptr) free(ptr)
#endif

#define true 1
#define false 0

// Union para tagged pointer
typedef union {
    uintptr_t raw;
    struct {
        uintptr_t address : 48;  // Bits 0-47: endereço
        uint8_t flags     : 8;   // Bits 48-55: flags
        uint8_t tags      : 8;   // Bits 56-63: tags
    };
    struct {
        uintptr_t : 48;          // Ignora o endereço
        uint8_t : 8;             // Ignora flags
        union {
            uint8_t : 8;   // Ignora tags
            struct {
                uint8_t : 7;  // Bits 56-62
                bool is_deallocated : 1;  // Bit 63
            };
        };
    };
} TaggedPointer;

// Verifica suporte a LAM/UAI
bool has_lam_uai() {
    unsigned int eax, ebx, ecx, edx;
    char vendor[13];

    // Obtém Vendor ID e máximo EAX suportado
    __cpuid(0, eax, ebx, ecx, edx);
    unsigned int max_eax = eax;
    *((unsigned int*)vendor) = ebx;
    *((unsigned int*)(vendor + 4)) = edx;
    *((unsigned int*)(vendor + 8)) = ecx;
    vendor[12] = '\0';

    // Intel LAM
    if (strncmp(vendor, "GenuineIntel", 12) == 0 && max_eax >= 7) {
        __cpuid(7, eax, ebx, ecx, edx);
        return (ecx & (1 << 26)) != 0;  // Bit 26 de ECX para LAM
    }
    // AMD UAI
    else if (strncmp(vendor, "AuthenticAMD", 12) == 0) {
        __cpuid(0x80000000, eax, ebx, ecx, edx);
        if (eax >= 0x80000008) {
            __cpuid(0x80000008, eax, ebx, ecx, edx);
            return (ecx & (1 << 31)) != 0;  // Verificar o bit correto para UAI
        }
    }
    return false;
}

// Cria um tagged pointer
TaggedPointer create_tagged_pointer(void* ptr, uint8_t flags, uint8_t tags, bool dealloc) {
    TaggedPointer tp;
    tp.raw = (uintptr_t)ptr;
    tp.flags = flags;
    tp.tags = tags;
    tp.is_deallocated = dealloc;
    return tp;
}

// Variável global para suporte a LAM/UAI
bool lam_uai_supported;
// Macro para obter o ponteiro real
#define GET_REAL_POINTER(tp) \
    ((lam_uai_supported) ? (void*)(tp).raw : \
     (void*)(((tp).address & (1ULL << 47)) ? \
             ((tp).address | 0xFFFF000000000000) : \
             ((tp).address & 0x0000FFFFFFFFFFFF)))

int main() {
    lam_uai_supported = has_lam_uai();
    printf("Suporte a LAM/UAI: %s\n", lam_uai_supported ? "Sim" : "Não");

    // Exemplo 1: Objeto simples (int)
    int* int_obj = MALLOC(sizeof(int));
    if (!int_obj) {
        printf("Falha ao alocar int_obj\n");
        return 1;
    }
    *int_obj = 42;
    printf("Ponteiro int sem tags: 0x%016lx\n", (uintptr_t)int_obj);
    TaggedPointer tp_int = create_tagged_pointer(int_obj, 0x01, 0x02, false);
    printf("Ponteiro int com tags: 0x%016lx\n", tp_int.raw);
    int* int_value = (int*)GET_REAL_POINTER(tp_int);
    printf("Valor int: %d\n", *int_value);  // 42
    printf("Flags: 0x%02x, Tags: 0x%02x, Deallocated: %d\n", tp_int.flags, tp_int.tags, tp_int.is_deallocated);

    tp_int.is_deallocated = true;
    FREE(int_value); // Desalocando usando o valor de GET_REAL_POINTER
    if (tp_int.is_deallocated) {
        printf("Int foi desalocada\n");
    }

    // Exemplo 2: Objeto string
    char* str_obj = MALLOC(13);  // "Hello World!" + null terminator
    if (!str_obj) {
        printf("Falha ao alocar str_obj\n");
        return 1;
    }
    strcpy(str_obj, "Hello World!");
    printf("Ponteiro str sem tags: 0x%016lx\n", (uintptr_t)str_obj);
    TaggedPointer tp_str = create_tagged_pointer(str_obj, 0x03, 0x04, false);
    printf("Ponteiro str com tags: 0x%016lx\n", tp_str.raw);
    char* str_value = (char*)GET_REAL_POINTER(tp_str);
    printf("Valor str: %s\n", str_value);  // "Hello World!"
    printf("Flags: 0x%02x, Tags: 0x%02x, Deallocated: %d\n", tp_str.flags, tp_str.tags, tp_str.is_deallocated);

    tp_str.is_deallocated = true;
    FREE(str_value);  // Desalocando usando o valor de GET_REAL_POINTER
    if (tp_str.is_deallocated) {
        printf("String foi desalocada\n");
    }

    return 0;
}