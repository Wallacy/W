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
typedef unsigned int uint;

// ***** Definições de SharedPointer ***** //
typedef struct {
    _Atomic uint ref_count;
    _Atomic uint tags;
    uintptr_t address;
} SharedPointer;

bool lam_uai_supported = false;
#if CHECK_CPU
#include <cpuid.h>
#endif
int main() {
#if defined(__x86_64__) || defined(__aarch64__)
#if CHECK_CPU

    // lam_uai_supported = has_lam_uai();
    {
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
            lam_uai_supported = (ecx & (1 << 26)) != 0;
        } else if (strncmp(vendor, "AuthenticAMD", 12) == 0) {
            __cpuid(0x80000000, eax, ebx, ecx, edx);
            if (eax >= 0x80000008) {
                __cpuid(0x80000008, eax, ebx, ecx, edx);
                lam_uai_supported = (ecx & (1 << 31)) != 0;
            }
        } else {
            lam_uai_supported = false;
        }
    }

    // int virtual_bits = get_virtual_address_bits();
    int virtual_bits;
    {
        unsigned int eax, ebx, ecx, edx;
        __cpuid(0x80000008, eax, ebx, ecx, edx);
        virtual_bits = (eax >> 8) & 0xFF;
    }
    printf("Número de bits de endereço virtual detectado: %d | Usando: %d\n", virtual_bits, ADDRESS_BITS);

    // printf("Suporta paginação %s\n", has_page5() ? "de 5 níveis (57 bits)" : "4 níveis (48 bits)");
    {
        unsigned int eax, ebx, ecx, edx;
        __cpuid(7, eax, ebx, ecx, edx);
        bool page5 = !!(ecx & (1 << 16));
        printf("Suporta paginação %s\n", page5 ? "de 5 níveis (57 bits)" : "4 níveis (48 bits)");
    }

    printf("Suporte a LAM/UAI: %s\n", lam_uai_supported ? "Sim" : "Não");
#endif
#endif

    uintptr_t tp = 0;

    // Exemplo 1: Inteiro Simples
    printf("\nExemplo 1 - Inteiro Simples:\n");
    // tp = create(sizeof(int), 113, false, false);
    {
    void* ptr = CALLOC(sizeof(int));
        printf("Create: 0x%016lx \n", (uintptr_t)ptr);
        if (!ptr) tp = 0;
        else {
            tp = (uintptr_t)ptr;
#if defined(__x86_64__) || defined(__aarch64__)
            tp |= ((uintptr_t)113 << ADDRESS_BITS);
#endif
            if (false) tp |= (1ULL << 62);
            if (false) tp |= (1ULL << 63);
            // printf("TaggedPointer: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", tp, *(int*)get_value(tp), get_tags(tp), get_shared(tp), get_null(tp));
            printf("TaggedPointer: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
                (uintptr_t)tp, 
                (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
                (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
                (tp >> 62) & 1, 
                (tp >> 63) & 1);
        }
    }

    int value_int = 42;
    // set_value(&tp, &value_int, sizeof(int));
    if (!((tp >> 63) & 1)) {
        memcpy((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))), &value_int, sizeof(int));
    }

    // INFO("SetValue: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n",tp,*(int*));
    printf("SetValue: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // dealloc(&tp);
    if (!((tp >> 63) & 1)) {
        if (!((tp >> 62) & 1)) {
            FREE((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))));
            if (true) tp |= (1ULL << 63);
            else tp &= ~(1ULL << 63);
        } else {
            SharedPointer* sp = (SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
            if ((sp->ref_count--) == 1) {
                FREE((void*)sp->address);
                FREE(sp);
                if (true) tp |= (1ULL << 63);
                else tp &= ~(1ULL << 63);
            }
        }
    }
    // INFO("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n",tp,*(int*));
    printf("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // Exemplo 2: String com Realloc
    printf("\nExemplo 2 - String com Realloc:\n");
    // tp = create(13, 149, false, false);
    {
        void* ptr = CALLOC(13);
        printf("Create: 0x%016lx \n", (uintptr_t)ptr);
        if (!ptr) tp = 0;
        else {
            tp = (uintptr_t)ptr;
#if defined(__x86_64__) || defined(__aarch64__)
            tp |= ((uintptr_t)149 << ADDRESS_BITS);
#endif
            if (false) tp |= (1ULL << 62);
            if (false) tp |= (1ULL << 63);
            printf("TaggedPointer: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
                (uintptr_t)tp, 
                (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
                (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
                (tp >> 62) & 1, 
                (tp >> 63) & 1);
        }
    }

    strcpy((char*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))), "Hello World!");
    // INFO("strcpy: 0x%016lx, Valor: %s, Tags: %lu, isShared: %lu, isNULL: %lu \n",tp,(char*));
    printf("strcpy: 0x%016lx, Valor: %s, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (char*)(((tp >> 63) & 1) ? (uintptr_t)NULL : ((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // update_tagged_pointer(&tp, 151, false, false, 21);
    {
        if (21 > 0) {
            void* new_ptr = REALLOC((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))), 21);
            if (!new_ptr) goto skip_update_2;
            tp = (uintptr_t)new_ptr;
        }
#if defined(__x86_64__) || defined(__aarch64__)
        tp = (tp & ~(0xFFFFULL << ADDRESS_BITS)) | ((uintptr_t)151 << ADDRESS_BITS);
#endif
        if (false) tp |= (1ULL << 62);
        if (false) tp |= (1ULL << 63);
    }
    skip_update_2:

    strcpy((char*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))), "Hello TaggedPointer!");
    // INFO("Update: 0x%016lx, Valor: %s, Tags: %lu, isShared: %lu, isNULL: %lu \n",tp,(char*));
    printf("Update: 0x%016lx, Valor: %s, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (char*)(((tp >> 63) & 1) ? (uintptr_t)NULL : ((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // dealloc(&tp);
    if (!((tp >> 63) & 1)) {
        if (!((tp >> 62) & 1)) {
            FREE((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))));
            if (true) tp |= (1ULL << 63);
            else tp &= ~(1ULL << 63);
        } else {
            SharedPointer* sp = (SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
            if ((sp->ref_count--) == 1) {
                FREE((void*)sp->address);
                FREE(sp);
                if (true) tp |= (1ULL << 63);
                else tp &= ~(1ULL << 63);
            }
        }
    }

    // INFO("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n",tp,*(char*));
    printf("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL :  *((char*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // Exemplo 3: Promoção Automática
    printf("\nExemplo 3 - Promoção Automática:\n");
    // tp = create(sizeof(int), MAX_TAGS-14, false, false);
    {
    void* ptr = CALLOC(sizeof(int));
        printf("Create: 0x%016lx \n", (uintptr_t)ptr);
        if (!ptr) tp = 0;
        else {
            tp = (uintptr_t)ptr;
#if defined(__x86_64__) || defined(__aarch64__)
            tp |= ((uintptr_t)(((1U << (62 - ADDRESS_BITS)) - 1) - 14) << ADDRESS_BITS);
#endif
            if (false) tp |= (1ULL << 62);
            if (false) tp |= (1ULL << 63);
            printf("TaggedPointer: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
                (uintptr_t)tp, 
                (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
                (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
                (tp >> 62) & 1, 
                (tp >> 63) & 1);
        }
    }

    int value_auto = 100;
    // set_value(&tp, &value_auto, sizeof(int));
    if (!((tp >> 63) & 1)) {
        memcpy((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))), &value_auto, sizeof(int));
    }

    // INFO("SetValue: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", tp, *(int*));
    printf("SetValue: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // for (int i = 0; i < 15; i++) ref(&tp);
    for (int i = 0; i < 15; i++) {
        if (!((tp >> 62) & 1) && !((tp >> 63) & 1)) {
#if defined(__x86_64__) || defined(__aarch64__)
            uint16_t tags = (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1));
            const uint16_t max_tags = ((1U << (62 - ADDRESS_BITS)) - 1);
            if (tags < max_tags) {
                if (((tp >> 62) & 1)) {
                    ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags = tags + 1;
                } else {
#if defined(__x86_64__) || defined(__aarch64__)
                    tp = (tp & ~(0xFFFFULL << ADDRESS_BITS)) | ((uintptr_t)(tags + 1) << ADDRESS_BITS);
#endif
                }
            } else {
                // promote_to_shared(&tp);
                SharedPointer* sp;
                sp = MALLOC(sizeof(SharedPointer));
                sp->address = tp & ((1ULL << ADDRESS_BITS) - 1);
                sp->ref_count = (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)) ? (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)) : 1;
                sp->tags = (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1));
                tp = (uintptr_t)sp;
#if defined(__x86_64__) || defined(__aarch64__)
                tp |= ((1ULL << (62 - ADDRESS_BITS)) - 1) << ADDRESS_BITS;
#endif
                tp |= (1ULL << 62);
            }
#else
            SharedPointer* sp;
            sp = MALLOC(sizeof(SharedPointer));
            sp->address = tp & ((1ULL << ADDRESS_BITS) - 1);
            sp->ref_count = (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)) ? (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)) : 1;
            sp->tags = (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1));
            tp = (uintptr_t)sp;
#if defined(__x86_64__) || defined(__aarch64__)
            tp |= ((1ULL << (62 - ADDRESS_BITS)) - 1) << ADDRESS_BITS;
#endif
            tp |= (1ULL << 62);
#endif
        } else if (((tp >> 62) & 1) && !((tp >> 63) & 1)) {
            SharedPointer* sp = (SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
            sp->ref_count++;
        }
    }

    // INFO("Após 15 refs: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", tp, *(int*));
    printf("Após 15 refs: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // if (get_shared(tp)) { INFO_SP("Promovido - ref_count: %u, tags: %u, address: 0x%016lx\n",sp); }
    if (((tp >> 62) & 1)) {
        SharedPointer* sp = (SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
        printf("Promovido - ref_count: %u, tags: %u, address: 0x%016lx\n", sp->ref_count, sp->tags, sp->address);
    }

    // for (int i = 0; i < 15; i++) dealloc(&tp); // Na verdade ele vai só fazer 1 vez pois ainda não ta correto.
    for (int i = 0; i < 15; i++) {
        if (!((tp >> 63) & 1)) {
            if (!((tp >> 62) & 1)) {
                FREE((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))));
                if (true) tp |= (1ULL << 63);
                else tp &= ~(1ULL << 63);
            } else {
                SharedPointer* sp = (SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
                if ((sp->ref_count--) == 1) {
                    FREE((void*)sp->address);
                    FREE(sp);
                    if (true) tp |= (1ULL << 63);
                    else tp &= ~(1ULL << 63);
                }
            }
        }
    }

    // INFO("Após 15 deallocs: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", tp, *(int*));
    printf("Após 15 deallocs: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1); 
    // Espero leak aqui até fazer o ajuste da promoção.

    // Exemplo 4: Promoção Forçada
    printf("\nExemplo 4 - Promoção Forçada:\n");
    // tp = create(sizeof(int), 0, false, false);
    {
    void* ptr = CALLOC(sizeof(int));
        printf("Create: 0x%016lx \n", (uintptr_t)ptr);
        if (!ptr) tp = 0;
        else {
            tp = (uintptr_t)ptr;
#if defined(__x86_64__) || defined(__aarch64__)
            tp |= ((uintptr_t)0 << ADDRESS_BITS);
#endif
            if (false) tp |= (1ULL << 62);
            if (false) tp |= (1ULL << 63);
            printf("TaggedPointer: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
                (uintptr_t)tp, 
                (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
                (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
                (tp >> 62) & 1, 
                (tp >> 63) & 1);
        }
    }

    int value_force = 200;
    // set_value(&tp, &value_force, sizeof(int));
    if (!((tp >> 63) & 1)) {
        memcpy((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))), &value_force, sizeof(int));
    }

    // INFO("Inicial: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", tp, *(int*));
    printf("Inicial: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // set_shared(&tp, true);
    if (true && !((tp >> 62) & 1) && !((tp >> 63) & 1)) {
        if ((((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)) != 0) {
            // promote_to_shared(&tp);
            SharedPointer* sp;
            sp = MALLOC(sizeof(SharedPointer));
            sp->address = tp & ((1ULL << ADDRESS_BITS) - 1);
            sp->ref_count = (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)) ? (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)) : 1;
            sp->tags = (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1));
            tp = (uintptr_t)sp;
#if defined(__x86_64__) || defined(__aarch64__)
            tp |= ((1ULL << (62 - ADDRESS_BITS)) - 1) << ADDRESS_BITS;
#endif
            tp |= (1ULL << 62);
        } else {
            tp |= (1ULL << 62);
        }
    } else {
        if (true) tp |= (1ULL << 62);
        else tp &= ~(1ULL << 62);
    }

    // INFO("SetShared: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", tp, *(int*));
    printf("SetShared: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    SharedPointer* sp_force = (SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
    // INFO_SP("Promovido - ref_count: %u, tags: %u, address: 0x%016lx\n",sp_force);
    printf("Promovido - ref_count: %u, tags: %u, address: 0x%016lx\n", sp_force->ref_count, sp_force->tags, sp_force->address);

    // dealloc(&tp);
    if (!((tp >> 63) & 1)) {
        if (!((tp >> 62) & 1)) {
            FREE((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))));
            if (true) tp |= (1ULL << 63);
            else tp &= ~(1ULL << 63);
        } else {
            SharedPointer* sp = (SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
            if ((sp->ref_count--) == 1) {
                FREE((void*)sp->address);
                FREE(sp);
                if (true) tp |= (1ULL << 63);
                else tp &= ~(1ULL << 63);
            }
        }
    }

    // INFO("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n",tp,*(int*));
    printf("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1); 
    // Espero leak aqui até fazer o ajuste da promoção.

    // Exemplo 5: Array de Inteiros com Realloc
    printf("\nExemplo 5 - Array de Inteiros com Realloc:\n");
    // tp = create(sizeof(int) * 3, 200, false, false);
    {
        void* ptr = CALLOC(sizeof(int) * 3);
        printf("Create: 0x%016lx \n", (uintptr_t)ptr);
        if (!ptr) tp = 0;
        else {
            tp = (uintptr_t)ptr;
#if defined(__x86_64__) || defined(__aarch64__)
            tp |= ((uintptr_t)200 << ADDRESS_BITS);
#endif
            if (false) tp |= (1ULL << 62);
            if (false) tp |= (1ULL << 63);
            printf("TaggedPointer: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
                (uintptr_t)tp, 
                (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
                (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
                (tp >> 62) & 1, 
                (tp >> 63) & 1);
        }
    }

    int* array = (int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
    for (int i = 0; i < 3; i++) array[i] = i * 10;

    // INFO("Inicial: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n",tp,*(int*));
    printf("Inicial: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    printf("Antes do realloc: [");
    for (int i = 0; i < 3; i++) printf("%d%s", array[i], i < 2 ? ", " : "");
    printf("]\n");

    // update_tagged_pointer(&tp, 200, false, false, sizeof(int) * 5);
    {
        if (sizeof(int) * 5 > 0) {
            void* new_ptr = REALLOC((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))), sizeof(int) * 5);
            if (!new_ptr) goto skip_update_5;
            tp = (uintptr_t)new_ptr;
        }
#if defined(__x86_64__) || defined(__aarch64__)
        tp = (tp & ~(0xFFFFULL << ADDRESS_BITS)) | ((uintptr_t)200 << ADDRESS_BITS);
#endif
        if (false) tp |= (1ULL << 62);
        if (false) tp |= (1ULL << 63);
    }
    skip_update_5:

    array = (int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
    for (int i = 3; i < 5; i++) array[i] = i * 10;

    printf("Após realloc: [");
    for (int i = 0; i < 5; i++) printf("%d%s", array[i], i < 4 ? ", " : "");
    printf("]\n");

    // dealloc(&tp);
    if (!((tp >> 63) & 1)) {
        if (!((tp >> 62) & 1)) {
            FREE((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))));
            if (true) tp |= (1ULL << 63);
            else tp &= ~(1ULL << 63);
        } else {
            SharedPointer* sp = (SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
            if ((sp->ref_count--) == 1) {
                FREE((void*)sp->address);
                FREE(sp);
                if (true) tp |= (1ULL << 63);
                else tp &= ~(1ULL << 63);
            }
        }
    }

    // INFO("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n",tp,*(int*));
    printf("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // Exemplo 6: Tags Dinâmicos
    printf("\nExemplo 6 - Tags Dinâmicos:\n");
    // tp = create(sizeof(int), 5, false, false);
    {
    void* ptr = CALLOC(sizeof(int));
        printf("Create: 0x%016lx \n", (uintptr_t)ptr);
        if (!ptr) tp = 0;
        else {
            tp = (uintptr_t)ptr;
#if defined(__x86_64__) || defined(__aarch64__)
            tp |= ((uintptr_t)5 << ADDRESS_BITS);
#endif
            if (false) tp |= (1ULL << 62);
            if (false) tp |= (1ULL << 63);
            printf("TaggedPointer: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
                (uintptr_t)tp, 
                (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
                (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
                (tp >> 62) & 1, 
                (tp >> 63) & 1);
        }
    }

    int value_tags = 500;
    // set_value(&tp, &value_tags, sizeof(int));
    if (!((tp >> 63) & 1)) {
        memcpy((void *)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))), &value_tags, sizeof(int));
    }

    // INFO("Inicial: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", tp, *(int*));
    printf("Inicial: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // set_tags(&tp, 10);
    if (((tp >> 62) & 1)) {
        ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags = 10;
    } else {
#if defined(__x86_64__) || defined(__aarch64__)
        tp = (tp & ~(0xFFFFULL << ADDRESS_BITS)) | ((uintptr_t)10 << ADDRESS_BITS);
#endif
    }

    // INFO("SetTags: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", tp, *(int*));
    printf("SetTags: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // dealloc(&tp);
    if (!((tp >> 63) & 1)) {
        if (!((tp >> 62) & 1)) {
            FREE((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))));
            if (true) tp |= (1ULL << 63);
            else tp &= ~(1ULL << 63);
        } else {
            SharedPointer* sp = (SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
            if ((sp->ref_count--) == 1) {
                FREE((void*)sp->address);
                FREE(sp);
                if (true) tp |= (1ULL << 63);
                else tp &= ~(1ULL << 63);
            }
        }
    }

    // INFO("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n",tp,*(int*));
    printf("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // Exemplo 7: Associando Ponteiro com set_pointer
    printf("\nExemplo 7 - Associando Ponteiro com set_pointer:\n");
    char* str;
    str = MALLOC(20);
    strcpy(str, "Direct Pointer");

    // tp = create(0, 99, false, false);
    {
        void* ptr = CALLOC(0);
        printf("Create: 0x%016lx \n", (uintptr_t)ptr);
        if (!ptr) tp = 0;
        else {
            tp = (uintptr_t)ptr;
#if defined(__x86_64__) || defined(__aarch64__)
            tp |= ((uintptr_t)99 << ADDRESS_BITS);
#endif
            if (false) tp |= (1ULL << 62);
            if (false) tp |= (1ULL << 63);
            printf("TaggedPointer: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
                (uintptr_t)tp, 
                (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
                (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
                (tp >> 62) & 1, 
                (tp >> 63) & 1);
        }
    }

    // INFO("Inicial: 0x%016lx, Valor: %s, Tags: %lu, isShared: %lu, isNULL: %lu \n", tp, (char*));
    printf("Inicial: 0x%016lx, Valor: %s, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (char*)(((tp >> 63) & 1) ? (uintptr_t)NULL : ((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // set_pointer(&tp, str);
    if (!((tp >> 63) & 1)) {
        tp = (uintptr_t)str;
    }

    // INFO("SetPointer: 0x%016lx, Valor: %s, Tags: %lu, isShared: %lu, isNULL: %lu \n", tp, (char*));
    printf("SetPointer: 0x%016lx, Valor: %s, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (char*)(((tp >> 63) & 1) ? (uintptr_t)NULL : ((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    // dealloc(&tp);
    if (!((tp >> 63) & 1)) {
        if (!((tp >> 62) & 1)) {
            FREE((void*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))));
            if (true) tp |= (1ULL << 63);
            else tp &= ~(1ULL << 63);
        } else {
            SharedPointer* sp = (SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF)));
            if ((sp->ref_count--) == 1) {
                FREE((void*)sp->address);
                FREE(sp);
                if (true) tp |= (1ULL << 63);
                else tp &= ~(1ULL << 63);
            }
        }
    }

    // INFO("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n",tp,*(int*));
    printf("Final: 0x%016lx, Valor: %lu, Tags: %lu, isShared: %lu, isNULL: %lu \n", 
        (uintptr_t)tp, 
        (((tp >> 63) & 1) ? (uintptr_t)NULL : *((int*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))), 
        (((tp >> 62) & 1) ? ((SharedPointer*)((lam_uai_supported) ? tp : ((tp & (1ULL << 47)) ? (tp | 0xFFFF000000000000) : (tp & 0x0000FFFFFFFFFFFF))))->tags : (tp >> ADDRESS_BITS) & ((1U << (62 - ADDRESS_BITS)) - 1)),
        (tp >> 62) & 1, 
        (tp >> 63) & 1);

    return 0;
}