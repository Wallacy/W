#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_MIMALLOC
#include <mimalloc.h>
#define MALLOC(size) mi_malloc(size)
#define FREE(ptr) mi_free(ptr)
#else
#define MALLOC(size) malloc(size)
#define FREE(ptr) free(ptr)
#endif

#define CHECK_CPU 0 // Define se deve verificar suporte a LAM/UAI ou Page-5
#define ADDRESS_BITS_48 1
#define true 1
#define false 0

// ***** Definições de Tagged Pointers para 64 bits ***** //
#if defined(__x86_64__) || defined(__aarch64__)
typedef union {
    uintptr_t raw;
    struct {
        uintptr_t address       : 48;  // Bits 0-47: endereço
        uint16_t sdata          : 13;  // Bits 48-60: small data
        uint8_t is_shared       : 1;   // Bit 61
        uint8_t is_deallocated  : 1;   // Bit 62
        uint8_t reserved        : 1;   // Bit 63: reservado
    };
} TaggedPointerX86_64_48;

typedef union {
    uintptr_t raw;
    struct {
        uintptr_t address       : 57;  // Bits 0-56: endereço
        uint8_t sdata           : 4;   // Bits 57-60: small data
        uint8_t is_shared       : 1;   // Bit 61
        uint8_t is_deallocated  : 1;   // Bit 62
        uint8_t reserved        : 1;   // Bit 63: reservado
    };
} TaggedPointerX86_64_57;

typedef TaggedPointerX86_64_48 TaggedPointerARM64_48;

typedef union {
    uintptr_t raw;
    struct {
        uintptr_t address       : 52;  // Bits 0-51: endereço
        uint16_t sdata          : 9;   // Bits 52-60: small data
        uint8_t is_shared       : 1;   // Bit 61
        uint8_t is_deallocated  : 1;   // Bit 62
        uint8_t reserved        : 1;   // Bit 63: reservado
    };
} TaggedPointerARM64_52;

typedef union {
    uintptr_t raw;
    struct {
        uintptr_t address       : 56;  // Bits 0-55: endereço
        uint8_t sdata           : 5;   // Bits 56-60: small data
        uint8_t is_shared       : 1;   // Bit 61
        uint8_t is_deallocated  : 1;   // Bit 62
        uint8_t reserved        : 1;   // Bit 63: reservado
    };
} TaggedPointerARM64_56;

// ***** Definições de Tagged Pointers para 32 bits ***** //
#elif defined(__arm__) || defined(__i386__)
typedef union {
    uint32_t raw;
    struct {
        uint32_t is_deallocated : 1;   // Bit 0
        uint32_t is_shared      : 1;   // Bit 1
        uint32_t address        : 30;  // Bits 2-31: endereço
    };
} TaggedPointer32;

#endif

// Definindo TaggedPointer com base na arquitetura
#if defined(__x86_64__) && defined(ADDRESS_BITS_48)
typedef TaggedPointerX86_64_48 TaggedPointer;
#elif defined(__x86_64__) && defined(ADDRESS_BITS_57)
typedef TaggedPointerX86_64_57 TaggedPointer;
#elif defined(__aarch64__) && defined(ADDRESS_BITS_48)
typedef TaggedPointerARM64_48 TaggedPointer;
#elif defined(__aarch64__) && defined(ADDRESS_BITS_52)
typedef TaggedPointerARM64_52 TaggedPointer;
#elif defined(__aarch64__) && defined(ADDRESS_BITS_56)
typedef TaggedPointerARM64_56 TaggedPointer;
#elif defined(__arm__) || defined(__i386__)
typedef TaggedPointer32 TaggedPointer;
#else
#error "Plataforma ou tamanho de endereço não suportado"
#endif

#ifdef W_KERNEL_MODE
#define RESERVED_BIT 0
#else
#define RESERVED_BIT 1
#endif

// ***** Funções de verificação de CPU (apenas para 64 bits) ***** //
#if defined(__x86_64__) || defined(__aarch64__)
    #if CHECK_CPU
        #include <cpuid.h>

        void has_page5() {
            unsigned int eax, ebx, ecx, edx;
            __cpuid(7, eax, ebx, ecx, edx);  // CPUID com EAX=7 e ECX=0
            if (ecx & (1 << 16)) {
                printf("Suporta paginação de 5 níveis (57 bits)\n");
            } else {
                printf("Suporta apenas paginação de 4 níveis (48 bits)\n");
            }
        }

        bool has_lam_uai() {
            unsigned int eax, ebx, ecx, edx;
            char vendor[13];
            __cpuid(0, eax, ebx, ecx, edx);
            unsigned int max_eax = eax;
            memcpy(vendor, &ebx, 4);
            memcpy(vendor + 4, &edx, 4);
            memcpy(vendor + 8, &ecx, 4);
            vendor[12] = '\0';

            if (strncmp(vendor, "GenuineIntel", 12) == 0 && max_eax >= 7) {
                __cpuid(7, eax, ebx, ecx, edx);
                return (ecx & (1 << 26)) != 0;  // LAM
            } else if (strncmp(vendor, "AuthenticAMD", 12) == 0) {
                __cpuid(0x80000000, eax, ebx, ecx, edx);
                if (eax >= 0x80000008) {
                    __cpuid(0x80000008, eax, ebx, ecx, edx);
                    return (ecx & (1 << 31)) != 0;  // UAI
                }
            }
            return false;
        }
    #endif
#endif

// ***** Criação de Tagged Pointer ***** //
#if defined(__x86_64__) || defined(__aarch64__)
TaggedPointer create_tagged_pointer(void* ptr, uint16_t sdata, bool dealloc) {
    TaggedPointer tp;
    tp.raw = (uintptr_t)ptr;
    tp.sdata = sdata;
    tp.is_deallocated = dealloc;
    tp.is_shared = 0;  // Inicializa como 0 por padrão
    tp.reserved = RESERVED_BIT;
    return tp;
}
#elif defined(__arm__) || defined(__i386__)
TaggedPointer create_tagged_pointer(void* ptr, uint8_t flags, bool dealloc) {
    TaggedPointer tp;
    tp.raw = (uint32_t)((uintptr_t)ptr >> 2);  // Remove 2 bits LSB (alinhamento)
    tp.is_deallocated = dealloc;
    tp.is_shared = flags & 0x1;  // Usa o bit 0 de flags
    return tp;
}
#endif

// ***** Variável global e macro para obter o ponteiro real ***** //
#if defined(__x86_64__) || defined(__aarch64__)
bool lam_uai_supported = false;
#define GET_REAL_POINTER(tp) \
    ((lam_uai_supported) ? (void*)(tp).raw : \
     (void*)(((tp).address & (1ULL << 47)) ? \
             ((tp).address | 0xFFFF000000000000) : \
             ((tp).address & 0x0000FFFFFFFFFFFF)))
#elif defined(__arm__) || defined(__i386__)
#define GET_REAL_POINTER(tp) \
    ((void*)((tp).address << 2))  // Restaura o endereço shiftando de volta
#endif

// ***** Função Principal ***** //
int main() {
    #if defined(__x86_64__) || defined(__aarch64__)
        #if CHECK_CPU
            has_page5();
            lam_uai_supported = has_lam_uai();
            printf("Suporte a LAM/UAI: %s\n", lam_uai_supported ? "Sim" : "Não");
        #endif
    #endif

    // Exemplo 1: Objeto simples (int)
    int* int_obj = MALLOC(sizeof(int));
    if (!int_obj) {
        printf("Falha ao alocar int_obj\n");
        return 1;
    }
    *int_obj = 42;
    printf("Ponteiro int sem tags: 0x%016lx\n", (uintptr_t)int_obj);

    #if defined(__x86_64__) || defined(__aarch64__)
    TaggedPointer tp_int = create_tagged_pointer(int_obj, 132, false);
    #elif defined(__arm__) || defined(__i386__)
    TaggedPointer tp_int = create_tagged_pointer(int_obj, 0, false);
    #endif

    printf("Ponteiro int com tags: 0x%016lx\n", (uintptr_t)tp_int.raw);
    int* int_value = (int*)GET_REAL_POINTER(tp_int);
    printf("Valor int: %d\n", *int_value);  // 42

    #if defined(__x86_64__) || defined(__aarch64__)
    printf("SMI: %d, Deallocated: %d\n", tp_int.sdata, tp_int.is_deallocated);
    #elif defined(__arm__) || defined(__i386__)
    printf("Flags: %d, Deallocated: %d\n", tp_int.is_shared, tp_int.is_deallocated);
    #endif

    tp_int.is_deallocated = true;
    FREE(int_value);
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

    #if defined(__x86_64__) || defined(__aarch64__)
    TaggedPointer tp_str = create_tagged_pointer(str_obj, 114, false);
    #elif defined(__arm__) || defined(__i386__)
    TaggedPointer tp_str = create_tagged_pointer(str_obj, 0, false);
    #endif

    printf("Ponteiro str com tags: 0x%016lx\n", (uintptr_t)tp_str.raw);
    char* str_value = (char*)GET_REAL_POINTER(tp_str);
    printf("Valor str: %s\n", str_value);  // "Hello World!"

    #if defined(__x86_64__) || defined(__aarch64__)
    printf("SMI: %d, Deallocated: %d\n", tp_str.sdata, tp_str.is_deallocated);
    #elif defined(__arm__) || defined(__i386__)
    printf("Flags: %d, Deallocated: %d\n", tp_str.is_shared, tp_str.is_deallocated);
    #endif

    tp_str.is_deallocated = true;
    FREE(str_value);
    if (tp_str.is_deallocated) {
        printf("String foi desalocada\n");
    }

    return 0;
}