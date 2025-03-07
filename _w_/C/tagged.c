#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#ifdef USE_MIMALLOC
#include <mimalloc.h>
#define MALLOC(size) mi_malloc(size)
#define REALLOC(obj, size) mi_realloc(obj, size)
#define FREE(ptr) mi_free((void *)ptr)
#else
#define MALLOC(size) malloc(size)
#define REALLOC(obj, size) realloc(obj, size)
#define FREE(ptr) free((void *)ptr)
#endif

#ifndef W_KERNEL_MODE
#define W_KERNEL_MODE 0
#endif

#ifndef ADDRESS_BITS
#define ADDRESS_BITS 48
#endif

#define CHECK_CPU 1
#define true 1
#define false 0

// ***** Definições de SharedPointer ***** //
#if defined(__x86_64__) || defined(__aarch64__)
typedef struct {
    _Atomic uint64_t ref_count;
    _Atomic uint64_t tags;
    uintptr_t address;
} SharedPointer;
#elif defined(__arm__) || defined(__i386__)
typedef struct {
    _Atomic uint32_t ref_count;
    _Atomic uint32_t tags;
    uintptr_t address;
} SharedPointer;
#endif

// ***** Definições de TaggedPointer ***** //
typedef union {
    _Atomic uintptr_t raw;
    struct {
#if defined(__x86_64__) || defined(__aarch64__)
        uintptr_t address : ADDRESS_BITS;  // Bits 0-(ADDRESS_BITS-1): endereço
        uint16_t tags     : (62 - ADDRESS_BITS);  // Bits ADDRESS_BITS-61: tags ou ref_count
        uint8_t is_shared : 1;   // Bit 62
        uint8_t is_null   : 1;   // Bit 63
#elif defined(__arm__) || defined(__i386__)
        uint32_t address  : 30;  // Bits 0-29: endereço
        uint32_t is_shared: 1;   // Bit 30
        uint32_t is_null  : 1;   // Bit 31
        union {
            uint32_t       : 30;
            uint32_t tags  : 2;  // Bits 30-31: tags
        };
#endif
    };
} TaggedPointer;

// ***** Funções Auxiliares para 64 bits ***** //
#if defined(__x86_64__) || defined(__aarch64__)
bool lam_uai_supported = false;

#if CHECK_CPU
#include <cpuid.h>
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
    return false;
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

// ***** Declarações de Funções ***** //
static inline bool get_shared(TaggedPointer tp);
static inline uint16_t get_tags(TaggedPointer tp);
static inline void promote_to_shared(TaggedPointer* tp);
static inline void set_tags(TaggedPointer* tp, uint16_t tags);
static inline void set_shared(TaggedPointer* tp, bool isShared);
static inline void set_null(TaggedPointer* tp, bool isNull);
static inline bool get_null(TaggedPointer tp);
static inline void ref(TaggedPointer* tp);
static inline void dealloc(TaggedPointer* tp);
static inline TaggedPointer create(size_t size, uint16_t tags, bool is_shared, bool is_null);
static inline void update_tagged_pointer(TaggedPointer* tp, uint16_t tags, bool is_shared, bool is_null, size_t realloc_size);

// ***** Funções de Acesso ao Ponteiro Real ***** //
static inline void* get_real_pointer(TaggedPointer tp) {
#if defined(__x86_64__) || defined(__aarch64__)
    uintptr_t adjusted_ptr = (lam_uai_supported) ? tp.raw :
        ((tp.address & (1ULL << 47)) ?
         (tp.address | 0xFFFF000000000000) :
         (tp.address & 0x0000FFFFFFFFFFFF));
    
    adjusted_ptr = (W_KERNEL_MODE) ? (adjusted_ptr | ((uintptr_t)1 << 63)) : adjusted_ptr;
    return (void*)adjusted_ptr;
#elif defined(__arm__) || defined(__i386__)
    return (void*)(tp.address << 2);
#endif
}

static inline void set_value(TaggedPointer tp, void* value, size_t size) {
    if (!get_null(tp)) {
        memcpy(get_real_pointer(tp), value, size);
    }
}

static inline void* get_value(TaggedPointer tp) {
    return get_null(tp) ? NULL : get_real_pointer(tp);
}

// ***** Funções de Manipulação de TaggedPointer ***** //
static inline TaggedPointer create(size_t size, uint16_t tags, bool is_shared, bool is_null) {
    void* ptr = MALLOC(size);
    if (!ptr) return (TaggedPointer){0};
    TaggedPointer tp = { .raw = (uintptr_t)ptr };
#if defined(__x86_64__) || defined(__aarch64__)
    tp.tags = tags;
#endif
    tp.is_shared = is_shared;
    tp.is_null = is_null;
    return tp;
}

static inline void update_tagged_pointer(TaggedPointer* tp, uint16_t tags, bool is_shared, bool is_null, size_t realloc_size) {
    if (realloc_size > 0) {
        void* new_ptr = REALLOC((void*)get_real_pointer(*tp), realloc_size);
        if (!new_ptr) return;
        tp->raw = (uintptr_t)new_ptr;
    }
#if defined(__x86_64__) || defined(__aarch64__)
    tp->tags = tags;
#endif
    tp->is_shared = is_shared;
    tp->is_null = is_null;
}

static inline void set_tags(TaggedPointer* tp, uint16_t tags) {
    if (get_shared(*tp)) {
        SharedPointer* sp = (SharedPointer*)tp->address;
        atomic_store(&sp->tags, tags);
    } else {
#if defined(__x86_64__) || defined(__aarch64__)
        tp->tags = tags;
#endif
    }
}

static inline uint16_t get_tags(TaggedPointer tp) {
    if (get_shared(tp)) {
        SharedPointer* sp = (SharedPointer*)tp.address;
        return atomic_load(&sp->tags);
    } else {
#if defined(__x86_64__) || defined(__aarch64__)
        return (tp.raw >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1);
#else
        return 0;
#endif
    }
}

static inline void set_shared(TaggedPointer* tp, bool isShared) {
    if (isShared && !tp->is_shared && !tp->is_null) {
        if (get_tags(*tp) != 0) {
            promote_to_shared(tp);
        } else {
            tp->is_shared = true;
        }
    } else {
        tp->is_shared = isShared;
    }
}

static inline bool get_shared(TaggedPointer tp) {
#if defined(__x86_64__) || defined(__aarch64__)
    return (tp.raw >> 62) & 1;
#elif defined(__arm__) || defined(__i386__)
    return (tp.raw >> 30) & 1;
#endif
}

static inline void set_null(TaggedPointer* tp, bool isNull) {
#if defined(__x86_64__) || defined(__aarch64__)
    tp->is_null = isNull;
#elif defined(__arm__) || defined(__i386__)
    tp->is_null = isNull;
#endif
}

static inline bool get_null(TaggedPointer tp) {
#if defined(__x86_64__) || defined(__aarch64__)
    return (tp.raw >> 63) & 1;
#elif defined(__arm__) || defined(__i386__)
    return (tp.raw >> 31) & 1;
#endif
}

// ***** Funções de Gestão de Referência ***** //
static inline void promote_to_shared(TaggedPointer* tp) {
    SharedPointer* sp = MALLOC(sizeof(SharedPointer));
    sp->address = tp->address;
    sp->ref_count = get_tags(*tp) ? get_tags(*tp) : 1;
    sp->tags = get_tags(*tp);
    tp->address = (uintptr_t)sp;
#if defined(__x86_64__) || defined(__aarch64__)
    tp->tags = (1U << (62 - ADDRESS_BITS)) - 1;  // max_tags
#endif
    tp->is_shared = true;
}

static inline void ref(TaggedPointer* tp) {
    if (!get_shared(*tp) && !get_null(*tp)) {
#if defined(__x86_64__) || defined(__aarch64__)
        uint16_t tags = get_tags(*tp);
        const uint16_t max_tags = (1U << (62 - ADDRESS_BITS)) - 1;
        if (tags < max_tags) {
            set_tags(tp, tags + 1);
        } else {
            promote_to_shared(tp);
        }
#else
        promote_to_shared(tp);
#endif
    } else if (get_shared(*tp) && !get_null(*tp)) {
        SharedPointer* sp = (SharedPointer*)tp->address;
        atomic_fetch_add(&sp->ref_count, 1);
    }
}

static inline void dealloc(TaggedPointer* tp) {
    if (get_null(*tp)) return;
    if (!get_shared(*tp)) {
        FREE((void*)get_real_pointer(*tp));
        set_null(tp, true);
    } else {
        SharedPointer* sp = (SharedPointer*)tp->address;
        if (atomic_fetch_sub(&sp->ref_count, 1) == 1) {
            FREE((void*)sp->address);
            FREE(sp);
            set_null(tp, true);
        }
    }
}

// ***** Função Principal com Exemplos ***** //
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

    // Exemplo 1: Inteiro Simples
    TaggedPointer tp_int = create(sizeof(int), 113, false, false);
    int value_int = 42;
    set_value(tp_int, &value_int, sizeof(int));
    printf("\nExemplo 1 - Inteiro Simples:\n");
    printf("Ponteiro: 0x%016lx, Valor: %d, Tags: %d\n", (uintptr_t)tp_int.raw, *(int*)get_value(tp_int), get_tags(tp_int));
    dealloc(&tp_int);

    // Exemplo 2: String com Realloc
    TaggedPointer tp_str = create(13, 149, false, false);
    strcpy((char*)get_real_pointer(tp_str), "Hello World!");
    printf("\nExemplo 2 - String com Realloc:\n");
    printf("Ponteiro: 0x%016lx, Valor: %s, Tags: %d\n", (uintptr_t)tp_str.raw, (char*)get_real_pointer(tp_str), get_tags(tp_str));
    update_tagged_pointer(&tp_str, 151, false, false, 21);
    strcpy((char*)get_real_pointer(tp_str), "Hello TaggedPointer!");
    printf("Após realloc: %s, Tags: %d\n", (char*)get_real_pointer(tp_str), get_tags(tp_str));
    dealloc(&tp_str);

    // Exemplo 3: Promoção Automática
    printf("\nExemplo 3 - Promoção Automática:\n");
    TaggedPointer tp_auto = create(sizeof(int), 0, false, false);
    int value_auto = 100;
    set_value(tp_auto, &value_auto, sizeof(int));
    printf("Inicial: tags/ref_count: %d, isShared: %d\n", get_tags(tp_auto), get_shared(tp_auto));
    for (int i = 0; i < 15; i++) ref(&tp_auto);
    printf("Após 15 refs: tags: %d, isShared: %d\n", get_tags(tp_auto), get_shared(tp_auto));
    if (get_shared(tp_auto)) {
        SharedPointer* sp = (SharedPointer*)(uintptr_t)tp_auto.address;
        printf("Promovido - ref_count: %lu\n", (unsigned long)sp->ref_count);
    }
    dealloc(&tp_auto);
    printf("Após dealloc: isNull: %d\n", get_null(tp_auto));

    // Exemplo 4: Promoção Forçada
    printf("\nExemplo 4 - Promoção Forçada:\n");
    TaggedPointer tp_force = create(sizeof(int), 42, false, false);
    int value_force = 200;
    set_value(tp_force, &value_force, sizeof(int));
    printf("Inicial: tags: %d, isShared: %d\n", get_tags(tp_force), get_shared(tp_force));
    set_shared(&tp_force, true);
    printf("Após set_shared: tags: %d, isShared: %d\n", get_tags(tp_force), get_shared(tp_force));
    SharedPointer* sp_force = (SharedPointer*)(uintptr_t)tp_force.address;
    printf("Promovido - tags: %lu, ref_count: %lu\n", (unsigned long)sp_force->tags, (unsigned long)sp_force->ref_count);
    dealloc(&tp_force);

    // Exemplo 5: Array de Inteiros com Realloc
    printf("\nExemplo 5 - Array de Inteiros com Realloc:\n");
    TaggedPointer tp_array = create(sizeof(int) * 3, 200, false, false);
    int* array = (int*)get_real_pointer(tp_array);
    for (int i = 0; i < 3; i++) array[i] = i * 10;
    printf("Ponteiro: 0x%016lx, Tags: %d\n", (uintptr_t)tp_array.raw, get_tags(tp_array));
    printf("Antes do realloc: [");
    for (int i = 0; i < 3; i++) printf("%d%s", array[i], i < 2 ? ", " : "");
    printf("]\n");
    update_tagged_pointer(&tp_array, 200, false, false, sizeof(int) * 5);
    array = (int*)get_real_pointer(tp_array);
    for (int i = 3; i < 5; i++) array[i] = i * 10;
    printf("Após realloc: [");
    for (int i = 0; i < 5; i++) printf("%d%s", array[i], i < 4 ? ", " : "");
    printf("]\n");
    dealloc(&tp_array);

    // Exemplo 6: Tags Dinâmicos
    printf("\nExemplo 6 - Tags Dinâmicos:\n");
    TaggedPointer tp_tags = create(sizeof(int), 5, false, false);
    int value_tags = 500;
    set_value(tp_tags, &value_tags, sizeof(int));
    printf("Inicial: Valor: %d, Tags: %d\n", *(int*)get_value(tp_tags), get_tags(tp_tags));
    set_tags(&tp_tags, 10);
    printf("Após set_tags: Valor: %d, Tags: %d\n", *(int*)get_value(tp_tags), get_tags(tp_tags));
    dealloc(&tp_tags);

    return 0;
}