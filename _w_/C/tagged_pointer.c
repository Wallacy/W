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
#define TYPE_BITS 3
#define MAX_TAGS ((1UL << (64 - ADDRESS_BITS - TYPE_BITS - 1)) - 1)
#define ADDRESS_TAGS ((1UL << ADDRESS_BITS) -1 )

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

typedef union TaggedPointer {
    _Atomic uintptr_t raw;
    struct {
#if defined(__x86_64__) || defined(__aarch64__)
        uintptr_t address : ADDRESS_BITS;
        uint16_t tags     : (64 - ADDRESS_BITS - TYPE_BITS - 1);
        uint8_t type      : TYPE_BITS;
        uint8_t is_erro   : 1;
#elif defined(__arm__) || defined(__i386__)
        uint32_t address  : 30;
        uint32_t tags     : (32 - 30 - TYPE_BITS - 1);
        uint32_t type     : TYPE_BITS;
        uint32_t is_erro  : 1;
#endif
    };
} __attribute__((aligned(sizeof(uintptr_t)))) TaggedPointer;

enum Type {
    TYPE_NULL = 0, TYPE_INT = 1, TYPE_FLOAT = 2, TYPE_CHAR = 3, TYPE_ARRAY = 4, TYPE_CUSTOM = 7
};

static inline bool is_null(TaggedPointer* tp) { return tp->address == 0; }
static inline bool is_error(TaggedPointer* tp) { return tp->is_erro; }
static inline bool is_tagged_value(TaggedPointer* tp) { return tp->address == ((1ULL << ADDRESS_BITS) - 1); }
static inline bool is_indirect(TaggedPointer* tp) { return tp->address != 0 && tp->address != ((1ULL << ADDRESS_BITS) - 1); }

static inline void* get_real_pointer(TaggedPointer* tp) {
    if (!is_indirect(tp)) return NULL;
#if defined(__x86_64__) || defined(__aarch64__)
    uintptr_t addr = tp->address;
    if (addr & (1ULL << (ADDRESS_BITS - 1))) {
        addr |= ~((1ULL << ADDRESS_BITS) - 1);
    } else {
        addr &= (1ULL << ADDRESS_BITS) - 1;
    }
    return (void*)addr;
#elif defined(__arm__) || defined(__i386__)
    return (void*)(tp->address << 2);
#endif
}

static inline uintptr_t get_value(TaggedPointer* tp) {
    if (is_null(tp) || is_error(tp)) {
        return 0;
    }
    if (is_tagged_value(tp)) {
        return tp->tags;
    }
    if (is_indirect(tp)) {
        if (tp->type == TYPE_CUSTOM) {
            SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
            return *(uintptr_t*)sp->address;
        } else if (tp->type == TYPE_INT) {
            return *(int*)get_real_pointer(tp);
        } else if (tp->type == TYPE_FLOAT) {
            return *(float*)get_real_pointer(tp);
        } else {
            return (uintptr_t)get_real_pointer(tp);
        }
    }
    return 0;
}

static inline void INFO(TaggedPointer* tp) {
    printf("TaggedPointer { address: 0x%lx, tags: %u, type: %u, is_erro: %d, value: %lu",
           tp->address, tp->tags, tp->type, tp->is_erro, get_value(tp));
    if (tp->type == TYPE_CUSTOM && is_indirect(tp)) {
        SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
        printf(", ref_count: %u", sp->ref_count);
    }
    printf(" }\n");
}

static inline TaggedPointer create(size_t size, uint16_t tags, uint8_t type) {
    TaggedPointer tp = { .raw = 0 };
    tp.address = ((uintptr_t)1 << ADDRESS_BITS) - 1;  // Valor inicial inválido
    tp.tags = tags > MAX_TAGS ? MAX_TAGS : tags;
    tp.type = type;
    tp.is_erro = 0;

    if (type == TYPE_CUSTOM) {
        SharedPointer* sp = CALLOC(sizeof(SharedPointer));
        if (sp) {
            sp->address = (uintptr_t)CALLOC(size);  // Aloca memória para os dados
            sp->ref_count = 1;                      // Inicializa o contador de referências
            sp->type = type;                        // Define o tipo
            tp.address = (uintptr_t)sp & ((1ULL << ADDRESS_BITS) - 1);  // Armazena apenas ADDRESS_BITS
        }
    } else if (size > sizeof(uintptr_t)) {
        void* ptr = CALLOC(size);
        if (ptr) {
            tp.address = (uintptr_t)ptr & ((1ULL << ADDRESS_BITS) - 1);  // Armazena apenas ADDRESS_BITS
        }
    }
    return tp;
}

static inline void set_error(TaggedPointer* tp, uint32_t error_code, const char* message) {
    // Se for indireto, liberar a memória anterior
    if (is_indirect(tp)) {
        if (is_error(tp)) {
            Error* err = (Error*)get_real_pointer(tp);
            FREE(err->message); // Liberar a string duplicada
            FREE(err);          // Liberar a estrutura Error
        } else {
            FREE(get_real_pointer(tp)); // Liberar qualquer outro ponteiro
        }
    }
    // Alocar novo erro
    Error* err = CALLOC(sizeof(Error));
    if (err) {
        err->error_code = error_code;
        err->message = strdup(message);
        err->prev_address = tp->address; // Preservar o endereço anterior, se necessário
        tp->address = (uintptr_t)err;
        tp->is_erro = 1;
        tp->type = TYPE_NULL;
    }
}

static inline void set_value(TaggedPointer* tp, void* value, size_t size, uint8_t type) {
    if (is_error(tp)) return;
    if (size <= sizeof(uint16_t) && type != TYPE_CUSTOM) {
        tp->address = ((uintptr_t)1 << ADDRESS_BITS) - 1;
        tp->tags = *(uint16_t*)value;
    } else {
        if (is_indirect(tp)) {
            if (tp->type == TYPE_CUSTOM) {
                SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
                void* ptr = (void*)sp->address;  // Usa o endereço dos dados
                if (ptr) {
                    memcpy(ptr, value, size);    // Copia o valor para o endereço dos dados
                }
            } else {
                FREE(get_real_pointer(tp));
                void* ptr = CALLOC(size);
                if (ptr) {
                    memcpy(ptr, value, size);
                    tp->address = (uintptr_t)ptr & ((1ULL << ADDRESS_BITS) - 1);
                }
            }
        } else if (type == TYPE_CUSTOM && is_indirect(tp)) {
            SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
            void* ptr = CALLOC(size);
            if (ptr) {
                memcpy(ptr, value, size);
                sp->address = (uintptr_t)ptr;
            }
        }
    }
    tp->type = type;
}

static inline void ref(TaggedPointer* tp) {
    if (is_indirect(tp) && tp->type == TYPE_CUSTOM) {
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
static inline void deref(TaggedPointer* tp) {
    if (is_indirect(tp) && tp->type == TYPE_CUSTOM) {
        SharedPointer* sp = (SharedPointer*)get_real_pointer(tp);
        if ((sp->ref_count--) == 1) {
            FREE((void*)sp->address);
            FREE(sp);
            tp->address = 0; // Reseta o ponteiro após desalocação
            tp->type = TYPE_NULL;
        }
    }
}

// Desalocação
static inline void dealloc(TaggedPointer* tp) {
    if (is_null(tp)) return;
    if (is_error(tp) && is_indirect(tp)) {
        Error* err = (Error*)tp->address;
        FREE(err->message);
        FREE(err);
    } else if (is_indirect(tp)) {
        FREE(get_real_pointer(tp));
    }
    tp->raw = 0;
}

// Exemplo de Uso
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

    printf("Exemplo 1: Valor nas Tags \n");
    TaggedPointer tp1 = create(0, 42, TYPE_INT);
    printf("create: "); INFO(&tp1);
    set_value(&tp1, &(int){100}, sizeof(int), TYPE_INT);
    printf("setValue: "); INFO(&tp1);
    dealloc(&tp1);
    printf("dealloc: "); INFO(&tp1);

    printf("Exemplo 2: Valor no Heap\n");
    float f = 3.14;
    TaggedPointer tp2 = create(sizeof(float), 2, TYPE_FLOAT);
    printf("create: "); INFO(&tp2);
    set_value(&tp2, &f, sizeof(float), TYPE_FLOAT);
    printf("setValue: "); INFO(&tp2);
    dealloc(&tp2);
    printf("dealloc: "); INFO(&tp2);

    printf("Exemplo 3: Erro\n");
    TaggedPointer tp3 = create(0, 0, TYPE_NULL);
    printf("create: "); INFO(&tp3);
    set_error(&tp3, 404, "Not Found");
    printf("set_error: "); INFO(&tp3);
    dealloc(&tp3);
    printf("dealloc: "); INFO(&tp3);

    printf("Exemplo 4: Array\n");
    int arr[] = {1, 2, 3};
    TaggedPointer tp4 = create(sizeof(int) * 3, 3, TYPE_ARRAY);
    printf("create: "); INFO(&tp4);
    set_value(&tp4, arr, sizeof(int) * 3, TYPE_ARRAY);
    printf("setValue: "); INFO(&tp4);
    printf("Array: [%d, %d, %d]\n", ((int*)get_real_pointer(&tp4))[0], ((int*)get_real_pointer(&tp4))[1], ((int*)get_real_pointer(&tp4))[2]);
    dealloc(&tp4);
    printf("dealloc: "); INFO(&tp4);

    printf("Exemplo 5: SharedPointer com ref_count\n");
    TaggedPointer tp5 = create(sizeof(int), 0, TYPE_CUSTOM);
    printf("create: "); INFO(&tp5);
    int value = 42;
    set_value(&tp5, &value, sizeof(int), TYPE_CUSTOM);
    printf("setValue: "); INFO(&tp5);
    ref(&tp5);
    printf("ref: "); INFO(&tp5);
    deref(&tp5);
    printf("deref: "); INFO(&tp5);
    deref(&tp5);
    printf("deref: "); INFO(&tp5);

    printf("Exemplo 6: String\n");
    char str[] = "Hello World!";
    TaggedPointer tp6 = create(sizeof(str), 0, TYPE_CHAR);
    printf("create: "); INFO(&tp6);
    set_value(&tp6, str, sizeof(str), TYPE_CHAR);
    printf("setValue: "); INFO(&tp6);
    printf("String: %s\n", (char*)get_real_pointer(&tp6));
    dealloc(&tp6);
    printf("dealloc: "); INFO(&tp6);

    return 0;
}