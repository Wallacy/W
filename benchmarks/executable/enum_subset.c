// C23 reference for the Restaurant enum-subset executable workload.

#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum service_stage {
    SERVICE_ACCEPTED,
    SERVICE_RESERVING,
    SERVICE_PREPARING,
    SERVICE_SERVING,
    SERVICE_COMPLETED,
};

// C has no type-level enum subsets; this alias carries the WorkStage contract.
typedef enum service_stage work_stage;

static long long work_value(work_stage stage) {
    switch (stage) {
        case SERVICE_PREPARING: return 1;
        case SERVICE_SERVING: return 2;
        default: return 0;
    }
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    const work_stage preparing = SERVICE_PREPARING;
    const work_stage serving = SERVICE_SERVING;
    if (printf("Work %lld/%lld\n",
               work_value(preparing), work_value(serving)) < 0) return 1;
    return 0;
}
