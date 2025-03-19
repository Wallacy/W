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

#define NULL_VALUE ((uintptr_t)((1ULL << (VALUE_BITS - 1)) | 1ULL))
#define ERRO_VALUE ((uintptr_t)((1ULL << (VALUE_BITS - 1)) - 1))

#if UINTPTR_MAX == 0xffffffffffffffffULL
#define FLOAT_T double
#else
#define FLOAT_T float
#endif

#define red "\e[91m"
#define grn "\e[92m"
#define blu "\e[94m"
#define DEF "\e[0m"

#ifndef IS_ATOMIC
#define IS_ATOMIC 1
#endif

#ifndef CHECK_OVERFLOW
#define CHECK_OVERFLOW 1
#endif

typedef union TaggedPointer {
    _Atomic uintptr_t raw;
    struct {
        intptr_t value : VALUE_BITS;
        uint8_t type : 2;
    } scalar;
    struct {
        uintptr_t address : ADDRESS_BITS;
        uint16_t tags : TAG_VALUE_BITS;
        uint8_t subtype : SUBTYPE_BITS;
        uint8_t type : 2;
    } compound;
    struct {
        uintptr_t address : ADDRESS_BITS;
        uint16_t tags : TAG_BITS;
        uint8_t type : 2;
    } custom;
} TaggedPointer;

void print_bits(void* x) {
    uintptr_t bits;
    memcpy(&bits, x, sizeof(uintptr_t));
    int total_bits = sizeof(uintptr_t) * 8;
    for (int i = total_bits - 1; i >= 0; i--) {
        uint8_t b = (bits >> i) & 1;
        b ? printf(grn) : printf(DEF);
        printf("%u", b);
        if (i == total_bits - 1 || (total_bits == 64 && i == 52) || i == 2) printf("    ");
    }
    printf(DEF"\n");
}

static inline uintptr_t get_raw(const TaggedPointer* tp) {
    return IS_ATOMIC ? atomic_load(&tp->raw) : tp->raw;
}

static inline void set_raw(TaggedPointer* tp, uintptr_t raw) {
    if (IS_ATOMIC) atomic_store(&tp->raw, raw);
    else tp->raw = raw;
}

static inline void set_int(TaggedPointer* tp, intptr_t value) {
    set_raw(tp, ((uintptr_t)value << 2) | TYPE_INT);
}

static inline void set_float(TaggedPointer* tp, FLOAT_T value) {
    uintptr_t raw;
    memcpy(&raw, &value, sizeof(FLOAT_T));
    set_raw(tp, (raw | TYPE_FLOAT));
}

static inline void set_int_null(TaggedPointer* tp) {
    set_int(tp, (intptr_t)NULL_VALUE);
}

static inline void set_int_erro(TaggedPointer* tp) {
    set_int(tp, (intptr_t)ERRO_VALUE);
}

static inline void set_float_null(TaggedPointer* tp) {
    uintptr_t raw = (NULL_VALUE << 2) | TYPE_FLOAT;
    set_raw(tp, raw);
}

static inline void set_float_erro(TaggedPointer* tp) {
    uintptr_t raw = (ERRO_VALUE << 2) | TYPE_FLOAT;
    set_raw(tp, raw);
}

void print_tp(TaggedPointer* tp) {
    uintptr_t raw = get_raw(tp);
    uint8_t type = raw & TYPE_MASK;
    printf("raw: 0x%016lx, ", raw);
    if (type == TYPE_INT) {
        intptr_t ivalue = (raw  >> 2); \
        if (ivalue == NULL_VALUE) printf("type: int, value: "blu"NULL"DEF"\n");
        else if (ivalue == ERRO_VALUE) printf("type: int, value: "red"ERRO"DEF"\n");
        else printf("type: int, value: %ld\n", ivalue);
    } else if (type == TYPE_FLOAT) {
        uintptr_t uvalue = ((raw  >> 2) << 2);
        FLOAT_T fvalue;
        memcpy(&fvalue, &uvalue, sizeof(FLOAT_T));
        if (uvalue == NULL_VALUE << 2) printf("type: float, value: "blu"NULL"DEF"\n");
        else if (uvalue == ERRO_VALUE << 2) printf("type: float, value: "red"ERRO"DEF"\n");
        else printf("type: float, value: %.2f\n", fvalue);
    } else if (type == TYPE_COMPOUND) {
        const char* subtype_str = (tp->compound.subtype == SUBTYPE_STRING) ? "string" :
                                  (tp->compound.subtype == SUBTYPE_ARRAY) ? "array" :
                                  (tp->compound.subtype == SUBTYPE_ENUM) ? "enum" : "custom";
        printf("type: compound, subtype: %s, address: 0x%lx, tags: %d\n",
               subtype_str, tp->compound.address, tp->compound.tags);
    } else {
        printf("type: custom, address: 0x%lx, tags: %d\n",
               tp->custom.address, tp->custom.tags);
    }
    printf("%d bits: ", (int)(sizeof(uintptr_t) * 8));
    print_bits(&raw);
    printf("\n");
}

#define OPERATION_INT(op_name, op, builtin_op) \
static inline void op_name##_int(TaggedPointer* tp, intptr_t operand) { \
    if (IS_ATOMIC) { \
        uintptr_t old_raw, new_raw; \
        do { \
            old_raw = get_raw(tp); \
            if ((old_raw & TYPE_MASK) != TYPE_INT) return; \
            intptr_t value = (intptr_t)(old_raw >> 2); \
            if (value == NULL_VALUE || value == ERRO_VALUE) return; \
            intptr_t new_value; \
            if (CHECK_OVERFLOW && builtin_op(value, operand, &new_value)) { \
                set_int_erro(tp); return; \
            } else { \
                new_value = value op operand; \
            } \
            new_raw = (new_value << 2) | TYPE_INT; \
        } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw)); \
    } else { \
        uintptr_t raw = get_raw(tp); \
        if ((raw & TYPE_MASK) != TYPE_INT) return; \
        uintptr_t value = raw >> 2; \
        if (value == NULL_VALUE || value == ERRO_VALUE) return; \
        if (CHECK_OVERFLOW) { \
            intptr_t new_value; \
            if (builtin_op(value, operand, &new_value)) { \
                set_int_erro(tp); return; \
            } \
            set_int(tp, new_value); \
        } else { \
            set_int(tp, value op operand); \
        } \
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

#define OPERATION_FLOAT(op_name, op) \
static inline void op_name##_float(TaggedPointer* tp, FLOAT_T operand) { \
    if (IS_ATOMIC) { \
        uintptr_t old_raw, new_raw; \
        do { \
            old_raw = get_raw(tp); \
            if ((old_raw & TYPE_MASK) != TYPE_FLOAT) return; \
            uintptr_t value = ((old_raw >> 2) << 2); \
            if (value == NULL_VALUE || value == ERRO_VALUE) return; \
            FLOAT_T fvalue; \
            memcpy(&fvalue, &value, sizeof(FLOAT_T)); \
            FLOAT_T new_fvalue = fvalue op operand; \
            uintptr_t new_value; \
            memcpy(&new_value, &new_fvalue, sizeof(FLOAT_T)); \
            new_raw = new_value | TYPE_FLOAT; \
        } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw)); \
    } else { \
        uintptr_t raw = get_raw(tp); \
        if ((raw & TYPE_MASK) != TYPE_FLOAT) return; \
        uintptr_t value = ((raw >> 2) << 2); \
        FLOAT_T fvalue; \
        memcpy(&fvalue, &value, sizeof(FLOAT_T)); \
        if (value == NULL_VALUE || value == ERRO_VALUE) return; \
        fvalue op##= operand; \
        set_float(tp, fvalue); \
    } \
}

OPERATION_FLOAT(add, +)
OPERATION_FLOAT(sub, -)
OPERATION_FLOAT(mul, *)
OPERATION_FLOAT(div, /)

static inline void inc_int(TaggedPointer* tp) { add_int(tp, 1); }
static inline void dec_int(TaggedPointer* tp) { sub_int(tp, 1); }
static inline void inc_float(TaggedPointer* tp) { add_float(tp, 1.0); }
static inline void dec_float(TaggedPointer* tp) { sub_float(tp, 1.0); }

int main() {
    TaggedPointer tp = {0};
    printf("=== Testes com Inteiros ===\n");
    printf("Após set_int(5):\n"); set_int(&tp, 5); print_tp(&tp);
    printf("Após add_int(3):\n"); add_int(&tp, 3); print_tp(&tp);
    printf("Após sub_int(2):\n"); sub_int(&tp, 2); print_tp(&tp);
    printf("Após mul_int(4):\n"); mul_int(&tp, 4); print_tp(&tp);
    printf("Após div_int(2):\n"); div_int(&tp, 2); print_tp(&tp);
    printf("Após inc_int():\n"); inc_int(&tp); print_tp(&tp);
    printf("Após dec_int():\n"); dec_int(&tp); print_tp(&tp);
    printf("Após set_int_null():\n"); set_int_null(&tp); print_tp(&tp);
    printf("Após set_int_erro():\n"); set_int_erro(&tp); print_tp(&tp);

    printf("\n=== Testes com Floats ===\n");
    printf("Após set_float(1.5):\n"); set_float(&tp, 1.5); print_tp(&tp);
    printf("Após add_float(2.5):\n"); add_float(&tp, 2.5); print_tp(&tp);
    printf("Após sub_float(1.0):\n"); sub_float(&tp, 1.0); print_tp(&tp);
    printf("Após mul_float(2.0):\n"); mul_float(&tp, 2.0); print_tp(&tp);
    printf("Após div_float(2.0):\n"); div_float(&tp, 2.0); print_tp(&tp);
    printf("Após inc_float():\n"); inc_float(&tp); print_tp(&tp);
    printf("Após dec_float():\n"); dec_float(&tp); print_tp(&tp);
    printf("Após set_float_null():\n"); set_float_null(&tp); print_tp(&tp);
    printf("Após set_float_erro():\n"); set_float_erro(&tp); print_tp(&tp);
    return 0;
}