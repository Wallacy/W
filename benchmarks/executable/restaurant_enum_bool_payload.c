// C23 source variant for the Restaurant enum-bool-payload workload.

#include <stdbool.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum service_state_kind {
    SERVICE_OFFLINE,
    SERVICE_FLAGS,
    SERVICE_CHARGE,
    SERVICE_CHECKED,
};

struct service_state {
    enum service_state_kind kind;
    union {
        struct {
            bool open;
            bool staffed;
            bool stocked;
            bool licensed;
        } flags_values;
        struct {
            long long amount;
        } charge_values;
        struct {
            bool open;
            long long amount;
        } checked_values;
    } payload;
};

static struct service_state flags(bool open) {
    return (struct service_state){
        .kind = SERVICE_FLAGS,
        .payload.flags_values = {
            .licensed = true,
            .open = open,
            .staffed = false,
            .stocked = true,
        },
    };
}

static bool is_open(struct service_state state) {
    switch (state.kind) {
        case SERVICE_CHECKED: return state.payload.checked_values.open;
        case SERVICE_CHARGE: return false;
        case SERVICE_OFFLINE: return false;
        case SERVICE_FLAGS: return state.payload.flags_values.open;
    }
    return false;
}

static long long amount(struct service_state state) {
    switch (state.kind) {
        case SERVICE_CHARGE: return state.payload.charge_values.amount;
        case SERVICE_CHECKED: return state.payload.checked_values.amount;
        case SERVICE_OFFLINE: return 0;
        case SERVICE_FLAGS: return 0;
    }
    return 0;
}

static bool is_licensed(struct service_state state) {
    switch (state.kind) {
        case SERVICE_FLAGS: return state.payload.flags_values.licensed;
        case SERVICE_OFFLINE: return false;
        case SERVICE_CHARGE: return false;
        case SERVICE_CHECKED: return false;
    }
    return false;
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    const struct service_state open = flags(true);
    const struct service_state closed = flags(false);
    const struct service_state charge = {
        .kind = SERVICE_CHARGE,
        .payload.charge_values = { .amount = 17 },
    };
    const struct service_state checked = {
        .kind = SERVICE_CHECKED,
        .payload.checked_values = { .amount = 31, .open = true },
    };
    const bool open_result = is_open(open);
    const bool closed_result = is_open(closed);
    const bool charge_result = is_open(charge);
    const bool checked_result = is_open(checked);
    const long long charge_amount = amount(charge);
    const long long checked_amount = amount(checked);
    const bool licensed = is_licensed(closed);
    if (printf("States %s/%s/%s/%s; charges %lld/%lld; licensed %s\n",
               open_result ? "true" : "false",
               closed_result ? "true" : "false",
               charge_result ? "true" : "false",
               checked_result ? "true" : "false",
               charge_amount,
               checked_amount,
               licensed ? "true" : "false") < 0) return 1;
    return 0;
}
