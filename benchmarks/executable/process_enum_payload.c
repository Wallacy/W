// C23 reference for the public process-enum-payload executable workload.
// Expected output cases (argv => exit; stdout):
// [] => 7; "arguments-missing count=0 amount=17 over-limit=false\n"
// [""] => 0; "arguments-present count=1 amount=17 over-limit=false\n"
// ["alpha", "beta"] => 0; "arguments-present count=2 amount=17 over-limit=false\n"
// ["alpha", "beta", "gamma"] => 0; "arguments-present count=3 amount=17 over-limit=true\n"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum argument_state_kind {
    ARGUMENT_STATE_UNAVAILABLE,
    ARGUMENT_STATE_OBSERVED,
};

struct argument_state {
    enum argument_state_kind kind;
    union {
        struct {
            bool missing;
            bool over_limit;
            int64_t amount;
        } observed;
    } payload;
};

static struct argument_state build_argument_state(bool missing, bool over_limit) {
    return (struct argument_state){
        .kind = ARGUMENT_STATE_OBSERVED,
        .payload.observed = {
            .missing = missing,
            .over_limit = over_limit,
            .amount = 17,
        },
    };
}

static bool argument_state_is_missing(struct argument_state state) {
    switch (state.kind) {
        case ARGUMENT_STATE_UNAVAILABLE:
            return false;
        case ARGUMENT_STATE_OBSERVED:
            return state.payload.observed.missing;
    }
    return false;
}

static bool argument_state_is_over_limit(struct argument_state state) {
    switch (state.kind) {
        case ARGUMENT_STATE_UNAVAILABLE:
            return false;
        case ARGUMENT_STATE_OBSERVED:
            return state.payload.observed.over_limit;
    }
    return false;
}

static int64_t argument_state_amount(struct argument_state state) {
    switch (state.kind) {
        case ARGUMENT_STATE_UNAVAILABLE:
            return 0;
        case ARGUMENT_STATE_OBSERVED:
            return state.payload.observed.amount;
    }
    return 0;
}

int main(int argc, char **argv) {
    (void)argv;

#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif

    const int count = argc - 1;
    const struct argument_state state = build_argument_state(argc == 1, count > 2);
    const bool missing = argument_state_is_missing(state);
    const bool over_limit = argument_state_is_over_limit(state);
    const int64_t amount = argument_state_amount(state);
    if (missing) {
        if (printf("arguments-missing count=%d amount=%lld over-limit=%s\n",
                   count, (long long)amount, over_limit ? "true" : "false") < 0) return 1;
        return 7;
    }

    if (printf("arguments-present count=%d amount=%lld over-limit=%s\n",
               count, (long long)amount, over_limit ? "true" : "false") < 0) return 1;
    return 0;
}
