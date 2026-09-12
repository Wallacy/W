// C23 reference for the public process-enum-payload executable workload.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum admission_state_kind {
    ADMISSION_UNAVAILABLE,
    ADMISSION_OBSERVED,
};

struct admission_state {
    enum admission_state_kind kind;
    union {
        struct {
            bool missing;
            int64_t amount;
        } observed;
    } payload;
};

static struct admission_state build_admission(bool missing) {
    return (struct admission_state){
        .kind = ADMISSION_OBSERVED,
        .payload.observed = {
            .missing = missing,
            .amount = 17,
        },
    };
}

static bool admission_is_missing(struct admission_state state) {
    switch (state.kind) {
        case ADMISSION_UNAVAILABLE:
            return false;
        case ADMISSION_OBSERVED:
            return state.payload.observed.missing;
    }
    return false;
}

int main(int argc, char **argv) {
    (void)argv;

#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif

    const struct admission_state state = build_admission(argc == 1);
    const bool repeated = argc == 1;
    if (admission_is_missing(state)) {
        if (printf("enum-missing %s\n", repeated ? "true" : "false") < 0) return 1;
        return 7;
    }

    if (printf("enum-received %s\n", repeated ? "true" : "false") < 0) return 1;
    return 0;
}
