import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  runPackageReleaseProgram,
  validatePackageReleaseOperation,
} from "./package-release-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const casesPath = path.join(toolingDirectory, "package-release-cases.json");
const snapshotPath = path.join(toolingDirectory, "package-release-results.snapshot.jsonl");
const corpus = JSON.parse(fs.readFileSync(casesPath, "utf8"));
const errors = [];
const caseIds = new Set();
const results = [];
const coveredOperations = new Set();
let operationCount = 0;

const requiredOperations = new Set([
  "acceptRegistrySnapshot",
  "createRealm",
  "addRequirement",
  "solveRealm",
  "verifySelection",
  "writeLock",
  "validateLocked",
  "declareObject",
  "storeObject",
  "registerMirror",
  "fetchMirror",
  "requireOffline",
  "createRecipe",
  "verifyRecipeRelation",
  "runBuild",
  "verifyBuildRelation",
  "createRelease",
  "verifyRelease",
  "addAdvisory",
  "yankRelease",
  "revokeRelease",
  "verifyReleaseState",
]);

function requireString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    errors.push(`${location} must be a non-empty string.`);
    return false;
  }
  return true;
}

function resolveReference(reference, location) {
  if (!reference || typeof reference !== "object") {
    errors.push(`${location} must be an object.`);
    return;
  }
  if (!requireString(reference.path, `${location}.path`)) return;
  const resolved = path.resolve(toolingDirectory, reference.path);
  const relative = path.relative(wDirectory, resolved);
  if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
    errors.push(`${location}.path must stay inside the W repository.`);
    return;
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    errors.push(`${location}.path references a missing file.`);
    return;
  }
  if (
    requireString(reference.symbol, `${location}.symbol`) &&
    !fs.readFileSync(resolved, "utf8").includes(reference.symbol)
  ) {
    errors.push(`${location}.symbol is absent from ${reference.path}.`);
  }
}

function expandFixture(name, stack = []) {
  if (stack.includes(name)) {
    errors.push(`Fixture cycle: ${[...stack, name].join(" -> ")}.`);
    return [];
  }
  const fixture = corpus.fixtures?.[name];
  if (!fixture) {
    errors.push(`Unknown fixture ${name}.`);
    return [];
  }
  const prefix = (fixture.includes ?? []).flatMap((included) =>
    expandFixture(included, [...stack, name]),
  );
  return [...prefix, ...(fixture.operations ?? [])];
}

function compactState(state) {
  return {
    trustedSnapshotVersion: state.trustedSnapshotVersion,
    snapshots: Object.fromEntries(
      Object.entries(state.snapshots).map(([name, snapshot]) => [
        name,
        { version: snapshot.version, digest: snapshot.digest },
      ]),
    ),
    realms: Object.fromEntries(
      Object.entries(state.realms).map(([name, realm]) => [
        name,
        { digest: realm.digest ?? null, selections: realm.selections },
      ]),
    ),
    locks: Object.fromEntries(
      Object.entries(state.locks).map(([name, lock]) => [
        name,
        { digest: lock.digest, validated: lock.validated },
      ]),
    ),
    casDigests: Object.keys(state.cas).sort(),
    recipes: Object.fromEntries(
      Object.entries(state.recipes).map(([name, recipe]) => [name, recipe.digest]),
    ),
    builds: Object.fromEntries(
      Object.entries(state.builds).map(([name, build]) => [
        name,
        {
          recipeDigest: build.recipeDigest,
          artifactDigest: build.artifactDigest,
          inputsComplete: build.inputsComplete,
          outputsComplete: build.outputsComplete,
          builderIdentity: build.builderIdentity,
          operatorIdentity: build.operatorIdentity,
          credentialIdentity: build.credentialIdentity,
          executionRootIdentity: build.executionRootIdentity,
        },
      ]),
    ),
    releases: Object.fromEntries(
      Object.entries(state.releases).map(([name, release]) => [
        name,
        {
          decision: release.decision,
          revoked: release.revoked,
          yanked: release.yanked,
          advisories: release.advisories,
        },
      ]),
    ),
  };
}

if (corpus.$schema !== "w-package-release-cases-1") {
  errors.push("package-release-cases.json must use schema w-package-release-cases-1.");
}
if (corpus.status !== "design-oracle-input") {
  errors.push("package-release-cases.json must have status design-oracle-input.");
}
if (!corpus.fixtures || typeof corpus.fixtures !== "object") {
  errors.push("package-release-cases.json must contain fixtures.");
}
if (!Array.isArray(corpus.cases) || corpus.cases.length === 0) {
  errors.push("package-release-cases.json must contain cases.");
}

for (const [fixtureName, fixture] of Object.entries(corpus.fixtures ?? {})) {
  if (!Array.isArray(fixture.operations)) {
    errors.push(`fixtures.${fixtureName}.operations must be an array.`);
  }
  if (
    fixture.includes !== undefined &&
    (!Array.isArray(fixture.includes) || fixture.includes.some((name) => typeof name !== "string"))
  ) {
    errors.push(`fixtures.${fixtureName}.includes must contain names.`);
  }
}

for (const [caseIndex, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${caseIndex}]`;
  if (!/^P0-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(testCase.id ?? "")) {
    errors.push(`${location}.id must use the P0-kebab-case form.`);
  } else if (caseIds.has(testCase.id)) {
    errors.push(`${location}.id duplicates ${testCase.id}.`);
  } else {
    caseIds.add(testCase.id);
  }

  if (!Array.isArray(testCase.references) || testCase.references.length === 0) {
    errors.push(`${location}.references must link to Last Light.`);
  } else {
    testCase.references.forEach((reference, referenceIndex) =>
      resolveReference(reference, `${location}.references[${referenceIndex}]`),
    );
  }

  const operations = [
    ...(testCase.fixtures ?? []).flatMap((fixture) => expandFixture(fixture)),
    ...(testCase.operations ?? []),
  ];
  if (operations.length === 0) {
    errors.push(`${location}.operations must not be empty after fixture expansion.`);
    continue;
  }
  operationCount += operations.length;
  operations.forEach((operation, operationIndex) => {
    coveredOperations.add(operation.op);
    if (!validatePackageReleaseOperation(operation)) {
      errors.push(`${location}.operations[${operationIndex}] is malformed.`);
    }
  });

  if (!["accepted", "rejected"].includes(testCase.expected?.status)) {
    errors.push(`${location}.expected.status must be accepted or rejected.`);
    continue;
  }
  if (testCase.expected.status === "accepted") {
    if (testCase.expected.code !== undefined || testCase.expected.at !== undefined) {
      errors.push(`${location}.expected accepted outcome cannot contain rejection fields.`);
    }
  } else if (
    !requireString(testCase.expected.code, `${location}.expected.code`) ||
    testCase.expected.at !== "last"
  ) {
    errors.push(`${location}.expected rejection must identify code at the last operation.`);
  }

  const actual = runPackageReleaseProgram(operations);
  if (actual.status !== testCase.expected.status) {
    errors.push(
      `${location}.expected.status is ${testCase.expected.status}; actual is ${actual.status}.`,
    );
  }
  if (testCase.expected.status === "rejected") {
    if (actual.code !== testCase.expected.code) {
      errors.push(
        `${location}.expected.code is ${testCase.expected.code}; actual is ${actual.code}.`,
      );
    }
    if (actual.operation !== operations.length - 1) {
      errors.push(
        `${location} rejected at operation ${actual.operation}; expected the last operation ${operations.length - 1}.`,
      );
    }
  }
  results.push({
    caseId: testCase.id,
    status: actual.status,
    ...(actual.code ? { code: actual.code, operation: actual.operation } : {}),
    state: compactState(actual.state),
    trace: actual.trace,
  });
}

for (const operation of requiredOperations) {
  if (!coveredOperations.has(operation)) {
    errors.push(`The P0 corpus does not cover ${operation}.`);
  }
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

const expectedSnapshot = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
const acceptedCount = results.filter((result) => result.status === "accepted").length;
const rejectedCount = results.length - acceptedCount;
const summary =
  `Packages and releases: ${results.length} cases, ${operationCount} operations, ` +
  `${acceptedCount} accepted, ${rejectedCount} rejected, ` +
  `${requiredOperations.size}/${requiredOperations.size} required operations.`;

if (process.argv.includes("--write")) {
  fs.writeFileSync(snapshotPath, expectedSnapshot);
  process.stdout.write(`${summary}\nUpdated ${path.basename(snapshotPath)}.\n`);
  process.exit(0);
}

if (!fs.existsSync(snapshotPath)) {
  process.stderr.write(`${path.basename(snapshotPath)} is missing; run with --write.\n`);
  process.exit(1);
}
if (fs.readFileSync(snapshotPath, "utf8") !== expectedSnapshot) {
  process.stderr.write(`${path.basename(snapshotPath)} is stale; run with --write.\n`);
  process.exit(1);
}

process.stdout.write(`${summary}\n`);
