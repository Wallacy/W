import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

export const HUM0_SCHEMA = "w-hum0-human-review-protocol-1";
export const HUM0_SLICES = [
  "diagnostics-w-explain",
  "ownership-borrow-shared-weak",
  "allocator-contextual",
  "execution-forms",
  "task-channel-backpressure",
  "services-generations",
  "package-build-repl",
  "ffi-callback-lease",
];
export const TASK_KINDS = ["explain", "recall", "repair", "change"];
export const INPUT_KINDS = ["primary", "adversarial"];
export const FORBIDDEN_PARTICIPANT_FIELDS = [
  "expected",
  "status",
  "route",
  "role",
  "path",
  "digest",
  "oracle",
];
const FORBIDDEN_PARTICIPANT_WORDS = /\b(expected|status|route|role|path|digest|oracle)\b/i;
const SHA256_PATTERN = /^sha256:[0-9a-f]{64}$/;
const TOP_LEVEL_KEYS = ["$schema", "id", "status", "title", "question", "scope", "claimsHumanResults", "claimsModelResults", "promotionPolicy", "taskKinds", "inputKinds", "slices", "resultContracts", "records", "stopCondition", "metricsPolicy"];
const SLICE_KEYS = ["id", "title", "problemKey", "outcomeKey", "sourceRefs", "oracleRefs", "inputs", "tasks", "hiddenInternalFacts", "explainableFacts", "counterbalance", "blinding"];
const SOURCE_REF_KEYS = ["id", "path", "symbol", "digest", "claim"];
const ORACLE_REF_KEYS = ["id", "path", "digest", "claim"];
const INPUT_KEYS = ["id", "kind", "problemKey", "outcomeKey", "stimulus", "observerOnly", "participantInput"];
const STIMULUS_KEYS = ["sourceRefId", "symbol", "beforeLines", "afterLines", "maxBytes", "derivedStimulusDigest"];
const OBSERVER_ONLY_KEYS = ["baseInputId", "mutation", "expectedRepair"];
const MUTATION_KEYS = ["find", "replace", "occurrences"];
const TASK_KEYS = ["id", "kind", "inputId", "participantInput"];
const INPUT_PARTICIPANT_KEYS = ["scenario", "task"];
const TASK_PARTICIPANT_KEYS = ["instruction"];
const COUNTERBALANCE_KEYS = ["orders", "assignment"];
const BLINDING_KEYS = ["hiddenFields", "hideSourceIdentity", "hideVariantAssignment", "hideStimulusMetadata"];
const RESULT_LIKE_KEYS = new Set(["score", "scores", "preference", "ergonomicWin", "promotion", "result", "results"]);
const HUMAN_CONTRACT_KEYS = ["participantIdHash", "background", "timeMs", "queryCount", "confidence", "oracleCheckedOutcomes", "observerReceiptDigest", "noPiiFields"];
const MODEL_CONTRACT_KEYS = ["provider", "model", "version", "tokenizer", "params", "inputDigest", "observerReceiptDigest", "tokens", "oracleCheckedOutcomes"];
const HUMAN_RECORD_KEYS = ["participantIdHash", "background", "timeMs", "queryCount", "confidence", "oracleCheckedOutcomes", "observerReceiptDigest"];
const MODEL_RECORD_KEYS = ["provider", "model", "version", "tokenizer", "params", "inputDigest", "observerReceiptDigest", "tokens", "oracleCheckedOutcomes"];
const REQUIRED_BLINDING_FIELDS = [...FORBIDDEN_PARTICIPANT_FIELDS, "sourceRefId", "mutation", "expectedRepair"];
const METRICS_POLICY_KEYS = ["readiness", "human", "model", "forbidden"];
const OUTCOME_KEYS = ["semantic", "repair", "change"];
const OUTCOME_VALUES = new Set(["pass", "fail", "inconclusive"]);
const PARAM_KEYS = new Set(["temperature", "topP", "seed", "maxTokens", "reasoningEffort", "serviceTier"]);
const PARTICIPANT_RENDER_KEYS = ["scenario", "task", "instruction", "source", "blindedLabel"];

function assertKeys(value, allowed, location, errors) {
  if (!value || typeof value !== "object" || Array.isArray(value)) return;
  for (const key of Object.keys(value)) {
    if (!allowed.includes(key)) errors.push(`${location}.${key} is an unknown field.`);
  }
}

function occurrencesInBytes(bytes, needle) {
  const target = Buffer.from(needle, "utf8");
  if (target.length === 0) return 0;
  let count = 0;
  let offset = 0;
  while (true) {
    const found = bytes.indexOf(target, offset);
    if (found < 0) return count;
    count += 1;
    offset = found + target.length;
  }
}

function sha256(bytes) {
  return `sha256:${crypto.createHash("sha256").update(bytes).digest("hex")}`;
}

function decodeUtf8(bytes, location, errors) {
  try {
    const text = new TextDecoder("utf-8", { fatal: true }).decode(bytes);
    if (!Buffer.from(text, "utf8").equals(bytes)) errors.push(`${location} is not a stable UTF-8 byte sequence.`);
    return text;
  } catch {
    errors.push(`${location} must be valid UTF-8.`);
    return undefined;
  }
}

export function digestFile(filePath) {
  return sha256(fs.readFileSync(filePath));
}

function canonical(value) {
  if (Array.isArray(value)) return `[${value.map(canonical).join(",")}]`;
  if (value && typeof value === "object") {
    return `{${Object.keys(value).sort().map((key) => `${JSON.stringify(key)}:${canonical(value[key])}`).join(",")}}`;
  }
  return JSON.stringify(value);
}

function participantValues(value, location, errors) {
  if (Array.isArray(value)) {
    value.forEach((item, index) => participantValues(item, `${location}[${index}]`, errors));
    return;
  }
  if (!value || typeof value !== "object") {
    if (typeof value === "string" && FORBIDDEN_PARTICIPANT_WORDS.test(value)) {
      errors.push(`${location} contains a forbidden participant-visible word.`);
    }
    return;
  }
  for (const [key, child] of Object.entries(value)) {
    if (FORBIDDEN_PARTICIPANT_FIELDS.includes(key)) {
      errors.push(`${location}.${key} is forbidden in participant-visible input.`);
    }
    participantValues(child, `${location}.${key}`, errors);
  }
}

function relativeFile(root, relativePath, location, errors) {
  if (typeof relativePath !== "string" || relativePath.trim() === "") {
    errors.push(`${location} must be a non-empty repository-relative path.`);
    return undefined;
  }
  const resolved = path.resolve(root, relativePath);
  const relative = path.relative(root, resolved);
  if (relative === "" || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
    errors.push(`${location} must stay inside the repository.`);
    return undefined;
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    errors.push(`${location} references a missing file.`);
    return undefined;
  }
  return resolved;
}

function validateDigestRef(ref, { root, location, requireSymbol = false }) {
  const errors = [];
  if (!ref || typeof ref !== "object" || Array.isArray(ref)) {
    return [`${location} must be an object.`];
  }
  assertKeys(ref, requireSymbol ? SOURCE_REF_KEYS : ORACLE_REF_KEYS, location, errors);
  if (typeof ref.id !== "string" || ref.id.trim() === "") errors.push(`${location}.id must be a non-empty string.`);
  const file = relativeFile(root, ref.path, `${location}.path`, errors);
  if (!SHA256_PATTERN.test(ref.digest ?? "")) {
    errors.push(`${location}.digest must use a lowercase sha256 digest.`);
  } else if (file && digestFile(file) !== ref.digest) {
    errors.push(`${location}.digest is stale; expected ${digestFile(file)}.`);
  }
  if (requireSymbol) {
    if (typeof ref.symbol !== "string" || ref.symbol.trim() === "") {
      errors.push(`${location}.symbol must be a non-empty string.`);
    } else if (file) {
      const bytes = fs.readFileSync(file);
      const count = occurrencesInBytes(bytes, ref.symbol);
      if (count === 0) errors.push(`${location}.symbol is absent from the source.`);
      if (count !== 1) errors.push(`${location}.symbol must occur exactly once; found ${count}.`);
    }
  }
  if (typeof ref.claim !== "string" || ref.claim.trim() === "") {
    errors.push(`${location}.claim must be a non-empty string.`);
  }
  return errors;
}

function permutation(values, expected) {
  return Array.isArray(values) && values.length === expected.length &&
    [...values].sort().join("\0") === [...expected].sort().join("\0");
}

function sourceRefForInput(slice, input) {
  return (slice.sourceRefs ?? []).find((ref) => ref.id === input.stimulus?.sourceRefId);
}

export function deriveInputStimulus(slice, input, { root }) {
  const errors = [];
  const location = `slices.${slice.id}.inputs.${input.id}`;
  assertKeys(input, INPUT_KEYS, location, errors);
  const stimulus = input.stimulus;
  if (!stimulus || typeof stimulus !== "object" || Array.isArray(stimulus)) {
    return { errors: [`${location}.stimulus must be an object.`], bytes: Buffer.alloc(0), digest: undefined, participantStimulus: "" };
  }
  assertKeys(stimulus, STIMULUS_KEYS, `${location}.stimulus`, errors);
  const sourceRef = sourceRefForInput(slice, input);
  if (!sourceRef) errors.push(`${location}.stimulus.sourceRefId must reference a slice sourceRef.`);
  if (sourceRef && stimulus.symbol !== sourceRef.symbol) errors.push(`${location}.stimulus.symbol must match its sourceRef.`);
  if (!Number.isInteger(stimulus.beforeLines) || stimulus.beforeLines < 0) errors.push(`${location}.stimulus.beforeLines must be a non-negative integer.`);
  if (!Number.isInteger(stimulus.afterLines) || stimulus.afterLines < 0) errors.push(`${location}.stimulus.afterLines must be a non-negative integer.`);
  if (!Number.isInteger(stimulus.maxBytes) || stimulus.maxBytes <= 0) errors.push(`${location}.stimulus.maxBytes must be a positive integer.`);
  if (!SHA256_PATTERN.test(stimulus.derivedStimulusDigest ?? "")) errors.push(`${location}.stimulus.derivedStimulusDigest must be a sha256 digest.`);
  const file = sourceRef && relativeFile(root, sourceRef.path, `${location}.sourceRef.path`, errors);
  if (!file || !sourceRef || !Number.isInteger(stimulus.beforeLines) || !Number.isInteger(stimulus.afterLines) || !Number.isInteger(stimulus.maxBytes)) {
    return { errors, bytes: Buffer.alloc(0), digest: undefined, participantStimulus: "" };
  }
  const sourceBytes = fs.readFileSync(file);
  decodeUtf8(sourceBytes, `${location}.sourceRef`, errors);
  const symbolBytes = Buffer.from(stimulus.symbol, "utf8");
  if (occurrencesInBytes(sourceBytes, stimulus.symbol) !== 1) {
    errors.push(`${location}.stimulus.symbol must occur exactly once in the source.`);
    return { errors, bytes: Buffer.alloc(0), digest: undefined, participantStimulus: "" };
  }
  const symbolOffset = sourceBytes.indexOf(symbolBytes);
  const lineStarts = [0];
  for (let index = 0; index < sourceBytes.length; index += 1) {
    if (sourceBytes[index] === 0x0a) lineStarts.push(index + 1);
  }
  const symbolLine = Math.max(0, lineStarts.findIndex((start, index) =>
    symbolOffset >= start && (index + 1 === lineStarts.length || symbolOffset < lineStarts[index + 1])));
  const firstLine = Math.max(0, symbolLine - stimulus.beforeLines);
  const endLine = Math.min(lineStarts.length, symbolLine + stimulus.afterLines + 1);
  const start = lineStarts[firstLine];
  const end = endLine < lineStarts.length ? lineStarts[endLine] : sourceBytes.length;
  const primaryBytes = sourceBytes.subarray(start, end);
  if (primaryBytes.length > stimulus.maxBytes) errors.push(`${location}.stimulus window exceeds maxBytes.`);
  const primaryText = decodeUtf8(primaryBytes, `${location}.stimulus window`, errors) ?? "";
  if (input.kind === "primary") {
    if (input.observerOnly !== undefined) errors.push(`${location}.observerOnly is not allowed on a primary input.`);
    const digest = sha256(primaryBytes);
    if (digest !== stimulus.derivedStimulusDigest) errors.push(`${location}.stimulus.derivedStimulusDigest is stale; expected ${digest}.`);
    return { errors, bytes: primaryBytes, digest, lineCount: endLine - firstLine, startByte: start, endByte: end, participantStimulus: primaryText };
  }
  if (input.kind !== "adversarial") errors.push(`${location}.kind must be primary or adversarial.`);
  const observerOnly = input.observerOnly;
  if (!observerOnly || typeof observerOnly !== "object" || Array.isArray(observerOnly)) {
    errors.push(`${location}.observerOnly is required for an adversarial input.`);
    return { errors, bytes: primaryBytes, digest: sha256(primaryBytes), lineCount: endLine - firstLine, startByte: start, endByte: end, participantStimulus: primaryText };
  }
  assertKeys(observerOnly, OBSERVER_ONLY_KEYS, `${location}.observerOnly`, errors);
  if (typeof observerOnly.baseInputId !== "string") errors.push(`${location}.observerOnly.baseInputId is required.`);
  const mutation = observerOnly.mutation;
  if (!mutation || typeof mutation !== "object" || Array.isArray(mutation)) {
    errors.push(`${location}.observerOnly.mutation is required.`);
    return { errors, bytes: primaryBytes, digest: sha256(primaryBytes), lineCount: endLine - firstLine, startByte: start, endByte: end, participantStimulus: primaryText };
  }
  assertKeys(mutation, MUTATION_KEYS, `${location}.observerOnly.mutation`, errors);
  if (typeof mutation.find !== "string" || mutation.find.length === 0) errors.push(`${location}.observerOnly.mutation.find must be non-empty.`);
  if (typeof mutation.replace !== "string") errors.push(`${location}.observerOnly.mutation.replace must be a string.`);
  if (mutation.occurrences !== 1) errors.push(`${location}.observerOnly.mutation.occurrences must be exactly 1.`);
  const findCount = typeof mutation.find === "string" ? occurrencesInBytes(primaryBytes, mutation.find) : 0;
  if (findCount !== 1) errors.push(`${location}.observerOnly.mutation.find must occur exactly once in the primary window; found ${findCount}.`);
  if (typeof observerOnly.expectedRepair !== "string" || observerOnly.expectedRepair.trim() === "") errors.push(`${location}.observerOnly.expectedRepair must be non-empty.`);
  const mutatedBytes = findCount === 1 && typeof mutation.replace === "string"
    ? Buffer.from(primaryText.replace(mutation.find, mutation.replace), "utf8")
    : primaryBytes;
  decodeUtf8(mutatedBytes, `${location}.adversarial stimulus`, errors);
  const digest = sha256(mutatedBytes);
  if (digest !== stimulus.derivedStimulusDigest) errors.push(`${location}.stimulus.derivedStimulusDigest is stale; expected ${digest}.`);
  return { errors, bytes: mutatedBytes, digest, lineCount: endLine - firstLine, startByte: start, endByte: end, participantStimulus: mutatedBytes.toString("utf8") };
}

export function deriveStimuli(protocol, { root }) {
  const result = [];
  const errors = [];
  for (const slice of protocol.slices ?? []) {
    const sliceInputs = [];
    for (const input of slice.inputs ?? []) {
      const derived = deriveInputStimulus(slice, input, { root });
      errors.push(...derived.errors);
      sliceInputs.push({ inputId: input.id, kind: input.kind, digest: derived.digest, byteLength: derived.bytes.length, lineCount: derived.lineCount ?? 0, startByte: derived.startByte, endByte: derived.endByte, bytes: derived.bytes, participantStimulus: derived.participantStimulus });
    }
    result.push({ sliceId: slice.id, inputs: sliceInputs });
  }
  return { errors, slices: result };
}

export function renderParticipantPrompt(slice, input, task, {
  root = path.resolve(process.cwd()),
  blindedLabel = "blinded-restaurant-task",
} = {}) {
  if (!slice || !input || !task) throw new Error("slice, input, and task are required for participant rendering.");
  const derived = deriveInputStimulus(slice, input, { root });
  const errors = [...derived.errors];
  if (!input.participantInput || typeof input.participantInput !== "object" || Array.isArray(input.participantInput)) {
    errors.push("participant input must be an object.");
  }
  if (!task.participantInput || typeof task.participantInput !== "object" || Array.isArray(task.participantInput)) {
    errors.push("task participant input must be an object.");
  }
  if (typeof blindedLabel !== "string" || blindedLabel.trim() === "") errors.push("blindedLabel must be a non-empty string.");
  const rendered = {
    scenario: input.participantInput?.scenario,
    task: input.participantInput?.task,
    instruction: task.participantInput?.instruction,
    source: derived.participantStimulus,
    blindedLabel,
  };
  if (JSON.stringify(Object.keys(rendered)) !== JSON.stringify(PARTICIPANT_RENDER_KEYS)) {
    errors.push("participant renderer produced an unexpected field.");
  }
  participantValues(rendered, "participant", errors);
  if (errors.length > 0) throw new Error(errors.join("\n"));
  return rendered;
}

function validateResultContracts(protocol, errors) {
  const contracts = protocol.resultContracts;
  if (!contracts || typeof contracts !== "object") {
    errors.push("resultContracts must be present.");
    return;
  }
  assertKeys(contracts, ["human", "model"], "resultContracts", errors);
  const human = contracts.human;
  const model = contracts.model;
  if (!human || !model) {
    errors.push("resultContracts.human and resultContracts.model are required.");
    return;
  }
  assertKeys(human, HUMAN_CONTRACT_KEYS, "resultContracts.human", errors);
  assertKeys(model, MODEL_CONTRACT_KEYS, "resultContracts.model", errors);
  for (const field of HUMAN_CONTRACT_KEYS) if (!(field in human)) errors.push(`resultContracts.human.${field} is required.`);
  if (human.participantIdHash !== "sha256") errors.push("resultContracts.human.participantIdHash must require sha256.");
  if (!Array.isArray(human.background) || human.background.length === 0 || human.background.some((item) => !["C", "Rust", "Python", "W"].includes(item))) {
    errors.push("resultContracts.human.background must be a non-empty C/Rust/Python/W subset.");
  }
  if (human.timeMs !== "nonnegative-integer" || human.queryCount !== "nonnegative-integer") errors.push("human timeMs and queryCount must be nonnegative integers.");
  if (human.confidence !== "required-integer-1-to-5") errors.push("human confidence must be required 1..5.");
  if (human.observerReceiptDigest !== "sha256") errors.push("human observerReceiptDigest must require sha256.");
  if (!Array.isArray(human.noPiiFields) || !human.noPiiFields.includes("name") || !human.noPiiFields.includes("email")) errors.push("human noPiiFields must exclude direct identifiers.");
  for (const field of MODEL_CONTRACT_KEYS) if (!(field in model)) errors.push(`resultContracts.model.${field} is required.`);
  for (const field of ["provider", "model", "version", "tokenizer"]) if (model[field] !== "nonempty-string") errors.push(`model ${field} must require a nonempty string.`);
  if (model.params !== "closed-json-object") errors.push("model params must be a closed JSON object.");
  if (model.inputDigest !== "sha256" || model.observerReceiptDigest !== "sha256") errors.push("model digests must require sha256.");
  if (model.tokens !== "{input,output,total}:nonnegative-integer-sum") errors.push("model tokens must require input/output/total with a sum.");
  const contractOutcome = { semantic: "pass|fail|inconclusive", repair: "pass|fail|inconclusive", change: "pass|fail|inconclusive" };
  for (const [name, contract] of [["human", human], ["model", model]]) {
    if (JSON.stringify(contract.oracleCheckedOutcomes) !== JSON.stringify(contractOutcome)) errors.push(`resultContracts.${name}.oracleCheckedOutcomes must be exact semantic/repair/change enum.`);
  }
}

export function validateProtocol(protocol, { root }) {
  const errors = [];
  if (!protocol || typeof protocol !== "object" || Array.isArray(protocol)) {
    return ["protocol must be an object."];
  }
  assertKeys(protocol, TOP_LEVEL_KEYS, "protocol", errors);
  if (protocol.$schema !== HUM0_SCHEMA) errors.push(`protocol.$schema must be ${HUM0_SCHEMA}.`);
  if (protocol.id !== "HUM0") errors.push("protocol.id must be HUM0.");
  if (!["protocol-ready", "protocol-staged"].includes(protocol.status)) errors.push("protocol.status must be protocol-ready or protocol-staged.");
  if (protocol.claimsHumanResults !== false || protocol.claimsModelResults !== false) {
    errors.push("protocol must explicitly claim no human or model results.");
  }
  if (protocol.promotionPolicy !== "no-automatic-promotion") errors.push("promotionPolicy must disable automatic promotion.");
  if (!protocol.metricsPolicy || typeof protocol.metricsPolicy !== "object" || Array.isArray(protocol.metricsPolicy)) errors.push("metricsPolicy must be an object.");
  else assertKeys(protocol.metricsPolicy, METRICS_POLICY_KEYS, "metricsPolicy", errors);

  if (!Array.isArray(protocol.slices) || protocol.slices.length !== HUM0_SLICES.length) {
    errors.push(`protocol.slices must contain exactly ${HUM0_SLICES.length} slices.`);
  }
  const sliceIds = new Set();
  const sourceKeys = new Set();
  const taskIds = new Set();
  for (const [index, slice] of (protocol.slices ?? []).entries()) {
    const location = `slices[${index}]`;
    if (!slice || typeof slice !== "object" || Array.isArray(slice)) {
      errors.push(`${location} must be an object.`);
      continue;
    }
    assertKeys(slice, SLICE_KEYS, location, errors);
    if (slice.id !== HUM0_SLICES[index]) errors.push(`${location}.id must be ${HUM0_SLICES[index] ?? "absent"}.`);
    if (sliceIds.has(slice.id)) errors.push(`${location}.id is duplicated.`);
    sliceIds.add(slice.id);
    for (const field of ["title", "problemKey", "outcomeKey"]) {
      if (typeof slice[field] !== "string" || slice[field].trim() === "") errors.push(`${location}.${field} must be non-empty.`);
    }

    const sourceIds = new Set();
    if (!Array.isArray(slice.sourceRefs) || slice.sourceRefs.length === 0) {
      errors.push(`${location}.sourceRefs must be non-empty.`);
    } else {
      for (const [refIndex, ref] of slice.sourceRefs.entries()) {
        const refLocation = `${location}.sourceRefs[${refIndex}]`;
        errors.push(...validateDigestRef(ref, { root, location: refLocation, requireSymbol: true }));
        if (ref && sourceIds.has(ref.id)) errors.push(`${refLocation}.id is duplicated.`);
        if (ref) sourceIds.add(ref.id);
        const key = ref && `${ref.path}\0${ref.symbol}`;
        if (key && sourceKeys.has(key)) errors.push(`${refLocation} duplicates a source path/symbol.`);
        if (key) sourceKeys.add(key);
      }
    }
    const oracleIds = new Set();
    if (!Array.isArray(slice.oracleRefs) || slice.oracleRefs.length === 0) {
      errors.push(`${location}.oracleRefs must be non-empty.`);
    } else {
      for (const [refIndex, ref] of slice.oracleRefs.entries()) {
        errors.push(...validateDigestRef(ref, { root, location: `${location}.oracleRefs[${refIndex}]` }));
        if (ref && oracleIds.has(ref.id)) errors.push(`${location}.oracleRefs[${refIndex}].id is duplicated.`);
        if (ref) oracleIds.add(ref.id);
      }
    }

    if (!Array.isArray(slice.inputs) || slice.inputs.length !== INPUT_KINDS.length) {
      errors.push(`${location}.inputs must contain primary and adversarial.`);
    }
    const inputIds = new Set();
    const inputKeys = new Set();
    for (const [inputIndex, input] of (slice.inputs ?? []).entries()) {
      const inputLocation = `${location}.inputs[${inputIndex}]`;
      if (!input || typeof input !== "object" || Array.isArray(input)) {
        errors.push(`${inputLocation} must be an object.`);
        continue;
      }
      assertKeys(input, INPUT_KEYS, inputLocation, errors);
      if (typeof input.id !== "string" || input.id.trim() === "") errors.push(`${inputLocation}.id must be a non-empty string.`);
      if (!INPUT_KINDS.includes(input.kind)) errors.push(`${inputLocation}.kind must be primary or adversarial.`);
      if (inputIds.has(input.id)) errors.push(`${inputLocation}.id is duplicated.`);
      inputIds.add(input.id);
      if (input.problemKey !== slice.problemKey || input.outcomeKey !== slice.outcomeKey) {
        errors.push(`${inputLocation} must preserve the slice problem and outcome.`);
      }
      if (!input.participantInput || typeof input.participantInput !== "object") {
        errors.push(`${inputLocation}.participantInput must be an object.`);
      } else {
        assertKeys(input.participantInput, INPUT_PARTICIPANT_KEYS, `${inputLocation}.participantInput`, errors);
        participantValues(input.participantInput, `${inputLocation}.participantInput`, errors);
        inputKeys.add(canonical(input.participantInput));
      }
      const derived = deriveInputStimulus(slice, input, { root });
      errors.push(...derived.errors);
    }
    if (inputIds.size !== INPUT_KINDS.length || ![...inputIds].every(Boolean)) errors.push(`${location}.inputs must have two distinct ids.`);
    if (inputKeys.size !== INPUT_KINDS.length) errors.push(`${location}.inputs must keep primary and adversarial prompts distinct.`);
    const primaryInput = (slice.inputs ?? []).find((input) => input.kind === "primary");
    for (const input of slice.inputs ?? []) {
      if (input?.kind !== "adversarial" || !input.observerOnly) continue;
      if (input.observerOnly.baseInputId !== primaryInput?.id) errors.push(`${location}.inputs.${input.id}.observerOnly.baseInputId must reference the slice primary input.`);
      const fields = ["sourceRefId", "symbol", "beforeLines", "afterLines", "maxBytes"];
      if (primaryInput && fields.some((field) => input.stimulus?.[field] !== primaryInput.stimulus?.[field])) {
        errors.push(`${location}.inputs.${input.id}.stimulus must use the same primary window.`);
      }
    }

    if (!Array.isArray(slice.tasks) || slice.tasks.length !== TASK_KINDS.length) {
      errors.push(`${location}.tasks must contain exactly explain, recall, repair, and change.`);
    }
    const kinds = [];
    for (const [taskIndex, task] of (slice.tasks ?? []).entries()) {
      const taskLocation = `${location}.tasks[${taskIndex}]`;
      if (!task || typeof task !== "object" || Array.isArray(task)) {
        errors.push(`${taskLocation} must be an object.`);
        continue;
      }
      assertKeys(task, TASK_KEYS, taskLocation, errors);
      if (typeof task.id !== "string" || task.id.trim() === "") errors.push(`${taskLocation}.id must be a non-empty string.`);
      kinds.push(task.kind);
      if (!TASK_KINDS.includes(task.kind)) errors.push(`${taskLocation}.kind is not a HUM0 task kind.`);
      if (taskIds.has(task.id)) errors.push(`${taskLocation}.id is duplicated.`);
      taskIds.add(task.id);
      if (!inputIds.has(task.inputId)) errors.push(`${taskLocation}.inputId must reference a slice input.`);
      const taskInput = (slice.inputs ?? []).find((input) => input.id === task.inputId);
      if (taskInput && ((task.kind === "repair" || task.kind === "change") ? taskInput.kind !== "adversarial" : taskInput.kind !== "primary")) {
        errors.push(`${taskLocation} must use primary for explain/recall and adversarial for repair/change.`);
      }
      if (!task.participantInput || typeof task.participantInput !== "object") {
        errors.push(`${taskLocation}.participantInput must be an object.`);
      } else {
        assertKeys(task.participantInput, TASK_PARTICIPANT_KEYS, `${taskLocation}.participantInput`, errors);
        participantValues(task.participantInput, `${taskLocation}.participantInput`, errors);
      }
    }
    if (!permutation(kinds, TASK_KINDS)) errors.push(`${location}.tasks must contain each task kind exactly once.`);

    if (!Array.isArray(slice.hiddenInternalFacts) || slice.hiddenInternalFacts.length === 0) errors.push(`${location}.hiddenInternalFacts must be non-empty.`);
    if (!Array.isArray(slice.explainableFacts) || slice.explainableFacts.length === 0) errors.push(`${location}.explainableFacts must be non-empty.`);
    if (!slice.counterbalance || typeof slice.counterbalance !== "object" || Array.isArray(slice.counterbalance)) {
      errors.push(`${location}.counterbalance must be an object.`);
    } else {
      assertKeys(slice.counterbalance, COUNTERBALANCE_KEYS, `${location}.counterbalance`, errors);
    }
    if (!slice.counterbalance || !Array.isArray(slice.counterbalance.orders) || slice.counterbalance.orders.length !== 2) {
      errors.push(`${location}.counterbalance.orders must contain two orders.`);
    } else if (slice.counterbalance.orders.some((order) => !permutation(order, TASK_KINDS))) {
      errors.push(`${location}.counterbalance.orders must be task permutations.`);
    }
    if (!slice.blinding || typeof slice.blinding !== "object" || Array.isArray(slice.blinding)) {
      errors.push(`${location}.blinding must be an object.`);
    } else {
      assertKeys(slice.blinding, BLINDING_KEYS, `${location}.blinding`, errors);
    }
    if (!slice.blinding || !Array.isArray(slice.blinding.hiddenFields) ||
      !REQUIRED_BLINDING_FIELDS.every((field) => slice.blinding.hiddenFields.includes(field))) {
      errors.push(`${location}.blinding.hiddenFields must hide all forbidden fields.`);
    }
  }
  if (![...sliceIds].every((id, index) => id === HUM0_SLICES[index])) errors.push("slices must use the canonical problem-first order.");
  if (taskIds.size !== HUM0_SLICES.length * TASK_KINDS.length) errors.push("protocol must have exactly 32 distinct tasks.");

  validateResultContracts(protocol, errors);
  if (!protocol.records || !Array.isArray(protocol.records.human) || !Array.isArray(protocol.records.model)) {
    errors.push("records.human and records.model must be empty arrays in a protocol-ready study.");
  } else {
    assertKeys(protocol.records, ["human", "model"], "records", errors);
    if (protocol.records.human.length !== 0 || protocol.records.model.length !== 0) errors.push("protocol-ready HUM0 cannot contain human or model result records.");
    for (const [index, record] of protocol.records.human.entries()) errors.push(...validateHumanRecord(record, { protocol }).map((error) => `records.human[${index}]: ${error}`));
    for (const [index, record] of protocol.records.model.entries()) errors.push(...validateModelRecord(record).map((error) => `records.model[${index}]: ${error}`));
  }
  if (typeof protocol.stopCondition !== "string" || !/first/i.test(protocol.stopCondition) || !/research/i.test(protocol.stopCondition)) {
    errors.push("stopCondition must stop on the first protocol failure and retain Research classification.");
  }
  const statusErrors = errors.filter((error) => error.startsWith("protocol.status"));
  const expectedStatus = errors.length === statusErrors.length ? "protocol-ready" : "protocol-staged";
  if (protocol.status !== expectedStatus && statusErrors.length === 0) errors.push(`protocol.status must be ${expectedStatus} for the current validation state.`);
  return errors;
}

export function deriveReadiness(protocol, errors = []) {
  const slices = Array.isArray(protocol?.slices) ? protocol.slices : [];
  const tasks = slices.flatMap((slice) => Array.isArray(slice.tasks) ? slice.tasks : []);
  return {
    status: errors.length === 0 ? "protocol-ready" : "blocked",
    protocolReady: errors.length === 0,
    sliceCount: slices.length,
    taskCount: tasks.length,
    taskKinds: TASK_KINDS,
    humanRecordCount: Array.isArray(protocol?.records?.human) ? protocol.records.human.length : 0,
    modelRecordCount: Array.isArray(protocol?.records?.model) ? protocol.records.model.length : 0,
    humanResultsClaimed: protocol?.claimsHumanResults === true,
    modelResultsClaimed: protocol?.claimsModelResults === true,
  };
}

export function makeSnapshot(protocol, errors = [], { root = path.resolve(process.cwd()) } = {}) {
  const readiness = deriveReadiness(protocol, errors);
  const stimuli = deriveStimuli(protocol, { root });
  return {
    schema: "w-hum0-human-review-results-1",
    status: "protocol-readiness-output",
    evidence: {
      kind: "protocol-structure-only",
      claimsHumanResults: false,
      claimsModelResults: false,
      participantRecords: "none",
      designDecision: "none",
    },
    readiness,
    slices: (protocol.slices ?? []).map((slice) => ({
      id: slice.id,
      sourceRefCount: Array.isArray(slice.sourceRefs) ? slice.sourceRefs.length : 0,
      oracleRefCount: Array.isArray(slice.oracleRefs) ? slice.oracleRefs.length : 0,
      inputKinds: (slice.inputs ?? []).map((input) => input.kind),
      taskKinds: (slice.tasks ?? []).map((task) => task.kind),
      sameProblemAndOutcome: (slice.inputs ?? []).every((input) =>
        input.problemKey === slice.problemKey && input.outcomeKey === slice.outcomeKey),
      hiddenInternalFactCount: Array.isArray(slice.hiddenInternalFacts) ? slice.hiddenInternalFacts.length : 0,
      explainableFactCount: Array.isArray(slice.explainableFacts) ? slice.explainableFacts.length : 0,
      stimulusDigests: stimuli.slices.find((entry) => entry.sliceId === slice.id)?.inputs.map((input) => input.digest) ?? [],
      stimulusByteLengths: stimuli.slices.find((entry) => entry.sliceId === slice.id)?.inputs.map((input) => input.byteLength) ?? [],
      stimulusLineCounts: stimuli.slices.find((entry) => entry.sliceId === slice.id)?.inputs.map((input) => input.lineCount) ?? [],
    })),
  };
}

function validateOutcomeObject(value, location, errors) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    errors.push(`${location} must be an object.`);
    return;
  }
  assertKeys(value, OUTCOME_KEYS, location, errors);
  if (JSON.stringify(Object.keys(value).sort()) !== JSON.stringify(OUTCOME_KEYS.slice().sort())) {
    errors.push(`${location} must have exactly semantic, repair, and change.`);
  }
  for (const key of OUTCOME_KEYS) if (!OUTCOME_VALUES.has(value[key])) errors.push(`${location}.${key} must be pass, fail, or inconclusive.`);
}

function validateSerializableParams(params, location, errors) {
  if (!params || typeof params !== "object" || Array.isArray(params)) {
    errors.push(`${location} must be a JSON object.`);
    return;
  }
  for (const key of Object.keys(params)) {
    if (!PARAM_KEYS.has(key)) errors.push(`${location}.${key} is not an allowed parameter.`);
    const value = params[key];
    if (value === undefined || typeof value === "function" || typeof value === "symbol" || (typeof value === "number" && !Number.isFinite(value))) {
      errors.push(`${location}.${key} must be JSON-serializable.`);
    }
  }
  try {
    JSON.stringify(params);
  } catch {
    errors.push(`${location} must be JSON-serializable.`);
  }
}

function validateTokens(tokens, location, errors) {
  if (!tokens || typeof tokens !== "object" || Array.isArray(tokens)) {
    errors.push(`${location} must be an object.`);
    return;
  }
  assertKeys(tokens, ["input", "output", "total"], location, errors);
  for (const key of ["input", "output", "total"]) if (!Number.isInteger(tokens[key]) || tokens[key] < 0) errors.push(`${location}.${key} must be a non-negative integer.`);
  if (Number.isInteger(tokens.input) && Number.isInteger(tokens.output) && Number.isInteger(tokens.total) && tokens.total !== tokens.input + tokens.output) errors.push(`${location}.total must equal input + output.`);
}

export function validateHumanRecord(record, { protocol } = {}) {
  const errors = [];
  if (!record || typeof record !== "object" || Array.isArray(record)) return ["human record must be an object."];
  assertKeys(record, HUMAN_RECORD_KEYS, "humanRecord", errors);
  for (const field of HUMAN_RECORD_KEYS) if (!(field in record)) errors.push(`humanRecord.${field} is required.`);
  if (!SHA256_PATTERN.test(record.participantIdHash ?? "")) errors.push("human participantIdHash must be a sha256 digest and cannot be an email.");
  if (!Array.isArray(record.background) || record.background.length === 0 || record.background.some((item) => !["C", "Rust", "Python", "W"].includes(item))) errors.push("human background must be a non-empty allowed subset.");
  if (!Number.isInteger(record.timeMs) || record.timeMs < 0) errors.push("human timeMs must be a non-negative integer.");
  if (!Number.isInteger(record.queryCount) || record.queryCount < 0) errors.push("human queryCount must be a non-negative integer.");
  if (!Number.isInteger(record.confidence) || record.confidence < 1 || record.confidence > 5) errors.push("human confidence is required and must be 1..5.");
  validateOutcomeObject(record.oracleCheckedOutcomes, "humanRecord.oracleCheckedOutcomes", errors);
  if (!SHA256_PATTERN.test(record.observerReceiptDigest ?? "")) errors.push("human observerReceiptDigest must be a sha256 digest.");
  if (protocol?.promotionPolicy !== "no-automatic-promotion") errors.push("human record cannot bypass promotion policy.");
  return errors;
}

export function validateModelRecord(record) {
  const errors = [];
  if (!record || typeof record !== "object" || Array.isArray(record)) return ["model record must be an object."];
  assertKeys(record, MODEL_RECORD_KEYS, "modelRecord", errors);
  for (const field of MODEL_RECORD_KEYS) if (!(field in record)) errors.push(`modelRecord.${field} is required.`);
  for (const field of ["provider", "model", "version", "tokenizer"]) if (typeof record[field] !== "string" || record[field].trim() === "") errors.push(`modelRecord.${field} must be a non-empty string.`);
  validateSerializableParams(record.params, "modelRecord.params", errors);
  if (!SHA256_PATTERN.test(record.inputDigest ?? "")) errors.push("model inputDigest must be a sha256 digest.");
  if (!SHA256_PATTERN.test(record.observerReceiptDigest ?? "")) errors.push("model observerReceiptDigest must be a sha256 digest.");
  validateTokens(record.tokens, "modelRecord.tokens", errors);
  validateOutcomeObject(record.oracleCheckedOutcomes, "modelRecord.oracleCheckedOutcomes", errors);
  return errors;
}
