#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_MIMALLOC
#include <mimalloc.h>
#define MALLOC(size) mi_malloc(size)
#define CALLOC(num,size) mi_calloc(num,size)
#define REALLOC(obj, size) mi_realloc(obj, size)
#define FREE(ptr) mi_free((void *)ptr)
#else
#define MALLOC(size) malloc(size)
#define CALLOC(num,size) calloc(num,size)
#define REALLOC(obj, size) realloc(obj, size)
#define FREE(ptr) free((void *)ptr)
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
static inline bool get_shared(uintptr_t* tp);
static inline uint16_t get_tags(uintptr_t* tp);
static inline void promote_to_shared(uintptr_t* tp);
static inline void set_tags(uintptr_t* tp, uint16_t tags);
static inline void set_shared(uintptr_t* tp, bool isShared);
static inline void set_null(uintptr_t* tp, bool isNull);
static inline bool get_null(uintptr_t* tp);
static inline void ref(uintptr_t* tp);
static inline void dealloc(uintptr_t* tp);
static inline uintptr_t create(size_t size, uint16_t tags, bool is_shared, bool is_null);
static inline void update_tagged_pointer(uintptr_t* tp, uint16_t tags, bool is_shared, bool is_null, size_t realloc_size);
static inline void set_pointer(uintptr_t* tp, void* value);
static inline uintptr_t get_value(uintptr_t* tp);
static inline void* get_real_pointer(uintptr_t* tp);

#define GET_STR(tp) (get_null(tp) ? "(null)" : (char*)(get_real_pointer(tp)))
static inline void INFO_STR(const char *msg,uintptr_t* tp) { printf("%s: 0x%016lx, Valor: %s, Tags: %d, isShared: %d, isNULL: %d \n",msg,*tp, GET_STR(tp), get_tags(tp), get_shared(tp),  get_null(tp)); }
static inline void INFO_INT(const char *msg,uintptr_t* tp) { printf("%s: 0x%016lx, Valor: %d, Tags: %d, isShared: %d, isNULL: %d \n",msg,*tp, (int)get_value(tp), get_tags(tp), get_shared(tp),  get_null(tp)); }
static inline void INFO_SP(SharedPointer *sp) { printf("SharedPointer - ref_count: %u, tags: %u, address: 0x%016lx\n", sp->ref_count,sp->tags,sp->address); }

// ***** Funções de Acesso ao Ponteiro Real ***** //
static inline void* get_real_pointer(uintptr_t* tp) {
    #if defined(__x86_64__) || defined(__aarch64__)
    return (void*)((LAM_UAI) ? *tp :
    ((*tp & (1ULL << 47)) ?
        (*tp | 0xFFFF000000000000) :
        (*tp & 0x0000FFFFFFFFFFFF)));
#elif defined(__arm__) || defined(__i386__)
    return (void*)(*tp << 2);
#endif
}

static inline void set_value(uintptr_t* tp, void* value, size_t size) {
    if (!get_null(tp)) {
        memcpy(get_real_pointer(tp), value, size);
    }
}

static inline void set_pointer(uintptr_t* tp, void* value) {
    if (!get_null(tp)) {
        *tp = (uintptr_t)value;
    }
}

static inline uintptr_t get_value(uintptr_t* tp) {
    void* value = get_null(tp) ? NULL : get_real_pointer(tp);
    return value ? (uintptr_t)(*(uintptr_t*)value) : 0;
}

// ***** Funções de Manipulação de TaggedPointer ***** //
static inline uintptr_t create(size_t size, uint16_t tags, bool is_shared, bool is_null) {
    void* ptr = CALLOC(1,size);
    printf("Create: 0x%016lx \n", (uintptr_t)ptr);
    if (!ptr) return 0;
    uintptr_t tp = (uintptr_t)ptr;
#if defined(__x86_64__) || defined(__aarch64__)
    tp |= ((uintptr_t)tags << ADDRESS_BITS);
#endif
    if (is_shared) tp |= (1ULL << 62);
    if (is_null) tp |= (1ULL << 63);
    printf("TaggedPointer: 0x%016lx, Valor: %lu, Tags: %d, isShared: %d, isNULL: %d \n", tp, get_value(&tp), get_tags(&tp),  get_shared(&tp),  get_null(&tp));
    return tp;
}

static inline void update_tagged_pointer(uintptr_t* tp, uint16_t tags, bool is_shared, bool is_null, size_t realloc_size) {
    if (realloc_size > 0) {
        void* new_ptr = REALLOC((void*)get_real_pointer(tp), realloc_size);
        if (!new_ptr) return;
        *tp = (uintptr_t)new_ptr;
    }
#if defined(__x86_64__) || defined(__aarch64__)
    *tp = (*tp & ~(0xFFFFULL << ADDRESS_BITS)) | ((uintptr_t)tags << ADDRESS_BITS);
#endif
    if (is_shared) *tp |= (1ULL << 62);
    if (is_null) *tp |= (1ULL << 63);
}

static inline void set_tags(uintptr_t* tp, uint16_t tags) {
    if (get_shared(tp)) {
        SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
        sp->tags = tags;
    } else {
#if defined(__x86_64__) || defined(__aarch64__)
        *tp = (*tp & ~(0xFFFFULL << ADDRESS_BITS)) | ((uintptr_t)tags << ADDRESS_BITS);
#endif
    }
}

static inline uint16_t get_tags(uintptr_t* tp) {
    if (get_shared(tp)) {
        SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
        return sp->tags;
    } else {
#if defined(__x86_64__) || defined(__aarch64__)
        return ((*tp) >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1);
#else
        return 0;
#endif
    }
}

static inline void set_shared(uintptr_t* tp, bool isShared) {
    if (isShared && !get_shared(tp) && !get_null(tp)) {
        promote_to_shared(tp);
    }
}

static inline bool get_shared(uintptr_t* tp) {
#if defined(__x86_64__) || defined(__aarch64__)
    return (*tp >> 62) & 1;
#elif defined(__arm__) || defined(__i386__)
    return (tp >> 30) & 1;
#endif
}

static inline void set_null(uintptr_t* tp, bool isNull) {
#if defined(__x86_64__) || defined(__aarch64__)
    if (isNull) *tp |= (1ULL << 63);
    else *tp &= ~(1ULL << 63);
#elif defined(__arm__) || defined(__i386__)
    if (isNull) *tp |= (1ULL << 31);
    else *tp &= ~(1ULL << 31);
#endif
}

static inline bool get_null(uintptr_t* tp) {
#if defined(__x86_64__) || defined(__aarch64__)
    return (*tp >> 63) & 1;
#elif defined(__arm__) || defined(__i386__)
    return (tp >> 31) & 1;
#endif
}

// ***** Funções de Gestão de Referência ***** //
static inline void promote_to_shared(uintptr_t* tp) {
    SharedPointer* sp = MALLOC(sizeof(SharedPointer));
    sp->address = *tp & ((1ULL << ADDRESS_BITS) - 1);
    sp->ref_count = get_tags(tp) ? get_tags(tp) : 1;
    sp->tags = get_tags(tp);
    update_tagged_pointer((uintptr_t*)sp, get_tags(tp), true, get_null(tp), 0);
// #if defined(__x86_64__) || defined(__aarch64__)
//     *tp |= ((1ULL << (62 - ADDRESS_BITS)) - 1) << ADDRESS_BITS;  // max_tags
// #endif
//     *tp |= (1ULL << 62);
}

#define MAX_TAGS (1U << (62 - ADDRESS_BITS))-1
static inline void ref(uintptr_t* tp) {
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
        SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
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
static inline void dealloc(uintptr_t* tp) {
    if (get_null(tp)) return;
    if (!get_shared(tp)) {
        FREE((void*)get_real_pointer(tp));
        set_null(tp, true);
    } else {
        SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
        if ((sp->ref_count--) == 1) {
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

    uintptr_t tp = 0;

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
    if (get_shared(&tp)) { INFO_SP((SharedPointer*)tp); }
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
    // SharedPointer* sp_force = get_real_pointer(&tp);
    // INFO_SP((SharedPointer*)tp);
    dealloc(&tp);
    INFO_INT("Após dealloc", &tp); // espero leak, ainda vou ajustar

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