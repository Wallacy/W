import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const treeRoot = resolve(root, "tooling", "tree-sitter-w");
const generated = [
  "tooling/tree-sitter-w/src/parser.c",
  "tooling/tree-sitter-w/src/grammar.json",
  "tooling/tree-sitter-w/src/node-types.json",
  "tooling/tree-sitter-w/src/tree_sitter/alloc.h",
  "tooling/tree-sitter-w/src/tree_sitter/array.h",
  "tooling/tree-sitter-w/src/tree_sitter/parser.h",
];
const scanner = "tooling/tree-sitter-w/src/scanner.c";
const errors = [];

function git(args) {
  const result = spawnSync("git", args, {
    cwd: root,
    encoding: "utf8",
    windowsHide: true,
  });
  if (result.error) {
    errors.push(`git ${args.join(" ")}: ${result.error.message}`);
    return "";
  }
  return result.stdout.trim();
}

const tracked = new Set(
  git(["ls-files", "--cached", "--"]).split(/\r?\n/).filter(Boolean),
);
const trackedSource = git(["ls-files", "--cached", "--", "tooling/tree-sitter-w/src"])
  .split(/\r?\n/)
  .filter(Boolean);
const expectedTrackedSource = [scanner];
if (trackedSource.length !== expectedTrackedSource.length || trackedSource[0] !== scanner) {
  errors.push(
    `tracked Tree-sitter src must be exactly ${expectedTrackedSource.join(", ")}`,
  );
}

if (!tracked.has(scanner)) {
  errors.push(`${scanner} must remain tracked and authored`);
}

for (const path of generated) {
  if (tracked.has(path)) {
    errors.push(`${path} must not be tracked`);
  }
  const ignored = spawnSync("git", ["check-ignore", "--no-index", "--quiet", "--", path], {
    cwd: root,
    windowsHide: true,
  });
  if (ignored.status !== 0) {
    errors.push(`${path} is not covered by an ignore rule`);
  }
}
const scannerIgnored = spawnSync("git", ["check-ignore", "--no-index", "--quiet", "--", scanner], {
  cwd: root,
  windowsHide: true,
});
if (scannerIgnored.status === 0) {
  errors.push(`${scanner} must not be ignored`);
} else if (scannerIgnored.status !== 1) {
  errors.push(`could not verify that ${scanner} is not ignored`);
}

let rootPackage;
let treePackage;
try {
  rootPackage = JSON.parse(readFileSync(resolve(root, "package.json"), "utf8"));
  treePackage = JSON.parse(readFileSync(resolve(treeRoot, "package.json"), "utf8"));
} catch (error) {
  errors.push(`package metadata is not valid JSON: ${error.message}`);
}

if (rootPackage && rootPackage.scripts?.["check:generated-policy"] !== "bun tooling/check-generated-policy.mjs") {
  errors.push("root check:generated-policy must call tooling/check-generated-policy.mjs");
}
if (
  rootPackage?.scripts?.["tooling:install"] !==
  "bun ci --cwd tooling/tree-sitter-w && bun run --cwd tooling/tree-sitter-w generate"
) {
  errors.push("tooling:install must install locked tooling and generate the parser outputs");
}
if (treePackage && treePackage.scripts?.["check:generated-policy"] !== "bun ../check-generated-policy.mjs") {
  errors.push("Tree-sitter check:generated-policy must call the shared checker");
}
if (treePackage?.private !== true) {
  errors.push("Tree-sitter package must remain private");
}
if (treePackage?.scripts?.generate !== "tree-sitter generate") {
  errors.push("Tree-sitter generate must remain the explicit bootstrap command");
}
if (treePackage?.scripts?.check !== "bun ../check-suite.mjs --suite tree-check") {
  errors.push("Tree-sitter check must use the declarative tree-check suite");
}
if (treePackage?.scripts?.["check:docs"] !== "bun ../check-suite.mjs --suite tree-docs") {
  errors.push("Tree-sitter check:docs must use the declarative tree-docs suite");
}
if (rootPackage?.scripts?.check !== "bun tooling/check-suite.mjs --suite root-check") {
  errors.push("root check must use the declarative root-check suite");
}
if (rootPackage?.scripts?.["check:docs"] !== "bun tooling/check-suite.mjs --suite root-docs") {
  errors.push("root check:docs must use the declarative root-docs suite");
}
if (rootPackage?.scripts?.["check:studies"] !== "bun tooling/check-suite.mjs --suite root-studies") {
  errors.push("root check:studies must use the declarative root-studies suite");
}
if (rootPackage?.scripts?.["check:suite-manifest"] !== "bun test tooling/check-suite.test.mjs && bun tooling/check-suite.mjs --check") {
  errors.push("root check:suite-manifest must test and validate the declarative suite manifest");
}

const workflowPath = resolve(root, ".github", "workflows", "validate.yml");
let workflow = "";
try {
  workflow = readFileSync(workflowPath, "utf8");
} catch (error) {
  errors.push(`CI workflow is not readable: ${error.message}`);
}
const installMarker = "run: bun ci --cwd tooling/tree-sitter-w";
const checkMarker = "run: bun run check";
const installAt = workflow.indexOf(installMarker);
const checkAt = workflow.indexOf(checkMarker);
if (installAt < 0) {
  errors.push("CI must install the locked Tree-sitter tooling with bun ci");
}
if (checkAt < 0) {
  errors.push("CI must run the integrated root check");
}
if (installAt >= 0 && checkAt >= 0 && installAt >= checkAt) {
  errors.push("CI must install Tree-sitter tooling before the integrated check");
}

if (errors.length > 0) {
  for (const error of errors) process.stderr.write(`generated-policy: ${error}\n`);
  process.exit(1);
}

process.stdout.write(
  `generated-policy: ok (scanner tracked; ${generated.length} generated paths ignored; bootstrap guarded)\n`,
);
