# PKG1 project transaction study

Status: design-oracle input. The study records the current route for the
identity split and the atomic replacement protocol. It does not claim W
compiler, runtime, package-manager, or provider implementation.

The current route keeps one physical `build.w` per directory. A standalone
package record or the declared workspace is the owner; a package member does
not duplicate `resolution` or `deployments`. A resolution record and each
deployment record have separate derived digests.

`w resolve` changes only the resolution. `w add`, `w remove`, and `w update`
stage the owner change with its new resolution. The host validates the full
replacement before it publishes one atomic replacement. A stale digest rejects
the write. A failed solve leaves the old bytes.

The POSIX and Windows reducers share the logical outcome. They use independent
event vocabularies. Atomic visibility is separate from crash durability. A
durability claim requires an explicit provider receipt. Missing evidence stays
`evidence-missing`.

Durable provider receipts remain Research. This study cannot prove a real
filesystem fault, W compilation, W execution, provider behavior, or human or
model comprehension.
