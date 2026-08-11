import assert from "node:assert/strict"
import test from "node:test"
import corpus from "./kernel-module-cases.json" with { type: "json" }
import { deriveKernelModuleContract } from "./kernel-module-machine.mjs"

function module(overrides = {}) {
  return { ...structuredClone(corpus.module), ...overrides }
}

function forecastArguments() {
  return structuredClone(corpus.forecastArguments)
}

test("physical checkout paths do not affect module identity", () => {
  const left = deriveKernelModuleContract({ subject: "module", module: module({ physicalPath: "D:/a.w" }) })
  const right = deriveKernelModuleContract({ subject: "module", module: module({ physicalPath: "E:/b.w" }) })
  assert.equal(left.accepted, true)
  assert.equal(left.identity, right.identity)
})

test("field names and normalized HIR affect distinct identities", () => {
  const base = deriveKernelModuleContract({ subject: "module", module: module() })
  const changed = module()
  changed.fields[0].name = "forecastV2"
  const renamed = deriveKernelModuleContract({ subject: "module", module: changed })
  assert.notEqual(base.interfaceIdentity, renamed.interfaceIdentity)
  assert.notEqual(base.identity, renamed.identity)

  const reorderedFields = module()
  reorderedFields.fields.reverse()
  const reorderedResult = deriveKernelModuleContract({
    subject: "module",
    module: reorderedFields,
  })
  assert.notEqual(base.interfaceIdentity, reorderedResult.interfaceIdentity)

  const changedHir = module()
  changedHir.fields[0].hirDigest =
    "sha256:9999999999999999999999999999999999999999999999999999999999999999"
  const changedHirResult = deriveKernelModuleContract({
    subject: "module",
    module: changedHir,
  })
  assert.equal(base.interfaceIdentity, changedHirResult.interfaceIdentity)
  assert.notEqual(base.implementationIdentity, changedHirResult.implementationIdentity)

  const renamedCallable = module()
  renamedCallable.fields[0].callableId = "last_light.ai_harness::forecastKernelRenamed"
  const renamedCallableResult = deriveKernelModuleContract({
    subject: "module",
    module: renamedCallable,
  })
  assert.equal(base.interfaceIdentity, renamedCallableResult.interfaceIdentity)
  assert.notEqual(base.implementationIdentity, renamedCallableResult.implementationIdentity)
})

test("effect order is normalized and the transitive call graph is implementation identity", () => {
  const base = deriveKernelModuleContract({ subject: "module", module: module() })
  const reordered = module()
  reordered.fields[0].effects.reverse()
  const reorderedResult = deriveKernelModuleContract({ subject: "module", module: reordered })
  assert.equal(base.identity, reorderedResult.identity)

  const changedGraph = module()
  changedGraph.fields[0].callGraphDigest =
    "sha256:9999999999999999999999999999999999999999999999999999999999999999"
  const changedGraphResult = deriveKernelModuleContract({ subject: "module", module: changedGraph })
  assert.equal(base.interfaceIdentity, changedGraphResult.interfaceIdentity)
  assert.notEqual(base.implementationIdentity, changedGraphResult.implementationIdentity)

  const duplicate = module()
  duplicate.fields[0].effects = ["tensor-read", "tensor-read"]
  assert.equal(
    deriveKernelModuleContract({ subject: "module", module: duplicate }).error,
    "W-KERNEL-0004",
  )
})

test("closures and suspending kernels fail before artifact work", () => {
  assert.equal(
    deriveKernelModuleContract({
      subject: "module",
      module: module({ conformanceOrigin: "user" }),
    }).error,
    "W-KERNEL-0001",
  )
  const captured = module()
  captured.fields[0].captures = ["weights"]
  assert.equal(
    deriveKernelModuleContract({ subject: "module", module: captured }).error,
    "W-KERNEL-0003",
  )
  const suspending = module()
  suspending.fields[0].suspension = "may"
  assert.equal(
    deriveKernelModuleContract({ subject: "module", module: suspending }).error,
    "W-KERNEL-0004",
  )
})

test("a specialization identity is stable and label order is exact", () => {
  const input = {
    subject: "specialization",
    module: module(),
    field: "forecast",
    staticArguments: forecastArguments(),
  }
  const first = deriveKernelModuleContract(input)
  const second = deriveKernelModuleContract(structuredClone(input))
  assert.equal(first.instanceIdentity, second.instanceIdentity)
  input.staticArguments.reverse()
  assert.equal(deriveKernelModuleContract(input).error, "W-KERNEL-0005")
})

test("type and value arguments both participate in specialization identity", () => {
  const generic = module()
  generic.fields[0].staticParameters = [
    {
      name: "Element",
      domain: "type",
      constraintDigest:
        "sha256:7777777777777777777777777777777777777777777777777777777777777777",
    },
    {
      name: "rows",
      domain: "value",
      typeIdentity:
        "sha256:3333333333333333333333333333333333333333333333333333333333333333",
    },
  ]
  const argumentsBase = [
    {
      name: "Element",
      origin: "type",
      typeIdentity:
        "sha256:8888888888888888888888888888888888888888888888888888888888888888",
    },
    {
      name: "rows",
      origin: "const",
      typeIdentity:
        "sha256:3333333333333333333333333333333333333333333333333333333333333333",
      constIdentity:
        "sha256:4444444444444444444444444444444444444444444444444444444444444444",
    },
  ]
  const first = deriveKernelModuleContract({
    subject: "specialization",
    module: generic,
    field: "forecast",
    staticArguments: argumentsBase,
  })
  const changedArguments = structuredClone(argumentsBase)
  changedArguments[0].typeIdentity =
    "sha256:9999999999999999999999999999999999999999999999999999999999999999"
  const changed = deriveKernelModuleContract({
    subject: "specialization",
    module: generic,
    field: "forecast",
    staticArguments: changedArguments,
  })
  assert.equal(first.accepted, true)
  assert.equal(changed.accepted, true)
  assert.notEqual(first.instanceIdentity, changed.instanceIdentity)

  const changedValue = structuredClone(argumentsBase)
  changedValue[1].constIdentity =
    "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  const changedValueResult = deriveKernelModuleContract({
    subject: "specialization",
    module: generic,
    field: "forecast",
    staticArguments: changedValue,
  })
  assert.notEqual(first.instanceIdentity, changedValueResult.instanceIdentity)
})

test("a closed artifact contains every reachable specialization without JIT", () => {
  const specialization = { field: "forecast", staticArguments: forecastArguments() }
  const empty = deriveKernelModuleContract({
    subject: "artifact",
    artifactClass: "closed",
    module: module(),
    reachable: [],
    materialized: [],
    target: structuredClone(corpus.target),
  })
  assert.equal(empty.error, "W-KERNEL-0006")

  const accepted = deriveKernelModuleContract({
    subject: "artifact",
    artifactClass: "closed",
    module: module(),
    reachable: [specialization],
    materialized: [specialization],
    target: structuredClone(corpus.target),
  })
  assert.equal(accepted.accepted, true)
  assert.equal(accepted.runtimeJit, false)

  const missing = deriveKernelModuleContract({
    subject: "artifact",
    artifactClass: "closed",
    module: module(),
    reachable: [specialization],
    materialized: [],
    target: structuredClone(corpus.target),
  })
  assert.equal(missing.error, "W-KERNEL-0006")
})

test("a source-backed family is reproducible but not launchable", () => {
  const input = {
    subject: "artifact",
    artifactClass: "sourceBacked",
    module: module(),
    sourceRecipeDigest:
      "sha256:1212121212121212121212121212121212121212121212121212121212121212",
    targetConstraintDigest:
      "sha256:1313131313131313131313131313131313131313131313131313131313131313",
    reachable: [],
    materialized: [],
  }
  const first = deriveKernelModuleContract(input)
  const second = deriveKernelModuleContract(structuredClone(input))
  assert.equal(first.accepted, true)
  assert.equal(first.launchable, false)
  assert.equal(first.artifactIdentity, second.artifactIdentity)

  input.reachable = [{ field: "forecast", staticArguments: forecastArguments() }]
  assert.equal(deriveKernelModuleContract(input).error, "W-KERNEL-0007")
})

test("artifact identity canonicalizes reachable instance order", () => {
  const forecast = { field: "forecast", staticArguments: forecastArguments() }
  const normalize = {
    field: "normalize",
    staticArguments: structuredClone(corpus.normalizeArguments),
  }
  const first = deriveKernelModuleContract({
    subject: "artifact",
    artifactClass: "closed",
    module: module(),
    reachable: [forecast, normalize],
    materialized: [forecast, normalize],
    target: structuredClone(corpus.target),
  })
  const reordered = deriveKernelModuleContract({
    subject: "artifact",
    artifactClass: "closed",
    module: module(),
    reachable: [normalize, forecast],
    materialized: [normalize, forecast],
    target: structuredClone(corpus.target),
  })
  assert.equal(first.accepted, true)
  assert.equal(first.artifactIdentity, reordered.artifactIdentity)
})
