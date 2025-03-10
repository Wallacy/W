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

#ifndef W_KERNEL_MODE
#define W_KERNEL_MODE 0
#endif

#ifndef ADDRESS_BITS
#define ADDRESS_BITS 48
#endif

#define CHECK_CPU 1
#define true 1
#define false 0
typedef unsigned int uint;

// ***** Definições de SharedPointer ***** //
typedef struct {
    _Atomic uint ref_count;
    _Atomic uint tags;
    uintptr_t address;
} SharedPointer;

// ***** Definições de TaggedPointer ***** //
typedef union TaggedPointer{
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
} __attribute__((aligned(sizeof(uintptr_t)))) TaggedPointer;

#define LAM_UAI 0
// ***** Funções Auxiliares para 64 bits ***** //
#if defined(__x86_64__) || defined(__aarch64__)

#if CHECK_CPU
#include <cpuid.h>

#undef LAM_UAI
bool lam_uai_supported = false;
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
static inline bool get_shared(TaggedPointer* tp);
static inline uint16_t get_tags(TaggedPointer* tp);
static inline void promote_to_shared(TaggedPointer* tp);
static inline void set_tags(TaggedPointer* tp, uint16_t tags);
static inline void set_shared(TaggedPointer* tp, bool isShared);
static inline void set_null(TaggedPointer* tp, bool isNull);
static inline bool get_null(TaggedPointer* tp);
static inline void ref(TaggedPointer* tp);
static inline void dealloc(TaggedPointer* tp);
static inline TaggedPointer create(size_t size, uint16_t tags, bool is_shared, bool is_null);
static inline void update_tagged_pointer(TaggedPointer* tp, uint16_t tags, bool is_shared, bool is_null, size_t realloc_size);
static inline uintptr_t get_value(TaggedPointer* tp);
static inline void* get_real_pointer(TaggedPointer* tp);
static inline void set_pointer(TaggedPointer *tp, void* value);

// ***** Funções de Acesso ao Ponteiro Real ***** //
#define GET_STR(tp) (get_null(tp) ? "(null)" : (char*)(get_real_pointer(tp)))
static inline void INFO_STR(const char *msg,TaggedPointer *tp) { printf("%s: 0x%016lx, Valor: %s, Tags: %d, isShared: %d, isNULL: %d \n",msg,tp->raw, GET_STR(tp), get_tags(tp), get_shared(tp),  get_null(tp)); }
static inline void INFO_INT(const char *msg,TaggedPointer *tp) { printf("%s: 0x%016lx, Valor: %d, Tags: %d, isShared: %d, isNULL: %d \n",msg,tp->raw, (int)get_value(tp), get_tags(tp), get_shared(tp),  get_null(tp)); }
static inline void INFO_SP(SharedPointer *sp) { printf("SharedPointer - ref_count: %u, tags: %u, address: 0x%016lx\n", sp->ref_count,sp->tags,sp->address); }

static inline void* get_real_pointer(TaggedPointer* tp) {
#if defined(__x86_64__) || defined(__aarch64__)    
    return (void*)(LAM_UAI ? tp->raw :
    ((tp->address & (1ULL << 47)) ?
     (tp->address | 0xFFFF000000000000) :
     (tp->address & 0x0000FFFFFFFFFFFF)));
#elif defined(__arm__) || defined(__i386__)
    return (void*)(tp->address << 2);
#endif
}

static inline void set_value(TaggedPointer* tp, void* value, size_t size) {
    if (!get_null(tp)) {
        memcpy(get_real_pointer(tp), value, size);
    }
}

static inline uintptr_t get_value(TaggedPointer* tp) {
    return get_null(tp) ? 0 : *(uintptr_t *)get_real_pointer(tp);
}

static inline void set_pointer(TaggedPointer *tp, void* value) {
    if (!get_null(tp)) {
        tp->address = (uintptr_t)value;
    }
}

// ***** Funções de Manipulação de TaggedPointer ***** //
static inline TaggedPointer create(size_t size, uint16_t tags, bool is_shared, bool is_null) {
    void* ptr = CALLOC(size);
    printf("Create: 0x%016lx \n", (uintptr_t)ptr);
    if (!ptr) return (TaggedPointer){0};
    TaggedPointer tp = { .raw = (uintptr_t)ptr };
#if defined(__x86_64__) || defined(__aarch64__)
    tp.tags = tags;
#endif
    tp.is_shared = is_shared;
    tp.is_null = is_null;
    INFO_INT("TaggedPointer", &tp);
    return tp;
}

static inline void update_tagged_pointer(TaggedPointer* tp, uint16_t tags, bool is_shared, bool is_null, size_t realloc_size) {
    if (realloc_size > 0) {
        void* new_ptr = REALLOC((void*)get_real_pointer(tp), realloc_size);
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
    if (get_shared(tp)) {
        SharedPointer* sp = (SharedPointer*)tp->address;
        sp->tags = tags;
    } else {
#if defined(__x86_64__) || defined(__aarch64__)
        tp->tags = tags;
#endif
    }
}

static inline uint16_t get_tags(TaggedPointer* tp) {
    if (get_shared(tp)) {
        SharedPointer* sp = (SharedPointer*)tp->address;
        return sp->tags;
    } else {
#if defined(__x86_64__) || defined(__aarch64__)
        return tp->tags;
#else
        return 0;
#endif
    }
}

static inline void set_shared(TaggedPointer* tp, bool isShared) {
    if (isShared && !tp->is_shared && !tp->is_null) {
        if (get_tags(tp) != 0) {
            promote_to_shared(tp);
        } else {
            tp->is_shared = true;
        }
    } else {
        tp->is_shared = isShared;
    }
}

static inline bool get_shared(TaggedPointer* tp) {
    return tp->is_shared;
}

static inline void set_null(TaggedPointer* tp, bool isNull) {
    tp->is_null = isNull;
}

static inline bool get_null(TaggedPointer* tp) {
    return tp->is_null;
}

// ***** Funções de Gestão de Referência ***** //
#define MAX_TAGS (1U << (62 - ADDRESS_BITS))-1

static inline void promote_to_shared(TaggedPointer* tp) {
    SharedPointer* sp = CALLOC(sizeof(SharedPointer));
    sp->address = tp->address;
    sp->ref_count = get_tags(tp) ? get_tags(tp) : 1;
    sp->tags = get_tags(tp);
    tp->address = (uintptr_t)sp;
#if defined(__x86_64__) || defined(__aarch64__)
    tp->tags = MAX_TAGS;
#endif
    tp->is_shared = true;
}

static inline void ref(TaggedPointer* tp) {
    if (!get_shared(tp) && !get_null(tp)) {
#if defined(__x86_64__) || defined(__aarch64__)
        uint16_t tags = get_tags(tp);
        const uint16_t max_tags = MAX_TAGS;
        if (tags < max_tags) {
            set_tags(tp, tags + 1);
        } else {
            promote_to_shared(tp);
        }
#else
        promote_to_shared(tp);
#endif
    } else if (get_shared(tp) && !get_null(tp)) {
        SharedPointer* sp = (SharedPointer*)tp->address;
        sp->ref_count++;
    }
}

/* ((sp->ref_count--) == 1)vai gerar:
            movq    -16(%rbp), %rax
            lock    decq    (%rax)
            sete    %al
            testb   $1, %al
            je      .LBB5_6
*/
/*  (atomic_fetch_sub(&sp->ref_count, 1) == 1) vai gerar:
    movq    $1, -24(%rbp)
    movq    -24(%rbp), %rax
    negq    %rax
    lock    xaddq   %rax, (%rcx)
    movq    %rax, -32(%rbp)
    cmpq    $1, -32(%rbp)
    jne     .LBB5_6
*/
// Devido ao fato do clando colocar o lock no lugar correto com decq e ainda ler corretamente o valor do ref_count anterior optei por usar a primeira opção.

static inline void dealloc(TaggedPointer* tp) {
    if (get_null(tp)) return;
    if (!get_shared(tp)) {
        FREE((void*)get_real_pointer(tp));
        set_null(tp, true);
    } else {
        SharedPointer* sp = (SharedPointer*)tp->address;
        if ((sp->ref_count--) == 1) {
            FREE((void*)sp->address);
            FREE(sp);
            set_null(tp, true);
        }
    }
}

// #define INFO(msg,tp,X) _Generic((X),   \
//                  default: INFO_INT,    \
//                  char*:     INFO_STR,  \
//                  int:  INFO_INT)(msg,tp)
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

    TaggedPointer tp;

    // Exemplo 1: Inteiro Simples
    printf("\nExemplo 1 - Inteiro Simples:\n");
    tp = create(sizeof(int), 113, false, false);
    INFO_INT("Inicial",&tp);
    int value_int = 42;
    set_value(&tp, &value_int, sizeof(int));
    INFO_INT("SetValue",&tp);
    dealloc(&tp);
    INFO_INT("Final",&tp);


    // Exemplo 2: String com Realloc
    printf("\nExemplo 2 - String com Realloc:\n");
    tp = create(13, 149, false, false);
    INFO_STR("Inicial", &tp);
    strcpy((char*)get_real_pointer(&tp), "Hello World!");
    INFO_STR("strcpy", &tp);
    update_tagged_pointer(&tp, 151, false, false, 21);
    INFO_STR("Update", &tp);
    strcpy((char*)get_real_pointer(&tp), "Hello TaggedPointer!");
    INFO_STR("Final", &tp);
    dealloc(&tp);

    // Exemplo 3: Promoção Automática
    printf("\nExemplo 3 - Promoção Automática:\n");
    tp = create(sizeof(int), 0, false, false);
    int value_auto = 100;
    INFO_INT("Inicial", &tp);
    set_value(&tp, &value_auto, sizeof(int));
    INFO_INT("SetValue", &tp);
    for (int i = 0; i < 15; i++) ref(&tp);
    INFO_INT("Após 15", &tp);
    if (get_shared(&tp)) { INFO_SP((SharedPointer*)tp.address); }
    dealloc(&tp);
    INFO_INT("Após dealloc", &tp);

    // Exemplo 4: Promoção Forçada
    printf("\nExemplo 4 - Promoção Forçada:\n");
    tp = create(sizeof(int), 42, false, false);
    INFO_INT("Inicial", &tp);
    int value_force = 200;
    set_value(&tp, &value_force, sizeof(int));
    INFO_INT("SetValue", &tp);
    set_shared(&tp, true);
    INFO_INT("SetShared", &tp);
    SharedPointer* sp_force = (SharedPointer*)(uintptr_t)tp.address;
    INFO_SP((SharedPointer*)tp.address);
    dealloc(&tp);
    INFO_INT("Após dealloc", &tp); // espero leak, ainda vou ajustar

    // Exemplo 5: Array de Inteiros com Realloc
    printf("\nExemplo 5 - Array de Inteiros com Realloc:\n");
    tp = create(sizeof(int) * 3, 200, false, false);
    INFO_INT("Inicial", &tp);
    int* array = (int*)get_real_pointer(&tp);
    for (int i = 0; i < 3; i++) array[i] = i * 10;
    INFO_INT("Inicial", &tp);
    printf("Antes do realloc: [");
    for (int i = 0; i < 3; i++) printf("%d%s", array[i], i < 2 ? ", " : "");
    printf("]\n");
    update_tagged_pointer(&tp, 200, false, false, sizeof(int) * 5);
    INFO_INT("Update", &tp);
    array = (int*)get_real_pointer(&tp);
    for (int i = 3; i < 5; i++) array[i] = i * 10;
    printf("Após realloc: [");
    for (int i = 0; i < 5; i++) printf("%d%s", array[i], i < 4 ? ", " : "");
    printf("]\n");
    dealloc(&tp);
    INFO_INT("Após dealloc", &tp);

    // Exemplo 6: Tags Dinâmicos
    printf("\nExemplo 6 - Tags Dinâmicos:\n");
    tp = create(sizeof(int), 5, false, false);
    int value_tags = 500;
    set_value(&tp, &value_tags, sizeof(int));
    INFO_INT("Inicial", &tp);
    set_tags(&tp, 10);
    INFO_INT("SetTags", &tp);
    dealloc(&tp);
    INFO_INT("Final", &tp);

    // Exemplo 7: Associando Ponteiro com set_pointer
    printf("\nExemplo 7 - Associando Ponteiro com set_pointer:\n");
    char* str = MALLOC(20);
    strcpy(str, "Direct Pointer");
    tp = create(0, 99, false, false);
    INFO_STR("Inicial", &tp);
    set_pointer(&tp, str);
    INFO_STR("SetPointer", &tp);
    dealloc(&tp);
    INFO_STR("Final", &tp);


    return 0;
}