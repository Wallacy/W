import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { runServiceRecoveryOperations } from "./service-recovery-machine.mjs";
import { runModuleRunProgram } from "./module-run-machine.mjs";
import { runPresentationProgram } from "./presentation-machine.mjs";
import { runDLPackProgram, compactDLPackState } from "./dlpack-machine.mjs";
import { runLazyBehaviorOperations } from "./lazy-behavior-machine.mjs";
import { runAllocatorScope } from "./allocator-scope-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");

function clone(value) {
  return value === undefined ? undefined : structuredClone(value);
}

function readJson(relativePath) {
  return JSON.parse(fs.readFileSync(path.resolve(repositoryRoot, relativePath), "utf8"));
}

function expandServiceOperations(corpus, item) {
  const operations = [];
  for (const operation of item.operations ?? []) {
    if (typeof operation.$use !== "string") {
      operations.push(clone(operation));
      continue;
    }
    const fixture = corpus.fixtures?.[operation.$use];
    if (!Array.isArray(fixture)) throw new Error(`missing service fixture ${operation.$use}`);
    for (const fragment of fixture) {
      const override = clone(operation.with) ?? {};
      operations.push({
        ...clone(fragment),
        ...override,
        ...(override.provider ? { provider: { ...fragment.provider, ...override.provider } } : {}),
        ...(override.limits ? { limits: { ...fragment.limits, ...override.limits } } : {}),
        ...(override.receipt ? { receipt: { ...fragment.receipt, ...override.receipt } } : {}),
        ...(override.evidence ? { evidence: { ...fragment.evidence, ...override.evidence } } : {}),
      });
    }
  }
  return operations;
}

function expandModuleOperations(corpus, item) {
  const operations = [];
  for (const fixtureName of item.fixtures ?? []) {
    const fixture = corpus.fixtures?.[fixtureName];
    if (!fixture || !Array.isArray(fixture.operations)) throw new Error(`missing module fixture ${fixtureName}`);
    operations.push(...fixture.operations.map(clone));
  }
  operations.push(...(item.operations ?? []).map(clone));
  return operations;
}

function expandLazyOperations(corpus, item) {
  const operations = [];
  for (const fixtureName of item.fixtures ?? []) {
    const fixture = corpus.fixtures?.[fixtureName];
    if (!Array.isArray(fixture)) throw new Error(`missing lazy fixture ${fixtureName}`);
    operations.push(...fixture.map(clone));
  }
  operations.push(...(item.operations ?? []).map(clone));
  return operations;
}

function runQuantityProgram(operations = []) {
  const state = {
    status: "accepted",
    canonicalSeconds: null,
    deltaK: null,
    referenceBits: null,
    exactBytes: null,
    jsonToken: null,
  };
  for (const operation of operations) {
    if (!operation || typeof operation.op !== "string") {
      return { status: "rejected", error: "operationMalformed", state };
    }
    if (operation.op === "durationCanonical") {
      if (!Number.isFinite(operation.seconds) || !Number.isFinite(operation.minutes)
        || operation.seconds !== operation.minutes * 60) {
        return { status: "rejected", error: "canonicalDurationMismatch", state };
      }
      const secondsBits = new Float64Array([operation.seconds]);
      const minutesBits = new Float64Array([operation.minutes * 60]);
      if (new Uint8Array(secondsBits.buffer).join(",") !== new Uint8Array(minutesBits.buffer).join(",")) {
        return { status: "rejected", error: "canonicalBitsMismatch", state };
      }
      state.canonicalSeconds = operation.seconds;
    } else if (operation.op === "temperatureDelta") {
      if (!Number.isFinite(operation.openingC) || !Number.isFinite(operation.closingC)) {
        return { status: "rejected", error: "temperatureInputInvalid", state };
      }
      const openingHundredths = Math.round(operation.openingC * 100) + 27315;
      const closingHundredths = Math.round(operation.closingC * 100) + 27315;
      state.deltaK = (openingHundredths - closingHundredths) / 100;
    } else if (operation.op === "memoryExactBytes") {
      if (operation.unit !== "iec.KiB" || !Number.isSafeInteger(operation.quantity) || operation.quantity < 0) {
        return { status: "rejected", error: "informationUnitInvalid", state };
      }
      state.referenceBits = operation.quantity * 1024 * 8;
      state.exactBytes = state.referenceBits / 8;
      if (!Number.isSafeInteger(state.referenceBits) || !Number.isSafeInteger(state.exactBytes)) {
        return { status: "rejected", error: "informationRange", state };
      }
    } else if (operation.op === "jsonToken") {
      state.jsonToken = operation.token;
      if (operation.token !== "s") return { status: "rejected", error: "fixedUnitToken", token: operation.token, state };
    } else {
      return { status: "rejected", error: "operationUnknown", state };
    }
  }
  return { ...state, status: "accepted" };
}

function normalizeMachineResult(kind, result) {
  if (kind === "allocator") return { status: result.accepted ? "accepted" : "rejected", state: result };
  if (kind === "dlpack") {
    const error = result.error?.code ?? null;
    return { status: result.status, error, state: compactDLPackState(result.state), result: result.result };
  }
  if (kind === "presentation") {
    return { status: result.status, error: result.error?.code ?? null, state: result.state };
  }
  if (kind === "module-run") {
    return { status: result.status, error: result.code ?? null, operation: result.operation ?? null, state: result.state };
  }
  if (kind === "service-recovery") {
    return { status: result.status, error: result.error ?? null, state: result.state, physical: result.physical };
  }
  if (kind === "lazy") {
    return { status: result.status, error: result.error ?? null, state: result.state, physical: result.physical };
  }
  return result;
}

export function runPRC0SourceCase(definition, sourceCorpus, sourceCase) {
  const kind = definition.source.kind;
  if (kind === "quantity") return runQuantityProgram(definition.operations);
  if (kind === "service-recovery") return normalizeMachineResult(kind, runServiceRecoveryOperations(expandServiceOperations(sourceCorpus, sourceCase)));
  if (kind === "module-run") return normalizeMachineResult(kind, runModuleRunProgram(expandModuleOperations(sourceCorpus, sourceCase)));
  if (kind === "presentation") return normalizeMachineResult(kind, runPresentationProgram(sourceCase.operations ?? []));
  if (kind === "dlpack") return normalizeMachineResult(kind, runDLPackProgram(sourceCase.operations ?? []));
  if (kind === "lazy") return normalizeMachineResult(kind, runLazyBehaviorOperations(expandLazyOperations(sourceCorpus, sourceCase)));
  if (kind === "allocator") return normalizeMachineResult(kind, runAllocatorScope(sourceCase.input));
  throw new Error(`unknown PRC0 source kind ${kind}`);
}

export function loadPRC0Source(definition) {
  if (definition.source.kind === "quantity") return { corpus: null, case: null };
  const corpus = readJson(definition.source.corpus);
  const sourceCase = (corpus.cases ?? []).find((item) => item.id === definition.source.caseId);
  if (!sourceCase) throw new Error(`missing source case ${definition.source.caseId}`);
  return { corpus, case: sourceCase };
}

export function runPRC0Case(definition) {
  const { corpus, case: sourceCase } = loadPRC0Source(definition);
  return runPRC0SourceCase(definition, corpus, sourceCase);
}
