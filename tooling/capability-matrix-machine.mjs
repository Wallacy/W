import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const AXIS_IDS = new Set(["BRX0", "ATOM0", "GEN0", "SYN0", "CYC0", "IPC0", "SRV0", "DYN0"]);
const REQUIRED_RISKS = ["memory", "effects", "interfaces", "concurrency", "ffi", "tooling", "lastLight"];
const REQUIRED_FOREIGN_LANGUAGES = ["c", "rust", "python"];
const REQUIRED_PRESERVE_PATTERNS = [
  /call-side.*cleanup.*domain/iu,
  /move-first.*ref\/inout\/shared.*memory/iu,
  /typed errors.*fault boundaries/iu,
  /reproducible artifacts.*ambient authority/iu,
  /interface.*ABI identities/iu,
];
const FORBIDDEN_KEYS = /^(?:maturity|popularity|community|featurecopying|feature-copying|stars|downloads|benchmarkscore|implementationstatus)$/iu;
const OFFICIAL_HOSTS = new Set(["www.open-std.org", "open-std.org", "doc.rust-lang.org", "docs.python.org", "pubs.opengroup.org", "llvm.org", "www.llvm.org", "docs.kernel.org"]);

export function deriveRoute(axis) {
  const problemSubcapabilities = (axis.coverage?.subcapabilities ?? []).filter((subcapability) => subcapability.scope === "problem" && subcapability.role === "problem");
  if (problemSubcapabilities.some((subcapability) => subcapability.classification === "intentionally-rejected")) return "intentionally-rejected";
  if (problemSubcapabilities.some((subcapability) => subcapability.classification === "research")) return "research";
  if (problemSubcapabilities.length > 0 && problemSubcapabilities.every((subcapability) => subcapability.classification === "current")) return "current";
  if (problemSubcapabilities.length > 0 && problemSubcapabilities.every((subcapability) => ["current", "composable"].includes(subcapability.classification))) return "composable";
  return "research";
}

function nonEmpty(value) {
  return typeof value === "string" && value.trim() !== "";
}

function hasOwn(value, key) {
  return value !== null && typeof value === "object" && Object.prototype.hasOwnProperty.call(value, key);
}

function walk(value, visit, location = "root") {
  visit(value, location);
  if (Array.isArray(value)) {
    value.forEach((child, index) => walk(child, visit, `${location}[${index}]`));
  } else if (value && typeof value === "object") {
    Object.entries(value).forEach(([key, child]) => walk(child, visit, `${location}.${key}`));
  }
}

function collectSourceRefs(value, base = "root") {
  const refs = [];
  walk(value, (node, location) => {
    if (location.endsWith(".sourceRefs") && Array.isArray(node)) {
      node.forEach((ref, index) => refs.push({ ref, location: `${location}[${index}]` }));
    }
  }, base);
  return refs;
}

function validateLocalRef(ref, location, root, errors, { requireDigest = false, requireUniqueSymbol = false, requireClaim = false, requireSymbol = true } = {}) {
  if (!ref || typeof ref !== "object" || Array.isArray(ref)) {
    errors.push(`${location} must be an object.`);
    return;
  }
  if (typeof ref.path === "string") {
    const resolved = path.resolve(root, ref.path);
    const relative = path.relative(root, resolved);
    if (relative === "" || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
      errors.push(`${location}.path must stay inside the repository.`);
      return;
    }
    if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
      errors.push(`${location}.path references a missing file.`);
      return;
    }
    const source = fs.readFileSync(resolved, "utf8");
    if (requireClaim && !nonEmpty(ref.claim)) errors.push(`${location}.claim must be present for a durable study ref.`);
    if (requireSymbol && !nonEmpty(ref.symbol)) errors.push(`${location}.symbol must be present for a local source ref.`);
    else if (nonEmpty(ref.symbol)) {
      const escaped = String(ref.symbol).replace(/[.*+?^${}()|[\]\\]/gu, "\\$&");
      const count = (source.match(new RegExp(`\\b${escaped}\\b`, "gu")) ?? []).length;
      if (count === 0 && !source.includes(ref.symbol)) errors.push(`${location}.symbol is absent from ${ref.path}.`);
      if (requireUniqueSymbol && count !== 1) errors.push(`${location}.symbol must occur exactly once in ${ref.path}; found ${count}.`);
    }
    if (requireDigest && ref.digest === undefined) errors.push(`${location}.digest is required for a durable local source ref.`);
    if (ref.digest !== undefined) {
      if (!/^sha256:[0-9a-f]{64}$/u.test(ref.digest)) errors.push(`${location}.digest must use a lowercase sha256 digest.`);
      else {
        const actual = `sha256:${crypto.createHash("sha256").update(fs.readFileSync(resolved)).digest("hex")}`;
        if (actual !== ref.digest) errors.push(`${location}.digest is stale; expected ${actual}.`);
      }
    }
    return;
  }
  if (typeof ref.url === "string") {
    let parsed;
    try { parsed = new URL(ref.url); } catch { errors.push(`${location}.url must be an absolute URL.`); return; }
    if (parsed.protocol !== "https:" || !OFFICIAL_HOSTS.has(parsed.hostname)) {
      errors.push(`${location}.url must use an official primary-source host.`);
    }
    return;
  }
  errors.push(`${location} must contain path or url.`);
}

function validateDocs(axis, location, errors, route, docsTargets, root, checkSources) {
  const docs = axis.documentation;
  if (!docs || typeof docs !== "object") { errors.push(`${location}.documentation is required.`); return; }
  for (const field of ["question", "teachingContrast", "whenToUse", "documentationTarget"]) {
    if (!nonEmpty(docs[field])) errors.push(`${location}.documentation.${field} must be non-empty.`);
  }
  if (docs.renderHint !== "paired") errors.push(`${location}.documentation.renderHint must be paired for side-by-side staging.`);
  if (docs.docsStatus !== "queued") errors.push(`${location}.documentation.docsStatus must be queued.`);
  if (JSON.stringify([...new Set(docs.audienceTargets ?? [])].sort()) !== JSON.stringify(["C", "Python", "Rust"].sort())) {
    errors.push(`${location}.documentation.audienceTargets must cover C, Rust, and Python.`);
  }
  if (!/^guides\/problems\/[a-z0-9-]+$/u.test(docs.documentationTarget ?? "") || docs.documentationTarget === "guides/problems/problem-guide") {
    errors.push(`${location}.documentation.documentationTarget must be a specific non-generic problem slug.`);
  } else if (docsTargets.has(docs.documentationTarget)) {
    errors.push(`${location}.documentation.documentationTarget duplicates another axis.`);
  } else {
    docsTargets.add(docs.documentationTarget);
  }
  const foreignSnippets = new Set();
  for (const language of REQUIRED_FOREIGN_LANGUAGES) {
    const example = docs.foreignExamples?.[language];
    const exampleLocation = `${location}.documentation.foreignExamples.${language}`;
    if (!example || typeof example !== "object" || !nonEmpty(example.text)) errors.push(`${exampleLocation} must contain text.`);
    if (example?.exampleKind !== "pseudocode") errors.push(`${exampleLocation}.exampleKind must be pseudocode.`);
    if (!nonEmpty(example?.snippet)) errors.push(`${exampleLocation}.snippet must be non-empty.`);
    else {
      const lineCount = String(example.snippet).split(/\r?\n/u).length;
      if (lineCount > 6) errors.push(`${exampleLocation}.snippet must contain at most 6 lines.`);
      if (example.snippet.trim() === String(example.text ?? "").trim()) errors.push(`${exampleLocation}.snippet must not duplicate prose text.`);
      const normalizedSnippet = example.snippet.trim();
      if (foreignSnippets.has(normalizedSnippet)) errors.push(`${exampleLocation}.snippet duplicates another foreign example.`);
      foreignSnippets.add(normalizedSnippet);
    }
    if (!Array.isArray(example?.sourceRefs) || example.sourceRefs.length === 0) errors.push(`${exampleLocation}.sourceRefs must be non-empty.`);
  }
  const wExample = docs.wExample;
  if (!wExample || typeof wExample !== "object" || !nonEmpty(wExample.text)) errors.push(`${location}.documentation.wExample must contain text.`);
  if (wExample?.exampleKind !== "source-ref") errors.push(`${location}.documentation.wExample.exampleKind must be source-ref.`);
  if (hasOwn(wExample ?? {}, "snippet")) errors.push(`${location}.documentation.wExample must not contain snippet; use its sourceRef.`);
  if (!(wExample?.sourceRefs?.length > 0)) errors.push(`${location}.documentation.wExample must be source-backed for ${route} routes.`);
  if (checkSources) {
    for (const [index, ref] of (wExample?.sourceRefs ?? []).entries()) {
      validateLocalRef(ref, `${location}.documentation.wExample.sourceRefs[${index}]`, root, errors, { requireDigest: true, requireUniqueSymbol: true });
    }
  }
}

export function validateCapabilityMatrix(corpus, { root = process.cwd(), checkSources = true } = {}) {
  const errors = [];
  if (corpus?.$schema !== "w-capability-matrix-1") errors.push("corpus must use schema w-capability-matrix-1.");
  if (corpus?.status !== "design-oracle-input-cap0") errors.push("corpus must use status design-oracle-input-cap0.");
  if (corpus?.id !== "CAP0") errors.push("corpus.id must be CAP0.");
  if (corpus?.method?.compare !== "same-problem") errors.push("method.compare must be same-problem.");
  if (JSON.stringify(corpus?.method?.languages ?? []) !== JSON.stringify(["c", "rust", "python"])) errors.push("method.languages must be c, rust, python.");
  if (!nonEmpty(corpus?.method?.capabilityLevelsSemantics) || !/same-problem/iu.test(corpus?.method?.capabilityLevelsSemantics ?? "")) errors.push("method.capabilityLevelsSemantics must state same-problem-only design levels.");
  if (JSON.stringify(corpus?.method?.levels ?? []) !== JSON.stringify(["languageDesign", "stdDesign", "userDefinableInDesign"])) errors.push("method.levels must separate languageDesign, stdDesign, and userDefinableInDesign.");
  if (!nonEmpty(corpus?.method?.levelRecordShape) || !/status.*rationale.*same problem/iu.test(corpus?.method?.levelRecordShape ?? "")) errors.push("method.levelRecordShape must define status and rationale records for the same problem.");
  if (!Array.isArray(corpus?.axes) || corpus.axes.length !== AXIS_IDS.size) errors.push(`axes must contain exactly ${AXIS_IDS.size} entries.`);
  const ids = new Set();
  const results = [];
  const seenSourceRefs = new Set();
  const seenStudyRefs = new Set();
  const docsTargets = new Set();
  for (const [index, axis] of (corpus.axes ?? []).entries()) {
    const location = `axes[${index}]`;
    if (!axis || typeof axis !== "object") { errors.push(`${location} must be an object.`); continue; }
    if (!AXIS_IDS.has(axis.id)) errors.push(`${location}.id is not a CAP0 axis.`);
    if (ids.has(axis.id)) errors.push(`${location}.id duplicates ${axis.id}.`);
    ids.add(axis.id);
    for (const field of ["title", "sameProblem", "globalSimplification", "foreignMechanisms", "wCurrentComposition", "exactGap", "route", "intentionalTradeoff", "compositionRisks", "evidence", "nextStudyGate", "preserveStrengths", "capabilityLevels", "capabilityLevelsBasis", "lastLight", "coverage"]) {
      if (!hasOwn(axis, field)) errors.push(`${location}.${field} is required.`);
    }
    if (hasOwn(axis, "decisionFacts")) errors.push(`${location}.decisionFacts is forbidden; route facts must be subcapability coverage.`);
    if (!nonEmpty(axis.sameProblem)) errors.push(`${location}.sameProblem must be non-empty.`);
    if (!nonEmpty(axis.globalSimplification)) errors.push(`${location}.globalSimplification must be non-empty.`);
    const coverage = axis.coverage ?? {};
    if (!coverage.currentContract || !["current", "partial", "none"].includes(coverage.currentContract.status) || !nonEmpty(coverage.currentContract.description)) errors.push(`${location}.coverage.currentContract must record a design status and description.`);
    if (hasOwn(coverage, "composition") || hasOwn(coverage, "residualGap") || hasOwn(coverage, "invariantConflict")) errors.push(`${location}.coverage must not duplicate wCurrentComposition or exactGap.`);
    if (!Array.isArray(coverage.subcapabilities)) errors.push(`${location}.coverage.subcapabilities must be an array.`);
    const componentIds = new Set((axis.wCurrentComposition?.components ?? []).map((component) => component?.id).filter(nonEmpty));
    if (componentIds.size !== (axis.wCurrentComposition?.components ?? []).length) errors.push(`${location}.wCurrentComposition.components must have unique non-empty ids.`);
    if (!(coverage.subcapabilities ?? []).some((subcapability) => subcapability?.scope === "problem" && subcapability?.role === "problem")) errors.push(`${location}.coverage.subcapabilities must contain a scope=problem, role=problem entry for sameProblem.`);
    const researchSubcapabilities = [];
    for (const [subIndex, subcapability] of (coverage.subcapabilities ?? []).entries()) {
      const subLocation = `${location}.coverage.subcapabilities[${subIndex}]`;
      if (!nonEmpty(subcapability?.id) || !nonEmpty(subcapability?.role) || subcapability?.scope !== subcapability?.role || !nonEmpty(subcapability?.problem) || !nonEmpty(subcapability?.description) || !["problem", "extension", "foreign-mechanism"].includes(subcapability?.scope) || !["current", "composable", "research", "intentionally-rejected"].includes(subcapability?.classification)) errors.push(`${subLocation} must contain matching scope/role, problem, description, and a valid classification.`);
      if (subcapability?.classification === "research") researchSubcapabilities.push(subcapability);
      if (subcapability?.scope === "problem" && ["current", "composable"].includes(subcapability?.classification)) {
        if (!Array.isArray(subcapability.componentRefs) || subcapability.componentRefs.length === 0 || subcapability.componentRefs.some((componentId) => !componentIds.has(componentId))) errors.push(`${subLocation}.componentRefs must reference real wCurrentComposition component ids.`);
      }
      if (subcapability?.scope === "problem" && subcapability?.classification === "research") {
        if (subcapability.gateId !== axis.nextStudyGate?.gateId) errors.push(`${subLocation}.gateId must equal nextStudyGate.gateId for the blocking problem.`);
        const lastLightSymbols = new Set((axis.lastLight?.sourceRefs ?? []).map((ref) => ref?.symbol).filter(nonEmpty));
        if (!Array.isArray(subcapability.evidenceRefs) || subcapability.evidenceRefs.length === 0 || subcapability.evidenceRefs.some((symbol) => !lastLightSymbols.has(symbol))) errors.push(`${subLocation}.evidenceRefs must reference blocking Last Light source symbols.`);
      }
    }
    const foreign = axis.foreignMechanisms ?? {};
    for (const language of REQUIRED_FOREIGN_LANGUAGES) {
      if (!Array.isArray(foreign[language]) || foreign[language].length === 0) errors.push(`${location}.foreignMechanisms.${language} must contain a problem comparison.`);
      for (const [mechanismIndex, mechanism] of (foreign[language] ?? []).entries()) {
        const mechanismLocation = `${location}.foreignMechanisms.${language}[${mechanismIndex}]`;
        for (const field of ["problem", "mechanism", "operationalTradeoff", "sourceRefs"]) {
          if (!nonEmpty(mechanism?.[field]) && field !== "sourceRefs") errors.push(`${mechanismLocation}.${field} must be non-empty.`);
        }
        if (!Array.isArray(mechanism?.sourceRefs) || mechanism.sourceRefs.length === 0) errors.push(`${mechanismLocation}.sourceRefs must be non-empty.`);
      }
    }
    if (!nonEmpty(axis.wCurrentComposition?.attempt) || !Array.isArray(axis.wCurrentComposition?.components) || axis.wCurrentComposition.components.length === 0) errors.push(`${location}.wCurrentComposition must record an attempt and components.`);
    if (!nonEmpty(axis.exactGap?.kind) || !["none", "research", "composition", "forbidden"].includes(axis.exactGap.kind)) errors.push(`${location}.exactGap.kind is invalid.`);
    if (!nonEmpty(axis.route?.rationale)) errors.push(`${location}.route.rationale must be non-empty.`);
    const derivedRoute = deriveRoute(axis);
    if (axis.route?.classification !== derivedRoute) errors.push(`${location}.route.classification must derive as ${derivedRoute}.`);
    const expectedGapKind = ["research", "intentionally-rejected"].includes(derivedRoute) ? (derivedRoute === "research" ? "research" : "forbidden") : "none";
    if (axis.exactGap?.kind !== expectedGapKind) errors.push(`${location}.exactGap.kind must align with the ${derivedRoute} same-problem route.`);
    const currentContractStatus = axis.coverage?.currentContract?.status;
    if (derivedRoute === "current" && currentContractStatus !== "current") errors.push(`${location}.coverage.currentContract.status must be current for a current route.`);
    if (derivedRoute === "composable" && !["current", "partial"].includes(currentContractStatus)) errors.push(`${location}.coverage.currentContract.status must be current or partial for a composable route.`);
    if (derivedRoute === "research" && !["partial", "none"].includes(currentContractStatus)) errors.push(`${location}.coverage.currentContract.status must be partial or none for a research route.`);
    if (axis.route?.classification === "intentionally-rejected" && !axis.foreignMechanismDisposition) errors.push(`${location} must separate rejected foreign mechanisms from the problem route.`);
    if (axis.foreignMechanismDisposition !== undefined) {
      if (!axis.foreignMechanismDisposition || !["current", "composable", "research", "intentionally-rejected"].includes(axis.foreignMechanismDisposition.classification)) errors.push(`${location}.foreignMechanismDisposition.classification is invalid.`);
      if (!Array.isArray(axis.foreignMechanismDisposition.mechanisms) || axis.foreignMechanismDisposition.mechanisms.length === 0 || !nonEmpty(axis.foreignMechanismDisposition.rationale)) errors.push(`${location}.foreignMechanismDisposition must name mechanisms and rationale.`);
    }
    if (!nonEmpty(axis.intentionalTradeoff?.choice) || !nonEmpty(axis.intentionalTradeoff?.reason)) errors.push(`${location}.intentionalTradeoff must contain choice and reason.`);
    for (const field of ["preserved", "rejected"]) if (!Array.isArray(axis.intentionalTradeoff?.[field]) || axis.intentionalTradeoff[field].length === 0) errors.push(`${location}.intentionalTradeoff.${field} must be non-empty.`);
    for (const risk of REQUIRED_RISKS) {
      if (!nonEmpty(axis.compositionRisks?.[risk]?.risk) || !nonEmpty(axis.compositionRisks?.[risk]?.mitigation)) errors.push(`${location}.compositionRisks.${risk} must contain risk and mitigation.`);
    }
    if (!Array.isArray(axis.preserveStrengths) || axis.preserveStrengths.length < 3) errors.push(`${location}.preserveStrengths must contain at least three invariants.`);
    for (const pattern of REQUIRED_PRESERVE_PATTERNS) if (!(axis.preserveStrengths ?? []).some((invariant) => typeof invariant === "string" && pattern.test(invariant))) errors.push(`${location}.preserveStrengths must retain ${pattern}.`);
    if (axis.capabilityLevelsBasis !== "sameProblem") errors.push(`${location}.capabilityLevelsBasis must be sameProblem.`);
    if (JSON.stringify(Object.keys(axis.capabilityLevels ?? {}).sort()) !== JSON.stringify(["languageDesign", "stdDesign", "userDefinableInDesign"])) errors.push(`${location}.capabilityLevels must separate languageDesign, stdDesign, and userDefinableInDesign.`);
    for (const [level, value] of Object.entries(axis.capabilityLevels ?? {})) {
      if (!value || typeof value !== "object" || !["yes", "partial", "no"].includes(value.status) || !nonEmpty(value.rationale)) errors.push(`${location}.capabilityLevels.${level} must contain a same-problem status and rationale.`);
      if (typeof value?.rationale === "string" && /foreign mechanism|rejected mechanism|extension Research|subcapability/iu.test(value.rationale)) errors.push(`${location}.capabilityLevels.${level}.rationale must not classify an extension or foreign mechanism.`);
    }
    if (!Array.isArray(axis.evidence?.current) || !Array.isArray(axis.evidence?.missing)) errors.push(`${location}.evidence must separate current and missing evidence.`);
    if ((axis.evidence?.current ?? []).some((item) => /implemented|runtime-executed|provider-ready/iu.test(item))) errors.push(`${location}.evidence.current must not claim implementation.`);
    for (const field of ["gateId", "question", "stopCondition"]) if (!nonEmpty(axis.nextStudyGate?.[field])) errors.push(`${location}.nextStudyGate.${field} must be non-empty.`);
    if (!Array.isArray(axis.nextStudyGate?.requiredEvidence) || axis.nextStudyGate.requiredEvidence.length === 0) errors.push(`${location}.nextStudyGate.requiredEvidence must be non-empty.`);
    if (!["design", "evidence"].includes(axis.nextStudyGate?.kind)) errors.push(`${location}.nextStudyGate.kind must be design or evidence.`);
    if (researchSubcapabilities.length > 0) {
      if (axis.nextStudyGate?.kind !== "design") errors.push(`${location}.nextStudyGate.kind must be design for Research subcapabilities.`);
      if (researchSubcapabilities.some((subcapability) => !nonEmpty(subcapability.gateId) || subcapability.gateId !== axis.nextStudyGate?.gateId || subcapability.id !== axis.nextStudyGate?.forSubcapability)) errors.push(`${location}. Research subcapabilities must point to the design gate focused on the exact extension or problem.`);
    } else if (axis.nextStudyGate?.kind === "design") {
      errors.push(`${location}.nextStudyGate.kind design has no Research subcapability focus.`);
    } else if (hasOwn(axis.nextStudyGate ?? {}, "forSubcapability")) {
      errors.push(`${location}.nextStudyGate.forSubcapability is only valid for a design gate.`);
    }
    if (hasOwn(axis.nextStudyGate ?? {}, "studyRefs")) {
      if (!Array.isArray(axis.nextStudyGate.studyRefs) || axis.nextStudyGate.studyRefs.length === 0) errors.push(`${location}.nextStudyGate.studyRefs must be a non-empty array.`);
      for (const [studyIndex, studyRef] of (axis.nextStudyGate.studyRefs ?? []).entries()) {
        const studyLocation = `${location}.nextStudyGate.studyRefs[${studyIndex}]`;
        if (checkSources) validateLocalRef(studyRef, studyLocation, root, errors, { requireDigest: true, requireClaim: true, requireSymbol: false });
        const key = `${studyRef?.path ?? ""}\0${studyRef?.claim ?? ""}`;
        if (seenStudyRefs.has(key)) errors.push(`${studyLocation} duplicates study reference ${key}.`);
        seenStudyRefs.add(key);
      }
    }
    validateDocs(axis, location, errors, derivedRoute, docsTargets, root, checkSources);
    if (checkSources) {
      for (const { ref, location: refLocation } of collectSourceRefs(axis, location)) {
        validateLocalRef(ref, refLocation, root, errors);
        const sourceRefList = refLocation.replace(/\[\d+\]$/u, "");
        const key = `${sourceRefList}\0${ref?.path ? `path:${ref.path}\0${ref.symbol ?? ""}` : `url:${ref?.url ?? ""}\0${ref?.claim ?? ""}`}`;
        if (seenSourceRefs.has(key)) errors.push(`${refLocation} duplicates source reference ${key}.`);
        seenSourceRefs.add(key);
      }
      const canonical = axis.lastLight?.canonicalSource;
      if (!canonical || typeof canonical !== "object") errors.push(`${location}.lastLight.canonicalSource is required.`);
      else {
        validateLocalRef(canonical, `${location}.lastLight.canonicalSource`, root, errors, { requireDigest: true, requireUniqueSymbol: true });
        const teachingRef = axis.documentation?.wExample?.sourceRefs?.[0];
        if (teachingRef?.path !== canonical.path || teachingRef?.digest !== canonical.digest) errors.push(`${location}.documentation.wExample must link the canonical Last Light source path and digest.`);
      }
    }
    walk(axis, (value, valueLocation) => {
      if (valueLocation.split(".").pop()?.match(FORBIDDEN_KEYS)) errors.push(`${valueLocation} uses a forbidden maturity or feature-copying field.`);
      if (typeof value === "string" && /W is implemented|W implementation is complete|compiler is ready/iu.test(value)) errors.push(`${valueLocation} makes an implementation claim.`);
    }, location);
    results.push({ id: axis.id, title: axis.title, derivedRoute, foreignMechanismDisposition: axis.foreignMechanismDisposition?.classification ?? null, sourceRefCount: collectSourceRefs(axis, location).length, evidenceCurrent: axis.evidence?.current ?? [], evidenceMissing: axis.evidence?.missing ?? [] });
  }
  for (const id of AXIS_IDS) if (!ids.has(id)) errors.push(`missing axis ${id}.`);
  return { errors, results };
}

export function loadCapabilityMatrix(file) {
  return JSON.parse(fs.readFileSync(file, "utf8"));
}
