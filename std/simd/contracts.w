// Draft source contract for portable SIMD policy.
//
// Simd and SimdMask are compiler-owned heads. The import resolves and
// catalogues them separately. They are not structs, runtime exports, or
// provider calls from this source module.

export enum ReductionMode: Copy & Equatable {
  strict
  fast
  reproducible
}
