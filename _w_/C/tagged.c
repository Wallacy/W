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

// ***** Definições de TaggedPointer para 64 bits ***** //
#if defined(__x86_64__) || defined(__aarch64__)
typedef union {
    _Atomic uintptr_t raw;
    struct {
        uintptr_t address : 48;  // Bits 0-47: endereço
        uint16_t tags     : 14;  // Bits 48-61: tags ou ref_count
        uint8_t is_shared : 1;   // Bit 62
        uint8_t is_null   : 1;   // Bit 63
    };
} TaggedPointer_48;

typedef union {
    _Atomic uintptr_t raw;
    struct {
        uintptr_t address : 52;  // Bits 0-51: endereço
        uint16_t tags     : 10;  // Bits 52-61: tags ou ref_count
        uint8_t is_shared : 1;   // Bit 62
        uint8_t is_null   : 1;   // Bit 63
    };
} TaggedPointer_52;

typedef union {
    _Atomic uintptr_t raw;
    struct {
        uintptr_t address : 56;  // Bits 0-55: endereço
        uint8_t tags      : 6;   // Bits 56-61: tags ou ref_count
        uint8_t is_shared : 1;   // Bit 62
        uint8_t is_null   : 1;   // Bit 63
    };
} TaggedPointer_56;

typedef union {
    _Atomic uintptr_t raw;
    struct {
        uintptr_t address : 57;  // Bits 0-56: endereço
        uint8_t tags      : 5;   // Bits 57-61: tags ou ref_count
        uint8_t is_shared : 1;   // Bit 62
        uint8_t is_null   : 1;   // Bit 63
    };
} TaggedPointer_57;

#if ADDRESS_BITS == 48
typedef TaggedPointer_48 TaggedPointer;
#elif ADDRESS_BITS == 52
typedef TaggedPointer_52 TaggedPointer;
#elif ADDRESS_BITS == 56
typedef TaggedPointer_56 TaggedPointer;
#elif ADDRESS_BITS == 57
typedef TaggedPointer_57 TaggedPointer;
#else
#error "Número de bits de endereço não suportado"
#endif

// ***** Definições de TaggedPointer para 32 bits ***** //
#elif defined(__arm__) || defined(__i386__)
typedef union {
    _Atomic uint32_t raw;
    struct {
        uint32_t address  : 30;  // Bits 0-29: endereço
        uint32_t is_shared: 1;   // Bit 30
        uint32_t is_null  : 1;   // Bit 31
    };
} TaggedPointer;
#endif

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

// ***** Macros de Acesso ao Ponteiro Real ***** //
#if defined(__x86_64__) || defined(__aarch64__)
#define GET_REAL_POINTER(tp) \
    ({ \
        uintptr_t adjusted_ptr = (lam_uai_supported) ? (tp).raw : \
            (((tp).address & (1ULL << (ADDRESS_BITS - 1))) ? \
             ((tp).address | ~((1ULL << ADDRESS_BITS) - 1)) : \
             ((tp).address & ((1ULL << ADDRESS_BITS) - 1))); \
        adjusted_ptr = (W_KERNEL_MODE) ? (adjusted_ptr | ((uintptr_t)1 << 63)) : adjusted_ptr; \
        (void*)adjusted_ptr; \
    })
#elif defined(__arm__) || defined(__i386__)
#define GET_REAL_POINTER(tp) \
    ((void*)((tp).address << 2))
#endif

#define SET(tp, value) \
    do { if (!get_null(tp)) { *(typeof(value)*)GET_REAL_POINTER(tp) = (value); } } while(0)

#define GET(tp, type) \
    (get_null(tp) ? (type)0 : *(type*)GET_REAL_POINTER(tp))

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

// ***** Funções de Manipulação de TaggedPointer ***** //
#if defined(__x86_64__) || defined(__aarch64__)
static inline TaggedPointer create(size_t size, uint16_t tags, bool is_shared, bool is_null) {
    void* ptr = MALLOC(size);
    if (!ptr) return (TaggedPointer){0};
    TaggedPointer tp = { .address = (uintptr_t)ptr, .tags = tags, .is_shared = is_shared, .is_null = is_null };
    return tp;
}

static inline void update_tagged_pointer(TaggedPointer* tp, uint16_t tags, bool is_shared, bool is_null, size_t realloc_size) {
    if (realloc_size > 0) {
        void* new_ptr = REALLOC((void*)GET_REAL_POINTER(*tp), realloc_size);
        if (!new_ptr) return;
        tp->address = (uintptr_t)new_ptr;
    }
    tp->tags = tags;
    tp->is_shared = is_shared;
    tp->is_null = is_null;
}

static inline void set_tags(TaggedPointer* tp, uint16_t tags) {
    if (get_shared(*tp) && get_tags(*tp) == ((1U << (62 - ADDRESS_BITS)) - 1)) {
        SharedPointer* sp = (SharedPointer*)tp->address;
        atomic_store(&sp->tags, tags);
    } else {
        tp->tags = tags;
    }
}

static inline uint16_t get_tags(TaggedPointer tp) {
    uint16_t tags = (tp.raw >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1);
    if (get_shared(tp) && tags == ((1U << (62 - ADDRESS_BITS)) - 1)) {
        SharedPointer* sp = (SharedPointer*)tp.address;
        return atomic_load(&sp->tags);
    }
    return tags;
}

static inline void set_shared(TaggedPointer* tp, bool isShared) {
    if (isShared && !tp->is_shared && !tp->is_null) {
        if (tp->tags != 0) {
            promote_to_shared(tp);
        } else {
            tp->is_shared = true;
        }
    } else {
        tp->is_shared = isShared;
    }
}

static inline bool get_shared(TaggedPointer tp) {
    return (tp.raw >> 62) & 1;
}

static inline void set_null(TaggedPointer* tp, bool isNull) {
    tp->is_null = isNull;
}

static inline bool get_null(TaggedPointer tp) {
    return (tp.raw >> 63) & 1;
}
#endif

// ***** Funções de Gestão de Referência ***** //
static inline void promote_to_shared(TaggedPointer* tp) {
    SharedPointer* sp = MALLOC(sizeof(SharedPointer));
    sp->address = tp->address;
    sp->ref_count = tp->tags ? tp->tags : 1;
    sp->tags = tp->tags;
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
        FREE((void*)GET_REAL_POINTER(*tp));
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
    printf("Número de bits de endereço virtual: %d\n", virtual_bits);
    printf("Suporta paginação %s\n", has_page5() ? "de 5 níveis (57 bits)" : "4 níveis (48 bits)");
    printf("Suporte a LAM/UAI: %s\n", lam_uai_supported ? "Sim" : "Não");
#endif
#endif

    // Exemplo 1: Inteiro Simples
    TaggedPointer tp_int = create(sizeof(int), 113, false, false);
    SET(tp_int, 42);
    printf("\nExemplo 1 - Inteiro Simples:\n");
    printf("Ponteiro: 0x%016lx, Valor: %d, Tags: %d\n", (uintptr_t)tp_int.raw, GET(tp_int, int), get_tags(tp_int));
    dealloc(&tp_int);

    // Exemplo 2: String com Realloc
    TaggedPointer tp_str = create(13, 149, false, false);
    strcpy((char*)GET_REAL_POINTER(tp_str), "Hello World!");
    printf("\nExemplo 2 - String com Realloc:\n");
    printf("Ponteiro: 0x%016lx, Valor: %s, Tags: %d\n", (uintptr_t)tp_str.raw, (char*)GET_REAL_POINTER(tp_str), get_tags(tp_str));
    update_tagged_pointer(&tp_str, 149, false, false, 21);
    strcpy((char*)GET_REAL_POINTER(tp_str), "Hello TaggedPointer!");
    printf("Após realloc: %s, Tags: %d\n", (char*)GET_REAL_POINTER(tp_str), get_tags(tp_str));
    dealloc(&tp_str);

    // Exemplo 3: Promoção Automática
    printf("\nExemplo 3 - Promoção Automática:\n");
    TaggedPointer tp_auto = create(sizeof(int), 0, false, false);
    SET(tp_auto, 100);
    printf("Inicial: tags/ref_count: %d, isShared: %d\n", get_tags(tp_auto), get_shared(tp_auto));
    for (int i = 0; i < 15; i++) ref(&tp_auto);
    printf("Após 15 refs: tags: %d, isShared: %d\n", get_tags(tp_auto), get_shared(tp_auto));
    if (get_shared(tp_auto)) {
        SharedPointer* sp = (SharedPointer*)tp_auto.address;
        printf("Promovido - ref_count: %lu\n", (unsigned long)sp->ref_count);
    }
    dealloc(&tp_auto);
    printf("Após dealloc: isNull: %d\n", get_null(tp_auto));

    // Exemplo 4: Promoção Forçada
    printf("\nExemplo 4 - Promoção Forçada:\n");
    TaggedPointer tp_force = create(sizeof(int), 42, false, false);
    SET(tp_force, 200);
    printf("Inicial: tags: %d, isShared: %d\n", get_tags(tp_force), get_shared(tp_force));
    set_shared(&tp_force, true);
    printf("Após set_shared: tags: %d, isShared: %d\n", get_tags(tp_force), get_shared(tp_force));
    SharedPointer* sp_force = (SharedPointer*)tp_force.address;
    printf("Promovido - tags: %lu, ref_count: %lu\n", (unsigned long)sp_force->tags, (unsigned long)sp_force->ref_count);
    dealloc(&tp_force);

    // Exemplo 5: Array de Inteiros com Realloc
    printf("\nExemplo 5 - Array de Inteiros com Realloc:\n");
    TaggedPointer tp_array = create(sizeof(int) * 3, 200, false, false);
    int* array = (int*)GET_REAL_POINTER(tp_array);
    for (int i = 0; i < 3; i++) array[i] = i * 10;
    printf("Ponteiro: 0x%016lx, Tags: %d\n", (uintptr_t)tp_array.raw, get_tags(tp_array));
    printf("Antes do realloc: [");
    for (int i = 0; i < 3; i++) printf("%d%s", array[i], i < 2 ? ", " : "");
    printf("]\n");
    update_tagged_pointer(&tp_array, 200, false, false, sizeof(int) * 5);
    array = (int*)GET_REAL_POINTER(tp_array);
    for (int i = 3; i < 5; i++) array[i] = i * 10;
    printf("Após realloc: [");
    for (int i = 0; i < 5; i++) printf("%d%s", array[i], i < 4 ? ", " : "");
    printf("]\n");
    dealloc(&tp_array);

    // Exemplo 6: Tags Dinâmicos
    printf("\nExemplo 6 - Tags Dinâmicos:\n");
    TaggedPointer tp_tags = create(sizeof(int), 5, false, false);
    SET(tp_tags, 500);
    printf("Inicial: Valor: %d, Tags: %d\n", GET(tp_tags, int), get_tags(tp_tags));
    set_tags(&tp_tags, 10);
    printf("Após set_tags: Valor: %d, Tags: %d\n", GET(tp_tags, int), get_tags(tp_tags));
    dealloc(&tp_tags);

    return 0;
}