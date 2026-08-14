import fs from "node:fs";
import path from "node:path";
import { digestFile, parseWFile } from "./syn1-typed-generation-machine.mjs";

const DIGEST = /^sha256:[0-9a-f]{64}$/;
const CURRENT_EVIDENCE = new Set(["source-fixtures", "host-oracle", "official-primary-refs", "tree-sitter-generated-w-parse", "two-target-projections", "target-registry-host"]);
const MISSING_EVIDENCE = new Set(["w-compile", "w-name-type-ownership-effect", "w-constir", "w-run", "provider", "target-compiler-provider", "human-study", "model-study"]);
const ALLOWED_ROLES = new Set(["fixture-current", "research-candidate", "rejected-witness"]);

function required(value, location, errors) {
  if (typeof value !== "string" || value.trim() === "") errors.push(`${location} must be a non-empty string.`);
}

function contained(base, relative, root, location, errors) {
  required(relative, location, errors);
  if (typeof relative !== "string") return undefined;
  const file = path.resolve(base, relative);
  const fromRoot = path.relative(root, file);
  if (fromRoot === "" || fromRoot.startsWith(`..${path.sep}`) || path.isAbsolute(fromRoot)) {
    errors.push(`${location} must resolve inside the repository.`);
    return undefined;
  }
  if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
    errors.push(`${location} references a missing file.`);
    return undefined;
  }
  return file;
}

function digest(file, expected, location, errors) {
  if (!DIGEST.test(expected ?? "")) {
    errors.push(`${location} must use a lowercase sha256 digest.`);
    return;
  }
  const actual = digestFile(file);
  if (actual !== expected) errors.push(`${location} is stale; expected ${actual}.`);
}

function occurrences(source, symbol) {
  return symbol ? source.split(symbol).length - 1 : 0;
}

export function validateSyn1StudyManifest(manifest, { studyDirectory }) {
  const errors = [];
  const root = path.resolve(studyDirectory, "../..", "..");
  const corpus = JSON.parse(fs.readFileSync(path.join(root, "tooling", "syn1-typed-generation-cases.json"), "utf8"));
  if (manifest?.$schema !== "w-syn1-typed-generation-study-3") errors.push("study manifest must use w-syn1-typed-generation-study-3.");
  if (manifest?.status !== "design-oracle-input-syn1") errors.push("study manifest must have status design-oracle-input-syn1.");
  if (manifest?.id !== "SYN1" || manifest?.gate !== "SYN0-R1") errors.push("study manifest must identify SYN1 and SYN0-R1.");
  required(manifest?.title, "study.title", errors);
  if (manifest?.preferredCandidate !== "generated-module-artifact") errors.push("study.preferredCandidate must be generated-module-artifact.");
  if (manifest?.currentRoute !== "composable") errors.push("study.currentRoute must remain composable.");
  if (!Array.isArray(manifest?.variants) || manifest.variants.length !== 7) errors.push("study.variants must contain the four fixture, candidate, and rejected witnesses.");
  const ids = new Set();
  const expectedRoles = new Map([
    ["current-closed-synthesis", "fixture-current"],
    ["current-row-synthesis", "fixture-current"],
    ["current-menu-transform", "fixture-current"],
    ["current-kernel-module", "fixture-current"],
    ["candidate-generated-module", "research-candidate"],
    ["rejected-declaration-recipe", "rejected-witness"],
    ["rejected-dynamic-mutation", "rejected-witness"],
  ]);
  for (const [index, variant] of (manifest?.variants ?? []).entries()) {
    const location = `study.variants[${index}]`;
    required(variant?.id, `${location}.id`, errors);
    if (ids.has(variant?.id)) errors.push(`${location}.id duplicates a witness.`);
    ids.add(variant?.id);
    if (!expectedRoles.has(variant?.id) || variant.role !== expectedRoles.get(variant.id)) errors.push(`${location}.role is not the exact fixture/candidate/rejected disposition.`);
    if (!ALLOWED_ROLES.has(variant?.role)) errors.push(`${location}.role is not allowed.`);
    const expectedLanguage = variant?.role === "fixture-current" ? "w-fixture" : "w-reserved";
    if (variant?.language !== expectedLanguage) errors.push(`${location}.language must be ${expectedLanguage}.`);
    const file = contained(studyDirectory, variant?.path, root, `${location}.path`, errors);
    if (file) {
      digest(file, variant.digest, `${location}.digest`, errors);
      const extension = path.extname(file).toLowerCase();
      if (variant.role === "fixture-current" && extension !== ".w") errors.push(`${location} fixture must be a .w source.`);
      if (variant.role !== "fixture-current" && extension === ".w") errors.push(`${location} reserved candidate/rejected witness must not be a .w source.`);
      if (variant.role === "fixture-current" && !variant.fixture) errors.push(`${location} must mark synthetic W source as fixture.`);
      if (variant.role !== "fixture-current" && variant.fixture !== false) errors.push(`${location} reserved witness must not be a current fixture.`);
      if (variant.role === "fixture-current" && variant.disposition !== "fixture") errors.push(`${location} fixture disposition must be fixture.`);
      if (variant.role === "research-candidate" && variant.disposition !== "research-candidate") errors.push(`${location} candidate disposition must remain Research.`);
      if (variant.role === "rejected-witness" && variant.disposition !== "intentionally-rejected") errors.push(`${location} rejected disposition must be intentionally-rejected.`);
    }
  }
  for (const id of expectedRoles.keys()) if (!ids.has(id)) errors.push(`study.variants is missing ${id}.`);
  const expectedArtifacts = new Set(["generated-menu-conformance.w", "generated-menu-const.w", "generated-menu-dishid-relocated.w", "generated-menu-dishid.w", "generated-menu-duplicate.w", "generated-menu-enum.w", "generated-menu-field-secondary.w", "generated-menu-field.w", "generated-menu-import.w", "generated-menu-multiline.w", "generated-menu-order.w", "generated-menu-private.w", "generated-menu-routing.w", "generated-menu-unicode.w"]);
  if (!Array.isArray(manifest?.generatedArtifacts) || manifest.generatedArtifacts.length !== expectedArtifacts.size) errors.push("study.generatedArtifacts must inventory the fourteen bounded W source-shape witnesses.");
  const artifactPaths = new Set();
  for (const [index, artifact] of (manifest?.generatedArtifacts ?? []).entries()) {
    const location = `study.generatedArtifacts[${index}]`; const file = contained(studyDirectory, artifact?.path, root, `${location}.path`, errors);
    if (!expectedArtifacts.has(artifact?.path) || artifactPaths.has(artifact?.path)) errors.push(`${location}.path must be a unique expected generated W witness.`); artifactPaths.add(artifact?.path);
    if (artifact?.disposition !== "research-candidate" || artifact?.frontendReceipt !== "Research" || artifact?.compilerEvidence !== "missing") errors.push(`${location} must separate Research receipts from missing compiler evidence.`);
    if (file) { digest(file, artifact.digest, `${location}.digest`, errors); const relative = path.relative(root, file).split(path.sep).join("/"); if (!parseWFile(root, relative).ok) errors.push(`${location}.path must parse with the current Tree-sitter W grammar without recovery.`); }
  }
  if (artifactPaths.size !== expectedArtifacts.size || [...expectedArtifacts].some((item) => !artifactPaths.has(item))) errors.push("study.generatedArtifacts must equal the closed generated W fixture inventory.");
  const targetRegistry = manifest?.targetRegistry;
  if (!targetRegistry || targetRegistry.path !== "target-registry.json" || targetRegistry.claim !== "durable host target registry fixture; compiler/provider target evidence missing") errors.push("study.targetRegistry must name the exact durable host registry claim.");
  else { const file = contained(studyDirectory, targetRegistry.path, root, "study.targetRegistry.path", errors); if (file) { digest(file, targetRegistry.digest, "study.targetRegistry.digest", errors); const registry = JSON.parse(fs.readFileSync(file, "utf8")); if (targetRegistry.schema !== registry.$schema || targetRegistry.revision !== registry.revision) errors.push("study.targetRegistry schema/revision is stale."); } }
  if (!Array.isArray(manifest?.sourceRefs) || manifest.sourceRefs.length !== corpus.sourceRefs.length) errors.push("study.sourceRefs must equal the corpus Last Light evidence set.");
  const refs = new Set();
  for (const [index, reference] of (manifest?.sourceRefs ?? []).entries()) {
    const location = `study.sourceRefs[${index}]`;
    const expected = corpus.sourceRefs[index];
    if (!expected || JSON.stringify(reference) !== JSON.stringify(expected)) errors.push(`${location} must equal the normalized corpus source reference.`);
    const file = contained(root, reference?.path, root, `${location}.path`, errors);
    required(reference?.symbol, `${location}.symbol`, errors);
    required(reference?.claim, `${location}.claim`, errors);
    if (file) {
      digest(file, reference.digest, `${location}.digest`, errors);
      const text = fs.readFileSync(file, "utf8");
      const count = occurrences(text, reference.symbol);
      if (count !== 1) errors.push(`${location}.symbol must identify one source occurrence; found ${count}.`);
      const key = `${reference.path}\0${reference.symbol}`;
      if (refs.has(key)) errors.push(`${location} duplicates a source reference.`);
      refs.add(key);
    }
  }
  if (!Array.isArray(manifest?.officialRefs) || JSON.stringify(manifest.officialRefs) !== JSON.stringify(corpus.officialRefs)) errors.push("study.officialRefs must equal the normalized corpus primary references.");
  const officialDocs = new Set();
  for (const [index, reference] of (manifest?.officialRefs ?? []).entries()) {
    const location = `study.officialRefs[${index}]`;
    required(reference?.url, `${location}.url`, errors);
    try {
      const url = new URL(reference.url);
      const normalized = `${url.origin}${url.pathname}`;
      const allowed = ["open-std.org", "doc.rust-lang.org", "docs.python.org"];
      if (url.protocol !== "https:" || !allowed.some((host) => url.hostname === host || url.hostname.endsWith(`.${host}`))) errors.push(`${location}.url must use HTTPS and an allowlisted primary host.`);
      if (officialDocs.has(normalized)) errors.push(`${location}.url duplicates an official document after fragment normalization.`);
      officialDocs.add(normalized);
    } catch { errors.push(`${location}.url must be an absolute HTTPS URL.`); }
    required(reference?.claim, `${location}.claim`, errors);
  }
  if (!Array.isArray(manifest?.evidence?.current) || !Array.isArray(manifest?.evidence?.missing)) errors.push("study.evidence must separate current and missing evidence.");
  else {
    for (const label of manifest.evidence.current) if (!CURRENT_EVIDENCE.has(label)) errors.push(`study.evidence.current has unknown current evidence ${label}.`);
    for (const label of manifest.evidence.missing) if (!MISSING_EVIDENCE.has(label)) errors.push(`study.evidence.missing has unknown missing evidence ${label}.`);
    for (const label of CURRENT_EVIDENCE) if (!manifest.evidence.current.includes(label)) errors.push(`study.evidence.current must include ${label}.`);
    for (const label of MISSING_EVIDENCE) if (!manifest.evidence.missing.includes(label)) errors.push(`study.evidence.missing must include ${label}.`);
    if (manifest.evidence.current.some((label) => manifest.evidence.missing.includes(label))) errors.push("study.evidence cannot overlap current and missing.");
  }
  if (manifest?.oracle?.path === "../../syn1-typed-generation-reference.test.mjs") {
    const file = contained(studyDirectory, manifest.oracle.path, root, "study.oracle.path", errors);
    if (file) digest(file, manifest.oracle.digest, "study.oracle.digest", errors);
  } else errors.push("study.oracle must reference the exact independent SYN1 reference test.");
  return errors;
}
