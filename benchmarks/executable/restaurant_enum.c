// C23/c2x source variant for the Restaurant payloadless-enum switch workload.

#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum course {
    COURSE_STARTER,
    COURSE_MAIN,
    COURSE_DESSERT,
};

static long long price(enum course course) {
    switch (course) {
        case COURSE_STARTER: return 10;
        case COURSE_MAIN: return 30;
        case COURSE_DESSERT: return 20;
    }
    return 0;
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    if (printf("Courses %lld/%lld/%lld\n",
               price(COURSE_STARTER),
               price(COURSE_MAIN),
               price(COURSE_DESSERT)) < 0) return 1;
    return 0;
}
