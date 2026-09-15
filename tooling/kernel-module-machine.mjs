import { createHash } from "node:crypto"

const ALLOWED_EFFECTS = new Set([
  "arithmetic",
  "tensor-read",
  "tensor-write",
  "tensor-result",
  "device-intrinsic",
])
const PARAMETER_DOMAINS = new Set(["type", "value"])
const NUMERIC_MODES = new Set(["strict", "reproducible", "fast"])
const IDENTIFIER_ENCODER = new TextEncoder()

class KernelModuleError extends Error {
  constructor(code) {
    super(code)
    this.code = code
  }
}

function fail(code) {
  throw new KernelModuleError(code)
}

function digest(value) {
  return `sha256:${createHash("sha256").update(JSON.stringify(value)).digest("hex")}`
}

function requireString(value, code) {
  if (typeof value !== "string" || value.length === 0) fail(code)
}

function requireDigest(value, code) {
  if (!/^sha256:[0-9a-f]{64}$/.test(value ?? "")) fail(code)
}

function compareIdentifier(left, right) {
  const leftBytes = IDENTIFIER_ENCODER.encode(left)
  const rightBytes = IDENTIFIER_ENCODER.encode(right)
  const length = Math.min(leftBytes.length, rightBytes.length)
  for (let index = 0; index < length; index += 1) {
    if (leftBytes[index] !== rightBytes[index]) return leftBytes[index] - rightBytes[index]
  }
  return leftBytes.length - rightBytes.length
}

function normalizeStaticParameters(parameters) {
  if (!Array.isArray(parameters)) fail("W-KERNEL-0002")
  const names = new Set()
  return parameters.map((parameter) => {
    requireString(parameter?.name, "W-KERNEL-0002")
    if (names.has(parameter.name) || !PARAMETER_DOMAINS.has(parameter.domain)) {
      fail("W-KERNEL-0002")
    }
    names.add(parameter.name)
    if (parameter.domain === "type") {
      requireDigest(parameter.constraintDigest, "W-KERNEL-0002")
      return {
        name: parameter.name,
        domain: "type",
        constraintDigest: parameter.constraintDigest,
      }
    }
    requireDigest(parameter.typeIdentity, "W-KERNEL-0002")
    return { name: parameter.name, domain: "value", typeIdentity: parameter.typeIdentity }
  })
}

function normalizeField(field) {
  requireString(field?.name, "W-KERNEL-0002")
  requireString(field?.callableId, "W-KERNEL-0003")
  requireDigest(field?.interfaceDigest, "W-KERNEL-0003")
  requireDigest(field?.hirDigest, "W-KERNEL-0003")
  requireDigest(field?.callGraphDigest, "W-KERNEL-0003")
  if (field.directSymbol !== true || field.runtimeLookup === true) fail("W-KERNEL-0003")
  if (!Array.isArray(field.captures) || field.captures.length !== 0) fail("W-KERNEL-0003")
  if (field.suspension !== "never" || field.failure !== "never") fail("W-KERNEL-0004")
  if (
    !Array.isArray(field.effects) ||
    field.effects.some((effect) => !ALLOWED_EFFECTS.has(effect)) ||
    new Set(field.effects).size !== field.effects.length
  ) {
    fail("W-KERNEL-0004")
  }
  return {
    name: field.name,
    callableId: field.callableId,
    interfaceDigest: field.interfaceDigest,
    hirDigest: field.hirDigest,
    callGraphDigest: field.callGraphDigest,
    staticParameters: normalizeStaticParameters(field.staticParameters ?? []),
    effects: [...field.effects].sort(),
  }
}

function normalizeModule(module) {
  if (
    module?.contract !== "module-kernels@1" ||
    module.declarationKind !== "module-header" ||
    module.conformanceOrigin !== "compiler"
  ) {
    fail("W-KERNEL-0001")
  }
  requireString(module.moduleId, "W-KERNEL-0001")
  if (module.familyKind !== "kernel-family" || module.runtimeLookup === true) {
    fail("W-KERNEL-0002")
  }
  if (!Array.isArray(module.fields) || module.fields.length === 0) fail("W-KERNEL-0002")

  const fields = module.fields.map(normalizeField)
  const names = new Set()
  for (const field of fields) {
    if (names.has(field.name)) fail("W-KERNEL-0002")
    names.add(field.name)
  }
  // Public labels define canonical interface order.  Source reorder changes
  // provenance only; it must not change ModuleIdentity or ordinals.
  fields.sort((left, right) => compareIdentifier(left.name, right.name))

  const interfaceIdentity = digest({
    contract: module.contract,
    moduleId: module.moduleId,
    conformanceOrigin: module.conformanceOrigin,
    fields: fields.map((field) => ({
      name: field.name,
      interfaceDigest: field.interfaceDigest,
      staticParameters: field.staticParameters,
      effects: field.effects,
    })),
  })
  const implementationIdentity = digest({
    interfaceIdentity,
    kernels: fields.map((field) => ({
      callableId: field.callableId,
      hirDigest: field.hirDigest,
      callGraphDigest: field.callGraphDigest,
    })),
  })
  const identity = digest({ interfaceIdentity, implementationIdentity })
  if (module.claimedIdentity !== undefined && module.claimedIdentity !== identity) {
    fail("W-KERNEL-0002")
  }
  return {
    contract: module.contract,
    moduleId: module.moduleId,
    fields,
    interfaceIdentity,
    implementationIdentity,
    identity,
  }
}

function normalizeArgument(parameter, argument) {
  if (argument?.name !== parameter.name) fail("W-KERNEL-0005")
  if (parameter.domain === "type") {
    if (argument.origin !== "type") fail("W-KERNEL-0005")
    requireDigest(argument.typeIdentity, "W-KERNEL-0005")
    return { name: parameter.name, domain: "type", typeIdentity: argument.typeIdentity }
  }
  if (argument.origin !== "const" || argument.typeIdentity !== parameter.typeIdentity) {
    fail("W-KERNEL-0005")
  }
  requireDigest(argument.constIdentity, "W-KERNEL-0005")
  return {
    name: parameter.name,
    domain: "value",
    typeIdentity: parameter.typeIdentity,
    constIdentity: argument.constIdentity,
  }
}

function specialize(module, fieldName, staticArguments) {
  const field = module.fields.find((candidate) => candidate.name === fieldName)
  if (!field || !Array.isArray(staticArguments)) fail("W-KERNEL-0005")
  if (staticArguments.length !== field.staticParameters.length) fail("W-KERNEL-0005")
  const argumentsNormalized = field.staticParameters.map((parameter, index) =>
    normalizeArgument(parameter, staticArguments[index]))
  const identity = digest({
    moduleIdentity: module.identity,
    field: field.name,
    callableId: field.callableId,
    staticArguments: argumentsNormalized,
  })
  return {
    field: field.name,
    callableId: field.callableId,
    staticArguments: argumentsNormalized,
    identity,
  }
}

function normalizeProjection(input, module) {
  requireString(input.modulePath, "W-KERNEL-0008")
  if (input.modulePath !== module.moduleId) fail("W-KERNEL-0008")
  if (!new Set(["named", "qualified"]).has(input.projectionKind)) {
    fail("W-KERNEL-0008")
  }
  if (!Array.isArray(input.items) || input.items.length === 0) {
    fail("W-KERNEL-0008")
  }

  const occupied = new Set()
  for (const name of input.localBindings ?? []) {
    requireString(name, "W-KERNEL-0008")
    if (occupied.has(name)) fail("W-KERNEL-0008")
    occupied.add(name)
  }

  let projectionAlias = null
  if (input.projectionKind === "qualified") {
    requireString(input.alias, "W-KERNEL-0008")
    if (occupied.has(input.alias)) fail("W-KERNEL-0008")
    occupied.add(input.alias)
    projectionAlias = input.alias
    if (!new Set(["spawn", "open"]).has(input.route)) fail("W-KERNEL-0008")
  } else if (input.route !== "spawn") {
    fail("W-KERNEL-0008")
  }

  const projections = []
  const fields = new Set()
  for (const item of input.items) {
    requireString(item?.field, "W-KERNEL-0008")
    if (fields.has(item.field)) fail("W-KERNEL-0008")
    fields.add(item.field)
    const localName = input.projectionKind === "named"
      ? (item.localName ?? item.field)
      : `${projectionAlias}.${item.field}`
    if (input.projectionKind === "named") {
      requireString(localName, "W-KERNEL-0008")
      if (occupied.has(localName)) fail("W-KERNEL-0008")
      occupied.add(localName)
    }
    const instance = specialize(module, item.field, item.staticArguments ?? [])
    projections.push({
      localName,
      field: instance.field,
      moduleIdentity: module.identity,
      kernelInstanceId: instance.identity,
      origin: {
        moduleIdentity: module.identity,
        kernelInstanceId: instance.identity,
      },
    })
  }

  return {
    moduleIdentity: module.identity,
    projectionKind: input.projectionKind,
    projectionAlias,
    route: input.projectionKind === "named"
      ? "spawn-immediate"
      : input.route === "open" ? "open-qualified" : "spawn-qualified",
    projectionIdentity: digest({
      moduleIdentity: module.identity,
      kernels: projections
        .map((projection) => ({
          field: projection.field,
          kernelInstanceId: projection.kernelInstanceId,
        }))
        .sort((left, right) =>
          compareIdentifier(left.field, right.field)
          || compareIdentifier(left.kernelInstanceId, right.kernelInstanceId)),
    }),
    provenanceIdentity: digest({
      projectionKind: input.projectionKind,
      modulePath: input.modulePath,
      alias: projectionAlias,
      projections: projections.map((projection) => ({
        localName: projection.localName,
        field: projection.field,
      })),
    }),
    projections,
    runtimeAllocation: false,
    runtimeReflection: false,
    authority: false,
    ordinaryBindingCreated: false,
    aliasesAffectIdentity: false,
  }
}

function targetFacts(target) {
  requireString(target?.backend, "W-KERNEL-0007")
  requireString(target?.triple, "W-KERNEL-0007")
  requireDigest(target?.featuresDigest, "W-KERNEL-0007")
  requireDigest(target?.providerAbiDigest, "W-KERNEL-0007")
  if (!NUMERIC_MODES.has(target.numericMode)) fail("W-KERNEL-0007")
  return {
    backend: target.backend,
    triple: target.triple,
    featuresDigest: target.featuresDigest,
    providerAbiDigest: target.providerAbiDigest,
    numericMode: target.numericMode,
  }
}

function deriveArtifact(input, module) {
  if (input.runtimeJit === true) fail("W-KERNEL-0007")
  if (input.artifactClass === "sourceBacked") {
    if ((input.reachable?.length ?? 0) !== 0 || (input.materialized?.length ?? 0) !== 0) {
      fail("W-KERNEL-0007")
    }
    if (input.target !== undefined) fail("W-KERNEL-0007")
    requireDigest(input.sourceRecipeDigest, "W-KERNEL-0007")
    requireDigest(input.targetConstraintDigest, "W-KERNEL-0007")
    return {
      accepted: true,
      artifactClass: "sourceBacked",
      moduleIdentity: module.identity,
      launchable: false,
      runtimeJit: false,
      artifactIdentity: digest({
        artifactClass: "sourceBacked",
        moduleIdentity: module.identity,
        sourceRecipeDigest: input.sourceRecipeDigest,
        targetConstraintDigest: input.targetConstraintDigest,
      }),
    }
  }
  if (input.artifactClass !== "closed") fail("W-KERNEL-0007")
  const reachable = (input.reachable ?? []).map((item) =>
    specialize(module, item.field, item.staticArguments))
  const materialized = (input.materialized ?? []).map((item) =>
    specialize(module, item.field, item.staticArguments))
  if (reachable.length === 0) fail("W-KERNEL-0006")
  if (new Set(reachable.map((item) => item.identity)).size !== reachable.length) {
    fail("W-KERNEL-0006")
  }
  if (new Set(materialized.map((item) => item.identity)).size !== materialized.length) {
    fail("W-KERNEL-0006")
  }
  const materializedIds = new Set(materialized.map((item) => item.identity))
  if (reachable.some((item) => !materializedIds.has(item.identity))) fail("W-KERNEL-0006")
  const target = targetFacts(input.target)
  const selected = materialized.filter((item) =>
    reachable.some((candidate) => candidate.identity === item.identity))
    .sort((left, right) => left.identity.localeCompare(right.identity))
  return {
    accepted: true,
    artifactClass: "closed",
    moduleIdentity: module.identity,
    launchable: true,
    reachableInstances: reachable.length,
    materializedInstances: selected.length,
    strippedInstances: materialized.length - selected.length,
    runtimeJit: false,
    artifactIdentity: digest({
      artifactClass: "closed",
      moduleIdentity: module.identity,
      instances: selected.map((item) => item.identity),
      target,
    }),
  }
}

export function deriveKernelModuleContract(input) {
  try {
    const module = normalizeModule(input.module)
    if (input.subject === "module") {
      return {
        accepted: true,
        identity: module.identity,
        interfaceIdentity: module.interfaceIdentity,
        implementationIdentity: module.implementationIdentity,
        fields: module.fields.map((field) => field.name),
        runtimeAllocation: false,
        runtimeReflection: false,
      }
    }
    if (input.subject === "specialization") {
      const instance = specialize(module, input.field, input.staticArguments)
      return {
        accepted: true,
        moduleIdentity: module.identity,
        instanceIdentity: instance.identity,
        field: instance.field,
        staticArguments: instance.staticArguments,
        usingFirst: true,
        preservesParameterModes: true,
        maySuspend: true,
        failure: "LaunchError",
        hiddenTransfer: false,
      }
    }
    if (input.subject === "projection") {
      return {
        accepted: true,
        ...normalizeProjection(input, module),
      }
    }
    if (input.subject === "artifact") return deriveArtifact(input, module)
    fail("W-KERNEL-0001")
  } catch (error) {
    if (!(error instanceof KernelModuleError)) throw error
    return { accepted: false, error: error.code }
  }
}

function corpusSpecialization(corpus, name) {
  if (name === "forecast") {
    return { field: "forecast", staticArguments: structuredClone(corpus.forecastArguments) }
  }
  if (name === "normalize") {
    return {
      field: "normalize",
      staticArguments: structuredClone(corpus.normalizeArguments),
    }
  }
  return { field: name, staticArguments: [] }
}

export function prepareKernelModuleCase(corpus, testCase) {
  const input = structuredClone(testCase.input)
  const module = structuredClone(corpus.module)
  Object.assign(module, input.modulePatch ?? {})
  if (input.duplicateField) {
    const field = module.fields.find((candidate) => candidate.name === input.duplicateField)
    module.fields.push(structuredClone(field))
  }
  if (input.fieldPatch) {
    const index = module.fields.findIndex((field) => field.name === input.fieldPatch.name)
    if (index >= 0) module.fields[index] = { ...module.fields[index], ...input.fieldPatch }
  }
  input.module = module
  if (input.subject === "specialization") {
    input.staticArguments ??= structuredClone(corpus.forecastArguments)
    if (input.argumentPatch) {
      const index = input.staticArguments.findIndex((argument) =>
        argument.name === input.argumentPatch.name)
      input.staticArguments[index] = { ...input.staticArguments[index], ...input.argumentPatch }
    }
    if (input.reverseArguments) input.staticArguments.reverse()
  }
  if (input.subject === "artifact") {
    input.reachable = (input.reachable ?? []).map((name) => corpusSpecialization(corpus, name))
    input.materialized = (input.materialized ?? []).map((name) => corpusSpecialization(corpus, name))
    if (input.artifactClass === "closed") {
      input.target = { ...structuredClone(corpus.target), ...(input.targetPatch ?? {}) }
    }
  }
  if (input.subject === "projection") {
    input.items = (input.items ?? []).map((item) => {
      if (item.specialization === undefined || item.staticArguments !== undefined) {
        return item
      }
      const specialization = corpusSpecialization(corpus, item.specialization)
      return {
        ...item,
        staticArguments: specialization.staticArguments,
      }
    })
  }
  delete input.modulePatch
  delete input.duplicateField
  delete input.fieldPatch
  delete input.argumentPatch
  delete input.reverseArguments
  delete input.targetPatch
  return input
}

export function countKernelModuleOperations(input) {
  return Object.keys(input).length + input.module.fields.length
    + (input.staticArguments?.length ?? 0)
    + (input.reachable?.length ?? 0)
    + (input.materialized?.length ?? 0)
}
