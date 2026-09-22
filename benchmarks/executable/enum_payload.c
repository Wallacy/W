// C23 source variant for the Restaurant enum-payload workload.

#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum course_kind {
    COURSE_STARTER,
    COURSE_MAIN,
    COURSE_DESSERT,
};

struct course {
    enum course_kind kind;
    union {
        struct {
            long long price;
            long long tax;
        } main_values;
        struct {
            long long price;
        } dessert_values;
    } payload;
};

static struct course order(long long price, long long tax) {
    return (struct course){
        .kind = COURSE_MAIN,
        .payload.main_values = { .price = price, .tax = tax },
    };
}

static long long bill(struct course course) {
    switch (course.kind) {
        case COURSE_DESSERT: return course.payload.dessert_values.price;
        case COURSE_STARTER: return 10;
        case COURSE_MAIN:
            return course.payload.main_values.price + course.payload.main_values.tax;
    }
    return 0;
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    const struct course first = order(30, 2);
    const struct course second = order(41, 3);
    const struct course starter = { .kind = COURSE_STARTER };
    const struct course dessert = {
        .kind = COURSE_DESSERT,
        .payload.dessert_values = { .price = 7 },
    };
    if (printf("Bills %lld/%lld/%lld/%lld\n",
               bill(first), bill(second), bill(starter), bill(dessert)) < 0) return 1;
    return 0;
}
