// CYC1 Research witness: alternatives are library compositions, not new syntax.
// CycleFixture is represented by the three explicit source-shaped compositions.
// Each function keeps the key/value relation explicit and avoids a value-to-key
// strong back edge.  These are syntax-shaped witnesses only; they do not prove
// ephemeron semantics or compiler/runtime behavior.

module cyc1_conditional_liveness

export object GenerationIdCache {
  generation: u64
  value: String
}

export fn generationIdCacheWithInvalidation(_ cache: GenerationIdCache): String {
  return cache.value
}

export object OwnerScopedCacheLease {
  value: String

  fn close() {
    value = ""
  }
}

export fn ownerScopedLeaseWithClose(_ lease: OwnerScopedCacheLease): String {
  return lease.value
}

export object DetachedCacheValue {
  keyId: u64
  payload: String
}

export fn detachedValueWithoutBackEdge(_ value: DetachedCacheValue): String {
  return value.payload
}
