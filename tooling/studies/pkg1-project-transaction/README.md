# PKG1 project transaction study

Status: design-oracle input. The study records the current route for the
identity split and the atomic replacement protocol. It does not claim W
compiler, runtime, package-manager, or provider implementation.

The current route keeps one physical `build.w` root per directory. It contains
one or more direct package records and may contain one local-only `build`
coordinator. Each package has an exact local root and remains independently
publishable. Package-authored requirements, profiles, and recipes remain part
of that package's identity; the local root and coordinator do not. A package
set digest is independent of package record order. Resolution, deployments,
and the resolved build plan have separate derived digests. The build-plan
digest also binds the exact package-name/root map, keeping relocation local
without changing public package identity.

`w resolve` changes only local resolution. `w add`, `w remove`, and `w update`
stage package-set changes with their new resolution. The host validates the full
replacement before it publishes one atomic replacement. A stale digest rejects
the write. A failed solve leaves the old bytes.

The POSIX and Windows reducers share the logical outcome. They use independent
event vocabularies. Atomic visibility is separate from crash durability. A
durability claim requires an explicit provider receipt. Missing evidence stays
`evidence-missing`.

Durable provider receipts remain Research. This study cannot prove a real
filesystem fault, W compilation, W execution, provider behavior, or human or
model comprehension.
