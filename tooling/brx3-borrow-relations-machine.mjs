import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { evaluateBorrowRelationCase } from "./brx2-borrow-relations-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "brx3-borrow-relations-cases.json");
const RESULT_SCHEMA = "w-brx3-borrow-relations-results-1";
const DECLARATION_OWNERS = new Set(["requirement", "interface"]);

function clone(value) {
  return value === undefined ? undefined : structuredClone(value);
}

function digest(value) {
  return "sha256:" + crypto.createHash("sha256").update(JSON.stringify(value)).digest("hex");
}

function stable(value) {
  if (Array.isArray(value)) return value.map(stable);
  if (!value || typeof value !== "object") return value;
  return Object.fromEntries(Object.keys(value).sort().map((key) => [key, stable(value[key])]));
}

function slotName(slot, index) {
  return typeof slot === "string" ? slot : String(slot?.slot ?? slot?.name ?? `parameter:${index}`);
}

function sourceIndex(ref, declaration) {
  const inputs = declaration.inputs ?? [];
  if (Number.isInteger(ref)) {
    if (ref < 0) return { error: { code: "borrowSourceSlotNegative", facts: { source: ref } } };
    if (ref >= inputs.length) return { error: { code: "borrowSourceSlotOutOfRange", facts: { source: ref, count: inputs.length } } };
    return { index: ref, slot: slotName(inputs[ref], ref) };
  }
  if (typeof ref !== "string" || ref.length === 0) {
    return { error: { code: "borrowSourceSlotInvalid", facts: { source: ref } } };
  }
  const index = inputs.findIndex((slot, ordinal) => slotName(slot, ordinal) === ref);
  if (index < 0) return { error: { code: "borrowSourceSlotUnknown", facts: { source: ref } } };
  return { index, slot: slotName(inputs[index], index) };
}

function resultIndex(ref, declaration) {
  const results = declaration.results ?? [];
  if (!Number.isInteger(ref)) return { error: { code: "borrowResultOrdinalRequired", facts: { result: ref } } };
  if (ref < 0) return { error: { code: "borrowResultSlotNegative", facts: { result: ref } } };
  if (ref >= results.length) return { error: { code: "borrowResultSlotOutOfRange", facts: { result: ref, count: results.length } } };
  return { index: ref, slot: slotName(results[ref], ref) };
}

function syntaxError(code, facts = {}) {
  return { code, facts };
}

/**
 * Resolve the source spelling into the data-only relation consumed by BRX2.
 * The source clause stays ordered. Canonical lowering below sorts ordinals.
 */
function lowerBorrowClause(input) {
  const declaration = clone(input.declaration ?? {});
  const clause = input.borrowClause;
  const diagnostics = [];
  if (clause === undefined || clause === null) return { declaration, diagnostics, canonical: null };
  if (clause.placement && clause.placement !== "declaration") {
    diagnostics.push(syntaxError("borrowClausePlacementInvalid", { placement: clause.placement }));
    return { declaration, diagnostics, canonical: null };
  }
  if (!DECLARATION_OWNERS.has(clause.owner)) {
    diagnostics.push(syntaxError("borrowRelationOwnerInvalid", { owner: clause.owner }));
  }
  if (!Array.isArray(clause.pairs)) {
    diagnostics.push(syntaxError("borrowPairsInvalid"));
    return { declaration, diagnostics, canonical: null };
  }
  const loweredPairs = [];
  const canonicalPairs = [];
  const seenResults = new Set();
  for (const [pairIndex, pair] of clause.pairs.entries()) {
    const result = resultIndex(pair?.result, declaration);
    if (result.error) {
      diagnostics.push(syntaxError(result.error.code, { pair: pairIndex, ...result.error.facts }));
      continue;
    }
    if (seenResults.has(result.index)) {
      diagnostics.push(syntaxError("borrowResultDuplicate", { result: result.index }));
    }
    seenResults.add(result.index);
    if (!Array.isArray(pair?.sources)) {
      diagnostics.push(syntaxError("borrowSourceListInvalid", { result: result.index }));
      continue;
    }
    if (pair.sources.length === 0) {
      diagnostics.push(syntaxError("borrowSourceListEmpty", { result: result.index }));
    }
    const sources = [];
    const sourceOrdinals = [];
    const seenSources = new Set();
    for (const sourceRef of pair.sources) {
      const source = sourceIndex(sourceRef, declaration);
      if (source.error) {
        diagnostics.push(syntaxError(source.error.code, { result: result.index, ...source.error.facts }));
        continue;
      }
      if (seenSources.has(source.index)) {
        diagnostics.push(syntaxError("borrowSourceDuplicate", { result: result.index, source: source.index }));
      }
      seenSources.add(source.index);
      sources.push(source.slot);
      sourceOrdinals.push(source.index);
    }
    const resultMode = declaration.results?.[result.index]?.mode ?? "value";
    const actualModes = [...new Set(sourceOrdinals.map((ordinal) =>
      declaration.inputs?.[ordinal]?.mode === "inout" ? "exclusive" : "shared"))].sort();
    if (pair?.mode !== undefined && (actualModes.length !== 1 || pair.mode !== actualModes[0])) {
      diagnostics.push(syntaxError("borrowEdgeModeInvalid", {
        result: result.index,
        expected: pair.mode,
        actual: actualModes,
      }));
    }
    loweredPairs.push({ result: result.slot, sources, mode: pair?.mode });
    canonicalPairs.push({
      result: result.index,
      resultMode,
      sources: [...new Set(sourceOrdinals)].sort((left, right) => left - right),
      sourceModes: sourceOrdinals.map((ordinal) => ({ ordinal, mode: declaration.inputs?.[ordinal]?.mode ?? "value" }))
        .sort((left, right) => left.ordinal - right.ordinal),
    });
  }
  declaration.relationContract = {
    owner: clause.owner,
    sealed: true,
    pairs: loweredPairs,
  };
  canonicalPairs.sort((left, right) => left.result - right.result);
  return {
    declaration,
    diagnostics,
    canonical: {
      schema: "w-borrow-relation/1",
      pairs: canonicalPairs,
      digest: digest({ schema: "w-borrow-relation/1", pairs: canonicalPairs }),
    },
  };
}

function sourceFacts(corpus) {
  const errors = [];
  for (const [index, sourceRef] of (corpus.sourceRefs ?? []).entries()) {
    const location = `sourceRefs[${index}]`;
    if (typeof sourceRef?.path !== "string" || typeof sourceRef?.symbol !== "string") {
      errors.push(`${location} must contain path and symbol.`);
      continue;
    }
    const file = path.resolve(repositoryRoot, sourceRef.path);
    const relative = path.relative(repositoryRoot, file);
    if (!relative || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative) || !fs.existsSync(file)) {
      errors.push(`${location}.path is outside the repository or missing.`);
      continue;
    }
    const source = fs.readFileSync(file, "utf8");
    const occurrences = source.split(sourceRef.symbol).length - 1;
    if (occurrences !== 1) errors.push(`${location}.symbol must occur exactly once; found ${occurrences}.`);
  }
  return errors;
}

function diagnosticCodes(result) {
  return [...new Set((result.diagnostics ?? []).map((item) => item.code).filter((code) => code !== "relationOmitted"))].sort();
}

function relationAssay(input, lowered) {
  if (input.borrowClause === undefined || lowered.diagnostics.length > 0 || !lowered.canonical) return null;
  const declaration = lowered.declaration;
  const events = [];
  for (const [index, pair] of (input.borrowClause.pairs ?? []).entries()) {
    const result = Number(pair?.result);
    const resultSlot = declaration.results?.[result];
    if (!resultSlot || !Array.isArray(pair?.sources) || pair.sources.length === 0) return null;
    const sources = pair.sources.map((source) => sourceIndex(source, declaration));
    if (sources.some((source) => source.error)) return null;
    if (sources.length === 1) {
      events.push({ operation: "return", result: slotName(resultSlot, result), source: sources[0].slot });
      continue;
    }
    const output = `relation:${index}`;
    events.push({ operation: "union", output, inputs: sources.map((source) => source.slot) });
    events.push({ operation: "return", result: slotName(resultSlot, result), source: output });
  }
  return { kind: "independent-assay", problemTrace: events };
}

export function evaluateBRX3Case(rawInput) {
  const input = clone(rawInput ?? {});
  const lowered = lowerBorrowClause(input);
  const assay = relationAssay(input, lowered);
  const base = evaluateBorrowRelationCase({
    ...input,
    ...(assay ? { assay } : {}),
    declaration: lowered.declaration,
  });
  const diagnostics = [
    ...lowered.diagnostics,
    ...(base.diagnostics ?? []),
    ...(base.invocation?.code ? [{ code: base.invocation.code, facts: { reason: base.invocation.reason } }] : []),
  ];
  const unique = [];
  for (const item of diagnostics) if (!unique.some((candidate) => candidate.code === item.code)) unique.push(item);
  const declarationAccepted = lowered.diagnostics.length === 0 &&
    !base.diagnostics?.some((item) => [
      "callerRelationClaimRejected", "runtimeLifetimeMetadataRejected", "wAbiRuntimeFieldRejected",
      "relationOwnerInvalid", "relationNotSealed", "relationDigestStale", "relationInputSlotUnknown",
      "relationResultSlotUnknown", "relationResultDuplicate", "relationResultSlotMissing", "relationEmptyDependent",
      "relationSourceModeInvalid", "relationSourceDuplicate", "relationEdgeModeInvalid", "relationResultRecursion",
      "implementationRelationConflict", "interfaceWitnessMismatch", "genericRelationVariance", "providerInterfaceKeyMismatch",
      "consumerProviderExpectationMismatch", "interfaceLockMismatch", "semanticInterfaceKeyMismatch",
      "interfaceRelationDigestMismatch", "relationDigestMismatch", "W-BORROW-0011",
    ].includes(item.code));
  const relationAccepted = input.borrowClause !== undefined && lowered.diagnostics.length === 0 &&
    base.mapping.relationError === null && base.mapping.relation !== undefined;
  const output = {
    id: String(input.id ?? ""),
    status: declarationAccepted && (relationAccepted || base.mapping.baselineExact) ? "accepted" : "rejected",
    declarationAccepted,
    relationAccepted,
    invocationStatus: base.invocation?.status ?? "accepted",
    diagnostics: unique,
    diagnosticCodes: diagnosticCodes({ diagnostics: unique }),
    canonicalRelation: lowered.canonical,
    mapping: base.mapping,
    interfaces: base.interfaces,
    abi: base.abi,
    invocation: base.invocation,
    digest: digest({
      status: declarationAccepted && (relationAccepted || base.mapping.baselineExact) ? "accepted" : "rejected",
      canonicalRelation: lowered.canonical,
      diagnostics: unique,
      semanticInterfaceKey: base.interfaces?.semanticInterfaceKey ?? null,
      wAbiChanged: base.abi?.wAbiChanged ?? false,
    }),
  };
  return output;
}

export function validateBRX3Corpus(corpus) {
  const errors = [];
  if (corpus?.$schema !== "w-brx3-borrow-relations-cases-1") errors.push("BRX3 corpus schema is invalid.");
  if (corpus?.status !== "design-oracle-input") errors.push("BRX3 corpus status is invalid.");
  if (corpus?.id !== "BRX3-borrow-relation-source-clause") errors.push("BRX3 corpus identity is invalid.");
  if (!Array.isArray(corpus?.cases) || corpus.cases.length < 20) errors.push("BRX3 corpus must contain at least 20 cases.");
  errors.push(...sourceFacts(corpus));
  const ids = new Set();
  const results = [];
  for (const [index, testCase] of (corpus?.cases ?? []).entries()) {
    const location = `cases[${index}]`;
    if (!/^BRX3-[a-z0-9]+(?:-[a-z0-9]+)*$/u.test(testCase?.id ?? "")) errors.push(`${location}.id is not BRX3 kebab-case.`);
    if (ids.has(testCase?.id)) errors.push(`${location}.id is duplicated.`);
    ids.add(testCase?.id);
    const result = evaluateBRX3Case(testCase);
    results.push(result);
    const expected = testCase.expected ?? {};
    if (result.status !== expected.status) errors.push(`${testCase.id}.status expected ${expected.status}, got ${result.status}.`);
    const expectedDiagnostics = [...new Set(expected.diagnostics ?? [])].sort();
    if (JSON.stringify(result.diagnosticCodes) !== JSON.stringify(expectedDiagnostics)) {
      errors.push(`${testCase.id}.diagnostics expected ${expectedDiagnostics.join(",") || "none"}, got ${result.diagnosticCodes.join(",") || "none"}.`);
    }
    if (expected.invocationStatus !== undefined && result.invocationStatus !== expected.invocationStatus) {
      errors.push(`${testCase.id}.invocationStatus expected ${expected.invocationStatus}, got ${result.invocationStatus}.`);
    }
    if (expected.canonicalDigest !== undefined && result.canonicalRelation?.digest !== expected.canonicalDigest) {
      errors.push(`${testCase.id}.canonicalDigest is stale.`);
    }
  }
  const accepted = results.filter((result) => result.status === "accepted").length;
  const rejected = results.length - accepted;
  return {
    errors,
    results,
    metrics: {
      caseCount: results.length,
      accepted,
      rejected,
      relationAccepted: results.filter((result) => result.relationAccepted).length,
      diagnostics: results.reduce((total, result) => total + result.diagnostics.length, 0),
      canonicalRelations: results.filter((result) => result.canonicalRelation !== null).length,
      wAbiDrift: results.filter((result) => result.status === "accepted" && result.abi?.wAbiChanged).length,
    },
  };
}

export function buildBRX3Snapshot(corpus) {
  const checked = validateBRX3Corpus(corpus);
  const records = checked.results.map((result) => ({
    schema: RESULT_SCHEMA,
    id: result.id,
    status: result.status,
    relationAccepted: result.relationAccepted,
    invocationStatus: result.invocationStatus,
    diagnostics: result.diagnosticCodes,
    canonicalRelation: result.canonicalRelation,
    semanticInterfaceKey: result.interfaces?.semanticInterfaceKey ?? null,
    wAbiChanged: result.abi?.wAbiChanged ?? false,
    digest: result.digest,
  }));
  const text = records.map((record) => JSON.stringify(stable(record))).join("\n") + "\n";
  return { text, metrics: checked.metrics, results: checked.results };
}

if (import.meta.main) {
  const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
  const checked = validateBRX3Corpus(corpus);
  if (checked.errors.length > 0) {
    console.error(checked.errors.join("\n"));
    process.exitCode = 1;
  } else {
    console.log(`BRX3 oracle: ${checked.metrics.caseCount} cases, ${checked.metrics.accepted} accepted, ${checked.metrics.rejected} rejected.`);
  }
}
