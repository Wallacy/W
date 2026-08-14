import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import {
  HUM0_SLICES,
  TASK_KINDS,
  deriveReadiness,
  deriveStimuli,
  makeSnapshot,
  renderParticipantPrompt,
  validateHumanRecord,
  validateModelRecord,
  validateProtocol,
} from "./hum0-human-review-machine.mjs";

const repositoryRoot = path.resolve(import.meta.dir, "..");
const protocol = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "hum0-human-review-protocol.json"), "utf8"));

describe("HUM0 human review protocol", () => {
  test("derives eight problem-first slices and 32 tasks without results", () => {
    const errors = validateProtocol(protocol, { root: repositoryRoot });
    expect(errors).toEqual([]);
    const readiness = deriveReadiness(protocol, errors);
    expect(readiness).toMatchObject({
      status: "protocol-ready",
      sliceCount: 8,
      taskCount: 32,
      humanRecordCount: 0,
      modelRecordCount: 0,
      humanResultsClaimed: false,
      modelResultsClaimed: false,
    });
    expect(protocol.slices.map((slice) => slice.id)).toEqual(HUM0_SLICES);
    expect(protocol.slices.every((slice) => slice.tasks.map((task) => task.kind).sort().join(",") === TASK_KINDS.slice().sort().join(","))).toBe(true);
    expect(makeSnapshot(protocol, errors).evidence).toMatchObject({
      participantRecords: "none",
      designDecision: "none",
    });
    const snapshotText = JSON.stringify(makeSnapshot(protocol, errors));
    expect(makeSnapshot(protocol, errors).slices.every((slice) => slice.stimulusDigests.length === 2)).toBe(true);
    expect(snapshotText).not.toContain("sourceRefId");
    expect(snapshotText).not.toContain("mutation");
    expect(snapshotText).not.toContain("expectedRepair");
    expect(snapshotText).not.toContain("\"path\"");
    const stimuli = deriveStimuli(protocol, { root: repositoryRoot });
    expect(stimuli.errors).toEqual([]);
    expect(stimuli.slices.flatMap((slice) => slice.inputs).every((input) => input.participantStimulus.length > 0)).toBe(true);
    expect(stimuli.slices.flatMap((slice) => slice.inputs).every((input) => input.lineCount > 0)).toBe(true);
    expect(protocol.slices.flatMap((slice) => slice.inputs).every((input) => "beforeLines" in input.stimulus && "afterLines" in input.stimulus)).toBe(true);
    for (const slice of protocol.slices) {
      for (const input of stimuli.slices.find((entry) => entry.sliceId === slice.id).inputs) {
        const sourceRef = slice.sourceRefs.find((ref) => ref.id === slice.inputs.find((candidate) => candidate.id === input.inputId).stimulus.sourceRefId);
        const sourceBytes = fs.readFileSync(path.join(repositoryRoot, sourceRef.path));
        expect(input.startByte === 0 || sourceBytes[input.startByte - 1] === 0x0a).toBe(true);
        expect(input.endByte === sourceBytes.length || sourceBytes[input.endByte - 1] === 0x0a).toBe(true);
        expect(new TextDecoder("utf-8", { fatal: true }).decode(input.bytes)).toBe(input.participantStimulus);
      }
    }
  });

  test("renders a closed participant-only view without internal metadata", () => {
    const forbidden = /\b(expected|status|route|role|path|digest|oracle)\b/i;
    for (const slice of protocol.slices) {
      for (const task of slice.tasks) {
        const input = slice.inputs.find((candidate) => candidate.id === task.inputId);
        const rendered = renderParticipantPrompt(slice, input, task, { root: repositoryRoot, blindedLabel: "blind-A" });
        expect(Object.keys(rendered)).toEqual(["scenario", "task", "instruction", "source", "blindedLabel"]);
        expect(rendered.source.length).toBeGreaterThan(0);
        expect(JSON.stringify(rendered)).not.toMatch(forbidden);
      }
    }
  });

  test("rejects expected echo and hidden fields in participant input", () => {
    const mutated = structuredClone(protocol);
    mutated.slices[0].inputs[0].participantInput.expected = "accepted";
    const errors = validateProtocol(mutated, { root: repositoryRoot });
    expect(errors.some((error) => error.includes("forbidden in participant-visible input"))).toBe(true);
  });

  test("rejects stale, missing, duplicate, divergent, and leaked stimuli", () => {
    const stale = structuredClone(protocol);
    stale.slices[0].inputs[0].stimulus.derivedStimulusDigest = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
    expect(validateProtocol(stale, { root: repositoryRoot }).some((error) => error.includes("derivedStimulusDigest is stale"))).toBe(true);

    const missingFind = structuredClone(protocol);
    missingFind.slices[0].inputs[1].observerOnly.mutation.find = "not present in bounded window";
    expect(validateProtocol(missingFind, { root: repositoryRoot }).some((error) => error.includes("occur exactly once"))).toBe(true);

    const duplicateFind = structuredClone(protocol);
    duplicateFind.slices[0].inputs[1].observerOnly.mutation.occurrences = 2;
    expect(validateProtocol(duplicateFind, { root: repositoryRoot }).some((error) => error.includes("occurrences must be exactly 1"))).toBe(true);

    const divergent = structuredClone(protocol);
    divergent.slices[0].inputs[1].observerOnly.baseInputId = "other-input";
    divergent.slices[0].inputs[1].outcomeKey = "different-outcome";
    expect(validateProtocol(divergent, { root: repositoryRoot }).some((error) => error.includes("baseInputId"))).toBe(true);
    expect(validateProtocol(divergent, { root: repositoryRoot }).some((error) => error.includes("preserve the slice problem and outcome"))).toBe(true);

    const divergentWindow = structuredClone(protocol);
    divergentWindow.slices[0].inputs[1].stimulus.afterLines += 1;
    expect(validateProtocol(divergentWindow, { root: repositoryRoot }).some((error) => error.includes("same primary window"))).toBe(true);

    const leaked = structuredClone(protocol);
    leaked.slices[0].tasks[2].participantInput.mutation = "hidden mutation";
    expect(validateProtocol(leaked, { root: repositoryRoot }).some((error) => error.includes("unknown field"))).toBe(true);
  });

  test("rejects unknown schema and result-like fields at every protocol boundary", () => {
    const mutations = [
      (value) => { value.score = 1; },
      (value) => { value.slices[0].extra = true; },
      (value) => { value.slices[0].sourceRefs[0].extra = true; },
      (value) => { value.slices[0].oracleRefs[0].extra = true; },
      (value) => { value.slices[0].inputs[0].extra = true; },
      (value) => { value.slices[0].tasks[0].extra = true; },
      (value) => { value.slices[0].counterbalance.extra = true; },
      (value) => { value.slices[0].blinding.extra = true; },
      (value) => { value.resultContracts.human.preference = "x"; },
      (value) => { value.metricsPolicy.score = false; },
      (value) => { value.records.extra = []; },
    ];
    for (const mutate of mutations) {
      const value = structuredClone(protocol);
      mutate(value);
      expect(validateProtocol(value, { root: repositoryRoot }).some((error) => error.includes("unknown field"))).toBe(true);
    }
  });

  test("stages the protocol when stimulus evidence is incomplete", () => {
    const staged = structuredClone(protocol);
    staged.status = "protocol-staged";
    staged.slices[0].inputs[0].stimulus.derivedStimulusDigest = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
    const stagedErrors = validateProtocol(staged, { root: repositoryRoot });
    expect(stagedErrors.some((error) => error.includes("derivedStimulusDigest is stale"))).toBe(true);
    expect(stagedErrors.some((error) => error.includes("protocol.status must be protocol-ready"))).toBe(false);

    const inconsistent = structuredClone(protocol);
    inconsistent.status = "protocol-staged";
    expect(validateProtocol(inconsistent, { root: repositoryRoot }).some((error) => error.includes("current validation state"))).toBe(true);
  });

  test("keeps result contracts provenance-bound and PII-free", () => {
    const outcomes = { semantic: "pass", repair: "fail", change: "inconclusive" };
    const digest = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
    expect(validateHumanRecord({ participantIdHash: digest, background: ["Rust"], timeMs: 12, queryCount: 1, confidence: 4, oracleCheckedOutcomes: outcomes, observerReceiptDigest: digest }, { protocol })).toEqual([]);
    expect(validateHumanRecord({ participantIdHash: "ada@example.com", background: ["Rust"], timeMs: 12, queryCount: 1, confidence: 4, oracleCheckedOutcomes: outcomes, observerReceiptDigest: digest }, { protocol }).some((error) => error.includes("sha256"))).toBe(true);
    expect(validateHumanRecord({ participantIdHash: digest, timeMs: 12, queryCount: 1, oracleCheckedOutcomes: outcomes, observerReceiptDigest: digest }, { protocol }).some((error) => error.includes("background"))).toBe(true);
    expect(validateHumanRecord({ participantIdHash: digest, background: ["Rust"], timeMs: 12, queryCount: 1, oracleCheckedOutcomes: outcomes, observerReceiptDigest: digest }, { protocol }).some((error) => error.includes("confidence"))).toBe(true);
    expect(validateHumanRecord({ participantIdHash: digest, background: ["Rust"], timeMs: 12, queryCount: 1, confidence: 4, observerReceiptDigest: digest }, { protocol }).some((error) => error.includes("oracleCheckedOutcomes"))).toBe(true);
    expect(validateModelRecord({ provider: "provider", model: "model", version: "v1", tokenizer: "tok", params: { temperature: 0 }, inputDigest: digest, observerReceiptDigest: digest, tokens: { input: 2, output: 3, total: 5 }, oracleCheckedOutcomes: outcomes })).toEqual([]);
    expect(validateModelRecord({ provider: "provider", model: "model", version: "v1", tokenizer: "tok", params: { unknown: 1 }, inputDigest: "not-a-digest", observerReceiptDigest: "not-a-digest", tokens: { input: 2, output: 3, total: 6 }, oracleCheckedOutcomes: outcomes }).length).toBeGreaterThan(2);
  });
});
