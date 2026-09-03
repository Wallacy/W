import { describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { compareGen1Lowerings, extractSourceSlice, measureBundleVariants, measureSourceText, runGen1Program, validateGen1Operation, validateVariantDisposition } from "./gen1-incremental-suspension-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "gen1-incremental-suspension-cases.json"), "utf8"));
const bundle = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "studies", "gen1-incremental-suspension", "bundle.json"), "utf8"));

describe("GEN1 incremental suspension host oracle", () => {
  test("pull uses a linear token and closes after cleanup", () => {
    const result = runGen1Program(corpus.cases.find((candidate) => candidate.id === "GEN1-pull-simple").operations);
    expect(result.status).toBe("accepted"); expect(result.state.typedResult).toBe("stream-none"); expect(result.state.singleOwnerProof).toBe(true);
    expect(result.state.ownerGraph[0]).toMatchObject({ phase: "closed", cleanupDone: true, dropped: true, drained: true });
  });
  test("failure item continues while terminal failure closes", () => {
    const item = runGen1Program(corpus.cases.find((candidate) => candidate.id === "GEN1-injected-failure-continue").operations);
    const terminal = runGen1Program(corpus.cases.find((candidate) => candidate.id === "GEN1-injected-failure-terminal").operations);
    expect(item.status).toBe("accepted"); expect(item.logicalTrace.some((event) => event.event === "result-error-item")).toBe(true); expect(item.state.typedResult).toBe("stream-none");
    expect(terminal.state.typedResult).toBe("failure:KitchenFailure");
  });
  test("future Stream pull returns none, while custom resume rejects terminal", () => {
    const stream = runGen1Program(corpus.cases.find((candidate) => candidate.id === "GEN1-stream-terminal-next-none").operations);
    const continuation = runGen1Program(corpus.cases.find((candidate) => candidate.id === "GEN1-terminal-resume-rejected").operations);
    expect(stream.status).toBe("accepted"); expect(stream.logicalTrace.at(-1).event).toBe("stream-none"); expect(continuation.reason).toBe("resume-terminal");
  });
  test("delegation joins only after child cleanup, drop, drain, and commit", () => {
    const result = runGen1Program(corpus.cases.find((candidate) => candidate.id === "GEN1-delegation-equivalent").operations);
    expect(result.status).toBe("accepted"); expect(result.logicalTrace.map((event) => event.event)).toContain("child-join");
    const bad = runGen1Program([{ op: "open", owner: "root" }, { op: "spawn", owner: "root", child: "child" }, { op: "join", owner: "root", child: "child" }]);
    expect(bad.reason).toBe("child-drain");
  });
  test("both engines agree on logical invariants and differ physically", () => {
    for (const testCase of corpus.cases) {
      const comparison = compareGen1Lowerings(testCase.operations);
      expect(comparison.equivalence.equivalent).toBe(true); expect(comparison.equivalence.physicalDifferent).toBe(true);
      expect(comparison.equivalence.ownerGraphEqual).toBe(true); expect(comparison.equivalence.commitEqual).toBe(true); expect(comparison.equivalence.resultEqual).toBe(true); expect(comparison.equivalence.cleanupEqual).toBe(true);
    }
  });
  test("host mutation makes an engine divergence visible in owner/result/commit/cleanup", () => {
    const operations = corpus.cases.find((candidate) => candidate.id === "GEN1-pull-simple").operations;
    const mutation = compareGen1Lowerings(operations, { mutate: "returned-commit" });
    expect(mutation.equivalence.equivalent).toBe(false);
    expect([mutation.equivalence.ownerGraphEqual, mutation.equivalence.resultEqual, mutation.equivalence.commitEqual, mutation.equivalence.cleanupEqual]).toContain(false);
    const frameMutation = compareGen1Lowerings(operations, { mutate: "frame-commit" });
    expect(frameMutation.equivalence.equivalent).toBe(false); expect(frameMutation.equivalence.resultEqual).toBe(false);
  });
  test("channel and callback mutations are engine-specific divergence witnesses", () => {
    const channelOps = corpus.cases.find((candidate) => candidate.id === "GEN1-backpressure-cap1").operations;
    const channelMutation = compareGen1Lowerings(channelOps, { mutate: "returned-channel" });
    expect(channelMutation.equivalence.equivalent).toBe(false);
    expect([channelMutation.equivalence.logicalEqual, channelMutation.equivalence.resultEqual, channelMutation.equivalence.ownerGraphEqual]).toContain(false);
    const callbackOps = corpus.cases.find((candidate) => candidate.id === "GEN1-ffi-callback-lease-drain").operations;
    const callbackMutation = compareGen1Lowerings(callbackOps, { mutate: "returned-callback" });
    expect(callbackMutation.equivalence.equivalent).toBe(false);
    expect([callbackMutation.equivalence.logicalEqual, callbackMutation.equivalence.cleanupEqual, callbackMutation.equivalence.resultEqual]).toContain(false);
  });
  test("channels derive admission and cancellation from queue state", () => {
    expect(runGen1Program(corpus.cases.find((candidate) => candidate.id === "GEN1-backpressure-cap0").operations).state.typedResult).toBe("rendezvous-commit");
    expect(runGen1Program(corpus.cases.find((candidate) => candidate.id === "GEN1-backpressure-cap1").operations).state.typedResult).toBe("bounded-commit");
    expect(runGen1Program(corpus.cases.find((candidate) => candidate.id === "GEN1-cancel-before-commit").operations).state.typedResult).toBe("owner-returned");
    expect(runGen1Program(corpus.cases.find((candidate) => candidate.id === "GEN1-cancel-after-commit").operations).state.typedResult).toBe("commit-wins");
  });
  test("source metrics ignore comments and strings and track structural mutations", () => {
    const source = fs.readFileSync(path.join(toolingDirectory, "studies", "gen1-incremental-suspension", "stream-structured.w"), "utf8");
    const base = measureSourceText(source, { id: "base" });
    expect(measureSourceText(`${source}\n// yield continuation frame capacity`, { id: "comment" }).hiddenStateCount).toBe(base.hiddenStateCount);
    expect(measureSourceText(`${source}\nlet text = "yield continuation"`, { id: "string" }).hiddenStateCount).toBe(base.hiddenStateCount);
    expect(measureSourceText(`${source}\nlet frame = 1`, { id: "frame-name" }).hiddenStateCount).toBe(base.hiddenStateCount);
    expect(measureSourceText(source.replace("take some", "some"), { id: "owner" }).explicitOwnerHandoffs.length).toBeLessThan(base.explicitOwnerHandoffs.length);
    const helperSource = fs.readFileSync(path.join(toolingDirectory, "studies", "gen1-incremental-suspension", "builder-helper.w"), "utf8");
    expect(measureSourceText(helperSource.replace("capacity", "limit"), { id: "capacity" }).capacityFacts.length).toBeLessThan(measureSourceText(helperSource, { id: "helper" }).capacityFacts.length);
    expect(measureSourceText(source.replace("export async fn closePull", "async fn closePull"), { id: "public" }).publicDeclarationCount).toBeLessThan(base.publicDeclarationCount);
    const metrics = Object.fromEntries(measureBundleVariants(bundle).map((metric) => [metric.variant, metric]));
    expect(metrics["public-resumable-frame"].hiddenStateCount).toBeGreaterThan(0);
    expect(metrics["compiler-stream-block"].hiddenStateCount).toBe(0);
    expect(metrics["compiler-stream-block"].hiddenStatePolicy).toBe("compiler-owned");
  });
  test("source-shaped builder owns both bounded channel pairs", () => {
    const helper = bundle.helpers.find((candidate) => candidate.id === "bounded-dialogue-builder");
    const source = fs.readFileSync(path.join(toolingDirectory, "studies", "gen1-incremental-suspension", helper.path), "utf8");
    expect(source.match(/Channel<.*\.open\(capacity:/gu)?.length).toBe(2); expect(source).toContain("take requestsOut"); expect(source).toContain("take repliesOut");
  });
  test("scenario symbol mutations reject missing, duplicate, and false applicability", () => {
    const stream = fs.readFileSync(path.join(toolingDirectory, "studies", "gen1-incremental-suspension", "stream-structured.w"), "utf8");
    expect(extractSourceSlice(stream, "missingScenario").count).toBe(0);
    expect(extractSourceSlice(`${stream}\nexport fn observeTraversal(): usize { return 0 }`, "observeTraversal").count).toBe(2);
    expect(extractSourceSlice(stream, "observeDialogue").count).toBe(0);
  });
  test("variant role swaps do not forge a disposition", () => {
    const compiler = bundle.variants.find((variant) => variant.id === "compiler-stream-block");
    const publicFrame = bundle.variants.find((variant) => variant.id === "public-resumable-frame");
    expect(validateVariantDisposition(compiler)).toBe(true);
    expect(validateVariantDisposition(publicFrame)).toBe(true);
    expect(validateVariantDisposition({ ...compiler, role: publicFrame.role, disposition: publicFrame.disposition })).toBe(false);
    expect(validateVariantDisposition({ ...compiler, role: "research-candidate", disposition: "research-candidate" })).toBe(false);
    expect(validateVariantDisposition({ ...publicFrame, id: compiler.id, role: compiler.role, disposition: compiler.disposition, hiddenStatePolicy: compiler.hiddenStatePolicy })).toBe(false);
    expect(validateVariantDisposition({ ...compiler, hiddenStatePolicy: "public" })).toBe(false);
    expect(validateVariantDisposition({ ...publicFrame, hiddenStatePolicy: "compiler-owned" })).toBe(false);
  });
  test("caller echo and invented W diagnostics are absent", () => {
    expect(JSON.stringify(corpus)).not.toMatch(new RegExp("W-" + "GEN1-"));
    for (const testCase of corpus.cases) for (const operation of testCase.operations) expect(["commit", "concurrent", "late", "pairedSend", "pairedReceive", "failureMode", "phase"].some((field) => Object.prototype.hasOwnProperty.call(operation, field))).toBe(false);
  });
  test("validator rejects hidden frame operations", () => {
    expect(validateGen1Operation({ op: "resumeAcquire", owner: "source", token: "t" })).toBe(true); expect(validateGen1Operation({ op: "yield", owner: "source" })).toBe(false); expect(validateGen1Operation({ op: "resumeAcquire", frame: "hidden", owner: "source", token: "t" })).toBe(false);
  });
});
