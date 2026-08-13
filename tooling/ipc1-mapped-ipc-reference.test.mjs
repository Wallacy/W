import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { evaluateIpc1Case, reducePosixCase, reduceWindowsCase, validateIpc1 } from "./ipc1-mapped-ipc-machine.mjs";

const root = path.resolve(import.meta.dir, "..");
const corpus = JSON.parse(fs.readFileSync(path.join(import.meta.dir, "ipc1-mapped-ipc-cases.json"), "utf8"));
const byId = new Map(corpus.cases.map((testCase) => [testCase.id, testCase]));
const oracleOptions = { providers: corpus.providers, layouts: corpus.layouts, schemas: corpus.schemas };

describe("IPC1 mapped IPC reference host checks", () => {
  test("source-backed corpus has two independent target projections", () => {
    const checked = validateIpc1(corpus, { root });
    expect(checked.errors).toEqual([]);
    expect(checked.results.every((result) => result.targetProjections.length === 2)).toBe(true);
    expect(checked.results.filter((result) => result.code === "target-divergence").map((result) => result.caseId)).toEqual([]);
  });

  test("target reducers expose backing, namespace, and receipt differences", () => {
    const testCase = byId.get("IPC1-menu-immutable-publish");
    const posix = reducePosixCase(testCase, corpus.providers["posix-file-durable"], { layouts: corpus.layouts, schemas: corpus.schemas });
    const windows = reduceWindowsCase(testCase, corpus.providers["windows-file-durable"], { layouts: corpus.layouts, schemas: corpus.schemas });
    expect(posix.physical.events.some((event) => event.op === "msync")).toBe(true);
    expect(posix.physical.events.some((event) => event.op === "fsync")).toBe(true);
    expect(windows.physical.events.some((event) => event.op === "FlushViewOfFile")).toBe(true);
    expect(windows.physical.events.some((event) => event.op === "FlushFileBuffers")).toBe(true);
    const lifecycle = evaluateIpc1Case(byId.get("IPC1-unlink-last-lease-lifecycle"), oracleOptions);
    expect(lifecycle.logical.normalizedLifecycle).toBe("existing-leases-remain-until-own-close");
    expect(lifecycle.targetProjections[0].physical.events.some((event) => event.op === "unlink-file-name")).toBe(true);
    expect(lifecycle.targetProjections[1].physical.events.some((event) => event.op === "named-object-discovery")).toBe(true);
  });

  test("expected fields, bytes, and reducer mutations cannot choose the outcome", () => {
    const original = byId.get("IPC1-live-horizon-address-independent");
    const expectedMutation = structuredClone(original);
    expectedMutation.expected.code = "forged-result";
    expect(evaluateIpc1Case(expectedMutation, oracleOptions).code).toBe("immutable-generation");
    const byteMutation = structuredClone(original);
    byteMutation.input.snapshotBytesByTarget = { posix: [1], windows: [2] };
    expect(evaluateIpc1Case(byteMutation, oracleOptions).code).toBe("snapshot-bytes-diverge");
    const divergentProviders = structuredClone(corpus.providers);
    divergentProviders["windows-file-durable"].allowedSchemas[1].digest = "sha256:3333333333333333333333333333333333333333333333333333333333333333";
    expect(evaluateIpc1Case(original, { providers: divergentProviders, layouts: corpus.layouts, schemas: corpus.schemas }).code).toBe("target-divergence");
  });

  test("legacy result flags, provider bindings, and provider mutations are schema errors", () => {
    const legacy = structuredClone(corpus);
    legacy.cases[1].input.crashPoint = "beforePublish";
    expect(validateIpc1(legacy, { root }).errors.some((error) => error.includes("legacy oracle field"))).toBe(true);
    const missingBinding = structuredClone(corpus);
    delete missingBinding.cases[1].providerBindings.windows;
    expect(validateIpc1(missingBinding, { root }).errors.some((error) => error.includes("providerBindings.windows"))).toBe(true);
    const duplicateBinding = structuredClone(corpus);
    duplicateBinding.cases[1].providerBindings.windows = "posix-file-durable";
    expect(validateIpc1(duplicateBinding, { root }).errors.some((error) => error.includes("wrong kind") || error.includes("duplicate a provider"))).toBe(true);
    const forgedProvider = structuredClone(corpus);
    forgedProvider.providers["posix-file-durable"].authoritative = false;
    expect(validateIpc1(forgedProvider, { root }).errors.some((error) => error.includes("posix-file-durable.authoritative"))).toBe(true);
  });
});
