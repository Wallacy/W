#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef ADDRESS_BITS
#define ADDRESS_BITS 48
#endif

#define TYPE_BITS 2
#define SUBTYPE_BITS 2
#define VALUE_BITS (sizeof(uintptr_t) * 8 - TYPE_BITS)
#define TAG_VALUE_BITS (sizeof(uintptr_t) * 8 - ADDRESS_BITS - SUBTYPE_BITS - TYPE_BITS)
#define TAG_BITS (sizeof(uintptr_t) * 8 - ADDRESS_BITS - TYPE_BITS)

#define TYPE_MASK 0x3ULL
#define VALUE_MASK (((1ULL << VALUE_BITS) - 1) << TYPE_BITS)
#define ADDRESS_MASK (((1ULL << ADDRESS_BITS) - 1) << TYPE_BITS)

#define TYPE_INT 0x0
#define TYPE_FLOAT 0x1
#define TYPE_COMPOUND 0x2
#define TYPE_SHARED 0x3

#define SUBTYPE_STRING 0x0
#define SUBTYPE_ARRAY 0x1
#define SUBTYPE_ENUM 0x2
#define SUBTYPE_CUSTOM 0x3

// Valores especiais
#define MAX_VALUE ((uintptr_t)((1ULL << (VALUE_BITS - 1)) | 1ULL))
#define MIN_VALUE ((uintptr_t)((1ULL << (VALUE_BITS - 1)) - 1))

#define NULL_VALUE MAX_VALUE
#define ERRO_VALUE MIN_VALUE

#if UINTPTR_MAX == 0xffffffffffffffffULL
#define FLOAT_T double
#else
#define FLOAT_T float
#endif

#define red "\e[91m"
#define grn "\e[92m"
#define blu "\e[94m"
#define DEF "\e[0m"

// Flags globais para controle em tempo de execução

#if defined(NO_ATOMIC)
#define IS_ATOMIC 0
#else
static int IS_ATOMIC = 1;
#endif

#if defined(NO_OVERFLOW_CHECK)
#define CHECK_OVERFLOW 0
#else
static int CHECK_OVERFLOW = 1;
#endif


typedef union TaggedPointer {
    _Atomic uintptr_t raw;
    struct {
        uintptr_t value : VALUE_BITS;
        uintptr_t type : TYPE_BITS;
    } scalar;
    struct {
        uintptr_t address : ADDRESS_BITS;
        uintptr_t tags : TAG_VALUE_BITS;
        uintptr_t subtype : SUBTYPE_BITS;
        uintptr_t type : TYPE_BITS;
    } compound;
    struct {
        uintptr_t address : ADDRESS_BITS;
        uintptr_t tags : TAG_BITS;
        uintptr_t type : TYPE_BITS;
    } custom;
} TaggedPointer;

static inline uintptr_t get_raw(const TaggedPointer* tp) {
    return IS_ATOMIC ? atomic_load(&tp->raw) : tp->raw;
}

static inline void set_raw(TaggedPointer* tp, uintptr_t raw) {
    if (IS_ATOMIC) atomic_store(&tp->raw, raw);
    else tp->raw = raw;
}

static inline void set_int(TaggedPointer* tp, intptr_t value) {
    printf("set_int(%ld):\n", value);
    set_raw(tp, ((uintptr_t)value << TYPE_BITS) | TYPE_INT);
}

static inline void set_float(TaggedPointer* tp, FLOAT_T fvalue) {
    printf("set_float(%f):\n", fvalue);
    uintptr_t raw =  (*(uintptr_t*)&fvalue) | TYPE_FLOAT;
    set_raw(tp, raw | TYPE_FLOAT);
}

static inline void set_int_null(TaggedPointer* tp) {
    printf("set_int_null():\n");
    set_raw(tp, (NULL_VALUE << TYPE_BITS) | TYPE_INT);
}

static inline void set_int_erro(TaggedPointer* tp) {
    printf("set_int_erro():\n");
    set_raw(tp, (ERRO_VALUE << TYPE_BITS) | TYPE_INT);
}

static inline void set_float_null(TaggedPointer* tp) {
    printf("set_float_null():\n");
    set_raw(tp, (NULL_VALUE << TYPE_BITS) | TYPE_FLOAT);
}

static inline void set_float_erro(TaggedPointer* tp) {
    printf("set_float_erro():\n");
    set_raw(tp, (ERRO_VALUE << TYPE_BITS) | TYPE_FLOAT);
}

static inline intptr_t sign_extend(uintptr_t uvalue) {
    return (intptr_t)(uvalue << TYPE_BITS) >> TYPE_BITS;
}

static inline uintptr_t mantissa_extend(uintptr_t raw) {
    return (raw >> TYPE_BITS) << TYPE_BITS;
}


static inline void print_bits(void* x) {
    uintptr_t bits = (uintptr_t)x;
    int total_bits = sizeof(uintptr_t) * 8;
    for (int i = total_bits - 1; i >= 0; i--) {
        uint8_t b = (bits >> i) & 1;
        b ? printf(grn) : printf(DEF);
        printf("%u", b);
        if (i == total_bits - 1 || (total_bits == 64 && i == 52) || i == 2) printf(" ");
    }
    printf(DEF);
}

static inline void print_tp(TaggedPointer* tp) {
    uintptr_t raw = get_raw(tp);
    uint8_t type = raw & TYPE_MASK;
    printf("raw: 0x%016lx, ", raw);
    if (type == TYPE_INT) {
        uintptr_t uvalue = raw >> TYPE_BITS;
        if (uvalue == NULL_VALUE) {
            printf("type: int, value: "blu"NULL"DEF"\n");
        } else if (uvalue == ERRO_VALUE) {
            printf("type: int, value: "red"ERRO"DEF"\n");
        } else {
            intptr_t ivalue = sign_extend(uvalue);
            printf("type: int, value: %ld\n", ivalue);
        }
    } else if (type == TYPE_FLOAT) {
        uintptr_t uvalue = mantissa_extend(raw);
        FLOAT_T fvalue = *(FLOAT_T*)&uvalue; \
        if (uvalue == (NULL_VALUE << TYPE_BITS)) printf("type: float, value: "blu"NULL"DEF"\n");
        else if (uvalue == (ERRO_VALUE << TYPE_BITS)) printf("type: float, value: "red"ERRO"DEF"\n");
        else printf("type: float, value: %.2f\n", fvalue);
    }
    #if(PRINT_BITS == 1)
        printf("%d bits: ", (int)(sizeof(uintptr_t) * 8));
        print_bits(&raw);
        printf("\n");
    #endif

}

#define OPERATION_INT(op_name, op, builtin_op) \
static inline void op_name##_int(TaggedPointer* tp, intptr_t operand) { \
    printf(#op_name"_int(%ld):\n", operand); \
    if (IS_ATOMIC) { \
        uintptr_t old_raw, new_raw; \
        do { \
            old_raw = get_raw(tp); \
            if ((old_raw & TYPE_MASK) != TYPE_INT) return; \
            uintptr_t uvalue = old_raw >> TYPE_BITS; \
            if (uvalue == NULL_VALUE || uvalue == ERRO_VALUE) return; \
            intptr_t value = sign_extend(uvalue); \
            intptr_t new_value; \
            if (CHECK_OVERFLOW && builtin_op(value, operand, &new_value)) { \
                set_int_erro(tp); return; \
            } else { \
                new_value = value op operand; \
            } \
            new_raw = ((uintptr_t)new_value << TYPE_BITS) | TYPE_INT; \
        } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw)); \
    } else { \
        uintptr_t raw = get_raw(tp); \
        if ((raw & TYPE_MASK) != TYPE_INT) return; \
        uintptr_t uvalue = raw >> TYPE_BITS; \
        if (uvalue == NULL_VALUE || uvalue == ERRO_VALUE) return; \
        intptr_t value = sign_extend(uvalue); \
        intptr_t new_value; \
        if (CHECK_OVERFLOW && builtin_op(value, operand, &new_value)) { \
            set_int_erro(tp); return; \
        } else { \
            new_value = value op operand; \
        } \
        set_raw(tp, ((uintptr_t)new_value << TYPE_BITS) | TYPE_INT); \
    } \
}

#define OPERATION_FLOAT(op_name, op) \
static inline void op_name##_float(TaggedPointer* tp, FLOAT_T operand) { \
    printf(#op_name"_float(%lf):\n", operand); \
    if (IS_ATOMIC) { \
        uintptr_t old_raw, new_raw; \
        do { \
            old_raw = get_raw(tp); \
            if ((old_raw & TYPE_MASK) != TYPE_FLOAT) return; \
            uintptr_t value = mantissa_extend(old_raw); \
            if (value == (NULL_VALUE << TYPE_BITS) || value == (ERRO_VALUE << TYPE_BITS)) return; \
            FLOAT_T fvalue = *(FLOAT_T*)&value; \
            FLOAT_T new_fvalue = fvalue op operand; \
            uintptr_t new_value = *(uintptr_t*)&new_fvalue; \
            new_raw = new_value | TYPE_FLOAT; \
        } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw)); \
    } else { \
        uintptr_t raw = get_raw(tp); \
        if ((raw & TYPE_MASK) != TYPE_FLOAT) return; \
        uintptr_t value = mantissa_extend(raw); \
        if (value == (NULL_VALUE << TYPE_BITS) || value == (ERRO_VALUE << TYPE_BITS)) return; \
        FLOAT_T fvalue = *(FLOAT_T*)&value; \
        fvalue op##= operand; \
        tp->raw = (*(uintptr_t*)&fvalue) | TYPE_FLOAT; \
    } \
}

static inline int div_overflow_check(intptr_t a, intptr_t b, intptr_t* res) {
    if (b == 0 || (a == INTPTR_MIN && b == -1)) return 1;
    *res = a / b;
    return 0;
}

static inline int mod_overflow_check(intptr_t a, intptr_t b, intptr_t* res) {
    if (b == 0) return 1;
    *res = a % b;
    return 0;
}

OPERATION_INT(add, +, __builtin_add_overflow)
OPERATION_INT(sub, -, __builtin_sub_overflow)
OPERATION_INT(mul, *, __builtin_mul_overflow)
OPERATION_INT(div, /, div_overflow_check)
OPERATION_INT(mod, %, mod_overflow_check)

OPERATION_FLOAT(add, +)
OPERATION_FLOAT(sub, -)
OPERATION_FLOAT(mul, *)
OPERATION_FLOAT(div, /)

static inline void inc_int(TaggedPointer* tp) { add_int(tp, 1); }
static inline void dec_int(TaggedPointer* tp) { sub_int(tp, 1); }
static inline void inc_float(TaggedPointer* tp) { add_float(tp, 1.0); }
static inline void dec_float(TaggedPointer* tp) { sub_float(tp, 1.0); }

void run_tests(const char* variant, int atomic_flag, int overflow_flag) {
    #if !defined(NO_ATOMIC)
    IS_ATOMIC = atomic_flag;
    #endif
    #if !defined(NO_OVERFLOW_CHECK)
    CHECK_OVERFLOW = overflow_flag;
    #endif

    TaggedPointer tp = {0};

    printf("\n=== %s ===\n", variant);

    printf("\n--- Testes com Inteiros ---\n");
    set_int(&tp, 5); print_tp(&tp);
    add_int(&tp, 3); print_tp(&tp);
    sub_int(&tp, 2); print_tp(&tp);
    mul_int(&tp, 4); print_tp(&tp);
    div_int(&tp, 2); print_tp(&tp);
    inc_int(&tp); print_tp(&tp);
    dec_int(&tp); print_tp(&tp);
    set_int(&tp, -5); print_tp(&tp);
    add_int(&tp, 2); print_tp(&tp);
    sub_int(&tp, -15); print_tp(&tp);
    add_int(&tp, INT64_MAX); print_tp(&tp);
    sub_int(&tp, INT32_MAX); print_tp(&tp);
    set_int_null(&tp); print_tp(&tp);
    set_int_erro(&tp); print_tp(&tp);

    printf("\n--- Testes com Floats ---\n");
    set_float(&tp, 1.5); print_tp(&tp);
    add_float(&tp, 2.5); print_tp(&tp);
    sub_float(&tp, 1.0); print_tp(&tp);
    mul_float(&tp, 2.0); print_tp(&tp);
    div_float(&tp, 2.0); print_tp(&tp);
    inc_float(&tp); print_tp(&tp);
    dec_float(&tp); print_tp(&tp);
    add_float(&tp, -15); print_tp(&tp);
    sub_float(&tp, -10); print_tp(&tp);
    set_float(&tp, (0.f / 0.f)); print_tp(&tp);
    sub_float(&tp, 10); print_tp(&tp);
    set_float(&tp, (1.f / 0.f)); print_tp(&tp);
    div_float(&tp, 2.0); print_tp(&tp);
    set_float_null(&tp); print_tp(&tp);
    set_float_erro(&tp); print_tp(&tp);
}

int main() {
    // Executa todas as variantes por padrão, a menos que desabilitadas
    #if !defined(NO_ATOMIC) && !defined(NO_OVERFLOW_CHECK)
    run_tests("Variante: IS_ATOMIC=1, CHECK_OVERFLOW=1", 1, 1);
    #endif
    #if !defined(NO_ATOMIC)
    run_tests("Variante: IS_ATOMIC=1, CHECK_OVERFLOW=0", 1, 0);
    #endif
    #if !defined(NO_OVERFLOW_CHECK)
    run_tests("Variante: IS_ATOMIC=0, CHECK_OVERFLOW=1", 0, 1);
    #endif
    run_tests("Variante: IS_ATOMIC=0, CHECK_OVERFLOW=0", 0, 0);
    return 0;
}