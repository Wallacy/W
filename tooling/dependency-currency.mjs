import fs from "node:fs";
import path from "node:path";

export const DEPENDENCY_CURRENCY_SCHEMA = "w-dependency-currency-1";
export const DEPENDENCY_CLASSES = Object.freeze([
  "managed-dependency",
  "compatibility-floor/recipe",
  "evidence-snapshot",
  "external-evaluation",
]);
export const repositoryRoot = path.resolve(import.meta.dirname, "..");
export const dependencyCurrencyPath = path.join(
  repositoryRoot,
  "tooling",
  "dependency-currency.json",
);
export const dependencyCurrencyDocumentPath = path.join(
  repositoryRoot,
  "DEPENDENCIES.md",
);

const EXPECTED_MANAGED = Object.freeze({
  "bun-runtime": { version: "1.4.0" },
  "tree-sitter-cli": { version: "0.27.0" },
  "actions-checkout": {
    version: "7.0.1",
    tag: "v7.0.1",
    sha: "3d3c42e5aac5ba805825da76410c181273ba90b1",
  },
  "actions-setup-bun": {
    version: "2.2.0",
    tag: "v2.2.0",
    sha: "0c5077e51419868618aeaa5fe8019c62421857d6",
  },
  "vscode-vsce-on-demand": { version: "3.9.2" },
});

const REQUIRED_IDS = Object.freeze(Object.keys(EXPECTED_MANAGED));
const FORBIDDEN_SELECTOR_PATTERN = /(?:latest|nightly|canary|next)|[~^*<>=|]/iu;
const EXACT_VERSION_PATTERN = /^\d+\.\d+\.\d+$/u;
const SHA_PATTERN = /^[0-9a-f]{40}$/u;
const DATE_PATTERN = /^\d{4}-\d{2}-\d{2}$/u;

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function readText(root, relativePath, errors) {
  const absolutePath = path.join(root, relativePath);
  try {
    return fs.readFileSync(absolutePath, "utf8");
  } catch (error) {
    errors.push(`${relativePath} is not readable: ${error.message}`);
    return "";
  }
}

function readJson(root, relativePath, errors) {
  const source = readText(root, relativePath, errors);
  if (!source) return null;
  try {
    return JSON.parse(source);
  } catch (error) {
    errors.push(`${relativePath} is not valid JSON: ${error.message}`);
    return null;
  }
}

function validateUrlList(entry, errors) {
  if (!isObject(entry.source) || !Array.isArray(entry.source.urls) || entry.source.urls.length === 0) {
    errors.push(`${entry.id ?? "<unknown>"}.source.urls must contain at least one URL`);
    return;
  }
  for (const [index, url] of entry.source.urls.entries()) {
    if (typeof url !== "string" || !/^https:\/\//u.test(url)) {
      errors.push(`${entry.id ?? "<unknown>"}.source.urls[${index}] must be an HTTPS URL`);
    }
  }
}

function validateReleaseSource(entry, errors) {
  const tag = entry.selected?.tag ?? entry.latestStable?.tag;
  if (typeof tag !== "string" || tag.length === 0 || !Array.isArray(entry.source?.urls)) return;
  const releaseSuffix = `/releases/tag/${tag}`;
  const tagSuffix = `/tag/${tag}`;
  if (!entry.source.urls.some((url) => typeof url === "string" && (url.endsWith(releaseSuffix) || url.endsWith(tagSuffix)))) {
    errors.push(`${entry.id}.source.urls must include the official release URL for ${tag}`);
  }
}

function validateLocations(entry, root, errors) {
  if (!Array.isArray(entry.locations) || entry.locations.length === 0) {
    errors.push(`${entry.id ?? "<unknown>"}.locations must be non-empty`);
    return;
  }
  for (const relativePath of entry.locations) {
    if (
      typeof relativePath !== "string" ||
      relativePath.length === 0 ||
      path.isAbsolute(relativePath) ||
      relativePath.split(/[\\/]+/u).includes("..")
    ) {
      errors.push(`${entry.id ?? "<unknown>"}.locations contains an invalid repository path`);
      continue;
    }
    if (!fs.existsSync(path.join(root, relativePath))) {
      errors.push(`${entry.id ?? "<unknown>"}.locations references missing path ${relativePath}`);
    }
  }
}

function validateStateShape(entry, field, errors) {
  if (!Object.hasOwn(entry, field)) {
    errors.push(`${entry.id ?? "<unknown>"}.${field} must be present`);
    return null;
  }
  const value = entry[field];
  if (value !== null && !isObject(value)) {
    errors.push(`${entry.id ?? "<unknown>"}.${field} must be an object or null`);
    return null;
  }
  return value;
}

function validateStableRelease(entry, value, label, errors) {
  if (!isObject(value)) {
    errors.push(`${entry.id}.${label} must be an object`);
    return;
  }
  if (typeof value.version !== "string" || !EXACT_VERSION_PATTERN.test(value.version)) {
    errors.push(`${entry.id}.${label}.version must be an exact stable version`);
  }
  if (value.stability !== undefined && value.stability !== "stable") {
    errors.push(`${entry.id}.${label}.stability must be stable`);
  }
  if (typeof value.selector === "string" && FORBIDDEN_SELECTOR_PATTERN.test(value.selector)) {
    errors.push(`${entry.id}.${label}.selector must not be floating or a range`);
  }
}

function validateManaged(entry, errors) {
  const expected = EXPECTED_MANAGED[entry.id];
  if (!expected) {
    errors.push(`${entry.id} is not an approved managed dependency`);
    return;
  }
  if (typeof entry.source?.kind !== "string" || !entry.source.kind.includes("official")) {
    errors.push(`${entry.id}.source.kind must identify an official primary source for a managed currency claim`);
  }
  if (!isObject(entry.latestStable)) {
    errors.push(`${entry.id}.latestStable must identify the latest stable release`);
  } else {
    validateStableRelease(entry, entry.latestStable, "latestStable", errors);
    if (entry.latestStable.version !== expected.version) {
      errors.push(`${entry.id}.latestStable.version must be ${expected.version}`);
    }
  }
  const current = entry.current;
  const selected = entry.selected;
  validateStableRelease(entry, current, "current", errors);
  validateStableRelease(entry, selected, "selected", errors);
  if (current?.version !== expected.version) errors.push(`${entry.id}.current.version must be ${expected.version}`);
  if (selected?.version !== expected.version) errors.push(`${entry.id}.selected.version must be ${expected.version}`);
  if (selected?.stability !== "stable") errors.push(`${entry.id}.selected.stability must be stable`);
  if (selected?.selector === undefined || typeof selected.selector !== "string") {
    errors.push(`${entry.id}.selected.selector must be an exact selector`);
  } else if (FORBIDDEN_SELECTOR_PATTERN.test(selected.selector)) {
    errors.push(`${entry.id}.selected.selector must not be floating or a range`);
  }
  for (const field of ["tag", "sha"]) {
    if (expected[field] !== undefined && selected?.[field] !== expected[field]) {
      errors.push(`${entry.id}.selected.${field} must be ${expected[field]}`);
    }
  }
  if (expected.sha !== undefined && !SHA_PATTERN.test(selected?.sha ?? "")) {
    errors.push(`${entry.id}.selected.sha must be a full 40-character SHA`);
  }
  if (selected?.sha && typeof selected.selector === "string" && !selected.selector.endsWith(`@${selected.sha}`)) {
    errors.push(`${entry.id}.selected.selector must end with the selected SHA`);
  }
  if (entry.successor !== null) {
    errors.push(`${entry.id}.successor must be null until a newer stable release is selected`);
  }
  if (entry.requirements?.exact !== true || entry.requirements?.latestStable !== true) {
    errors.push(`${entry.id}.requirements must require an exact latest stable release`);
  }
}

function validateCompatibility(entry, errors) {
  if (entry.requirements?.mustNotRaiseForCurrency !== true) {
    errors.push(`${entry.id}.requirements.mustNotRaiseForCurrency must be true`);
  }
  if (entry.selected === null) {
    errors.push(`${entry.id}.selected must preserve its floor or recipe`);
  }
  if (entry.latestStable !== null && entry.latestStable !== undefined && !isObject(entry.latestStable)) {
    errors.push(`${entry.id}.latestStable must be an object or null`);
  }
  if (entry.successor !== null) {
    errors.push(`${entry.id}.successor must remain null for a compatibility floor or recipe`);
  }
}

function validateEvidence(entry, errors) {
  if (!isObject(entry.current)) errors.push(`${entry.id}.current must preserve an evidence snapshot`);
  if (entry.requirements?.preserveCurrent !== true) {
    errors.push(`${entry.id}.requirements.preserveCurrent must be true`);
  }
  if (entry.selected !== null && !isObject(entry.selected)) {
    errors.push(`${entry.id}.selected must be an object or null`);
  }
  if (typeof entry.status !== "string" || entry.status === "promoted" || entry.status.startsWith("promoted-")) {
    errors.push(`${entry.id}.status must not claim promotion`);
  }
  if (entry.selected?.promotion === "promoted") {
    errors.push(`${entry.id}.selected.promotion must not be promoted`);
  }
}

function validateExternal(entry, errors) {
  if (entry.status !== "evaluation-only") {
    errors.push(`${entry.id}.status must be evaluation-only`);
  }
  if (entry.requirements?.promotionAllowed !== false) {
    errors.push(`${entry.id}.requirements.promotionAllowed must be false`);
  }
  if (!Array.isArray(entry.limitations) || entry.limitations.length === 0) {
    errors.push(`${entry.id}.limitations must declare external evaluation limits`);
  }
}

function validateSpecialEntries(value, errors) {
  const byId = new Map(value.dependencies.map((entry) => [entry.id, entry]));
  const mlir = byId.get("mlir0-llvm-clang");
  if (!mlir) {
    errors.push("mlir0-llvm-clang entry is required");
  } else {
    if (mlir.current?.version !== "20.1.2") errors.push("MLIR0 current evidence must remain 20.1.2");
    if (mlir.selected?.version !== "23.1.0") errors.push("MLIR0 selected successor must be 23.1.0");
    if (mlir.selected?.tag !== "llvmorg-23.1.0") errors.push("MLIR0 selected tag must be llvmorg-23.1.0");
    if (mlir.selected?.tagObject !== "9b0f9b1eb4a233717c6ed014cff6f8a7c65512de") {
      errors.push("MLIR0 selected tag object is not the verified llvmorg-23.1.0 tag");
    }
    if (mlir.selected?.commit !== "ea7d852a70e8bdfaf601d6626a760f9771b2c4b4") {
      errors.push("MLIR0 selected commit is not the verified peeled llvmorg-23.1.0 commit");
    }
    if (mlir.selected?.promotion !== "blocked") errors.push("MLIR0 successor promotion must remain blocked");
    if (mlir.requirements?.zeroPromotion !== true) errors.push("MLIR0 must declare zero promotion");
    if (mlir.requirements?.currentEvidenceMustRemain !== "20.1.2") {
      errors.push("MLIR0 must declare 20.1.2 as the evidence snapshot");
    }
    if (!Array.isArray(mlir.components) || JSON.stringify(mlir.components) !== JSON.stringify(["MLIR", "LLVM", "Clang", "LLD"])) {
      errors.push("MLIR0 components must include MLIR, LLVM, Clang, and LLD");
    }
  }
  const unicode = byId.get("unicode-ucd");
  if (!unicode) {
    errors.push("unicode-ucd entry is required");
  } else if (unicode.current?.version !== "17.0.0" || unicode.selected?.version !== "17.0.0") {
    errors.push("Unicode UCD current and selected snapshots must remain 17.0.0");
  }
  const portable = byId.get("portable-mlir-toolchain");
  if (!portable) {
    errors.push("portable-mlir-toolchain evaluation entry is required");
  } else if (portable.status !== "evaluation-only" || portable.selected?.promotion !== "not-allowed") {
    errors.push("portable MLIR must remain evaluation-only and not allowed for promotion");
  }
}

function validateRepositoryConsistency(value, root, errors) {
  const byId = new Map(value.dependencies.map((entry) => [entry.id, entry]));
  const bun = byId.get("bun-runtime")?.selected?.version;
  const tree = byId.get("tree-sitter-cli")?.selected;
  const checkout = byId.get("actions-checkout")?.selected;
  const setupBun = byId.get("actions-setup-bun")?.selected;
  const vsce = byId.get("vscode-vsce-on-demand")?.selected?.version;

  const rootPackage = readJson(root, "package.json", errors);
  const portalPackage = readJson(root, "portal/package.json", errors);
  const treePackage = readJson(root, "tooling/tree-sitter-w/package.json", errors);
  const workflow = readText(root, ".github/workflows/validate.yml", errors);
  const treeReadme = readText(root, "tooling/tree-sitter-w/README.md", errors);
  const vscodeReadme = readText(root, "tooling/vscode-w/README.md", errors);
  const treeLock = readText(root, "tooling/tree-sitter-w/bun.lock", errors);
  const bunVersion = readText(root, ".bun-version", errors).trim();
  const repositoryMap = readText(root, "REPOSITORY.md", errors);
  const platformSupport = readJson(root, "tooling/platform-support.json", errors);
  const platformDocument = readText(root, "PLATFORM-SUPPORT.md", errors);

  if (bun && bunVersion !== bun) errors.push(`.bun-version must be ${bun}`);
  if (bun && rootPackage?.packageManager !== `bun@${bun}`) errors.push(`package.json packageManager must be bun@${bun}`);
  if (bun && rootPackage?.engines?.bun !== `>=${bun}`) errors.push(`package.json engines.bun must remain >=${bun}`);
  if (bun && portalPackage?.engines?.bun !== `>=${bun}`) errors.push(`portal/package.json engines.bun must remain >=${bun}`);
  if (bun && !workflow.includes(`bun-version: ${bun}`)) errors.push(`CI must use bun-version: ${bun}`);

  if (tree && treePackage?.devDependencies?.["tree-sitter-cli"] !== tree.version) {
    errors.push(`tooling/tree-sitter-w/package.json must use tree-sitter-cli ${tree.version}`);
  }
  if (tree && !treeLock.includes(`tree-sitter-cli@${tree.version}`)) {
    errors.push(`tooling/tree-sitter-w/bun.lock must contain tree-sitter-cli@${tree.version}`);
  }
  if (tree && tree.integrity && !treeLock.includes(tree.integrity)) {
    errors.push("tooling/tree-sitter-w/bun.lock must contain the selected Tree-sitter integrity");
  }
  if (tree && !treeReadme.includes(`tree-sitter-cli\` ${tree.version}`)) {
    errors.push(`tooling/tree-sitter-w/README.md must document tree-sitter-cli ${tree.version}`);
  }
  if (tree && !repositoryMap.includes(`tree-sitter-cli\` \`${tree.version}`)) {
    errors.push(`REPOSITORY.md must document tree-sitter-cli ${tree.version}`);
  }

  if (checkout && !workflow.includes(`actions/checkout@${checkout.sha} # ${checkout.tag}`)) {
    errors.push("CI checkout must use the selected full SHA and release tag comment");
  }
  if (setupBun && !workflow.includes(`oven-sh/setup-bun@${setupBun.sha} # ${setupBun.tag}`)) {
    errors.push("CI setup-bun must use the selected full SHA and release tag comment");
  }
  if (vsce && !vscodeReadme.includes(`bunx @vscode/vsce@${vsce} package`)) {
    errors.push(`VS Code README must use bunx @vscode/vsce@${vsce}`);
  }

  const mlir = byId.get("mlir0-llvm-clang")?.selected;
  const platformCurrency = platformSupport?.policy?.dependencyCurrency;
  if (mlir && platformCurrency) {
    const expected = {
      successorVersion: mlir.version,
      successorTag: mlir.tag,
      successorTagObject: mlir.tagObject,
      successorCommit: mlir.commit,
    };
    for (const [field, expectedValue] of Object.entries(expected)) {
      if (platformCurrency[field] !== expectedValue) {
        errors.push(`tooling/platform-support.json ${field} must match the selected MLIR successor`);
      }
    }
    if (platformCurrency.promotionBlocker !== "native-build-acquisition-provenance") {
      errors.push("tooling/platform-support.json must retain the native-build-acquisition-provenance promotion blocker");
    }
    if (!platformDocument.includes(mlir.tag) || !platformDocument.includes(mlir.commit)) {
      errors.push("PLATFORM-SUPPORT.md must publish the selected MLIR successor tag and peeled commit");
    }
    const plans = platformSupport.nativeToolchainPlans;
    if (!Array.isArray(plans) || plans.length === 0) {
      errors.push("tooling/platform-support.json must contain native toolchain plans for the selected successor");
    } else {
      for (const plan of plans) {
        if (plan.source?.tag !== mlir.tag || plan.source?.tagObject !== mlir.tagObject || plan.source?.commit !== mlir.commit) {
          errors.push(`native toolchain plan ${plan.id ?? "<unknown>"} must match the selected MLIR successor`);
        }
        if (!Array.isArray(plan.gaps) || !plan.gaps.includes("native-build-acquisition-provenance")) {
          errors.push(`native toolchain plan ${plan.id ?? "<unknown>"} must retain the promotion blocker`);
        }
      }
    }
  }
}

export function validateDependencyCurrency(value, { root = repositoryRoot } = {}) {
  const errors = [];
  if (!isObject(value)) return { errors: ["dependency currency catalog must be an object"] };
  if (value.$schema !== DEPENDENCY_CURRENCY_SCHEMA) errors.push(`catalog.$schema must be ${DEPENDENCY_CURRENCY_SCHEMA}`);
  if (value.version !== 1) errors.push("catalog.version must be 1");
  if (value.status !== "operational-catalog") errors.push("catalog.status must be operational-catalog");
  if (value.observedAt !== "2026-08-31" || !DATE_PATTERN.test(value.observedAt ?? "")) {
    errors.push("catalog.observedAt must be 2026-08-31");
  }
  if (!isObject(value.policy)) {
    errors.push("catalog.policy must be an object");
  } else {
    const managedPolicy = value.policy.managedDependency;
    if (managedPolicy?.selector !== "exact") errors.push("policy.managedDependency.selector must be exact");
    if (managedPolicy?.stability !== "stable") errors.push("policy.managedDependency.stability must be stable");
    if (managedPolicy?.mustMatchLatestStable !== true) errors.push("policy.managedDependency.mustMatchLatestStable must be true");
    if (!Array.isArray(managedPolicy?.forbiddenSelectors) || !managedPolicy.forbiddenSelectors.includes("latest")) {
      errors.push("policy.managedDependency.forbiddenSelectors must reject latest");
    }
    if (value.policy.evidenceSnapshot?.promotion !== "never-implicit") {
      errors.push("policy.evidenceSnapshot.promotion must be never-implicit");
    }
    if (value.policy.environment?.claimExactPin !== false) {
      errors.push("policy.environment.claimExactPin must be false");
    }
  }
  if (!Array.isArray(value.dependencies) || value.dependencies.length === 0) {
    errors.push("catalog.dependencies must be non-empty");
    return { errors };
  }

  const ids = new Set();
  for (const [index, entry] of value.dependencies.entries()) {
    const label = `catalog.dependencies[${index + 1}]`;
    if (!isObject(entry)) {
      errors.push(`${label} must be an object`);
      continue;
    }
    if (typeof entry.id !== "string" || entry.id.length === 0) errors.push(`${label}.id must be non-empty`);
    if (ids.has(entry.id)) errors.push(`catalog contains duplicate dependency id ${entry.id}`);
    ids.add(entry.id);
    if (typeof entry.name !== "string" || entry.name.length === 0) errors.push(`${entry.id ?? label}.name must be non-empty`);
    if (!DEPENDENCY_CLASSES.includes(entry.classification)) {
      errors.push(`${entry.id ?? label}.classification must be one of ${DEPENDENCY_CLASSES.join(", ")}`);
    }
    if (typeof entry.status !== "string" || entry.status.length === 0) errors.push(`${entry.id ?? label}.status must be non-empty`);
    if (typeof entry.scope !== "string" || entry.scope.length === 0) errors.push(`${entry.id ?? label}.scope must be non-empty`);
    validateUrlList(entry, errors);
    validateReleaseSource(entry, errors);
    validateLocations(entry, root, errors);
    validateStateShape(entry, "current", errors);
    validateStateShape(entry, "selected", errors);
    validateStateShape(entry, "successor", errors);
    if (entry.classification === "managed-dependency") validateManaged(entry, errors);
    if (entry.classification === "compatibility-floor/recipe") validateCompatibility(entry, errors);
    if (entry.classification === "evidence-snapshot") validateEvidence(entry, errors);
    if (entry.classification === "external-evaluation") validateExternal(entry, errors);
  }
  for (const id of REQUIRED_IDS) if (!ids.has(id)) errors.push(`catalog is missing managed dependency ${id}`);
  validateSpecialEntries(value, errors);
  validateRepositoryConsistency(value, root, errors);
  return { errors };
}

export function loadDependencyCurrency({ root = repositoryRoot } = {}) {
  const filePath = path.join(root, "tooling", "dependency-currency.json");
  return JSON.parse(fs.readFileSync(filePath, "utf8"));
}

function markdownCell(value) {
  return String(value ?? "—").replaceAll("|", "\\|").replaceAll("\n", "<br>");
}

function markdownTable(headers, rows) {
  const output = [
    `| ${headers.join(" | ")} |`,
    `| ${headers.map(() => "---").join(" | ")} |`,
  ];
  for (const row of rows) output.push(`| ${row.map(markdownCell).join(" | ")} |`);
  return output;
}

function stateSummary(value) {
  if (value === null || value === undefined) return "—";
  const fields = [];
  for (const field of ["version", "tag", "sha", "selector", "constraint", "edition", "command", "release", "llvmTag", "status", "promotion"]) {
    if (value[field] !== undefined && value[field] !== null) fields.push(`${field}: ${value[field]}`);
  }
  return fields.length > 0 ? fields.join("; ") : "recorded";
}

function sourceLinks(entry) {
  return entry.source.urls.map((url, index) => `[source ${index + 1}](${url})`).join("<br>");
}

function renderEnvironment(entry) {
  if (!isObject(entry.environment)) return [];
  const lines = ["", "Environment and build requirements for `" + entry.id + "`:"];
  if (entry.environment.latestStable) lines.push("- Latest stable observed: `" + entry.environment.latestStable + "`.");
  if (Array.isArray(entry.environment.observedHosts)) {
    for (const host of entry.environment.observedHosts) {
      lines.push(`- ${host.host}: ${Object.entries(host).filter(([key]) => key !== "host").map(([key, value]) => `${key}=${value ?? "not installed"}`).join(", ")}.`);
    }
  }
  return lines;
}

function renderGap(entry) {
  if (!isObject(entry.gap)) return [];
  return [
    "",
    "Open finite task for `" + entry.id + "`: " + entry.gap.task,
    `Stop condition: ${entry.gap.stopCondition}`,
  ];
}

export function renderDependencyCurrency(value) {
  const dependencies = Array.isArray(value.dependencies) ? value.dependencies : [];
  const lines = [
    "# Dependency currency",
    "",
    "<!-- Generated by tooling/dependency-currency.mjs. Edit tooling/dependency-currency.json. -->",
    "",
    "Observed: `" + (value.observedAt ?? "—") + "`.",
    "This catalog records tool versions, compatibility floors, evidence snapshots, and external evaluations.",
    "It does not define W language semantics.",
    "",
    "## Policy",
    "",
    "- Managed dependencies use one exact stable selector.",
    "- A managed dependency must match the latest stable release recorded in this catalog.",
    "- Compatibility floors and recipes do not rise only because a newer release exists.",
    "- Evidence snapshots preserve historical inputs and do not imply promotion.",
    "- External evaluations remain non-authoritative and cannot promote W support.",
    "",
  ];
  const sections = [
    ["managed-dependency", "Managed dependencies"],
    ["compatibility-floor/recipe", "Compatibility floors and recipes"],
    ["evidence-snapshot", "Evidence snapshots"],
    ["external-evaluation", "External evaluations"],
  ];
  for (const [classification, title] of sections) {
    const entries = dependencies.filter((entry) => entry.classification === classification);
    lines.push(`## ${title}`, "", ...markdownTable(
      ["ID", "Name", "Status", "Current", "Selected", "Successor", "Sources"],
      entries.map((entry) => [
        `\`${entry.id}\``,
        entry.name,
        entry.status,
        stateSummary(entry.current),
        stateSummary(entry.selected),
        stateSummary(entry.successor),
        sourceLinks(entry),
      ]),
    ));
    for (const entry of entries) {
      lines.push("", `### \`${entry.id}\``, "", `Scope: ${entry.scope}.`, `Locations: ${entry.locations.map((location) => `\`${location}\``).join(", ")}.`);
      if (entry.latestStable) lines.push(`Latest stable: \`${stateSummary(entry.latestStable)}\`.`);
      lines.push(...renderEnvironment(entry), ...renderGap(entry));
      if (Array.isArray(entry.limitations)) lines.push("", `Limits: ${entry.limitations.join("; ")}.`);
    }
    lines.push("");
  }
  lines.push(
    "## Project and schema versions",
    "",
    "Package, extension, manifest, schema, target, and protocol versions remain project metadata.",
    "They are not dependency currency records.",
    "",
    "## Verification",
    "",
    "Run `bun run check:dependency-currency` after changing a catalog entry, a managed selector, or this projection.",
    "The checker verifies repository pins, exact selectors, evidence preservation, and projection freshness.",
    "",
  );
  return `${lines.join("\n").replace(/\n{3,}/gu, "\n\n").replace(/\n+$/u, "")}\n`;
}

export function checkRenderedDependencies(actual, value) {
  const expected = renderDependencyCurrency(value);
  return actual === expected ? [] : ["DEPENDENCIES.md is stale or manually edited; run with --write."];
}

function main() {
  const args = process.argv.slice(2);
  if (args.length !== 1 || !["--write", "--check"].includes(args[0])) {
    process.stderr.write("Usage: bun tooling/dependency-currency.mjs --write|--check\n");
    process.exitCode = 2;
    return;
  }
  const value = loadDependencyCurrency();
  const validation = validateDependencyCurrency(value);
  if (validation.errors.length > 0) {
    for (const error of validation.errors) process.stderr.write(`dependency-currency: ${error}\n`);
    process.exitCode = 1;
    return;
  }
  const expected = renderDependencyCurrency(value);
  if (args[0] === "--write") {
    fs.writeFileSync(dependencyCurrencyDocumentPath, expected, "utf8");
    process.stdout.write("Dependency currency: wrote DEPENDENCIES.md.\n");
    return;
  }
  if (!fs.existsSync(dependencyCurrencyDocumentPath)) {
    process.stderr.write("DEPENDENCIES.md is missing; run with --write.\n");
    process.exitCode = 1;
    return;
  }
  const errors = checkRenderedDependencies(fs.readFileSync(dependencyCurrencyDocumentPath, "utf8"), value);
  if (errors.length > 0) {
    for (const error of errors) process.stderr.write(`dependency-currency: ${error}\n`);
    process.exitCode = 1;
    return;
  }
  process.stdout.write(`Dependency currency: ok (${value.dependencies.length} entries).\n`);
}

if (import.meta.main) main();
