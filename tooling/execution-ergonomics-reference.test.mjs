import assert from "node:assert/strict"
import test from "node:test"
import { deriveExecutionErgonomics, summarizeDiagnostics } from "./execution-ergonomics-machine.mjs"

test("optional callable and generic labels normalize without types", () => {
  const result = deriveExecutionErgonomics(`
    struct OvenSession<_ state: OvenSessionState> {}
    let a = OvenSession<.ready>.state
    let b = OvenSession<state: .ready>.state
    fn note(_ message) {}
    note("ready")
    note(message: "ready")
  `)
  assert.equal(result.labels.generic.identities[0].sameAsNext, true)
  assert.equal(result.labels.declarations[0].params[0].policy, "optional(name)")
  assert.deepEqual(result.labels.declarations[0].params[0].forms, ["positional", "message:"])
  assert.deepEqual(result.labels.diagnostics, [])
})

test("labels reject unknown, duplicate, and colliding forms", () => {
  const unknown = deriveExecutionErgonomics("fn note(_ value) {}\nnote(other: 1)")
  assert.ok(summarizeDiagnostics(unknown).includes("W-LABEL-0005"))
  const duplicate = deriveExecutionErgonomics("fn note(_ value) {}\nnote(value: 1, value: 2)")
  assert.ok(summarizeDiagnostics(duplicate).includes("W-LABEL-0006"))
  const collision = deriveExecutionErgonomics("fn note(_ value) {}\nfn note(value) {}")
  assert.ok(summarizeDiagnostics(collision).includes("W-LABEL-0004"))
})

test("external and internal labels are distinct without a label keyword", () => {
  const positive = deriveExecutionErgonomics("fn route(to range: Range, during duration: Duration) {}\nroute(to: 1, during: 2)")
  const route = positive.labels.declarations.find((declaration) => declaration.name === "route")
  assert.equal(route.params[0].external, "to")
  assert.equal(route.params[0].internal, "range")
  assert.equal(route.params[0].policy, "required(to)")
  assert.deepEqual(positive.labels.diagnostics, [])
  const unknown = deriveExecutionErgonomics("fn route(to range: Range) {}\nroute(from: 1)")
  assert.ok(summarizeDiagnostics(unknown).includes("W-LABEL-0005"))
})

test("plain callable parameters stay positional at every index", () => {
  const result = deriveExecutionErgonomics(`
    fn query(count: usize<(1...500)>, context: Context) {}
    query(20, context)
  `)
  const query = result.labels.declarations.find((declaration) => declaration.name === "query")
  assert.deepEqual(query.params.map((parameter) => parameter.policy), [
    "positionalOnly",
    "positionalOnly",
  ])
  assert.deepEqual(query.callShapes, ["positional|positional"])
  assert.deepEqual(result.labels.diagnostics, [])
})

test("named publishes a required label without duplicating the binding", () => {
  const accepted = deriveExecutionErgonomics(`
    fn query(count: usize, named context: Context) {}
    query(20, context: context)
  `)
  const query = accepted.labels.declarations.find((declaration) => declaration.name === "query")
  expect(query?.params[1]).toMatchObject({
    internal: "context",
    external: "context",
    policy: "required(context)",
    forms: ["context:"],
    named: true,
  })
  expect(accepted.labels.diagnostics).toEqual([])

  const rejected = deriveExecutionErgonomics(`
    fn query(count: usize, named context: Context) {}
    query(20, context)
  `)
  expect(summarizeDiagnostics(rejected)).toContain("W-LABEL-0005")

  const contextualIdentifier = deriveExecutionErgonomics(`
    fn inspect(named: Bool) {}
    inspect(flag)
  `)
  expect(contextualIdentifier.labels.declarations[0]?.params[0]).toMatchObject({
    internal: "named",
    policy: "positionalOnly",
    forms: ["positional"],
  })
  expect(contextualIdentifier.labels.diagnostics).toEqual([])
})

test("record-like initializers reject the redundant named marker", () => {
  const result = deriveExecutionErgonomics(`
    struct Seat {
      value: usize
      init(named value: usize) { self.value = value }
    }
  `)
  expect(summarizeDiagnostics(result)).toContain("W-LABEL-0007")
})

test("parameter contracts follow labels and bindings", () => {
  const canonical = deriveExecutionErgonomics(`
    fn transfer(
      source: ref Menu,
      target: inout Menu,
      payload: take Order,
      format: const Format,
    ) {}
  `)
  expect(canonical.labels.declarations[0]?.params.map((parameter) => parameter.contractMode))
    .toEqual(["ref", "inout", "take", "const"])
  expect(canonical.labels.diagnostics).toEqual([])

  for (const modifier of [
    "const",
    "copy",
    "inout",
    "mut",
    "pin",
    "ref",
    "shared",
    "take",
    "view",
    "weak",
  ]) {
    const rejected = deriveExecutionErgonomics(`fn invalid(${modifier} value: Menu) {}`)
    expect(summarizeDiagnostics(rejected)).toContain("W-OWNERSHIP-0016")
  }

  const initializer = deriveExecutionErgonomics(`
    struct Request { init(take value: Request) {} }
  `)
  expect(summarizeDiagnostics(initializer)).toContain("W-OWNERSHIP-0016")
})

test("call-site operations match parameter contracts without decorating rvalues", () => {
  const accepted = deriveExecutionErgonomics(`
    fn inspect(value: ref Menu) {}
    fn store(value: take Order) {}
    fn forward(borrowed: ref Menu, owned: Menu) {
      inspect(borrowed)
      inspect(ref owned)
      inspect(Menu())
      store(Order())
    }
  `)
  expect(accepted.labels.diagnostics).toEqual([])
  expect(accepted.labels.calls.filter((call) => call.callee === "inspect")
    .map((call) => call.arguments[0]?.operation)).toEqual(["value", "ref", "value"])

  const rejected = deriveExecutionErgonomics(`
    fn inspect(value: ref Menu) {}
    fn run(menu: Menu) { inspect(take menu) }
  `)
  expect(summarizeDiagnostics(rejected)).toContain("W-OWNERSHIP-0017")
})

test("parameter splitting distinguishes generic delimiters from operators", () => {
  const result = deriveExecutionErgonomics(`
    fn project(
      transform: fn(Int, Int) -> Int,
      range: Int<(value > 0)>,
      enabled: Bool = lower < upper,
    ) {}
    project(transform, range)
  `)
  const project = result.labels.declarations.find((declaration) => declaration.name === "project")
  expect(project?.params).toHaveLength(3)
  expect(project?.callShapes).toContain("positional|positional")
  expect(result.labels.diagnostics).toEqual([])
})

test("overload collision is scoped to one declaration owner", () => {
  const result = deriveExecutionErgonomics(`
    protocol Left { fn read(value: Value) }
    struct Right { fn read(value: Value) {} }
  `)
  assert.deepEqual(result.labels.diagnostics, [])
})

test("enum payload cases are not mistaken for direct calls", () => {
  const source = `
    enum GatewayError {
      dispatch(DispatchError)
    }

    async fn dispatch(command: Command, authority hostAuthority: HostAuthority): Result {
      return Result(command: command, authority: hostAuthority)
    }

    async fn route(command: Command): Result {
      return await dispatch(command, authority: .localOperator)
    }
  `
  const result = deriveExecutionErgonomics(source)
  expect(result.labels.diagnostics).toEqual([])
})

test("unnamed intrinsic slots and variadic calls remain positional", () => {
  const source = `
    fn intrinsic(ref Handle, usize): usize { return 0 }
    fn byteCount(_ messages: ref String...): usize { return 0 }
    fn route(handle: ref Handle): usize {
      let count = intrinsic(handle, 4)
      return count + byteCount("a", "b") + byteCount(messages: each labels)
    }
  `
  const result = deriveExecutionErgonomics(source)
  expect(result.labels.diagnostics).toEqual([])
  expect(result.labels.declarations.find((item) => item.name === "intrinsic")?.params)
    .toHaveLength(2)
})

test("extensions use the nominal owner's overload set", () => {
  const independent = `
    extension LeftStream { fn tee(limit: usize): Pair {} }
    extension RightStream { fn tee(limit: usize): Pair {} }
  `
  expect(deriveExecutionErgonomics(independent).labels.diagnostics).toEqual([])

  const collision = `
    extension Stream { fn tee(limit: usize): Pair {} }
    extension Stream { fn tee(limit: usize): Pair {} }
  `
  expect(summarizeDiagnostics(deriveExecutionErgonomics(collision)))
    .toContain("W-LABEL-0004")
})

test("variadic overlap is not bounded by a sampled arity", () => {
  const source = `
    fn collect(values: Value...) {}
    fn collect(first: Value, second: Value, third: Value) {}
  `
  expect(summarizeDiagnostics(deriveExecutionErgonomics(source)))
    .toContain("W-LABEL-0004")
})

test("complete ordered call shapes separate arities and reject real overlap", () => {
  const disjoint = deriveExecutionErgonomics("fn serve(_ value: Value) {}\nfn serve(_ value: Value, mode: Mode) {}")
  const declarations = disjoint.labels.declarations.filter((declaration) => declaration.name === "serve")
  assert.deepEqual(declarations[0].callShapes, ["positional", "value:"])
  assert.deepEqual(declarations[1].callShapes, ["positional|positional", "value:|positional"])
  assert.deepEqual(disjoint.labels.diagnostics, [])
  const collision = deriveExecutionErgonomics("fn serve(_ value: Value) {}\nfn serve(value: Value) {}")
  assert.ok(summarizeDiagnostics(collision).includes("W-LABEL-0004"))
})

test("suspension is inferred monotonically and child accepts sync or may", () => {
  const result = deriveExecutionErgonomics(`
    fn syncWorker(value) { return value }
    fn asyncWorker(value) { await execution#yield(); return value }
    fn caller(value) {
      let local = async syncWorker(value)
      let remote = spawn<.compute> asyncWorker(value)
      return try await (local, remote)
    }
    fn even(n) { if n == 0 { return true }; return await odd(n - 1) }
    fn odd(n) { await execution#yield(); return if n == 0 { false } else { await even(n - 1) } }
  `, { publicContract: { previous: "never", current: "may", exported: true } })
  assert.deepEqual(result.forms, {
    direct: "same-task/neverSuspend",
    await: "same-task/maySuspend",
    sync: "same-task/directEntry/neverSuspend",
    async: "structured-child/current-domain",
    spawn: "structured-child/explicit-domain",
  })
  assert.equal(result.suspension.declarations.find((item) => item.name === "odd").suspension, "may")
  assert.equal(result.suspension.declarations.find((item) => item.name === "even").suspension, "may")
  assert.equal(result.suspension.children.length, 2)
  assert.equal(result.suspension.public.sourceBreaking, true)
  assert.equal(result.suspension.tryOrthogonal, true)
})

test("bare maySuspend and blocking wait are errors; await never is removable", () => {
  const result = deriveExecutionErgonomics(`
    async fn remote() { return 1 }
    fn sync() { return 1 }
    fn caller() { remote(); await sync(); blockingWait(task) }
  `)
  const diagnostics = summarizeDiagnostics(result)
  assert.ok(diagnostics.includes("W-SUSPEND-0001"))
  assert.ok(diagnostics.includes("W-SUSPEND-0002"))
  assert.ok(diagnostics.includes("W-SUSPEND-0003"))
})

test("sync uses only a proven direct entry and never blocks or creates a task", () => {
  const accepted = deriveExecutionErgonomics("async fn func(): Value throws String { return value }\nlet y = try sync func()")
  assert.deepEqual(accepted.suspension.syncCalls, [{
    callee: "func",
    eligible: true,
    sourceSpelling: "explicit",
    directEntry: "available",
    publishedSuspension: "may",
    selectedEntry: "direct",
    selectedEntrySuspension: "never",
    blocksThread: false,
    createsTask: false,
    suspendsTask: false,
    sameTask: true,
    sameContext: true,
    sameDomain: true,
    runtimeFallback: false,
    partialEffectsBeforeRejection: false,
  }])
  assert.equal(accepted.suspension.tryOrthogonal, true)
  assert.equal(accepted.suspension.declarations.find((item) => item.name === "func").directEntry, "available")

  const suspending = deriveExecutionErgonomics("async fn func(): Value { await execution#yield(); return value }\nlet y = sync func()")
  assert.ok(summarizeDiagnostics(suspending).includes("W-SUSPEND-0005"))
  assert.equal(suspending.suspension.syncCalls[0].eligible, false)
  assert.equal(suspending.suspension.syncCalls[0].partialEffectsBeforeRejection, false)

  const dynamicPath = deriveExecutionErgonomics("async fn cached(hit: Bool): Value { if hit { return value }; return await catalog() }\nlet y = sync cached(true)", {
    functionTypes: [{ name: "catalog", suspension: "may", sourceSpelling: "explicit", directEntry: "absent" }],
  })
  assert.ok(summarizeDiagnostics(dynamicPath).includes("W-SUSPEND-0005"))
  assert.equal(dynamicPath.suspension.declarations.find((item) => item.name === "cached").directEntryProof.beforeSpecialization, true)
  assert.equal(dynamicPath.suspension.declarations.find((item) => item.name === "cached").directEntryProof.dynamicReadinessUsed, false)

  const inferred = deriveExecutionErgonomics("fn inferredMay() { await execution#yield(); return value }\nlet y = sync inferredMay()")
  assert.ok(summarizeDiagnostics(inferred).includes("W-SUSPEND-0005"))
  const ordinary = deriveExecutionErgonomics("fn ordinary() { return value }\nlet y = sync ordinary()")
  assert.ok(summarizeDiagnostics(ordinary).includes("W-SUSPEND-0005"))
  const bare = deriveExecutionErgonomics("async fn func() { await execution#yield(); return value }\nlet w = func()")
  assert.ok(summarizeDiagnostics(bare).includes("W-SUSPEND-0001"))
})

test("directEntry is declaration-wide, preserved by function type, and part of public identity", () => {
  for (const source of [
    "async fn blocked(): Value { let child = async work(); return value }\nlet y = sync blocked()",
    "async fn blocked(): Value { task.join(); return value }\nlet y = sync blocked()",
    "async fn blocked(): Value { defer async { cleanup() }; return value }\nlet y = sync blocked()",
  ]) assert.ok(summarizeDiagnostics(deriveExecutionErgonomics(source)).includes("W-SUSPEND-0005"))

  const service = deriveExecutionErgonomics("async fn blocked(): Value { return catalog.fetch() }\nlet y = sync blocked()")
  assert.ok(summarizeDiagnostics(service).includes("W-SUSPEND-0005"))

  const protocol = deriveExecutionErgonomics("protocol Loader { async fn load(): Value }\nlet y = sync load()")
  assert.equal(protocol.suspension.declarations.find((item) => item.name === "load").directEntry, "absent")
  assert.ok(summarizeDiagnostics(protocol).includes("W-SUSPEND-0005"))
  const foreign = deriveExecutionErgonomics("foreign c { async fn load(): Value }\nlet y = sync load()")
  assert.equal(foreign.suspension.declarations.find((item) => item.name === "load").directEntry, "absent")

  const indirect = deriveExecutionErgonomics("let y = sync worker()", {
    functionTypes: [{ name: "worker", suspension: "may", sourceSpelling: "explicit", directEntry: "available" }],
  })
  assert.equal(indirect.suspension.syncCalls[0].eligible, true)
  const erased = deriveExecutionErgonomics("let y = sync worker()", {
    functionTypes: [{ name: "worker", suspension: "may", sourceSpelling: "explicit", directEntry: "absent" }],
  })
  assert.ok(summarizeDiagnostics(erased).includes("W-SUSPEND-0005"))

  const publicFacet = deriveExecutionErgonomics("export async fn published(): Value { return value }", {
    publicContract: {
      previous: "may",
      current: "may",
      previousDirectEntry: "available",
      currentDirectEntry: "absent",
      exported: true,
    },
  })
  assert.equal(publicFacet.suspension.public.directEntryRemoved, true)
  assert.equal(publicFacet.suspension.public.sourceBreaking, true)
  assert.equal(publicFacet.suspension.public.semanticInterfaceKeyChanged, true)
  assert.equal(publicFacet.suspension.overloadResolutionBeforeDirectEntry, true)
  assert.equal(publicFacet.suspension.optimizerChangesSourceValidity, false)
})

test("directEntry composes through sync calls and recursive SCCs without a termination proof", () => {
  const composed = deriveExecutionErgonomics(`
    async fn leaf(): Value { return value }
    async fn wrapper(): Value { return sync leaf() }
    let result = sync wrapper()
  `)
  for (const name of ["leaf", "wrapper"]) {
    const declaration = composed.suspension.declarations.find((item) => item.name === name)
    assert.equal(declaration.suspension, "may")
    assert.equal(declaration.asyncEntrySuspension, "may")
    assert.equal(declaration.directEntry, "available")
    assert.equal(declaration.directEntrySuspension, "never")
  }
  assert.deepEqual(summarizeDiagnostics(composed), [])
  assert.deepEqual(
    composed.suspension.syncCalls.filter((call) => ["leaf", "wrapper"].includes(call.callee))
      .map((call) => [call.callee, call.publishedSuspension, call.selectedEntrySuspension]),
    [["leaf", "may", "never"], ["wrapper", "may", "never"]],
  )

  const lost = deriveExecutionErgonomics(`
    async fn leaf(): Value { return await catalog() }
    async fn middle(): Value { return sync leaf() }
    async fn top(): Value { return sync middle() }
    let result = sync top()
  `, { functionTypes: [{ name: "catalog", suspension: "may", sourceSpelling: "explicit", directEntry: "absent" }] })
  for (const name of ["leaf", "middle", "top"]) {
    assert.equal(lost.suspension.declarations.find((item) => item.name === name).directEntry, "absent")
  }
  assert.ok(summarizeDiagnostics(lost).includes("W-SUSPEND-0005"))

  const invalidLocal = deriveExecutionErgonomics(`
    fn ordinary(): Value { return value }
    async fn wrapper(): Value { return sync ordinary() }
    let result = sync wrapper()
  `)
  assert.equal(invalidLocal.suspension.declarations.find((item) => item.name === "wrapper").directEntry, "absent")
  assert.ok(summarizeDiagnostics(invalidLocal).includes("W-SUSPEND-0005"))

  const invalidExternal = deriveExecutionErgonomics(`
    async fn wrapper(): Value { return sync ordinary() }
    let result = sync wrapper()
  `, { functionTypes: [{ name: "ordinary", suspension: "never", sourceSpelling: "none", directEntry: "absent" }] })
  assert.equal(invalidExternal.suspension.declarations.find((item) => item.name === "wrapper").directEntry, "absent")
  assert.ok(summarizeDiagnostics(invalidExternal).includes("W-SUSPEND-0005"))

  const inconsistentExternal = deriveExecutionErgonomics(`
    async fn wrapper(): Value { return sync worker() }
    let result = sync wrapper()
  `, { functionTypes: [{ name: "worker", suspension: "never", sourceSpelling: "explicit", directEntry: "available" }] })
  assert.equal(inconsistentExternal.suspension.declarations.find((item) => item.name === "wrapper").directEntry, "absent")
  assert.ok(summarizeDiagnostics(inconsistentExternal).includes("W-SUSPEND-0005"))

  const invalidUnknown = deriveExecutionErgonomics(`
    async fn wrapper(): Value { return sync Foo() }
    let result = sync wrapper()
  `)
  assert.equal(invalidUnknown.suspension.declarations.find((item) => item.name === "wrapper").directEntry, "absent")
  assert.ok(summarizeDiagnostics(invalidUnknown).includes("W-SUSPEND-0005"))

  const recursive = deriveExecutionErgonomics(`
    async fn even(n: usize): Bool { return if n == 0 { true } else { sync odd(n - 1) } }
    async fn odd(n: usize): Bool { return if n == 0 { false } else { sync even(n - 1) } }
    let result = sync even(2)
  `)
  const component = recursive.suspension.directEntryScc.find((item) =>
    item.members.includes("even") && item.members.includes("odd"))
  assert.deepEqual(component, {
    members: ["even", "odd"],
    directEntry: "available",
    recursive: true,
    terminationProven: false,
    evaluationPerformed: false,
  })
  assert.deepEqual(summarizeDiagnostics(recursive), [])
})

test("spawn dispatches to serial or concurrent domains and requires a target", () => {
  const declaration = deriveExecutionErgonomics("spawn<.network> fn fetch(request) { return request }")
  assert.ok(summarizeDiagnostics(declaration).includes("W-PLACEMENT-0001"))
  const serial = deriveExecutionErgonomics("module kitchen<domains: [.serial(.thermal)]>\nlet work = spawn<.thermal> f()")
  assert.deepEqual(summarizeDiagnostics(serial), [])
  assert.equal(serial.placement.dispatches[0].domain, ".thermal")
  assert.equal(serial.placement.dispatches[0].scheduling, "serial-fifo")
  assert.equal(serial.placement.dispatches[0].overlapWithinTarget, false)
  const main = deriveExecutionErgonomics("let work = spawn<.main> f()")
  assert.equal(main.placement.dispatches[0].scheduling, "serial-fifo")
  const missing = deriveExecutionErgonomics("let work = spawn f()")
  assert.ok(summarizeDiagnostics(missing).includes("W-PLACEMENT-0002"))
  const unknown = deriveExecutionErgonomics("let work = spawn<.missing> f()", { availableDomains: [".main", ".compute"] })
  assert.ok(summarizeDiagnostics(unknown).includes("W-PLACEMENT-0002"))
  const forms = deriveExecutionErgonomics("let a = spawn<.compute> f()\nlet b = spawn<domain: .compute> f()")
  assert.equal(forms.placement.sameOptionalDomainForm, true)
})

test("barrier dispatch orders read epochs and requires a closed access graph", () => {
  const source = `
    fn read(state: ref Menu) { return state.revision }
    fn write(state: inout Menu) { state.revision += 1; return state.revision }
    let before = spawn<.catalog> read(ref menu)
    let update = spawn<.catalog, .barrier> write(inout menu)
    let after = spawn<domain: .catalog> read(ref menu)
  `
  const accepted = deriveExecutionErgonomics(source, {
    domainCapabilities: { ".catalog": ["concurrent", "barrierDispatch"] },
  })
  assert.deepEqual(summarizeDiagnostics(accepted), [])
  assert.equal(accepted.placement.dispatches[1].mode, ".barrier")
  assert.equal(accepted.placement.dispatches[1].scheduling, "exclusive-barrier")
  assert.deepEqual(accepted.placement.loanSequences[0].edges, [
    "0.complete->1.start",
    "1.complete->2.start",
  ])
  assert.equal(accepted.placement.loanSequences[0].closed, true)

  const unsupported = deriveExecutionErgonomics(source, {
    domainCapabilities: { ".catalog": ["concurrent"] },
  })
  assert.ok(summarizeDiagnostics(unsupported).includes("W-PLACEMENT-0003"))

  const open = deriveExecutionErgonomics(source, {
    domainCapabilities: { ".catalog": ["concurrent", "barrierDispatch"] },
    openPlaces: ["menu"],
  })
  assert.ok(summarizeDiagnostics(open).includes("W-OWNERSHIP-0012"))
})

test("barrier bodies cannot suspend and serial domains accept the marker", () => {
  const suspending = deriveExecutionErgonomics(`
    fn write(state: inout Menu) { await execution#yield(); return state.revision }
    let update = spawn<.catalog, .barrier> write(inout menu)
  `, { domainCapabilities: { ".catalog": ["concurrent", "barrierDispatch"] } })
  assert.ok(summarizeDiagnostics(suspending).includes("W-SUSPEND-0004"))

  const serial = deriveExecutionErgonomics(`
    module kitchen<domains: [.serial(.thermal)]>
    fn write(state: inout Menu) { return state.revision }
    let update = spawn<.thermal, .barrier> write(inout menu)
  `)
  assert.deepEqual(summarizeDiagnostics(serial), [])
  assert.equal(serial.placement.dispatches[0].barrierSupport, "serial")
})

test("dynamic serial lanes reuse a pool and release bounded reservations after drain", () => {
  const profile = {
    enabled: true,
    pool: "cpu",
    liveLimit: 4,
    aggregateReadyJobs: 64,
    aggregateFrameBytes: 1_048_576,
    laneMaximumJobs: 16,
    laneMaximumFrameBytes: 262_144,
  }
  const accepted = deriveExecutionErgonomics("", {
    dynamicSerial: {
      authority: true,
      profile,
      live: 2,
      aggregateReadyUsed: 16,
      aggregateFrameBytesUsed: 131_072,
      request: { kind: "serial", readyJobs: 8, frameBytes: 65_536 },
      operations: [
        { op: "open" },
        { op: "admit", job: "order", input: "order-owner", frameBytes: 4_096 },
        { op: "admit", job: "dessert", input: "dessert-owner", frameBytes: 8_192 },
        { op: "start", job: "order" },
        { op: "suspend", job: "order" },
        { op: "close" },
        { op: "start", job: "dessert" },
        { op: "complete", job: "dessert" },
        { op: "resume", job: "order" },
        { op: "complete", job: "order" },
        { op: "drain" },
      ],
    },
  }).dynamicSerial
  assert.equal(accepted.status, "accepted")
  assert.equal(accepted.phase, "drained")
  assert.equal(accepted.poolReuse, true)
  assert.equal(accepted.referenceExtendsOwner, false)
  assert.equal(accepted.state.live, 2)
  assert.equal(accepted.state.aggregateReadyUsed, 16)
  assert.equal(accepted.state.aggregateFrameBytesUsed, 131_072)
  assert.deepEqual(accepted.state.readyQueue, [])
  assert.equal(accepted.state.activeSegment, null)
  assert.deepEqual(accepted.trace.map((event) => event.operation), [
    "open", "admit", "admit", "start", "suspend", "close", "start", "complete", "resume", "complete", "drain",
  ])

  const closed = deriveExecutionErgonomics("", {
    dynamicSerial: {
      authority: true,
      profile,
      request: { kind: "serial", readyJobs: 8, frameBytes: 65_536 },
      operations: [
        { op: "open" },
        { op: "close" },
        { op: "admit", job: "late", input: "late-owner", frameBytes: 1_024 },
      ],
    },
  }).dynamicSerial
  assert.equal(closed.status, "rejected")
  assert.equal(closed.error, "closedDomain")
  assert.equal(closed.recoveredInput, "late-owner")

  const pending = deriveExecutionErgonomics("", {
    dynamicSerial: {
      authority: true,
      profile,
      request: { kind: "serial", readyJobs: 8, frameBytes: 65_536 },
      operations: [
        { op: "open" },
        { op: "admit", job: "waiting", input: "waiting-owner", frameBytes: 1_024 },
        { op: "start", job: "waiting" },
        { op: "suspend", job: "waiting" },
        { op: "close" },
        { op: "drain" },
      ],
    },
  }).dynamicSerial
  assert.equal(pending.error, "drainPending")
  assert.equal(pending.state.aggregateReadyUsed, 8)
  assert.equal(pending.state.aggregateFrameBytesUsed, 65_536)
})

test("dynamic serial lanes start FIFO and never overlap runnable segments", () => {
  const profile = {
    enabled: true,
    pool: "cpu",
    liveLimit: 4,
    aggregateReadyJobs: 64,
    aggregateFrameBytes: 1_048_576,
    laneMaximumJobs: 16,
    laneMaximumFrameBytes: 262_144,
  }
  const base = {
    authority: true,
    profile,
    request: { kind: "serial", readyJobs: 8, frameBytes: 65_536 },
  }
  const fifo = deriveExecutionErgonomics("", {
    dynamicSerial: {
      ...base,
      operations: [
        { op: "open" },
        { op: "admit", job: "first", input: "first-owner", frameBytes: 1_024 },
        { op: "admit", job: "second", input: "second-owner", frameBytes: 1_024 },
        { op: "start", job: "second" },
      ],
    },
  }).dynamicSerial
  assert.equal(fifo.error, "fifoViolation")

  const overlap = deriveExecutionErgonomics("", {
    dynamicSerial: {
      ...base,
      operations: [
        { op: "open" },
        { op: "admit", job: "first", input: "first-owner", frameBytes: 1_024 },
        { op: "admit", job: "second", input: "second-owner", frameBytes: 1_024 },
        { op: "start", job: "first" },
        { op: "start", job: "second" },
      ],
    },
  }).dynamicSerial
  assert.equal(overlap.error, "laneBusy")
})

test("dynamic serial lanes enforce frame reservations", () => {
  const profile = {
    enabled: true,
    pool: "cpu",
    liveLimit: 4,
    aggregateReadyJobs: 64,
    aggregateFrameBytes: 196_608,
    laneMaximumJobs: 16,
    laneMaximumFrameBytes: 131_072,
  }
  const aggregate = deriveExecutionErgonomics("", {
    dynamicSerial: {
      authority: true,
      profile,
      aggregateFrameBytesUsed: 163_840,
      request: { kind: "serial", readyJobs: 8, frameBytes: 65_536 },
      operations: [{ op: "open" }],
    },
  }).dynamicSerial
  assert.equal(aggregate.error, "aggregateFrameBytesExhausted")

  const admission = deriveExecutionErgonomics("", {
    dynamicSerial: {
      authority: true,
      profile,
      request: { kind: "serial", readyJobs: 8, frameBytes: 4_096 },
      operations: [
        { op: "open" },
        { op: "admit", job: "first", input: "first-owner", frameBytes: 3_072 },
        { op: "admit", job: "second", input: "second-owner", frameBytes: 2_048 },
      ],
    },
  }).dynamicSerial
  assert.equal(admission.error, "frameBudgetExhausted")
  assert.equal(admission.recoveredInput, "second-owner")
})

test("execution root is contextual, target gated, and nonescaping", () => {
  const root = deriveExecutionErgonomics("entry { let clock = execution.clock(); execution#checkCancellation() }", { root: true, profile: "native-process" })
  assert.deepEqual(root.execution.members, ["clock"])
  assert.deepEqual(root.execution.facets, ["checkCancellation"])
  assert.deepEqual(root.execution.diagnostics, [])
  const nonRoot = deriveExecutionErgonomics("module library\nlet clock = execution.clock()", { root: false, profile: "library" })
  assert.ok(summarizeDiagnostics(nonRoot).includes("W-EXECUTION-0001"))
  const escape = deriveExecutionErgonomics("entry(run)\nfn save() { return execution }", { root: true, profile: "native-process" })
  assert.ok(summarizeDiagnostics(escape).includes("W-EXECUTION-0002"))
  const service = deriveExecutionErgonomics("entry(run)\nservice.send(execution)", { root: true, profile: "native-process" })
  assert.ok(service.execution.diagnostics.some((diagnostic) => diagnostic.reason === "service crossing"))
  const serialized = deriveExecutionErgonomics("entry(run)\nserialize(execution)", { root: true, profile: "native-process" })
  assert.ok(serialized.execution.diagnostics.some((diagnostic) => diagnostic.reason === "serialization"))
  const unavailable = deriveExecutionErgonomics("entry(run)\nawait execution#yield()", {
    root: true,
    profile: "fpga-region",
    unavailableMembers: ["yield"],
  })
  assert.ok(summarizeDiagnostics(unavailable).includes("W-EXECUTION-0003"))
})

test("doctest terminal and effects are derived from comment input", () => {
  const positive = deriveExecutionErgonomics("/// @example\n/// call: clamp(2)\n/// result: 2\nfn clamp(value) { return value }")
  assert.equal(positive.doctest.hermetic, true)
  const duplicate = deriveExecutionErgonomics("/// @example\n/// call: clamp(2)\n/// result: 2\n/// error: RangeError\nfn clamp(value) { return value }")
  assert.ok(summarizeDiagnostics(duplicate).includes("W-DOC-0003"))
  const ambient = deriveExecutionErgonomics("/// @example\n/// call: readClock(execution.clock())\n/// result: \"data\"\nfn readClock(clock) { return \"data\" }")
  assert.ok(summarizeDiagnostics(ambient).includes("W-DOC-0005"))
  const twoBlocks = deriveExecutionErgonomics("/**\n * @example\n * call: first()\n * result: 1\n */\nfn first() { return 1 }\n/**\n * @example\n * call: second()\n * result: 2\n */\nfn second() { return 2 }")
  assert.equal(twoBlocks.doctest.examples.length, 2)
  assert.deepEqual(twoBlocks.doctest.examples.map((example) => example.call), ["first()", "second()"])
})

test("flat std derives authority and rejects tier fields", () => {
  const result = deriveExecutionErgonomics("std.io", {
    std: {
      modules: [
        { id: "std.io", targetFacts: [], capabilities: [], provider: "missing", reachability: "reachable" },
        { id: "std.tensor", targetFacts: [], capabilities: ["device"], provider: "missing", reachability: "unreachable" },
      ],
    },
  })
  assert.equal(result.std.hasTierField, false)
  assert.deepEqual(result.std.authorities, ["targetFacts", "capabilities", "provider", "reachability"])
  const invalid = deriveExecutionErgonomics("std.io", { std: { modules: [{ id: "std.io", tier: "old" }] } })
  assert.equal(invalid.std.hasTierField, true)
})

test("allocator contextual and ordinary declarations expose distinct call shapes", () => {
  const contextual = deriveExecutionErgonomics(`
    fn stage(allocator memory: ref Allocator, payload: ref Bytes) {}
    fn caller(allocator current: ref Allocator, payload: ref Bytes) { stage(payload) }
  `)
  const stage = contextual.labels.declarations.find((declaration) => declaration.name === "stage")
  assert.equal(stage.params[0].contextualAllocator, true)
  assert.deepEqual(stage.callShapes, ["positional", "allocator:|positional"])
  assert.deepEqual(contextual.labels.diagnostics, [])

  const ordinary = deriveExecutionErgonomics(`
    fn ordinary(allocator: ref Allocator, payload: ref Bytes) {}
    fn caller(payload: ref Bytes) { ordinary(payload) }
  `)
  const ordinaryDeclaration = ordinary.labels.declarations.find((declaration) => declaration.name === "ordinary")
  assert.equal(ordinaryDeclaration.params[0].contextualAllocator, undefined)
  assert.deepEqual(ordinaryDeclaration.callShapes, ["positional|positional"])
  assert.ok(summarizeDiagnostics(ordinary).includes("W-LABEL-0005"))
})

test("allocator contextual omission collision is derived from declarations", () => {
  const result = deriveExecutionErgonomics(`
    fn decode(allocator memory: ref Allocator, payload: ref Bytes) {}
    fn decode(payload: ref Bytes) {}
  `)
  const declarations = result.labels.declarations.filter((declaration) => declaration.name === "decode")
  assert.deepEqual(declarations.map((declaration) => declaration.callShapes), [
    ["positional", "allocator:|positional"],
    ["positional"],
  ])
  assert.ok(summarizeDiagnostics(result).includes("W-LABEL-0004"))
})
