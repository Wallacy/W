# CYC2 conditional-liveness closure

CYC2 closes the current Last Light cache problem with three compositions:

1. a generation or ID cache detaches the key and invalidates explicitly;
2. an owner-scoped lease closes before drain;
3. a detached value has no strong value-to-key back edge.

Weak-key and ephemeron behavior is intentionally rejected for the baseline.
Transparent collectors, finalizers, and reanimation remain rejected. Runtime,
provider, FFI, stress, and OOM evidence stays an implementation gap.
