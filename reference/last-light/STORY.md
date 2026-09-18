# The final service at the Last Light restaurant

> Generated from `story.json`. Edit the manifest, then run `bun tooling/last-light-story.mjs --write`.

Three shifts run before the observable universe closes: a quiet orbit proves the happy path, a photon rush stresses bounded concurrency, and a timeline collision makes overload and cancellation visible.

The Syntax Atlas owns the complete atomic source-form inventory. Last Light owns the connected application story. A form may be parse-only until its compiler or provider exists, but it may not be represented by an invalid placeholder statement.

Current inventory: 220 public grammar rules, 98 accepted atomic variants, and 18 story-assigned Atlas blocks. Mutually exclusive roots and entry defaults live in separate modules rather than being stacked into an impossible program.

## 1. open the doors

The build selects a product and entry, binds process capabilities, and opens the restaurant without executing imported entry modules.

Source witnesses:

- `reference/last-light/build.w` — `package { …`
- `reference/last-light/app.w` — `async fn runNative(`
- `reference/last-light/simulation_app.w` — `entry LastLightSimulation(runSimulation)`

Acceptance:

- one selected entry
- capabilities are explicit
- imports have no startup side effects

Atlas application families:

| Block | Family | Atomic variants assigned to this act |
| --- | --- | --- |
| `package-root` | manifest | `root-package` |
| `workspace-root` | manifest | `root-workspace` |
| `module-run-root` | roots | `root-module-run` |
| `source-roots-imports` | roots | `root-module`, `import-ordinary`, `import-module-binding`, `import-named`, `import-wildcard`, `reexport-wildcard`, `reexport-named`, `import-kernel-named`, `import-kernel-qualified`, `module-kernel-contract`, `import-domain`, `import-service`, `module-contract-configuration` |
| `entry-short-body` | entry | `entry-default-body` |
| `entry-declaration` | entry | `entry-named-body`, `entry-named-handler`, `entry-explicit` |
| `module-run-entry` | entry | `entry-default-handler` |

## 2. seat the guests

Named guests arrive with orders from three timelines; enums, refinements, patterns, literals, and units keep every value explicit.

Source witnesses:

- `reference/last-light/domain.w` — `export struct Order {`
- `reference/last-light/simulation.w` — `fn scenario(profile: SimulationProfile): Scenario`
- `reference/last-light/numerics.w` — `export type SeatNumber`

Acceptance:

- quiet orbit has three orders
- timeline identities remain data
- invalid refinements fail before service work

Atlas application families:

| Block | Family | Atomic variants assigned to this act |
| --- | --- | --- |
| `data-declarations` | declarations | `export-list`, `property-get`, `protocol-default-extension`, `property-set`, `property-let-value`, `property-let-ref`, `property-var-value`, `property-var-ref`, `property-var-mut-ref`, `property-var-inout`, `behavior-set-parameter`, `behavior-storage`, `protocol-contract-documentation` |
| `types-and-contracts` | types | `ownership-shared`, `ownership-weak`, `ownership-view`, `callable-some-fn`, `static-record`, `static-list`, `literal-unit`, `tuple-index`, `generic-application`, `intrinsic-value-contract` |
| `literals-and-collections` | values | `literal-string-double`, `literal-string-single`, `literal-raw-double`, `literal-raw-single`, `literal-multiline`, `literal-raw-multiline`, `literal-unit-suffix`, `literal-size` |
| `patterns` | patterns | `pattern-enum`, `pattern-struct`, `pattern-inferred-struct`, `pattern-tuple`, `pattern-range`, `pattern-wildcard` |
| `operators` | operators | `pipe-member` |

## 3. prepare the last meal

The kitchen reserves stock, samples aroma, plans energy, mixes in a parallel domain, and closes every lease on success or failure.

Source witnesses:

- `reference/last-light/restaurant.w` — `async fn prepareDish(`
- `reference/last-light/kitchen.w` — `export fn expectedEnergy( …`
- `reference/last-light/failure.w` — `export fn decodeWithCleanup(`

Acceptance:

- resource cleanup is single-owner
- energy and aroma failures are typed
- compensation remains observable

Atlas application families:

| Block | Family | Atomic variants assigned to this act |
| --- | --- | --- |
| `callables-and-foreign` | callables | `callable-positional`, `callable-required-homonym`, `callable-required-external`, `callable-default`, `callable-rest`, `callable-any-fn`, `callable-static`, `callable-generic`, `callable-borrow-relation`, `callable-abi`, `foreign-block`, `foreign-type`, `foreign-struct`, `foreign-function`, `inline-document-contract`, `reusable-document-contract`, `repeated-document-contract`, `const-contract-relation` |
| `allocator-and-bindings` | ownership | `allocator-named`, `allocator-anonymous`, `allocator-contextual-parameter`, `allocator-contextual-call`, `ownership-ref`, `ownership-inout`, `ownership-take`, `ownership-atomic` |
| `control-flow` | control | `control-break` |
| `restricted-expressions` | effects | `ownership-pin` |

## 4. survive the rush

Structured tasks, bounded pipelines, streams, and directional channels process the photon rush without hiding ordering, backpressure, or cancellation.

Source witnesses:

- `reference/last-light/execution.w` — `export async fn mixAcrossTwoKitchens(`
- `reference/last-light/streams.w` — `export async fn serveOneByOne`
- `reference/last-light/restaurant.w` — `service lastLight: RestaurantApi`

Acceptance:

- children remain structured
- channel capacity is finite
- failure and cancellation preserve owners

Atlas application families:

| Block | Family | Atomic variants assigned to this act |
| --- | --- | --- |
| `execution-forms` | execution | `execution-direct`, `execution-await`, `execution-sync`, `execution-async-initializer`, `execution-spawn`, `closure-copy`, `closure-ref`, `closure-take`, `closure-weak` |
| `stream-and-channel` | streams | `channel-send`, `channel-receive` |

## 5. close at the end of time

The deterministic simulation renders a final manifest and event log, then proves replay and overload outcomes before shutdown drains the service graph.

Source witnesses:

- `reference/last-light/simulation.w` — `export fn writeSimulation(`
- `reference/last-light/simulation.w` — `test "the quiet orbit completes without losing an order"`
- `reference/last-light/simulation.w` — `test "the timeline collision makes overload visible"`
- `reference/last-light/simulation.w` — `test "a simulation profile replays the same observable history"`
- `reference/last-light/app.w` — `async fn shutdown(`

Acceptance:

- quiet orbit completes 3 and loses 0
- timeline collision completes 1 and exposes 3 departures
- photon rush replays the same observable history

## Evidence boundary

The deterministic `LastLightSimulation` source is the first coherent execution target: quiet orbit, photon rush, and timeline collision already have explicit expected facts in `simulation.w`. The full service, UI, device, registry, and deployment products remain design sources until their corresponding compiler/runtime/provider gates exist.

Completeness is collective, not a claim that one binary should exercise mutually exclusive package roots, entries, targets, or foreign adapters. The gate requires every Atlas block to belong to exactly one narrative act and every act to cite real, unique W source witnesses.
