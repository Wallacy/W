import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const studiesDirectory = path.join(toolingDirectory, "studies");
const substitutionCorpus = JSON.parse(
  fs.readFileSync(path.join(toolingDirectory, "substitution-cases.json"), "utf8"),
);
const r0CaseIds = new Set(substitutionCorpus.cases.map((testCase) => testCase.id));
const studiedR0CaseIds = new Set();
const errors = [];
const bundleIds = new Set();
const requiredTaskKinds = ["explain", "recall", "repair", "change"];
let variantCount = 0;
let taskCount = 0;

function digest(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function requireString(value, location) {
  if (typeof value !== "string" || value.trim() === "") {
    errors.push(`${location} must be a non-empty string.`);
    return false;
  }

  return true;
}

function resolveContained(base, relative, boundary, location) {
  if (!requireString(relative, location)) {
    return undefined;
  }

  const resolved = path.resolve(base, relative);
  const relativeToBoundary = path.relative(boundary, resolved);

  if (
    relativeToBoundary === "" ||
    relativeToBoundary.startsWith(`..${path.sep}`) ||
    path.isAbsolute(relativeToBoundary)
  ) {
    errors.push(`${location} must resolve to a file inside ${boundary}.`);
    return undefined;
  }

  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    errors.push(`${location} references a missing file.`);
    return undefined;
  }

  return resolved;
}

function checkDigest(file, expected, location) {
  if (!file || !/^sha256:[0-9a-f]{64}$/.test(expected ?? "")) {
    if (file) errors.push(`${location} must use a lowercase sha256 digest.`);
    return;
  }

  const actual = digest(file);
  if (actual !== expected) {
    errors.push(`${location} is stale; expected ${actual}.`);
  }
}

const bundleFiles = fs
  .readdirSync(studiesDirectory, { withFileTypes: true })
  .filter((entry) => entry.isDirectory())
  .map((entry) => path.join(studiesDirectory, entry.name, "bundle.json"))
  .filter((file) => fs.existsSync(file))
  .sort((left, right) => Buffer.from(left).compare(Buffer.from(right)));

if (bundleFiles.length === 0) {
  errors.push("At least one R1 study bundle is required.");
}

for (const bundleFile of bundleFiles) {
  const bundleDirectory = path.dirname(bundleFile);
  const bundle = JSON.parse(fs.readFileSync(bundleFile, "utf8"));
  const location = path.relative(wDirectory, bundleFile).replaceAll(path.sep, "/");

  if (bundle.$schema !== "w-substitution-study-bundle-1") {
    errors.push(`${location} must use schema w-substitution-study-bundle-1.`);
  }
  if (bundle.status !== "design-oracle-input") {
    errors.push(`${location} must have status design-oracle-input.`);
  }
  if (!/^R1-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(bundle.id ?? "")) {
    errors.push(`${location}.id must use the R1-kebab-case form.`);
  } else if (bundleIds.has(bundle.id)) {
    errors.push(`${location}.id duplicates ${bundle.id}.`);
  } else {
    bundleIds.add(bundle.id);
  }
  requireString(bundle.title, `${location}.title`);
  requireString(bundle.entry, `${location}.entry`);

  if (!Array.isArray(bundle.r0Cases) || bundle.r0Cases.length === 0) {
    errors.push(`${location}.r0Cases must contain at least one R0 case ID.`);
  } else {
    for (const caseId of bundle.r0Cases) {
      if (!r0CaseIds.has(caseId)) {
        errors.push(`${location}.r0Cases references unknown case ${caseId}.`);
      } else {
        studiedR0CaseIds.add(caseId);
      }
    }
  }

  const sourceBase = resolveContained(
    bundleDirectory,
    bundle.sourceBase?.path,
    wDirectory,
    `${location}.sourceBase.path`,
  );
  checkDigest(sourceBase, bundle.sourceBase?.digest, `${location}.sourceBase.digest`);
  if (
    sourceBase &&
    requireString(bundle.sourceBase?.symbol, `${location}.sourceBase.symbol`) &&
    !fs.readFileSync(sourceBase, "utf8").includes(bundle.sourceBase.symbol)
  ) {
    errors.push(`${location}.sourceBase.symbol is absent from the source base.`);
  }

  if (!Array.isArray(bundle.variants) || bundle.variants.length < 2) {
    errors.push(`${location}.variants must contain at least two variants.`);
    continue;
  }

  variantCount += bundle.variants.length;
  const variantIds = new Set();
  const sourceDigests = new Set();
  let selectedCount = 0;

  for (const [index, variant] of bundle.variants.entries()) {
    const variantLocation = `${location}.variants[${index}]`;
    if (!/^[a-z0-9]+(?:-[a-z0-9]+)*$/.test(variant.id ?? "")) {
      errors.push(`${variantLocation}.id must use kebab-case.`);
    } else if (variantIds.has(variant.id)) {
      errors.push(`${variantLocation}.id duplicates ${variant.id}.`);
    } else {
      variantIds.add(variant.id);
    }
    if (variant.role === "selected") selectedCount += 1;
    if (!["selected", "alternative"].includes(variant.role)) {
      errors.push(`${variantLocation}.role must be selected or alternative.`);
    }
    requireString(variant.language, `${variantLocation}.language`);
    const variantFile = resolveContained(
      bundleDirectory,
      variant.path,
      bundleDirectory,
      `${variantLocation}.path`,
    );
    checkDigest(variantFile, variant.digest, `${variantLocation}.digest`);
    if (variant.digest) sourceDigests.add(variant.digest);
    if (
      variantFile &&
      requireString(bundle.entry, `${location}.entry`) &&
      !fs.readFileSync(variantFile, "utf8").includes(bundle.entry)
    ) {
      errors.push(`${variantLocation} does not contain entry ${bundle.entry}.`);
    }
    if (!Array.isArray(variant.changedConstructs) || variant.changedConstructs.length === 0) {
      errors.push(`${variantLocation}.changedConstructs must not be empty.`);
    } else {
      variant.changedConstructs.forEach((construct, constructIndex) =>
        requireString(construct, `${variantLocation}.changedConstructs[${constructIndex}]`),
      );
    }
  }

  if (selectedCount !== 1) {
    errors.push(`${location}.variants must contain exactly one selected role.`);
  }
  if (sourceDigests.size !== bundle.variants.length) {
    errors.push(`${location}.variants must reference distinct source bytes.`);
  }

  if (!Array.isArray(bundle.inputs) || bundle.inputs.length < 2) {
    errors.push(`${location}.inputs must contain a primary and adversarial input.`);
  } else {
    const inputIds = new Set();
    for (const [index, input] of bundle.inputs.entries()) {
      const inputLocation = `${location}.inputs[${index}]`;
      if (!requireString(input.id, `${inputLocation}.id`)) continue;
      if (inputIds.has(input.id)) errors.push(`${inputLocation}.id duplicates ${input.id}.`);
      inputIds.add(input.id);
      if (input.expected === undefined) {
        errors.push(`${inputLocation}.expected is required.`);
      }
    }
  }

  const taskKinds = Array.isArray(bundle.tasks) ? bundle.tasks.map((task) => task.kind) : [];
  if (JSON.stringify(taskKinds) !== JSON.stringify(requiredTaskKinds)) {
    errors.push(`${location}.tasks must use explain, recall, repair, change in that order.`);
  } else {
    taskCount += bundle.tasks.length;
    bundle.tasks.forEach((task, index) =>
      requireString(task.instruction, `${location}.tasks[${index}].instruction`),
    );
    if (!Number.isInteger(bundle.tasks[1].minimumDelaySeconds) || bundle.tasks[1].minimumDelaySeconds < 1) {
      errors.push(`${location}.tasks[1].minimumDelaySeconds must be a positive integer.`);
    }
    const mutationKeys = Object.keys(bundle.tasks[2].mutations ?? {}).sort();
    const expectedMutationKeys = [...variantIds].sort();
    if (JSON.stringify(mutationKeys) !== JSON.stringify(expectedMutationKeys)) {
      errors.push(`${location}.tasks[2].mutations must cover every variant exactly once.`);
    }
  }

  if (!Array.isArray(bundle.presentationOrders) || bundle.presentationOrders.length < 2) {
    errors.push(`${location}.presentationOrders must counterbalance the variants.`);
  } else {
    const expectedOrder = [...variantIds].sort().join("\0");
    const orders = new Set();
    for (const order of bundle.presentationOrders) {
      if (!Array.isArray(order) || [...order].sort().join("\0") !== expectedOrder) {
        errors.push(`${location}.presentationOrders contains an incomplete order.`);
        continue;
      }
      orders.add(order.join("\0"));
    }
    if (orders.size !== bundle.presentationOrders.length) {
      errors.push(`${location}.presentationOrders contains a duplicate order.`);
    }
  }

  const participantLabels = bundle.blinding?.participantLabels ?? {};
  if (
    JSON.stringify(Object.keys(participantLabels).sort()) !==
    JSON.stringify([...variantIds].sort())
  ) {
    errors.push(`${location}.blinding.participantLabels must cover every variant.`);
  } else {
    const labels = Object.values(participantLabels);
    if (
      new Set(labels).size !== labels.length ||
      labels.some((label) => !/^[A-Z]$/.test(label))
    ) {
      errors.push(`${location}.blinding participant labels must be unique uppercase letters.`);
    }
  }
  const hiddenFields = new Set(bundle.blinding?.hide ?? []);
  for (const field of ["id", "role", "path", "changedConstructs"]) {
    if (!hiddenFields.has(field)) {
      errors.push(`${location}.blinding.hide must include ${field}.`);
    }
  }

  const oracleFile = resolveContained(
    bundleDirectory,
    bundle.oracle?.path,
    bundleDirectory,
    `${location}.oracle.path`,
  );
  checkDigest(oracleFile, bundle.oracle?.digest, `${location}.oracle.digest`);

  if (!Array.isArray(bundle.evidence?.current) || !Array.isArray(bundle.evidence?.missing)) {
    errors.push(`${location}.evidence must separate current and missing evidence.`);
  } else {
    for (const evidence of ["tree-sitter-parse", "host-oracle"]) {
      if (!bundle.evidence.current.includes(evidence)) {
        errors.push(`${location}.evidence.current must include ${evidence}.`);
      }
    }
    for (const evidence of ["w-compile", "w-run", "human-study", "model-study"]) {
      if (!bundle.evidence.missing.includes(evidence)) {
        errors.push(`${location}.evidence.missing must include ${evidence}.`);
      }
    }
    const overlap = bundle.evidence.current.filter((item) =>
      bundle.evidence.missing.includes(item),
    );
    if (overlap.length > 0) {
      errors.push(`${location}.evidence cannot be both current and missing: ${overlap.join(", ")}.`);
    }
  }
}

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

process.stdout.write(
  `R1 study bundles: ${bundleFiles.length} ` +
    `${bundleFiles.length === 1 ? "bundle" : "bundles"}, ` +
    `${variantCount} variants, ${taskCount} tasks, ` +
    `${studiedR0CaseIds.size}/${r0CaseIds.size} R0 cases promoted.\n`,
);
