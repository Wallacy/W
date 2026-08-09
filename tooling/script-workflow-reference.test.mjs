import { expect, test } from "bun:test";
import { lockRootDigest, runScriptWorkflowProgram, scriptDigest } from "./script-workflow-machine.mjs";

const lockDigest = "sha256:f59a22a26aa53fc0d1555350c177b8013d2f1532554861872ff87f94ab0e8cf2";
const packageId = "sha256:ac9f98b1d18f719df7b37833dce7fb0619626d378a106fb08ceb3f0bdd74563a";
const selectionDigest = "sha256:b90c5dd4c89e687a909977bcce580e2c8a20ac7049b3192610a13643ed6e4943";
const artifactDigest = "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const toolchainDigest = "sha256:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";

const dependency = {
  alias: "chart",
  package: "fiction/chart",
  version: "^1.2.0",
  use: "product",
  source: { kind: "registry", authority: "w", immutable: true },
};

function evidence(sourceText, header, entry = "default") {
  return {
    sourceDigest: scriptDigest("w-script-source-v2", sourceText.replace(/\r\n?/g, "\n")),
    headerFacts: header,
    entry,
    topLevelExecution: false,
    hasHeader: header !== null,
  };
}

function header(lock = lockDigest) {
  return {
    edition: "2026",
    dependencies: [dependency],
    lock,
    requires: [".clock"],
  };
}

function lock(requiredHandles = []) {
  return {
    schema: "w.package-lock/1",
    resolver: "w.resolver/1",
    workspaceDigest: selectionDigest,
    manifestDigests: { "virtual-script": selectionDigest },
    contexts: [{ root: '.product("script")', use: "product", targetRole: "target", target: "x86_64-unknown-linux-gnu", features: [], targetVariants: [], activeSourceSet: selectionDigest, resolutionDigest: selectionDigest, nodes: [packageId], rootEdges: [{ alias: "chart", id: packageId }] }],
    packages: [{ id: packageId, name: "fiction/chart", version: "1.2.3", source: { ...dependency.source }, metadataDigest: "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", contentDigest: "sha256:eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", dependencies: [] }],
    artifacts: [{ nodeId: packageId, digest: artifactDigest, authority: "w", target: "x86_64-unknown-linux-gnu", use: "product", signatureRequired: false, signatureEvidence: null }],
    cas: ["sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", "sha256:eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", artifactDigest],
    requiredHandles,
    actionOutputs: [],
  };
}

test("P0 lock root normalization sorts declared arrays without injecting defaults", () => {
  const first = lock();
  const second = lock();
  second.contexts = [...second.contexts].reverse();
  second.contexts[0].nodes = [...second.contexts[0].nodes].reverse();
  second.packages = [...second.packages].reverse();
  expect(lockRootDigest(first)).toBe(lockRootDigest(second));
  const omitted = lock();
  delete omitted.packages[0].source.immutable;
  expect(lockRootDigest(first)).not.toBe(lockRootDigest(omitted));
});

function ready({ path = "examples/horizon_script.w", sourceText = "script horizon menu", imports = [], scriptHeader = header(), lockObject = lock() } = {}) {
  return [
    { op: "parseHeader", path, sourceText, header: scriptHeader, entry: "default", imports, parseEvidence: evidence(sourceText, scriptHeader) },
    { op: "selectContext", packageContext: true, packageRoot: "workspace" },
    { op: "resolveRoots" },
    { op: "validateImports", imports },
    { op: "validateResolution", target: "x86_64-unknown-linux-gnu", lockObject },
  ];
}

function built(options = {}) {
  return [
    ...ready(options),
    { op: "admitFetch", networkPolicy: "allow-pinned", cache: [lockDigest, "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"], fetches: [{ nodeId: packageId, digest: artifactDigest, authority: "w", signatureEvidence: null }] },
    { op: "verifyArtifact" },
    { op: "admitCapabilities", deployment: { grants: [".clock"] } },
    {
      op: "buildEphemeral",
      target: "x86_64-unknown-linux-gnu",
      hostProfile: "native-script@1",
      toolchain: { digest: toolchainDigest },
    },
  ];
}

function buildWithSidecar(mutator, { actionOutput = false, handle = false } = {}) {
  const lockObject = lock();
  mutator(lockObject);
  const operations = built({ lockObject });
  const fetch = operations.find((operation) => operation.op === "admitFetch");
  const artifactDigestForFetch = lockObject.artifacts[0].digest;
  fetch.cache = [lockDigest, "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", artifactDigestForFetch];
  fetch.fetches = [{ nodeId: packageId, digest: artifactDigestForFetch, authority: "w", signatureEvidence: lockObject.artifacts[0].signatureEvidence }];
  if (actionOutput) fetch.requiredActionOutput = lockObject.actionOutputs[0].name;
  if (handle) {
    const capabilities = operations.find((operation) => operation.op === "admitCapabilities");
    capabilities.receivedHandles = lockObject.requiredHandles;
  }
  return operations;
}

test("standalone header wins over workspace and identity excludes physical path", () => {
  const first = runScriptWorkflowProgram([
    ...built({ path: "examples/horizon_script.w" }),
    { op: "contextExplanation" },
    { op: "runEntry", entry: "default", args: ["horizon"] },
    { op: "cleanup" },
  ]);
  const moved = runScriptWorkflowProgram(built({ path: "moved/horizon_script.w" }));

  expect(first.status).toBe("accepted");
  expect(first.state.context.mode).toBe("standalone");
  expect(first.state.context.explanation.lockDigest).toBe(lockDigest);
  expect(first.state.product.identity).toMatch(/^sha256:[0-9a-f]{64}$/);
  expect(first.state.product.identity).toBe(moved.state.product.identity);
  expect(first.state.cleanup.hiddenArtifacts).toEqual([]);
});

test("source bytes and logical local-module graph change identity", () => {
  const sourceChanged = runScriptWorkflowProgram(built({ sourceText: "script horizon changed" }));
  const importChanged = runScriptWorkflowProgram(
    built({ imports: [{ path: "local.w", digest: "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd" }] }),
  );
  const importPathChanged = runScriptWorkflowProgram(
    built({ imports: [{ path: "other.w", digest: "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd" }] }),
  );
  const baseline = runScriptWorkflowProgram(built());

  expect(sourceChanged.status).toBe("accepted");
  expect(importChanged.status).toBe("accepted");
  expect(importPathChanged.status).toBe("accepted");
  expect(sourceChanged.state.product.identity).not.toBe(baseline.state.product.identity);
  expect(importChanged.state.product.identity).not.toBe(baseline.state.product.identity);
  expect(importPathChanged.state.product.identity).not.toBe(importChanged.state.product.identity);
});

test("locked sidecar selections are bound into the recipe identity", () => {
  const baseline = runScriptWorkflowProgram(built());
  const changedArtifact = runScriptWorkflowProgram(buildWithSidecar((lockObject) => {
    lockObject.artifacts[0].digest = "sha256:9999999999999999999999999999999999999999999999999999999999999999";
    lockObject.cas.push(lockObject.artifacts[0].digest);
  }));
  const changedPolicy = runScriptWorkflowProgram(buildWithSidecar((lockObject) => {
    lockObject.artifacts[0].signatureRequired = true;
    lockObject.artifacts[0].signatureEvidence = "sig:chart";
  }));
  const actionDigest = "sha256:1111111111111111111111111111111111111111111111111111111111111111";
  const provenanceDigest = "sha256:2222222222222222222222222222222222222222222222222222222222222222";
  const actionRecordDigest = scriptDigest("w-script-action-output-record-v1", {
    lockRootDigest: lockDigest,
    owner: packageId,
    actionDigest,
    outputDigest: "sha256:8888888888888888888888888888888888888888888888888888888888888888",
    policy: "explicit",
    provenanceDigest,
  });
  const changedAction = runScriptWorkflowProgram(buildWithSidecar((lockObject) => {
    lockObject.actionOutputs = [{ name: "chart-generated", owner: packageId, digest: "sha256:8888888888888888888888888888888888888888888888888888888888888888", policy: "explicit", actionDigest, provenanceDigest, recordDigest: actionRecordDigest }];
    lockObject.cas.push("sha256:8888888888888888888888888888888888888888888888888888888888888888");
  }, { actionOutput: true }));
  const handleRecordDigest = scriptDigest("w-script-handle-record-v1", { lockRootDigest: lockDigest, owner: packageId, metadataDigest: "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", name: "chart-clock", capability: ".clock", contract: "chart/1" });
  const withHandle = runScriptWorkflowProgram(buildWithSidecar((lockObject) => {
    lockObject.requiredHandles = [{ name: "chart-clock", owner: packageId, metadataDigest: "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", capability: ".clock", contract: "chart/1", recordDigest: handleRecordDigest }];
  }, { handle: true }));
  const withoutHandle = runScriptWorkflowProgram(buildWithSidecar(() => {}));
  expect(baseline.status).toBe("accepted");
  expect(changedArtifact.status).toBe("accepted");
  expect(changedPolicy.status).toBe("accepted");
  expect(changedAction.status).toBe("accepted");
  expect(withHandle.status).toBe("accepted");
  expect(withoutHandle.status).toBe("accepted");
  expect(changedArtifact.state.product.identity).not.toBe(baseline.state.product.identity);
  expect(changedPolicy.state.product.identity).not.toBe(baseline.state.product.identity);
  expect(changedAction.state.product.identity).not.toBe(baseline.state.product.identity);
  expect(withHandle.state.product.identity).not.toBe(withoutHandle.state.product.identity);
});

test("final source bytes, not edit history, determine root digest and identity", () => {
  const finalText = "script horizon menu";
  const direct = runScriptWorkflowProgram(built({ sourceText: finalText }));
  const edited = runScriptWorkflowProgram([
    ...ready({ sourceText: "script horizon original" }),
    { op: "scriptResolve", lockDigest, lockObject: lock(), resultSourceText: finalText, resultParseEvidence: evidence(finalText, header()) },
    { op: "admitFetch", networkPolicy: "allow-pinned", cache: [lockDigest, "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"], fetches: [{ nodeId: packageId, digest: artifactDigest, authority: "w", signatureEvidence: null }] },
    { op: "verifyArtifact" },
    { op: "admitCapabilities", deployment: { grants: [".clock"] } },
    { op: "buildEphemeral", target: "x86_64-unknown-linux-gnu", hostProfile: "native-script@1", toolchain: { digest: toolchainDigest } },
  ]);
  expect(direct.status).toBe("accepted");
  expect(edited.status).toBe("accepted");
  expect(edited.state.source.rootDigest).toBe(direct.state.source.rootDigest);
  expect(edited.state.product.identity).toBe(direct.state.product.identity);
  expect(edited.state.product.identity).not.toBe(runScriptWorkflowProgram(built({ sourceText: `${finalText}!` })).state.product.identity);
});

test("parser evidence is bound to source bytes and mutation evidence", () => {
  const mismatched = ready()[0];
  mismatched.parseEvidence.sourceDigest = "sha256:" + "0".repeat(64);
  const parsed = runScriptWorkflowProgram([mismatched]);
  expect(parsed).toMatchObject({ status: "rejected", code: "parseEvidenceSourceMismatch" });
  const before = runScriptWorkflowProgram(ready());
  const result = runScriptWorkflowProgram([
    ...ready(),
    { op: "scriptRemove", alias: "chart", resultSourceText: "script horizon menu", resultParseEvidence: evidence("script horizon menu", { edition: "2026", dependencies: [], requires: [".network"] }) },
  ]);
  expect(result).toMatchObject({ status: "rejected", code: "resultParseEvidenceHeaderMismatch" });
  expect(result.state.source.rootDigest).toBe(before.state.source.rootDigest);
  expect(result.state.source.parseEvidence).toEqual(before.state.source.parseEvidence);
});

test("offline admission requires the lock root and closure objects, and retires divergent bytes", () => {
  const miss = runScriptWorkflowProgram([
    ...ready(),
    { op: "admitFetch", offline: true, cache: [artifactDigest] },
  ]);
  expect(miss).toMatchObject({ status: "rejected", code: "offlineCacheMiss", operation: 5 });

  const mismatch = runScriptWorkflowProgram([
    ...ready(),
    {
      op: "admitFetch",
      networkPolicy: "allow-pinned",
      cache: [lockDigest],
      fetches: [{ nodeId: packageId, digest: "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", authority: "w", signatureEvidence: null }],
    },
  ]);
  expect(mismatch).toMatchObject({ status: "rejected", code: "fetchDigestMismatch" });
  expect(mismatch.state.fetches.at(-1).status).toBe("retired");
});

test("default capability set cannot satisfy a clock requirement and extras are not effective", () => {
  const result = runScriptWorkflowProgram([
    ...ready(),
    { op: "admitFetch", offline: true, cache: [lockDigest, "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", "sha256:eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", artifactDigest] },
    { op: "verifyArtifact" },
    { op: "admitCapabilities", deployment: { grants: [".network"] } },
  ]);
  expect(result).toMatchObject({ status: "rejected", code: "capabilityMissing" });

  const extras = runScriptWorkflowProgram([
    ...ready(),
    { op: "admitFetch", networkPolicy: "allow-pinned", cache: [lockDigest], fetches: [{ nodeId: packageId, digest: artifactDigest, authority: "w", signatureEvidence: null }] },
    { op: "verifyArtifact" },
    { op: "admitCapabilities", deployment: { grants: [".clock", ".network"] } },
  ]);
  expect(extras.status).toBe("accepted");
    expect(extras.state.capabilities.effective).toEqual([".clock", ".stdio"]);
  expect(extras.state.capabilities.matched).toEqual([".clock"]);
});

test("transitive capability requires an explicit handle contract", () => {
  const transitiveId = "sha256:f8e2070dd4d4a445a35d768a95ada18a3c53263bf99b53b73e193e4413e7cc60";
  const scienceId = "sha256:cad50bbaf73176d3060c72fe62982143990d4e4d846ffeed72d3aa01b29bf8df";
  const transitiveDigest = "sha256:fb9cc75c22e55c43fc014ef041d1b769b14ea730a7523db11a17ab73af97a0f5";
  const handleMetadataDigest = "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";
  const requiredHandles = [{ name: "chart-clock", owner: transitiveId, metadataDigest: handleMetadataDigest, capability: ".clock", contract: "chart/1", recordDigest: scriptDigest("w-script-handle-record-v1", { lockRootDigest: transitiveDigest, owner: transitiveId, metadataDigest: handleMetadataDigest, name: "chart-clock", capability: ".clock", contract: "chart/1" }) }];
  const transitiveLock = { ...lock(requiredHandles), contexts: [{ ...lock(requiredHandles).contexts[0], nodes: [transitiveId, scienceId], rootEdges: [{ alias: "chart", id: transitiveId }] }], packages: [{ id: transitiveId, name: "fiction/chart", version: "1.2.3", source: dependency.source, metadataDigest: "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", contentDigest: "sha256:eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", dependencies: [{ alias: "science", id: scienceId }] }, { id: scienceId, name: "fiction/science", version: "0.4.0", source: dependency.source, metadataDigest: "sha256:ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", contentDigest: "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", dependencies: [] }], artifacts: [{ nodeId: transitiveId, digest: artifactDigest, authority: "w", target: "x86_64-unknown-linux-gnu", use: "product", signatureRequired: false, signatureEvidence: null }, { nodeId: scienceId, digest: "sha256:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", authority: "w", target: "x86_64-unknown-linux-gnu", use: "product", signatureRequired: false, signatureEvidence: null }], cas: ["sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", "sha256:eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", "sha256:ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", artifactDigest, "sha256:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"] };
  const transitiveReady = { scriptHeader: header(transitiveDigest), lockObject: transitiveLock };
  const missing = runScriptWorkflowProgram([
    ...ready(transitiveReady),
    { op: "admitFetch", networkPolicy: "allow-pinned", cache: [transitiveDigest, "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", "sha256:eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", "sha256:ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", artifactDigest, "sha256:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"] },
    { op: "verifyArtifact" },
    { op: "admitCapabilities", deployment: { grants: [".clock"] } },
  ]);
  const received = runScriptWorkflowProgram([
    ...ready({
      ...transitiveReady,
    }),
    { op: "admitFetch", networkPolicy: "allow-pinned", cache: [transitiveDigest, "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", "sha256:eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", "sha256:ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", artifactDigest, "sha256:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"] },
    { op: "verifyArtifact" },
    {
      op: "admitCapabilities",
      deployment: { grants: [".clock"] },
      receivedHandles: requiredHandles,
    },
  ]);
  expect(missing).toMatchObject({ status: "rejected", code: "transitiveHandleMissing" });
  expect(received).toMatchObject({ status: "accepted" });
});

test("failed atomic add leaves the original header unchanged", () => {
  const operations = ready();
  const before = runScriptWorkflowProgram(operations);
  const originalDigest = before.state.source.headerDigest;
  const result = runScriptWorkflowProgram([
    ...operations,
    {
      op: "scriptAdd",
      dependency: { alias: "new", package: "fiction/new", version: "1.0.0", use: "product", source: { kind: "registry", authority: "w", immutable: true } },
      lockDigest,
      lockObject: lock(),
    },
  ]);
  expect(result).toMatchObject({ status: "rejected", code: "selectionDigestMismatch" });
  expect(result.state.source.headerDigest).toBe(originalDigest);
  expect(result.state.source.rootDigest).toBe(before.state.source.rootDigest);
  expect(result.state.source.textDigest).toBe(before.state.source.textDigest);
  expect(result.state.source.header.dependencies).toHaveLength(1);

  const removed = runScriptWorkflowProgram([
    ...operations,
    { op: "scriptRemove", alias: "chart", resultSourceText: "script horizon menu", resultParseEvidence: evidence("script horizon menu", { edition: "2026", dependencies: [], requires: [".clock"] }) },
  ]);
  expect(removed.status).toBe("accepted");
  expect(removed.state.source.header.lock).toBeNull();
  expect(removed.state.resolution.digest).toBeNull();
});
