// PYN2 session oracle. This source is a parseable design fixture.
// The host machine derives receipts and lifecycle state. It does not execute W.
// SessionId, incarnation, ordinal, and generation are separate identities.
// `w[ordinal]` is a prompt projection. `gN` is a generation display only.
// The transcript includes snapshot values, compiled dependents, drain failures,
// resource ownership, reset boundaries, cancellation, and external effects.

module repl_session_oracle

export type SessionId = String
export type SessionIncarnation = u64
export type ExecutionOrdinal = u64
export type GenerationId = String
export type BindingId = String

export enum DependencyKind {
  compiledLookup
  typeLayout
  constEval
  witness
  resourceOwner
  importSymbol
}

export enum OwnerState {
  session
  generation
  submission
  draining
  faulted
  closing
  forceBoundary
  drained
}

export enum DrainOutcome {
  ready
  rejectedPreflight
  degraded
}

export enum ReplOutcome {
  committed
  rejected
  runtimeFailure
  cancelled
  degraded
  incomplete
}

export enum SubmissionPhase {
  collected
  parsed
  checked
  staged
  preflight
  executing
  settling
  publish
  drainingOld
  committed
  ready
}

export struct GenerationReceipt {
  ordinal: ExecutionOrdinal
  generationBase: GenerationId
  generationFinal: GenerationId
  outcome: ReplOutcome
}

export struct BindingVersion {
  bindingId: BindingId
  name: String
  version: u64
  createdGeneration: GenerationId
  createdIncarnation: SessionIncarnation
}

export struct HardEdge {
  kind: DependencyKind
  from: BindingId
  to: BindingId
  version: u64
}

export struct SoftProvenance {
  from: BindingId
  to: BindingId
  relation: String
}

export struct ResourceEvent {
  resource: String
  owner: OwnerState
  providerState: String
  allowDrain: Bool
  outcome: DrainOutcome
}

export struct CancellationReceipt {
  requestId: String
  ordinal: ExecutionOrdinal
  published: Bool
  cooperative: Bool
}

export struct SubmissionReceipt {
  requestId: String
  sessionId: SessionId
  incarnation: SessionIncarnation
  ordinal: ExecutionOrdinal
  prompt: String
  generationBase: GenerationId
  generationFinal: GenerationId
  outcome: ReplOutcome
}

export struct SessionSnapshot {
  sessionId: SessionId
  incarnation: SessionIncarnation
  ordinal: ExecutionOrdinal
  generation: GenerationId
}

export const limit = 3_i32

export const fn snapshot(value: i32): i32 {
  return value * 2
}

// `doubled` reads the exact module binding version. It is a compiled hard
// dependency, unlike `snapshot(limit * 2)`, which stores the evaluated value.
export fn doubled(): i32 {
  return limit * 2
}

// Canonical transcript map:
// Horizon limit: `limit` starts at 3 and is rebound to 4 in a later generation.
// g0 is the initial empty graph for incarnation 1.
// w[1] g0 -> g1: let limit = 3
// w[2] g1 -> g2: fn doubled(): i32 { limit * 2 }
// w[3] g2 -> g3: let snapshot = limit * 2; fn menu() { doubled() }
//       snapshot retains value 6; menu has the exact hard edge doubled@1.
// w[4] g3 -> g4: rebind limit = 4 (doubled and menu invalidated; immediate
//       reasons are limit -> doubled and doubled -> menu)
// w[5] g4 -> g4: doubled() unavailable (invalidated binding; no implicit rerun)
// w[6] g4 -> g4: var broken: i32 = "x" (semantic rejection, generation unchanged)
// reset/restart: incarnation 2 opens g0; display gN is not an identity.
// A black-hole watcher/resource is owned by a binding owner scope. Drain
// preflight derives closure and provider state from provider events plus a
// structured allowDrain confirmation, then can reject a replacement.
// A post-publish drain failure commits a degraded state.

export const fn hardEdgeIsExact(edge: HardEdge, expected: BindingId): Bool {
  return edge.kind == DependencyKind.compiledLookup && edge.to == expected && edge.version > 0_u64
}

export const fn snapshotIsValue(provenance: SoftProvenance): Bool {
  return provenance.relation == "evaluated-value"
}

export const fn drainNeedsConfirmation(event: ResourceEvent): Bool {
  return event.owner == OwnerState.generation && event.providerState == "replaceable" && event.allowDrain == false
}

export const fn transitiveEdgeIsImmediate(edge: HardEdge, expected: BindingId): Bool {
  return hardEdgeIsExact(edge, expected)
}

export const fn snapshotRetainsValue(value: i32, current: i32): Bool {
  return value != current
}

export const fn cancelBeforePublish(receipt: CancellationReceipt): Bool {
  return receipt.published == false && receipt.ordinal > 0_u64
}

export const fn cancelAfterPublish(receipt: CancellationReceipt): Bool {
  return receipt.published && receipt.ordinal > 0_u64
}

test "session identity and invalidation are explicit" for snapshot {
  expect snapshot(value: limit) == 6
  expect hardEdgeIsExact(edge: HardEdge(
    kind: DependencyKind.compiledLookup,
    from: "b:doubled",
    to: "b:limit@1",
    version: 1_u64,
  ), expected: "b:limit@1")
  expect snapshotIsValue(provenance: SoftProvenance(
    from: "b:snapshot",
    to: "b:limit@1",
    relation: "evaluated-value",
  ))
  expect drainNeedsConfirmation(event: ResourceEvent(
    resource: "black-hole-watcher",
    owner: OwnerState.generation,
    providerState: "replaceable",
    allowDrain: false,
    outcome: DrainOutcome.rejectedPreflight,
  ))
}

test "snapshot retention is separate from compiled invalidation" for snapshotRetainsValue {
  expect snapshotRetainsValue(value: 6, current: 8)
}

test "transitive edges retain the immediate predecessor" for transitiveEdgeIsImmediate {
  expect transitiveEdgeIsImmediate(edge: HardEdge(
    kind: DependencyKind.compiledLookup,
    from: "b:menu",
    to: "b:doubled@1",
    version: 1_u64,
  ), expected: "b:doubled@1")
}

test "owner drain has ready and degraded facts" for drainNeedsConfirmation {
  expect drainNeedsConfirmation(event: ResourceEvent(
    resource: "watcher",
    owner: OwnerState.generation,
    providerState: "replaceable",
    allowDrain: false,
    outcome: DrainOutcome.ready,
  ))
}

test "cancel receipts distinguish before and after publish" for cancelBeforePublish {
  expect cancelBeforePublish(receipt: CancellationReceipt(
    requestId: "active-before",
    ordinal: 1_u64,
    published: false,
    cooperative: true,
  ))
  expect cancelAfterPublish(receipt: CancellationReceipt(
    requestId: "active-after",
    ordinal: 2_u64,
    published: true,
    cooperative: false,
  ))
}
