#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_MIMALLOC
#include <mimalloc.h>
#define MALLOC(size) mi_malloc(size)
#define CALLOC(size) mi_calloc(1,size)
#define REALLOC(obj, size) mi_realloc(obj, size)
#define FREE(ptr) mi_free((void *)ptr)
#else
#define MALLOC(size) malloc(size)
#define CALLOC(size) calloc(1,size)
#define REALLOC(obj, size) realloc(obj, size)
#define FREE(ptr) free((void *)ptr)
#endif

#ifndef ADDRESS_BITS
#define ADDRESS_BITS 48
#endif

#define CHECK_CPU 1
#define true 1
#define false 0
typedef struct {
    _Atomic uint32_t ref_count;
    _Atomic uint32_t type;
    _Atomic uintptr_t address;
} SharedPointer;

typedef struct {
    uint32_t error_code;
    char* message;
    uintptr_t prev_address;
} Error;

#define LAM_UAI 0

#if defined(__x86_64__) || defined(__aarch64__)
#if CHECK_CPU
#include <cpuid.h>

#undef LAM_UAI
bool lam_uai_supported = 0;
#define LAM_UAI lam_uai_supported

static inline bool has_page5() {
    unsigned int eax, ebx, ecx, edx;
    __cpuid(7, eax, ebx, ecx, edx);
    return !!(ecx & (1 << 16));
}

static inline bool has_lam_uai() {
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
        return (ecx & (1 << 26)) != 0;
    } else if (strncmp(vendor, "AuthenticAMD", 12) == 0) {
        __cpuid(0x80000000, eax, ebx, ecx, edx);
        if (eax >= 0x80000008) {
            __cpuid(0x80000008, eax, ebx, ecx, edx);
            return (ecx & (1 << 31)) != 0;
        }
    }
    return 0;
}

static inline int get_virtual_address_bits() {
    unsigned int eax, ebx, ecx, edx;
    __cpuid(0x80000008, eax, ebx, ecx, edx);
    return (eax >> 8) & 0xFF;
}
#endif
#elif defined(__arm__) || defined(__i386__)
static inline int get_virtual_address_bits() {
    return 32;
}
#endif

// ***** Definições de TaggedPointer ***** //
typedef union TaggedPointer {
    _Atomic uintptr_t raw; // Valor bruto atômico
    struct {
#if defined(__x86_64__) || defined(__aarch64__) // Arquitetura 64 bits
        uint64_t value : (64 - 2);  // 62 bits para escalares (64 - 2 flags)
        uint8_t is_compound : 1;    // Flag: 1 bit
        uint8_t is_error : 1;       // Flag: 1 bit
#elif defined(__arm__) || defined(__i386__) // Arquitetura 32 bits
        uint32_t value : (32 - 2);  // 30 bits para escalares (32 - 2 flags)
        uint8_t is_compound : 1;    // Flag: 1 bit
        uint8_t is_error : 1;       // Flag: 1 bit
#endif
    };
    struct {
#if defined(__x86_64__) || defined(__aarch64__) // Arquitetura 64 bits
        uintptr_t address : ADDRESS_BITS;          // Bits para endereço
        uint16_t tags : (64 - ADDRESS_BITS - 2);   // Bits restantes para tags
        uint8_t       : 1;                   // Flag: 1 bit
        uint8_t       : 1;                      // Flag: 1 bit
#elif defined(__arm__) || defined(__i386__) // Arquitetura 32 bits
        uint32_t address : ADDRESS_BITS;           // Bits para endereço
        uint8_t tags : (32 - ADDRESS_BITS - 2);    // Bits restantes para tags
        uint8_t      : 1;                   // Flag: 1 bit
        uint8_t      : 1;                      // Flag: 1 bit
#endif
    };
} __attribute__((aligned(sizeof(uintptr_t)))) TaggedPointer;

static inline void set_error(TaggedPointer* tp, uint32_t error_code, const char* message);

static inline bool is_null(TaggedPointer* tp) {
    if (tp->is_compound) {
        return tp->address == 0;
    }
    return 0;  // Escalares não são nulos
}

static inline bool is_error(TaggedPointer* tp) {
    return tp->is_error;
}

static inline bool is_scalar(TaggedPointer* tp) {
    return !tp->is_compound && !tp->is_error;
}

static inline bool is_indirect(TaggedPointer* tp) {
    return tp->is_compound && tp->address != 0 && !tp->is_error;
}

static inline void* get_real_pointer(TaggedPointer* tp) {
    if (!tp->is_compound) return NULL; // Não é um ponteiro indireto
#if defined(__x86_64__) || defined(__aarch64__)
    uintptr_t addr = tp->address;
    if (addr & (1ULL << (ADDRESS_BITS - 1))) { // Extensão de sinal
        addr |= ~((1ULL << ADDRESS_BITS) - 1);
    } else {
        addr &= (1ULL << ADDRESS_BITS) - 1;
    }
    return (void*)addr;
#elif defined(__arm__) || defined(__i386__)
    return (void*)(tp->address); // Ajuste simples para 32 bits
#endif
}

static inline uintptr_t get_value(TaggedPointer* tp) {
    if (is_error(tp)) {
        return 0;
    }
    if (is_scalar(tp)) {
        return tp->value;
    }
    if (is_indirect(tp)) {
        return (uintptr_t)get_real_pointer(tp);
    }
    return 0;
}

static inline void INFO(TaggedPointer* tp) {
    printf("TaggedPointer { ");
    if (tp->is_error) {
        Error* err = (Error*)get_real_pointer(tp);
        printf("is_error: 1, error_code: %u", err->error_code);
    } else if (tp->is_compound) {
        printf("is_compound: 1, address: 0x%lx, tags: %u", tp->address, tp->tags);
        if (is_indirect(tp) && tp->tags == 1) {
            SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
            printf(", ref_count: %u", sp->ref_count);
        }
    } else {
        printf("is_compound: 0, value: %llu", tp->value);
    }
    printf(" }\n");
}

static inline TaggedPointer create(bool is_compound, size_t size, uint16_t tags) {
    TaggedPointer tp = { .raw = 0 };
    if (is_compound) {
        tp.is_compound = 1;
        tp.is_error = 0;
        if (size > 0) {
            void* ptr = CALLOC(size);
            if (ptr) {
                tp.address = (uintptr_t)ptr & ((1ULL << ADDRESS_BITS) - 1);
                tp.tags = tags;
            }
        } else {
            tp.address = 0;  // null
        }
    } else {
        tp.is_compound = 0;
        tp.is_error = 0;
        tp.value = 0;
    }
    return tp;
}

static inline void set_value(TaggedPointer* tp, void* value, size_t size) {
    if (tp->is_error) return;
    if (!tp->is_compound) { // Escalar
#if defined(__x86_64__) || defined(__aarch64__)
        if (size <= sizeof(uint64_t)) {
            uint64_t temp;
            memcpy(&temp, value, size);
            tp->value = temp;
        } else {
            tp->is_error = 1;
        }
#elif defined(__arm__) || defined(__i386__)
        if (size <= sizeof(uint32_t)) {
            uint32_t temp;
            memcpy(&temp, value, size);
            tp->value = temp;
        } else {
            tp->is_error = 1;
        }
#endif
    } else { // Ponteiro indireto
        void* ptr = get_real_pointer(tp);
        if (ptr) memcpy(ptr, value, size);
    }
}

static inline void set_error(TaggedPointer* tp, uint32_t error_code, const char* message) {
    if (is_indirect(tp)) {
        FREE(get_real_pointer(tp));
    }
    Error* err = CALLOC(sizeof(Error));
    if (err) {
        err->error_code = error_code;
        err->message = strdup(message);
        err->prev_address = tp->address;
        tp->address = (uintptr_t)err & ((1ULL << ADDRESS_BITS) - 1);
        tp->is_error = 1;
        tp->is_compound = 1;
    }
}

static inline void ref(TaggedPointer* tp) {
    if (tp->is_compound && is_indirect(tp) && tp->tags == 1) {
        SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
        sp->ref_count++;
    }
}

static inline void deref(TaggedPointer* tp) {
    if (tp->is_compound && is_indirect(tp) && tp->tags == 1) {
        SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
        if (--sp->ref_count == 0) {
            FREE((void*)sp->address);
            FREE(sp);
            tp->address = 0;
        }
    }
}

static inline void dealloc(TaggedPointer* tp) {
    if (is_indirect(tp)) {
        if (tp->is_error) {
            Error* err = (Error*)get_real_pointer(tp);
            FREE(err->message);
            FREE(err);
        } else if (tp->tags == 1) {  // SharedPointer
            deref(tp);
        } else {
            FREE(get_real_pointer(tp));
        }
    }
    tp->raw = 0;
}

int main() {
    #if defined(__x86_64__) || defined(__aarch64__)
    #if CHECK_CPU
        lam_uai_supported = has_lam_uai();
        int virtual_bits = get_virtual_address_bits();
        printf("Número de bits de endereço virtual detectado: %d | Usando: %d\n", virtual_bits, ADDRESS_BITS);
        printf("Suporta paginação %s\n", has_page5() ? "de 5 níveis (57 bits)" : "4 níveis (48 bits)");
        printf("Suporte a LAM/UAI: %s\n", lam_uai_supported ? "Sim" : "Não");
    #endif
    #endif

    // Exemplo 1: Valor Escalar (int)
    printf("\nExemplo 1: Valor Escalar (int)\n");
    TaggedPointer tp1 = create(false, 0, 0);
    printf("create: "); INFO(&tp1);
    int val1 = 100;
    set_value(&tp1, &val1, sizeof(int));
    printf("setValue: "); INFO(&tp1);
    dealloc(&tp1);
    printf("dealloc: "); INFO(&tp1);

    // Exemplo 2: Valor Escalar (float)
    printf("\nExemplo 2: Valor Escalar (float)\n");
    TaggedPointer tp2 = create(false, 0, 0);
    printf("create: "); INFO(&tp2);
    float val2 = 3.14f;
    set_value(&tp2, &val2, sizeof(float));
    printf("setValue: "); INFO(&tp2);
    dealloc(&tp2);
    printf("dealloc: "); INFO(&tp2);

    // Exemplo 3: Erro
    printf("\nExemplo 3: Erro\n");
    TaggedPointer tp3 = create(false, 0, 0);
    printf("create: "); INFO(&tp3);
    set_error(&tp3, 404, "Not Found");
    printf("set_error: "); INFO(&tp3);
    dealloc(&tp3);
    printf("dealloc: "); INFO(&tp3);

    // Exemplo 4: Composto (array)
    printf("\nExemplo 4: Composto (array)\n");
    TaggedPointer tp4 = create(1, sizeof(int) * 3, 3);
    printf("create: "); INFO(&tp4);
    int arr[] = {1, 2, 3};
    set_value(&tp4, arr, sizeof(int) * 3);
    printf("setValue: "); INFO(&tp4);
    void* ptr4 = get_real_pointer(&tp4);
    if (ptr4) {
        int* arr_ptr = (int*)ptr4;
        printf("Array: [%d, %d, %d]\n", arr_ptr[0], arr_ptr[1], arr_ptr[2]);
    }
    dealloc(&tp4);
    printf("dealloc: "); INFO(&tp4);

    // Exemplo 5: Composto (array de floats)
    printf("\nExemplo 5: Composto (array de floats)\n");
    TaggedPointer tp5 = create(true, sizeof(float) * 2, 2);
    printf("create: "); INFO(&tp5);
    float arr5[] = {1.1f, 2.2f};
    set_value(&tp5, arr5, sizeof(float) * 2);
    printf("setValue: "); INFO(&tp5);
    void* ptr5 = get_real_pointer(&tp5);
    if (ptr5) {
        float* arr_ptr5 = (float*)ptr5;
        printf("Array: [%f, %f]\n", arr_ptr5[0], arr_ptr5[1]);
    }
    dealloc(&tp5);
    printf("dealloc: "); INFO(&tp5);

    // Exemplo 6: Composto (string)
    printf("\nExemplo 6: Composto (string)\n");
    TaggedPointer tp6 = create(true, strlen("Hello World!") + 1, strlen("Hello World!"));
    printf("create: "); INFO(&tp6);
    char str[] = "Hello World!";
    set_value(&tp6, str, sizeof(str));
    printf("setValue: "); INFO(&tp6);
    void* ptr6 = get_real_pointer(&tp6);
    if (ptr6) {
        printf("String: %s\n", (char*)ptr6);
    }
    dealloc(&tp6);
    printf("dealloc: "); INFO(&tp6);

    // Exemplo 7: Valor Escalar (uint64_t)
    printf("\nExemplo 7: Valor Escalar (uint64_t)\n");
    TaggedPointer tp7 = create(false, 0, 0);
    printf("create: "); INFO(&tp7);
    uint64_t val7 = 1234567890123456789ULL;
    set_value(&tp7, &val7, sizeof(uint64_t));
    printf("setValue: "); INFO(&tp7);
    dealloc(&tp7);
    printf("dealloc: "); INFO(&tp7);

    // Exemplo 8: Composto com SharedPointer
    printf("\nExemplo 8: Composto com SharedPointer\n");
    SharedPointer* sp = CALLOC(sizeof(SharedPointer));
    sp->ref_count = 1;
    sp->type = 0;
    sp->address = (uintptr_t)CALLOC(sizeof(int));
    *(int*)sp->address = 42;
    TaggedPointer tp8 = { .raw = 0 };
    tp8.is_compound = 1;
    tp8.is_error = 0;
    tp8.address = (uintptr_t)sp & ((1ULL << ADDRESS_BITS) - 1);
    tp8.tags = 1;
    printf("create: "); INFO(&tp8);
    ref(&tp8);
    printf("ref: "); INFO(&tp8);
    deref(&tp8);
    printf("deref: "); INFO(&tp8);
    deref(&tp8);
    printf("deref: "); INFO(&tp8);

    return 0;
}