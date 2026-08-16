import { expect, test } from "bun:test";
import {
  moduleRunDigest,
  runModuleRunProgram,
} from "./module-run-machine.mjs";

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
