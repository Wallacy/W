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

test("complete ordered call shapes separate arities and reject real overlap", () => {
  const disjoint = deriveExecutionErgonomics("fn serve(_ value: Value) {}\nfn serve(_ value: Value, mode: Mode) {}")
  const declarations = disjoint.labels.declarations.filter((declaration) => declaration.name === "serve")
  assert.deepEqual(declarations[0].callShapes, ["positional", "value:"])
  assert.deepEqual(declarations[1].callShapes, ["positional|mode:", "value:|mode:"])
  assert.deepEqual(disjoint.labels.diagnostics, [])
  const collision = deriveExecutionErgonomics("fn serve(_ value: Value) {}\nfn serve(value: Value) {}")
  assert.ok(summarizeDiagnostics(collision).includes("W-LABEL-0004"))
})

test("suspension is inferred monotonically and child accepts sync or may", () => {
  const result = deriveExecutionErgonomics(`
    fn syncWorker(value) { return value }
    fn asyncWorker(value) { await Task.yield(); return value }
    fn caller(value) {
      async let local = syncWorker(value)
      spawn<.compute> let remote = asyncWorker(value)
      return try await (local, remote)
    }
    fn even(n) { if n == 0 { return true }; return await odd(n - 1) }
    fn odd(n) { await Task.yield(); return if n == 0 { false } else { await even(n - 1) } }
  `, { publicContract: { previous: "never", current: "may", exported: true } })
  assert.deepEqual(result.forms, {
    direct: "same-task/neverSuspend",
    await: "same-task/maySuspend",
    "async let": "structured-child/current-domain",
    spawn: "structured-child/parallel-intent",
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

test("placement is caller choice and serial spawn is rejected", () => {
  const declaration = deriveExecutionErgonomics("spawn<.network> fn fetch(request) { return request }")
  assert.ok(summarizeDiagnostics(declaration).includes("W-PLACEMENT-0001"))
  const serial = deriveExecutionErgonomics("module kitchen<domains: [.serial(.thermal)]>\nspawn<.compute> let work = f()")
  assert.deepEqual(summarizeDiagnostics(serial), [])
  const serialConflict = deriveExecutionErgonomics("module kitchen<domains: [.serial(.thermal)]>\nspawn<.thermal> let work = f()")
  assert.ok(summarizeDiagnostics(serialConflict).includes("W-PLACEMENT-0002"))
  const forms = deriveExecutionErgonomics("spawn<.compute> let a = f()\nspawn<domain: .compute> let b = f()")
  assert.equal(forms.placement.sameOptionalDomainForm, true)
})

test("process projections are explicit, profile gated, and nonescaping", () => {
  const root = deriveExecutionErgonomics("let args = process.args\nlet context = process.context", { root: true, profile: "native-process" })
  assert.deepEqual(root.process.projections, ["args", "context"])
  assert.deepEqual(root.process.diagnostics, [])
  const nonRoot = deriveExecutionErgonomics("let context = process.context", { root: false, profile: "library" })
  assert.ok(summarizeDiagnostics(nonRoot).includes("W-PROCESS-0001"))
  const escape = deriveExecutionErgonomics("entry(run)\nfn save() { return ref process.context }", { root: true, profile: "native-process" })
  assert.ok(summarizeDiagnostics(escape).includes("W-PROCESS-0002"))
  const service = deriveExecutionErgonomics("entry(run)\nservice.send(process.context)", { root: true, profile: "native-process" })
  assert.ok(service.process.diagnostics.some((diagnostic) => diagnostic.reason === "service crossing"))
  const serialized = deriveExecutionErgonomics("entry(run)\nserialize(process.context)", { root: true, profile: "native-process" })
  assert.ok(serialized.process.diagnostics.some((diagnostic) => diagnostic.reason === "serialization"))
  const alias = deriveExecutionErgonomics("entry(run)\nlet value = process.ctx", { root: true, profile: "native-process" })
  assert.ok(summarizeDiagnostics(alias).includes("W-PROCESS-0003"))
})

test("doctest terminal and effects are derived from comment input", () => {
  const positive = deriveExecutionErgonomics("/// @example\n/// call: clamp(2)\n/// result: 2\nfn clamp(value) { return value }")
  assert.equal(positive.doctest.hermetic, true)
  const duplicate = deriveExecutionErgonomics("/// @example\n/// call: clamp(2)\n/// result: 2\n/// error: RangeError\nfn clamp(value) { return value }")
  assert.ok(summarizeDiagnostics(duplicate).includes("W-DOC-0003"))
  const ambient = deriveExecutionErgonomics("/// @example\n/// call: readFile(process.args)\n/// result: \"data\"\nfn readFile(path) { return path }")
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
