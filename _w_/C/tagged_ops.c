#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef ADDRESS_BITS
#define ADDRESS_BITS 48
#endif

#define TAG_MASK (((1ULL << TAG_BITS) - 1) << (ADDRESS_BITS + 2))
#define ADDRESS_MASK (((1ULL << ADDRESS_BITS) - 1) << 2)
#define VALUE_BITS (sizeof(uintptr_t) * 8 - 2) // 62 bits em 64-bit, 30 bits em 32-bit
#define TYPE_MASK 0x3ULL  // Bits 0 e 1
#define VALUE_MASK (((1ULL << VALUE_BITS) - 1) << 2)  // Bits 2 até o máximo

#define TAG_BITS (VALUE_BITS - ADDRESS_BITS)  // 14 bits se ADDRESS_BITS = 48

#define TYPE_INT 0x0
#define TYPE_FLOAT 0x1
#define TYPE_COMPOUND 0x2
#define TYPE_SHARED 0x3

// Valores especiais
#define NULL_VALUE ((uintptr_t)((1ULL << (VALUE_BITS - 1)) | 1ULL))
#define ERRO_VALUE ((uintptr_t)((1ULL << (VALUE_BITS - 1)) - 1))

// Define FLOAT_T baseado no tamanho de uintptr_t
#if UINTPTR_MAX == 0xffffffffffffffffULL
#define FLOAT_T double
#else
#define FLOAT_T float
#endif

#define red "\e[91m"
#define grn "\e[92m"
#define blu "\e[94m"
#define DEF "\e[0m"

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

typedef union TaggedPointer {
    _Atomic uintptr_t raw;
} TaggedPointer;

void print_tp(TaggedPointer* tp) {
    uintptr_t raw = atomic_load(&tp->raw);
    uintptr_t type = raw & TYPE_MASK;
    uintptr_t value = (raw & VALUE_MASK) >> 2;

    if (type == TYPE_INT) {
        if (value == NULL_VALUE) {
            printf("raw: 0x%016lx, type: int, value: "blu"NULL"DEF"\n", raw);
        } else if (value == ERRO_VALUE) {
            printf("raw: 0x%016lx, type: int, value: "red"ERRO"DEF"\n", raw);
        } else {
            intptr_t ivalue = (intptr_t)(value << (sizeof(uintptr_t) * 8 - VALUE_BITS)) >> (sizeof(uintptr_t) * 8 - VALUE_BITS);
            printf("raw: 0x%016lx, type: int, value: %ld\n", raw, ivalue);
        }
    } else if (type == TYPE_FLOAT) {
        if (value == NULL_VALUE) {
            printf("raw: 0x%016lx, type: float, value: "blu"NULL"DEF"\n", raw);
        } else if (value == ERRO_VALUE) {
            printf("raw: 0x%016lx, type: float, value: "red"ERRO"DEF"\n", raw);
        } else {
            FLOAT_T fvalue;
            uintptr_t adjusted_value = value << 2;
            memcpy(&fvalue, &adjusted_value, sizeof(FLOAT_T));
            printf("raw: 0x%016lx, type: float, value: %.15f\n", raw, fvalue);
        }
    } else {
        printf("raw: 0x%016lx, type: %s, address: 0x%lx\n", raw,
               type == TYPE_COMPOUND ? "compound" : "shared", value);
    }
    printf("%d bits: ", (int)(sizeof(uintptr_t) * 8));
    print_bits(&raw);
    printf("\n");
}

static inline void set_int(TaggedPointer* tp, intptr_t value) {
    uintptr_t stored_value = (uintptr_t)value & (VALUE_MASK >> 2);
    uintptr_t new_raw = (stored_value << 2) | TYPE_INT;
    atomic_store(&tp->raw, new_raw);
}

static inline void set_float(TaggedPointer* tp, FLOAT_T value) {
    uintptr_t value_raw;
    memcpy(&value_raw, &value, sizeof(FLOAT_T));
    uintptr_t stored_value = (value_raw >> 2) & (VALUE_MASK >> 2);
    uintptr_t new_raw = (stored_value << 2) | TYPE_FLOAT;
    atomic_store(&tp->raw, new_raw);
}

static inline void set_int_null(TaggedPointer* tp) {
    atomic_store(&tp->raw, (NULL_VALUE << 2) | TYPE_INT);
}

static inline void set_int_erro(TaggedPointer* tp) {
    atomic_store(&tp->raw, (ERRO_VALUE << 2) | TYPE_INT);
}

static inline void set_float_null(TaggedPointer* tp) {
    atomic_store(&tp->raw, (NULL_VALUE << 2) | TYPE_FLOAT);
}

static inline void set_float_erro(TaggedPointer* tp) {
    atomic_store(&tp->raw, (ERRO_VALUE << 2) | TYPE_FLOAT);
}

#define OPERATION_INT(op_name, operation) \
static inline void op_name##_int(TaggedPointer* tp, intptr_t operand) { \
    uintptr_t old_raw, new_raw; \
    do { \
        old_raw = atomic_load(&tp->raw); \
        if ((old_raw & TYPE_MASK) != TYPE_INT) return; \
        uintptr_t stored_value = (old_raw & VALUE_MASK) >> 2; \
        if (stored_value == NULL_VALUE || stored_value == ERRO_VALUE) return; \
        intptr_t real_value = (intptr_t)(stored_value << (sizeof(uintptr_t) * 8 - VALUE_BITS)) >> (sizeof(uintptr_t) * 8 - VALUE_BITS); \
        intptr_t new_real_value = real_value operation operand; \
        uintptr_t new_stored_value = (uintptr_t)new_real_value & (VALUE_MASK >> 2); \
        new_raw = (new_stored_value << 2) | TYPE_INT; \
    } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw)); \
}

OPERATION_INT(add, +)
OPERATION_INT(sub, -)
OPERATION_INT(mul, *)
OPERATION_INT(div, /)
OPERATION_INT(mod, %)

static inline void inc_int(TaggedPointer* tp) { add_int(tp, 1); }
static inline void dec_int(TaggedPointer* tp) { sub_int(tp, 1); }

#define OPERATION_FLOAT(op_name, operation) \
static inline void op_name##_float(TaggedPointer* tp, FLOAT_T operand) { \
    uintptr_t old_raw, new_raw; \
    do { \
        old_raw = atomic_load(&tp->raw); \
        if ((old_raw & TYPE_MASK) != TYPE_FLOAT) return; \
        uintptr_t value = (old_raw & VALUE_MASK) >> 2; \
        if (value == NULL_VALUE || value == ERRO_VALUE) return; \
        FLOAT_T fvalue; \
        uintptr_t adjusted_value = value << 2; \
        memcpy(&fvalue, &adjusted_value, sizeof(FLOAT_T)); \
        FLOAT_T new_fvalue = fvalue operation operand; \
        uintptr_t new_value_raw; \
        memcpy(&new_value_raw, &new_fvalue, sizeof(FLOAT_T)); \
        uintptr_t new_stored_value = (new_value_raw >> 2) & (VALUE_MASK >> 2); \
        new_raw = (new_stored_value << 2) | TYPE_FLOAT; \
    } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw)); \
}

OPERATION_FLOAT(add, +)
OPERATION_FLOAT(sub, -)
OPERATION_FLOAT(mul, *)
OPERATION_FLOAT(div, /)

static inline void inc_float(TaggedPointer* tp) { add_float(tp, 1); }
static inline void dec_float(TaggedPointer* tp) { sub_float(tp, 1); }

#define OPERATION_INT_BIT(op_name, operation) \
static inline void op_name##_int(TaggedPointer* tp, intptr_t operand) { \
    uintptr_t old_raw, new_raw; \
    do { \
        old_raw = atomic_load(&tp->raw); \
        if ((old_raw & TYPE_MASK) != TYPE_INT) return; \
        uintptr_t stored_value = (old_raw & VALUE_MASK) >> 2; \
        if (stored_value == NULL_VALUE || stored_value == ERRO_VALUE) return; \
        intptr_t real_value = (intptr_t)(stored_value << (sizeof(uintptr_t) * 8 - VALUE_BITS)) >> (sizeof(uintptr_t) * 8 - VALUE_BITS); \
        intptr_t new_real_value = real_value operation operand; \
        uintptr_t new_stored_value = (uintptr_t)new_real_value & (VALUE_MASK >> 2); \
        new_raw = (new_stored_value << 2) | TYPE_INT; \
    } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw)); \
}

OPERATION_INT_BIT(and, &)
OPERATION_INT_BIT(or, |)
OPERATION_INT_BIT(xor, ^)

static inline void shift_left_int(TaggedPointer* tp, unsigned int shift) {
    uintptr_t old_raw, new_raw;
    do {
        old_raw = atomic_load(&tp->raw);
        if ((old_raw & TYPE_MASK) != TYPE_INT) return;
        uintptr_t stored_value = (old_raw & VALUE_MASK) >> 2;
        if (stored_value == NULL_VALUE || stored_value == ERRO_VALUE) return;
        intptr_t real_value = (intptr_t)(stored_value << (sizeof(uintptr_t) * 8 - VALUE_BITS)) >> (sizeof(uintptr_t) * 8 - VALUE_BITS);
        intptr_t new_real_value = real_value << shift;
        uintptr_t new_stored_value = (uintptr_t)new_real_value & (VALUE_MASK >> 2);
        new_raw = (new_stored_value << 2) | TYPE_INT;
    } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw));
}

static inline void shift_right_int(TaggedPointer* tp, unsigned int shift) {
    uintptr_t old_raw, new_raw;
    do {
        old_raw = atomic_load(&tp->raw);
        if ((old_raw & TYPE_MASK) != TYPE_INT) return;
        uintptr_t stored_value = (old_raw & VALUE_MASK) >> 2;
        if (stored_value == NULL_VALUE || stored_value == ERRO_VALUE) return;
        intptr_t real_value = (intptr_t)(stored_value << (sizeof(uintptr_t) * 8 - VALUE_BITS)) >> (sizeof(uintptr_t) * 8 - VALUE_BITS);
        intptr_t new_real_value = real_value >> shift;
        uintptr_t new_stored_value = (uintptr_t)new_real_value & (VALUE_MASK >> 2);
        new_raw = (new_stored_value << 2) | TYPE_INT;
    } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw));
}

// Adicionais
#include <math.h>

static inline void pow_float(TaggedPointer* tp, FLOAT_T exponent) {
    uintptr_t old_raw, new_raw;
    do {
        old_raw = atomic_load(&tp->raw);
        if ((old_raw & TYPE_MASK) != TYPE_FLOAT) return;
        uintptr_t value = (old_raw & VALUE_MASK) >> 2;
        if (value == NULL_VALUE || value == ERRO_VALUE) return;
        FLOAT_T fvalue;
        uintptr_t adjusted_value = value << 2;
        memcpy(&fvalue, &adjusted_value, sizeof(FLOAT_T));
        FLOAT_T new_fvalue = pow(fvalue, exponent);
        uintptr_t new_value_raw;
        memcpy(&new_value_raw, &new_fvalue, sizeof(FLOAT_T));
        uintptr_t new_stored_value = (new_value_raw >> 2) & (VALUE_MASK >> 2);
        new_raw = (new_stored_value << 2) | TYPE_FLOAT;
    } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw));
}

static inline void sqrt_float(TaggedPointer* tp) {
    uintptr_t old_raw, new_raw;
    do {
        old_raw = atomic_load(&tp->raw);
        if ((old_raw & TYPE_MASK) != TYPE_FLOAT) return;
        uintptr_t value = (old_raw & VALUE_MASK) >> 2;
        if (value == NULL_VALUE || value == ERRO_VALUE) return;
        FLOAT_T fvalue;
        uintptr_t adjusted_value = value << 2;
        memcpy(&fvalue, &adjusted_value, sizeof(FLOAT_T));
        FLOAT_T new_fvalue = sqrt(fvalue);
        uintptr_t new_value_raw;
        memcpy(&new_value_raw, &new_fvalue, sizeof(FLOAT_T));
        uintptr_t new_stored_value = (new_value_raw >> 2) & (VALUE_MASK >> 2);
        new_raw = (new_stored_value << 2) | TYPE_FLOAT;
    } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw));
}

static inline void sin_float(TaggedPointer* tp) {
    uintptr_t old_raw, new_raw;
    do {
        old_raw = atomic_load(&tp->raw);
        if ((old_raw & TYPE_MASK) != TYPE_FLOAT) return;
        uintptr_t value = (old_raw & VALUE_MASK) >> 2;
        if (value == NULL_VALUE || value == ERRO_VALUE) return;
        FLOAT_T fvalue;
        uintptr_t adjusted_value = value << 2;
        memcpy(&fvalue, &adjusted_value, sizeof(FLOAT_T));
        FLOAT_T new_fvalue = sin(fvalue);
        uintptr_t new_value_raw;
        memcpy(&new_value_raw, &new_fvalue, sizeof(FLOAT_T));
        uintptr_t new_stored_value = (new_value_raw >> 2) & (VALUE_MASK >> 2);
        new_raw = (new_stored_value << 2) | TYPE_FLOAT;
    } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw));
}

static inline void cos_float(TaggedPointer* tp) {
    uintptr_t old_raw, new_raw;
    do {
        old_raw = atomic_load(&tp->raw);
        if ((old_raw & TYPE_MASK) != TYPE_FLOAT) return;
        uintptr_t value = (old_raw & VALUE_MASK) >> 2;
        if (value == NULL_VALUE || value == ERRO_VALUE) return;
        FLOAT_T fvalue;
        uintptr_t adjusted_value = value << 2;
        memcpy(&fvalue, &adjusted_value, sizeof(FLOAT_T));
        FLOAT_T new_fvalue = cos(fvalue);
        uintptr_t new_value_raw;
        memcpy(&new_value_raw, &new_fvalue, sizeof(FLOAT_T));
        uintptr_t new_stored_value = (new_value_raw >> 2) & (VALUE_MASK >> 2);
        new_raw = (new_stored_value << 2) | TYPE_FLOAT;
    } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw));
}
//

inline uintptr_t get_tags(TaggedPointer* tp) {
    uintptr_t raw = atomic_load(&tp->raw);
    if ((raw & TYPE_MASK) != TYPE_COMPOUND && (raw & TYPE_MASK) != TYPE_SHARED) return 0;
    return (raw & TAG_MASK) >> (ADDRESS_BITS + 2);
}

static inline void set_tags(TaggedPointer* tp, uintptr_t tags) {
    uintptr_t old_raw, new_raw;
    do {
        old_raw = atomic_load(&tp->raw);
        uintptr_t type = old_raw & TYPE_MASK;
        if (type != TYPE_COMPOUND && type != TYPE_SHARED) return;
        new_raw = (old_raw & ~TAG_MASK) | ((tags << (ADDRESS_BITS + 2)) & TAG_MASK);
    } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw));
}

static inline void inc_tags(TaggedPointer* tp) {
    uintptr_t old_raw, new_raw;
    do {
        old_raw = atomic_load(&tp->raw);
        uintptr_t type = old_raw & TYPE_MASK;
        if (type != TYPE_COMPOUND && type != TYPE_SHARED) return;
        uintptr_t tags = (old_raw & TAG_MASK) >> (ADDRESS_BITS + 2);
        tags = (tags + 1) & ((1ULL << TAG_BITS) - 1);  // Evita overflow nas tags
        new_raw = (old_raw & ~TAG_MASK) | (tags << (ADDRESS_BITS + 2));
    } while (!atomic_compare_exchange_strong(&tp->raw, &old_raw, new_raw));
}

typedef struct {
    _Atomic uintptr_t ref_count;
    TaggedPointer address;
} SharedPointer;

static inline void set_compound(TaggedPointer* tp, void* ptr, uintptr_t tags) {
    uintptr_t addr = (uintptr_t)ptr & ADDRESS_MASK;
    uintptr_t tag_value = (tags << (ADDRESS_BITS + 2)) & TAG_MASK;
    atomic_store(&tp->raw, addr | tag_value | TYPE_COMPOUND);
}

static inline void inc_ref(SharedPointer* sp) {
    atomic_fetch_add(&sp->ref_count, 1);
}

static inline void dec_ref(SharedPointer* sp) {
    if (atomic_fetch_sub(&sp->ref_count, 1) == 1) {
        // Libera o objeto apontado por sp->address (se necessário)
    }
}

int main() {
    TaggedPointer tp = {0};

    printf("=== Testes com Inteiros ===\n");
    set_int(&tp, 5);
    printf("Após set_int(5):\n");
    print_tp(&tp);

    add_int(&tp, 3);
    printf("Após add_int(3):\n");
    print_tp(&tp);

    sub_int(&tp, 2);
    printf("Após sub_int(2):\n");
    print_tp(&tp);

    mul_int(&tp, 4);
    printf("Após mul_int(4):\n");
    print_tp(&tp);

    div_int(&tp, 2);
    printf("Após div_int(2):\n");
    print_tp(&tp);

    mod_int(&tp, 3);
    printf("Após mod_int(3):\n");
    print_tp(&tp);

    set_int(&tp, 1);
    printf("Após set_int(1):\n");
    print_tp(&tp);

    inc_int(&tp);
    printf("Após inc_int():\n");
    print_tp(&tp);

    dec_int(&tp);
    printf("Após dec_int():\n");
    print_tp(&tp);

    set_int(&tp, -5);
    printf("Após set_int(-5):\n");
    print_tp(&tp);

    add_int(&tp, 3);
    printf("Após add_int(3):\n");
    print_tp(&tp);

    set_int_null(&tp);
    printf("Após set_int_null():\n");
    print_tp(&tp);

    set_int_erro(&tp);
    printf("Após set_int_erro():\n");
    print_tp(&tp);

    printf("\n=== Testes com Floats ===\n");
    set_float(&tp, 0.0);
    printf("Após set_float(0.0):\n");
    print_tp(&tp);

    set_float(&tp, 1.0);
    printf("Após set_float(1.0):\n");
    print_tp(&tp);

    add_float(&tp, 1.5);
    printf("Após add_float(1.5):\n");
    print_tp(&tp);

    sub_float(&tp, 0.5);
    printf("Após sub_float(0.5):\n");
    print_tp(&tp);

    mul_float(&tp, 2.0);
    printf("Após mul_float(2.0):\n");
    print_tp(&tp);

    div_float(&tp, 4.0);
    printf("Após div_float(4.0):\n");
    print_tp(&tp);

    set_float(&tp, -1.0);
    printf("Após set_float(-1.0):\n");
    print_tp(&tp);

    inc_float(&tp);
    printf("Após inc_float():\n");
    print_tp(&tp);

    dec_float(&tp);
    printf("Após dec_float():\n");
    print_tp(&tp);

    mul_float(&tp, 2.0);
    printf("Após mul_float(2.0):\n");
    print_tp(&tp);

    set_float_null(&tp);
    printf("Após set_float_null():\n");
    print_tp(&tp);

    set_float_erro(&tp);
    printf("Após set_float_erro():\n");
    print_tp(&tp);

    return 0;
}