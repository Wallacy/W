import { expect, test } from "bun:test";
import {
  moduleRunDigest,
  runModuleRunProgram,
} from "./module-run-machine.mjs";
import {
  ephemeralSourceDigest,
  runEphemeralModuleGraph,
} from "./ephemeral-module-graph-machine.mjs";

function providerFacts(sourceText, canonicalToken, overrides = {}) {
  const base = {
    providerId: "ru0-provider",
    rootToken: "ru0-root",
    owner: "ru0-owner",
    canonicalToken,
    opened: true,
    containment: "inside",
    snapshot: {
      stable: true,
      bytes: Buffer.byteLength(sourceText, "utf8"),
      digest: ephemeralSourceDigest(sourceText),
    },
  };
  return {
    ...base,
    ...overrides,
    snapshot: { ...base.snapshot, ...(overrides.snapshot ?? {}) },
  };
}

function graphEvidence({
  rootSourceId = "app.w",
  rootText = "module app\n",
  rootImports = [],
  sources = [],
  rootHeader,
  rootProvider,
  limits,
} = {}) {
  const root = {
    sourceId: rootSourceId,
    sourceText: rootText,
    imports: rootImports,
    provider: rootProvider ?? providerFacts(rootText, "root-token"),
    ...(rootHeader === undefined ? {} : { moduleHeader: rootHeader }),
  };
  return {
    rootSourceId,
    root,
    sources,
    ...(limits === undefined ? {} : { limits }),
  };
}

function localSource(sourceId, sourceText, imports = [], canonicalToken = sourceId, overrides = {}) {
  const { provider: providerOverrides, ...sourceOverrides } = overrides;
  return {
    sourceId,
    sourceText,
    imports,
    provider: providerFacts(sourceText, canonicalToken, providerOverrides),
    ...sourceOverrides,
  };
}

function projectReady({ path = "reference/last-light/horizon_tool.w" } = {}) {
  return [
    {
      op: "parseModule",
      path,
      sourceText: "module horizon_tool\nimport chart.science\nentry(runHorizon)\nprint(result)\n",
      entry: "default",
      imports: [{ path: "chart.science", external: true }],
    },
    { op: "selectContext", mode: "package", rootKind: "package" },
    { op: "resolveRoots", rootKind: "package", localRoot: "package" },
    { op: "validateImports", imports: [{ path: "chart.science", external: true }] },
    {
      op: "validateResolution",
      resolution: {
        schema: "w.resolution/1",
        resolver: "w.resolver/1",
        contexts: [{ kind: "package", root: "package", nodes: [], edges: [] }],
        packages: [],
      },
    },
  ];
}

test("module-run uses the explicit default descriptor and excludes physical paths", () => {
  const first = runModuleRunProgram([
    ...projectReady({ path: "reference/last-light/horizon_tool.w" }),
    { op: "buildModule" },
    { op: "runEntry", entry: "default", args: ["horizon"] },
    { op: "cleanup" },
  ]);
  const moved = runModuleRunProgram([
    ...projectReady({ path: "moved/horizon_tool.w" }),
    { op: "buildModule" },
  ]);
  expect(first.status).toBe("accepted");
  expect(first.state.source.entryForm).toBe("explicit");
  expect(first.state.product.kind).toBe("module");
  expect(first.state.product.identity).toBe(moved.state.product.identity);
  expect(first.state.cleanup.hiddenArtifacts).toEqual([]);
});

test("implicit entry and script header mutations remain rejected", () => {
  expect(runModuleRunProgram([
    { op: "parseModule", sourceText: "print(result)\n", imports: [], implicitEntryBody: true },
  ])).toMatchObject({ status: "rejected", code: "implicitEntryBodyRejected" });
  expect(runModuleRunProgram([
    { op: "parseModule", sourceText: "script { edition: 2026 }\n", imports: [], legacyHeaderKind: "script" },
  ])).toMatchObject({ status: "rejected", code: "scriptHeaderRejected" });
});

test("package.lock and deployment roots cannot replace the unified roots", () => {
  const lockRoot = runModuleRunProgram([
    { op: "parseModule", sourceText: "module x\nentry(default)\n", entry: "default", imports: [] },
    { op: "selectContext", mode: "package", rootKind: "package" },
    { op: "resolveRoots", rootKind: "lock", rootName: "package.lock" },
  ]);
  const deploymentRoot = runModuleRunProgram([
    { op: "parseModule", sourceText: "module x\nentry(default)\n", entry: "default", imports: [] },
    { op: "selectContext", mode: "workspace", rootKind: "workspace" },
    { op: "resolveRoots", rootKind: "deployment", rootName: "deployment" },
  ]);
  expect(lockRoot).toMatchObject({ status: "rejected", code: "moduleRootRejected" });
  expect(deploymentRoot).toMatchObject({ status: "rejected", code: "moduleRootRejected" });
});

test("a mutated legacy resolution root is rejected before build", () => {
  const result = runModuleRunProgram([
    { op: "parseModule", sourceText: "module x\nentry(default)\n", entry: "default", imports: [] },
    { op: "selectContext", mode: "package", rootKind: "package" },
    { op: "resolveRoots", rootKind: "package" },
    { op: "validateImports", imports: [] },
    {
      op: "validateResolution",
      resolution: {
        schema: "w.resolution/1",
        resolver: "w.resolver/1",
        contexts: [{ kind: "package", root: '.product("script")' }],
        packages: [],
      },
    },
  ]);
  expect(result).toMatchObject({ status: "rejected", code: "legacyRootFieldRejected" });
});

test("resolution and context explanation remain deterministic", () => {
  const first = runModuleRunProgram([
    { op: "parseModule", sourceText: "module x\nentry(default)\n", entry: "default", imports: [] },
    { op: "selectContext", mode: "ephemeral" },
    { op: "resolveRoots", localRoot: "examples" },
    { op: "validateImports", imports: [] },
    { op: "validateResolution" },
    { op: "buildModule" },
    { op: "contextExplanation" },
  ]);
  expect(first.status).toBe("accepted");
  expect(first.state.context.explanation.resolutionDigest).toBeNull();
  expect(moduleRunDigest("same", { b: 2, a: 1 })).toBe(moduleRunDigest("same", { a: 1, b: 2 }));
});

test("ephemeral graph maps local imports from the explicit root and keeps std separate", () => {
  const result = runEphemeralModuleGraph(graphEvidence({
    rootText: "module app\nimport command\nimport platform.native\nimport std.io\n",
    rootImports: [{ path: "command" }, { path: "platform.native" }, { path: "std.io" }],
    sources: [
      localSource("platform/native.w", "module native\n", [], "native-token"),
      localSource("command.w", "module command\nimport domain\n", [{ path: "domain" }], "command-token"),
      localSource("domain.w", "module domain\n", [], "domain-token"),
      { sourceId: "unused.w", sourceText: "module unused\n", imports: [], path: "moved/unused.w" },
      { sourceId: "std.w", sourceText: "module std\n", imports: [] },
    ],
  }));
  expect(result.status).toBe("accepted");
  expect(result.graph.inventory.map(({ ordinal, sourceId, modulePath }) => ({ ordinal, sourceId, modulePath }))).toEqual([
    { ordinal: 0, sourceId: "app.w", modulePath: "app" },
    { ordinal: 1, sourceId: "command.w", modulePath: "command" },
    { ordinal: 2, sourceId: "domain.w", modulePath: "domain" },
    { ordinal: 3, sourceId: "platform/native.w", modulePath: "platform.native" },
  ]);
  expect(result.graph.edges.find((edge) => edge.origin === "std.io")).toMatchObject({ provider: "std", targetSourceId: null });
  expect(result.graph.inventory.some(({ sourceId }) => sourceId === "unused.w" || sourceId === "std.w")).toBe(false);
  expect(JSON.stringify(result.graph.recipe)).not.toContain("canonicalToken");
  expect(JSON.stringify(result.graph.recipe)).not.toContain("moved/unused.w");
  const explicitOrigins = runEphemeralModuleGraph(graphEvidence({
    rootImports: [{ path: "child", kind: "reexport" }],
    sources: [
      localSource("child.w", "module child\nimport leaf\n", [{ path: "leaf", kind: "service-import" }], "origin-child"),
      localSource("leaf.w", "module leaf\n", [], "origin-leaf"),
    ],
  }));
  expect(explicitOrigins.status).toBe("accepted");
  expect(explicitOrigins.graph.edges.map(({ kind }) => kind)).toEqual(["reexport", "service-import"]);
});

test("root header may differ from its stem, while imported headers must match", () => {
  const positive = runEphemeralModuleGraph(graphEvidence({
    rootHeader: "main",
    rootImports: [{ path: "child" }],
    rootText: "module main\nimport child\n",
    sources: [localSource("child.w", "module child\n", [], "child-token")],
  }));
  expect(positive.status).toBe("accepted");
  expect(positive.graph.root.modulePath).toBe("main");
  expect(runEphemeralModuleGraph(graphEvidence({ rootSourceId: "nested/app.w" }))).toMatchObject({
    status: "rejected",
    code: "rootSourceIdNested",
  });
  const mismatch = runEphemeralModuleGraph(graphEvidence({
    rootImports: [{ path: "child" }],
    sources: [localSource("child.w", "module wrong\n", [], "child-token", { moduleHeader: "wrong" })],
  }));
  expect(mismatch).toMatchObject({ status: "rejected", code: "moduleHeaderMismatch" });
});

test("ephemeral graph inventory and recipe ignore candidate order and physical provenance", () => {
  const first = graphEvidence({
    rootText: "module app\nimport zeta\nimport alpha\n",
    rootImports: [{ path: "zeta" }, { path: "alpha" }],
    sources: [
      localSource("zeta.w", "module zeta\n", [], "token-z", { physicalDisplay: "one/zeta.w" }),
      localSource("alpha.w", "module alpha\n", [], "token-a", { physicalDisplay: "one/alpha.w" }),
    ],
  });
  const second = graphEvidence({
    rootText: first.root.sourceText,
    rootImports: [{ path: "alpha" }, { path: "zeta" }],
    sources: [
      localSource("alpha.w", "module alpha\n", [], "other-alpha", { physicalDisplay: "two/alpha.w" }),
      localSource("zeta.w", "module zeta\n", [], "other-zeta", { physicalDisplay: "two/zeta.w" }),
    ],
  });
  const firstResult = runEphemeralModuleGraph(first);
  const secondResult = runEphemeralModuleGraph(second);
  expect(firstResult.status).toBe("accepted");
  expect(secondResult.status).toBe("accepted");
  expect(firstResult.graph.inventory).toEqual(secondResult.graph.inventory);
  expect(firstResult.graph.recipe).toEqual(secondResult.graph.recipe);
  expect(firstResult.graph.recipeKey).toBe(secondResult.graph.recipeKey);
  expect(firstResult.graph.provenance).not.toEqual(secondResult.graph.provenance);
});

test("module-run keeps full product recipe identity around the logical ephemeral graph", () => {
  const rootText = "module app\nimport child\n";
  const imports = [{ path: "child" }];
  const makeGraph = (canonicalToken, physicalDisplay) => graphEvidence({
    rootText,
    rootImports: imports,
    rootHeader: "app",
    sources: [localSource("child.w", "module child\n", [], canonicalToken, { physicalDisplay })],
  });
  const makeProgram = ({ graph, target = "x86_64-unknown-linux-gnu", toolchainDigest = "toolchain-a" }) => runModuleRunProgram([
    { op: "parseModule", path: "reference/last-light/app.w", sourceText: rootText, moduleHeader: "app", entry: "default", imports },
    { op: "selectContext", mode: "ephemeral" },
    { op: "resolveRoots", localRoot: "reference/last-light" },
    { op: "validateImports", imports, localGraph: graph },
    { op: "validateResolution" },
    { op: "buildModule", target, toolchainDigest },
  ]);
  const baseline = makeProgram({ graph: makeGraph("child-a", "one/child.w") });
  const targetVariant = makeProgram({ graph: makeGraph("child-a", "one/child.w"), target: "wasm32-wasi" });
  const toolchainVariant = makeProgram({ graph: makeGraph("child-a", "one/child.w"), toolchainDigest: "toolchain-b" });
  const provenanceVariant = makeProgram({ graph: makeGraph("child-b", "two/child.w") });
  expect(baseline.status).toBe("accepted");
  expect(targetVariant.status).toBe("accepted");
  expect(toolchainVariant.status).toBe("accepted");
  expect(provenanceVariant.status).toBe("accepted");
  expect(targetVariant.state.product.identity).not.toBe(baseline.state.product.identity);
  expect(toolchainVariant.state.product.identity).not.toBe(baseline.state.product.identity);
  expect(provenanceVariant.state.product.identity).toBe(baseline.state.product.identity);
  expect(provenanceVariant.state.product.graphRecipeKey).toBe(baseline.state.product.graphRecipeKey);
});

test("module-run preserves exact ephemeral bytes and requires local graph evidence", () => {
  const rootText = "module app\r\nimport child\r\n";
  const childText = "module child\r\n";
  const imports = [{ path: "child" }];
  const makeGraph = (rootProvider, rootHeader = "app") => graphEvidence({
    rootText,
    rootImports: imports,
    rootProvider,
    rootHeader,
    sources: [localSource("child.w", childText, [], "child-token")],
  });
  const makeProgram = (graph, moduleHeader = "app") => runModuleRunProgram([
    { op: "parseModule", path: "reference/last-light/app.w", sourceText: rootText, moduleHeader, entry: "default", imports },
    { op: "selectContext", mode: "ephemeral" },
    { op: "resolveRoots", localRoot: "reference/last-light" },
    { op: "validateImports", imports, localGraph: graph },
    { op: "validateResolution" },
    { op: "buildModule" },
  ]);
  const positive = makeProgram(makeGraph(providerFacts(rootText, "root-token")));
  expect(positive.status).toBe("accepted");
  expect(positive.state.source.sourceDigest).toBe(moduleRunDigest("w-module-source-v1", rootText.replace(/\r\n?/g, "\n")));
  expect(positive.state.source.sourceBytesDigest).toBe(ephemeralSourceDigest(rootText));
  expect(positive.state.product.moduleGraph.inventory[0].digest).toBe(ephemeralSourceDigest(rootText));
  expect(makeProgram(makeGraph(providerFacts(rootText, "root-token", { snapshot: { digest: "sha256:stale" } })))).toMatchObject({
    status: "rejected",
    code: "sourceDigestMismatch",
  });
  expect(makeProgram(makeGraph(providerFacts(rootText, "root-token", { snapshot: { stable: false } })))).toMatchObject({
    status: "rejected",
    code: "unstableSnapshot",
  });
  expect(makeProgram(makeGraph(providerFacts(rootText, "root-token"), "wrong"))).toMatchObject({
    status: "rejected",
    code: "moduleHeaderEvidenceMismatch",
  });
  expect(runModuleRunProgram([
    { op: "parseModule", sourceText: "module app\nimport child\n", entry: "default", imports },
    { op: "selectContext", mode: "ephemeral" },
    { op: "resolveRoots", localRoot: "reference/last-light" },
    { op: "validateImports", imports },
  ])).toMatchObject({ status: "rejected", code: "localGraphEvidenceRequired" });
});

test("ephemeral graph rejects missing local dependencies and provider evidence", () => {
  expect(runEphemeralModuleGraph(graphEvidence({
    rootImports: [{ path: "missing" }],
  }))).toMatchObject({ status: "rejected", code: "externalDependencyInEphemeral" });
  expect(runEphemeralModuleGraph(graphEvidence({
    rootImports: [{ path: "child" }],
    sources: [{ sourceId: "child.w", sourceText: "module child\n", imports: [] }],
  }))).toMatchObject({ status: "rejected", code: "providerFactMissing" });
  for (const path of ["child/part", "child..part", "child.", ".child"]) {
    expect(runEphemeralModuleGraph(graphEvidence({ rootImports: [{ path }] }))).toMatchObject({
      status: "rejected",
      code: "importComponentRejected",
    });
  }
});

test("ephemeral graph rejects containment, symlink, canonical-token, NFC and snapshot faults", () => {
  const cases = [
    ["outsideContainment", { provider: { containment: "outside" } }],
    ["symlinkEscape", { provider: { symlink: true, symlinkContainment: "outside" } }],
    ["duplicateCanonicalToken", { canonicalToken: "root-token" }],
    ["unstableSnapshot", { provider: { snapshot: { stable: false } } }],
    ["sourceDigestMismatch", { provider: { snapshot: { digest: "sha256:stale" } } }],
  ];
  for (const [code, overrides] of cases) {
    expect(runEphemeralModuleGraph(graphEvidence({
      rootImports: [{ path: "child" }],
      sources: [localSource("child.w", "module child\n", [], "root-token", overrides)],
    }))).toMatchObject({ status: "rejected", code });
  }
  expect(runEphemeralModuleGraph(graphEvidence({
    rootImports: [{ path: "café" }, { path: "café" }],
    sources: [localSource("café.w", "module café\n", [], "cafe-token")],
  }))).toMatchObject({ status: "rejected", code: "nfcLogicalCollision" });
});

test("ephemeral graph rejects self cycles and strongly connected components", () => {
  expect(runEphemeralModuleGraph(graphEvidence({
    rootImports: [{ path: "app" }],
  }))).toMatchObject({ status: "rejected", code: "moduleGraphCycle" });
  expect(runEphemeralModuleGraph(graphEvidence({
    rootImports: [{ path: "one" }],
    sources: [
      localSource("one.w", "module one\nimport two\n", [{ path: "two" }], "one-token"),
      localSource("two.w", "module two\nimport one\n", [{ path: "one" }], "two-token"),
    ],
  }))).toMatchObject({ status: "rejected", code: "moduleGraphCycle" });
});

test("ephemeral graph applies the longest root-to-node depth after cycle checks", () => {
  const result = runEphemeralModuleGraph(graphEvidence({
    rootText: "module app\nimport a\nimport b\n",
    rootImports: [{ path: "a" }, { path: "b" }],
    limits: { maxSources: 64, maxEdges: 4096, maxDepth: 2, maxTotalSourceBytes: 16 * 1024 * 1024 },
    sources: [
      localSource("a.w", "module a\n", [], "long-a"),
      localSource("b.w", "module b\nimport c\n", [{ path: "c" }], "long-b"),
      localSource("c.w", "module c\nimport a\n", [{ path: "a" }], "long-c"),
    ],
  }));
  expect(result).toMatchObject({ status: "rejected", code: "ephemeralDepthLimit" });
  expect(result.facts).toMatchObject({ sourceId: "a.w", depth: 3, limit: 2 });
});

test("ephemeral graph enforces finite provider limits and deterministic duplicate edges", () => {
  const limited = (limits, rootImports = [{ path: "child" }]) => runEphemeralModuleGraph(graphEvidence({
    rootImports,
    limits,
    sources: [localSource("child.w", "module child\n", [], "child-token")],
  }));
  expect(limited({ maxSources: 1, maxEdges: 4096, maxDepth: 64, maxTotalSourceBytes: 16 * 1024 * 1024 })).toMatchObject({ status: "rejected", code: "ephemeralSourceLimit" });
  expect(limited({ maxSources: 64, maxEdges: 0, maxDepth: 64, maxTotalSourceBytes: 16 * 1024 * 1024 })).toMatchObject({ status: "rejected", code: "ephemeralEdgeLimit" });
  expect(limited({ maxSources: 64, maxEdges: 4096, maxDepth: 0, maxTotalSourceBytes: 16 * 1024 * 1024 })).toMatchObject({ status: "rejected", code: "ephemeralDepthLimit" });
  expect(limited({ maxSources: 64, maxEdges: 4096, maxDepth: 64, maxTotalSourceBytes: 10 })).toMatchObject({ status: "rejected", code: "ephemeralSourceBytesLimit" });

  const first = runEphemeralModuleGraph(graphEvidence({
    rootImports: [{ path: "b" }, { path: "a" }, { path: "b" }],
    sources: [
      localSource("a.w", "module a\n", [], "a-token"),
      localSource("b.w", "module b\n", [], "b-token"),
    ],
  }));
  const second = runEphemeralModuleGraph(graphEvidence({
    rootImports: [{ path: "b" }, { path: "b" }, { path: "a" }],
    sources: [
      localSource("b.w", "module b\n", [], "b-other"),
      localSource("a.w", "module a\n", [], "a-other"),
    ],
  }));
  expect(first.status).toBe("accepted");
  expect(second.status).toBe("accepted");
  expect(first.graph.recipeKey).toBe(second.graph.recipeKey);
  expect(first.graph.edges.filter(({ sourceOrdinal }) => sourceOrdinal === 0).map(({ origin }) => origin)).toEqual(["a", "b", "b"]);
  expect(first.graph.inventory[0].ordinal).toBe(0);
});
