import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";

const ROOT = path.resolve(import.meta.dirname, "..");
const ATLAS = path.join(ROOT, "reference", "syntax-atlas");
const GRAMMAR = path.join(ROOT, "tooling", "tree-sitter-w", "grammar.js");
const MANIFEST = path.join(ATLAS, "atlas-manifest.json");
const SYNTAX_COVERAGE = path.join(ATLAS, "SYNTAX-COVERAGE.md");
const DIGEST = /^sha256:[0-9a-f]{64}$/u;
const RULE_SET_DIGEST = "sha256:b982b531fb03cf3c544d050f622de37e9bf37d093b9bd68874a08027bce77fe6";
const SCHEMA = "w-syntax-atlas-1";

const ROOT_KINDS = new Set(["module", "package", "workspace"]);
const STATUSES = new Set(["current", "research", "rejected"]);
const EVIDENCE = new Set(["tree-sitter-parse-only", "tree-sitter-parse-only-provider-missing", "tree-sitter-parse-only-compiler-runtime-missing"]);

// IDs are the closed gate for the human-facing variant inventory. The
// construction text, witness, status, and evidence remain data in the
// manifest; this list only prevents an accidental omission.
const REQUIRED_VARIANT_IDS = [
  "root-module", "root-package", "root-workspace",
  "import-ordinary", "import-domain", "import-service", "import-wildcard",
  "entry-explicit", "allocator-named", "allocator-anonymous", "allocator-contextual-parameter", "allocator-contextual-call",
  "ownership-ref", "ownership-inout", "ownership-take", "ownership-shared", "ownership-weak", "ownership-view", "ownership-pin", "ownership-atomic",
  "execution-direct", "execution-await", "execution-sync", "execution-async-initializer", "execution-spawn",
  "callable-positional", "callable-required-homonym", "callable-optional-label", "callable-required-external", "callable-default", "callable-rest", "callable-some-fn", "callable-any-fn", "callable-static", "callable-generic",
  "closure-copy", "closure-ref", "closure-take", "closure-weak",
  "property-get", "property-set", "property-modify",
  "pattern-enum", "pattern-struct", "pattern-tuple", "pattern-range", "pattern-wildcard",
  "static-record", "static-list", "channel-send", "channel-receive",
];

const LEXICAL_RULES = new Set([
  "quantity_literal", "unit_suffix_literal", "size_literal", "number_literal", "string_literal",
  "raw_string_literal", "multiline_string_literal", "scalar_literal", "byte_literal", "boolean_literal",
  "identifier", "tuple_index", "behavior_identifier", "comment",
]);

// These rules have source-visible spellings. Keep this guard explicit so a
// future grammar change cannot silently turn a human construct into a waiver.
const VISIBLE_RULES_MUST_NOT_BE_INTERNAL = new Set([
  "declaration_prefix", "function_signature", "parameter", "rest_marker", "non_borrowed_type",
  "type_body", "protocol_body", "enum_body", "behavior_body", "property_accessor_body",
  "get_accessor", "set_accessor", "modify_accessor", "accessor_implementation", "behavior_initializer_parameters", "behavior_parameter_list",
  "behavior_parameter", "enum_case_parameter", "pattern_bound", "struct_pattern_field",
  "shorthand_struct_pattern_field", "labeled_struct_pattern_field", "tuple_element", "labeled_tuple_element",
  "map_entry", "capture_item", "closure_parameter", "closure_parameters", "switch_case", "catch_clause",
]);

// No public named rule is currently an invisible CST helper. If a future rule
// is truly invisible, add it here with a reason and a test witness.
const INTERNAL_RULES = new Set();

const RECOVERY_RULES = new Set(["foreign_body_content"]);

const MANIFEST_RULES = new Set([
  "build_manifest", "package_manifest", "workspace_manifest", "manifest_record",
  "manifest_field", "manifest_value", "manifest_list", "manifest_constructor", "manifest_argument",
]);

const ROOT_RULES = new Set(["source_file", "module_header", "module_contract"]);

const DIRECT_RULES = new Set([
  "module_header", "domain_import_statement", "service_import_statement", "import_statement", "reexport_declaration", "reexport_item", "export_list_declaration",
  "function_declaration", "struct_declaration", "object_declaration", "service_declaration", "protocol_declaration",
  "enum_declaration", "initializer_declaration", "field_declaration", "computed_property_declaration", "property_requirement",
  "enum_case", "type_declaration", "alias_declaration", "dimension_declaration", "unit_declaration", "extension_declaration",
  "behavior_declaration", "behavior_field_declaration", "behavior_initializer", "entry_declaration", "foreign_declaration", "const_declaration", "test_declaration", "type",
  "function_type", "tuple_type", "fixed_array_type", "allocator_statement", "binding_declaration", "return_statement",
  "commit_statement", "throw_statement", "break_statement", "continue_statement", "defer_statement", "guard_statement",
  "if_statement", "labeled_statement", "while_statement", "for_statement", "repeat_statement", "do_statement", "expression_statement",
  "pattern", "range_pattern", "enum_pattern", "struct_pattern", "tuple_pattern", "switch_expression", "assignment_expression",
  "bounded_range_expression", "one_sided_range_expression", "binary_expression", "unary_expression", "optional_try_expression",
  "type_query_expression", "conditional_cast_expression",
  "optional_propagation_expression", "call_expression", "generic_application_expression", "member_expression", "optional_member_expression",
  "index_expression", "closure_expression", "capture_expression", "pipeline_expression", "lock_expression", "transaction_expression",
  "unsafe_expression", "if_expression", "array_literal", "map_literal", "repeat_array_literal", "tuple_expression", "unit_literal",
  "stream_expression", "yield_statement", "borrow_clause",
  "package_manifest", "workspace_manifest",
]);

const MARKER_RULE_OVERRIDES = new Map([
  ["entry_declaration", "entry-declaration"],
  ["allocator_statement", "allocator-and-bindings"],
  ["allocator_builtin_plan", "allocator-and-bindings"],
  ["binding_declaration", "allocator-and-bindings"],
  ["task_contract", "allocator-and-bindings"],
  ["task_expression", "execution-forms"],
  ["optional_binding", "allocator-and-bindings"],
  ["pipeline_expression", "restricted-expressions"],
  ["lock_expression", "restricted-expressions"],
  ["transaction_expression", "restricted-expressions"],
  ["unsafe_expression", "restricted-expressions"],
  ["panic_expression", "restricted-expressions"],
  ["if_expression", "restricted-expressions"],
  ["closure_expression", "restricted-expressions"],
  ["capture_expression", "restricted-expressions"],
  ["stream_capture_list", "stream-and-channel"],
  ["stream_expression", "stream-and-channel"],
  ["yield_statement", "stream-and-channel"],
  ["assignment_expression", "restricted-expressions"],
  ["bounded_range_expression", "restricted-expressions"],
  ["one_sided_range_expression", "restricted-expressions"],
  ["binary_expression", "restricted-expressions"],
  ["type_query_expression", "types-and-contracts"],
  ["conditional_cast_expression", "restricted-expressions"],
  ["unary_expression", "execution-forms"],
  ["optional_try_expression", "execution-forms"],
  ["optional_propagation_expression", "execution-forms"],
  ["call_expression", "execution-forms"],
  ["generic_application_expression", "execution-forms"],
  ["generic_call_arguments", "execution-forms"],
  ["argument_list", "execution-forms"],
  ["argument", "execution-forms"],
  ["argument_expansion", "execution-forms"],
  ["member_expression", "execution-forms"],
  ["optional_member_expression", "execution-forms"],
  ["index_expression", "execution-forms"],
]);

function markerForRule(name) {
  if (MARKER_RULE_OVERRIDES.has(name)) return MARKER_RULE_OVERRIDES.get(name);
  if (name === "package_manifest") return "package-root";
  if (name === "workspace_manifest") return "workspace-root";
  if (name === "build_manifest") return "package-root";
  if (["manifest_record", "manifest_field", "manifest_value", "manifest_list", "manifest_constructor", "manifest_argument"].includes(name)) return "package-root";
  if (ROOT_RULES.has(name)) return "source-roots-imports";
  if (LEXICAL_RULES.has(name)) return "literals-and-collections";
  if (name === "foreign_body" || name.startsWith("foreign_")) return "callables-and-foreign";
  if (["domain_import_statement", "service_import_statement", "named_service_imports", "service_import_item", "service_key_contract", "import_statement", "reexport_declaration", "reexport_item", "wildcard_import", "named_imports", "import_item", "module_path"].includes(name)) return "source-roots-imports";
  if (["function_declaration", "function_signature", "language_tag", "abi_contract", "parameter_list", "generic_parameters", "generic_parameter", "function_type", "function_type_parameter", "rest_marker", "borrow_clause", "borrow_pair", "slot_ref"].includes(name)) return "callables-and-foreign";
  if (["struct_declaration", "object_declaration", "service_declaration", "protocol_declaration", "enum_declaration", "primary_associated_types", "conformance_clause", "associated_type_requirement", "associated_const_requirement", "initializer_declaration", "field_declaration", "computed_property_declaration", "property_requirement", "enum_case", "type_declaration", "alias_declaration", "dimension_declaration", "unit_declaration", "extension_declaration", "behavior_declaration", "behavior_field_declaration", "behavior_initializer", "behavior_accessor", "behavior_initializer_parameters", "deinit_declaration", "const_declaration", "test_declaration", "export_list_declaration", "export_item"].includes(name)) return "data-declarations";
  if (["type", "type_name", "type_arguments", "type_argument", "static_argument_value", "contract_expression_argument", "static_record_literal", "static_array_literal", "fixed_array_type", "tuple_type", "labeled_tuple_type_element", "unit_literal"].includes(name)) return "types-and-contracts";
  if (["declaration_prefix", "type_body", "protocol_body", "enum_body", "behavior_body", "behavior_initializer_parameters", "property_accessor_body", "get_accessor", "set_accessor", "modify_accessor", "accessor_implementation", "behavior_parameter_list", "behavior_parameter", "enum_case_parameter"].includes(name)) return "data-declarations";
  if (["non_borrowed_type"].includes(name)) return "types-and-contracts";
  if (["parameter", "rest_marker", "function_signature"].includes(name)) return "callables-and-foreign";
  if (["array_literal", "map_literal", "repeat_array_literal", "tuple_expression", "parenthesized_expression"].includes(name)) return "literals-and-collections";
  if (["map_entry", "tuple_element", "labeled_tuple_element"].includes(name)) return "literals-and-collections";
  if (["pattern", "range_pattern", "enum_pattern", "enum_payload_pattern", "labeled_pattern_field", "struct_pattern", "tuple_pattern", "rest_pattern", "switch_expression", "pattern_bound", "struct_pattern_field", "shorthand_struct_pattern_field", "labeled_struct_pattern_field", "switch_case"].includes(name)) return "patterns";
  if (["block", "return_statement", "commit_statement", "throw_statement", "break_statement", "continue_statement", "defer_statement", "guard_statement", "if_statement", "labeled_statement", "while_statement", "for_statement", "repeat_statement", "do_statement", "expression_statement", "catch_clause"].includes(name)) return "control-flow";
  if (["closure_parameters", "closure_parameter", "capture_item"].includes(name)) return "restricted-expressions";
  if (name === "contextual_member_expression") return "types-and-contracts";
  return null;
}

function classifyRule(name) {
  if (LEXICAL_RULES.has(name)) return { surface: "lexical", reason: "Lexical token or trivia. The atlas covers it in the values block." };
  if (RECOVERY_RULES.has(name)) return { surface: "recovery", reason: "External scanner node for opaque foreign-body recovery. It is not a standalone W construct." };
  if (INTERNAL_RULES.has(name)) return { surface: "internal", reason: "CST helper composed by a user-facing construct. It has no independent source form." };
  if (ROOT_RULES.has(name) || MANIFEST_RULES.has(name)) return { surface: "root", reason: "Document root or root-owned data grammar.", marker: markerForRule(name) };
  const marker = markerForRule(name);
  if (!marker) return undefined;
  const surface = DIRECT_RULES.has(name) ? "direct" : "composed";
  return { surface, reason: surface === "direct" ? `Direct source construction shown in the ${marker} atlas block.` : `Composed by the ${marker} atlas block.`, marker };
}

function digestBytes(bytes) {
  return `sha256:${crypto.createHash("sha256").update(bytes).digest("hex")}`;
}

function digestFile(file) {
  return digestBytes(fs.readFileSync(file));
}

function readText(file) {
  const bytes = fs.readFileSync(file);
  if (bytes.includes(0xef) && bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) throw new Error(`${file} has a UTF-8 BOM.`);
  const text = bytes.toString("utf8");
  if (text.includes("\r")) throw new Error(`${file} must use LF line endings.`);
  if (!text.endsWith("\n")) throw new Error(`${file} must end with a newline.`);
  return text;
}

function walk(directory) {
  const output = [];
  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    const file = path.join(directory, entry.name);
    if (entry.isDirectory()) output.push(...walk(file));
    else output.push(file);
  }
  return output;
}

function grammarRules() {
  const source = fs.readFileSync(GRAMMAR, "utf8");
  const names = [...source.matchAll(/^\s{4}([A-Za-z_][A-Za-z0-9_]*):\s*\((?:\$|_)\)/gmu)].map((match) => match[1]);
  names.push("foreign_body_content", "_foreign_body_error_sentinel");
  return [...new Set(names)].sort();
}

function parseW(file) {
  const relative = path.relative(ROOT, file).split(path.sep).join("/");
  const executable = process.platform === "win32"
    ? path.join(ROOT, "tooling", "tree-sitter-w", "node_modules", ".bin", "tree-sitter.cmd")
    : path.join(ROOT, "tooling", "tree-sitter-w", "node_modules", ".bin", "tree-sitter");
  // Windows exposes the package launcher as a `.cmd` shim. Direct spawn of
  // that shim returns status null/EINVAL on some Node releases, which makes a
  // valid atlas fixture look like a recovery parse. Use the platform shell
  // only for this repository-owned launcher; POSIX keeps the direct binary.
  const result = spawnSync(executable, ["parse", "--grammar-path", "tooling/tree-sitter-w", "--quiet", "--stat", relative], {
    cwd: ROOT,
    encoding: "utf8",
    shell: process.platform === "win32",
  });
  const output = `${result.stdout ?? ""}\n${result.stderr ?? ""}`;
  return { ok: result.status === 0 && !/\b(?:ERROR|MISSING)\b/u.test(output), output };
}

function parseAtlasFile(file) {
  const text = readText(file);
  const lines = text.split("\n");
  const blocks = [];
  let open;
  for (let index = 0; index < lines.length; index += 1) {
    const line = lines[index];
    const begin = /^\/\/ atlas:begin\s+([a-z0-9-]+)$/u.exec(line);
    const end = /^\/\/ atlas:end\s+([a-z0-9-]+)$/u.exec(line);
    if (begin) {
      if (open) throw new Error(`${file}:${index + 1} nests atlas markers.`);
      open = { id: begin[1], startLine: index };
    } else if (end) {
      if (!open) throw new Error(`${file}:${index + 1} closes no atlas marker.`);
      if (end[1] !== open.id) throw new Error(`${file}:${index + 1} closes ${end[1]} but opens ${open.id}.`);
      const snippetLines = lines.slice(open.startLine + 1, index);
      if (!snippetLines.some((lineValue) => lineValue.trim())) throw new Error(`${file}:${index + 1} has an empty atlas snippet.`);
      const snippet = `${snippetLines.join("\n")}\n`;
      blocks.push({
        id: open.id,
        file: path.relative(ATLAS, file).split(path.sep).join("/"),
        line: open.startLine + 1,
        snippet,
        snippetDigest: digestBytes(Buffer.from(snippet, "utf8")),
      });
      open = undefined;
    }
  }
  if (open) throw new Error(`${file}:${open.startLine + 1} has an unclosed atlas marker.`);
  const parse = parseW(file);
  if (!parse.ok) throw new Error(`${file} does not parse without recovery.\n${parse.output.split("\n").filter((line) => /ERROR|MISSING|failed parses/u.test(line)).slice(0, 8).join("\n")}`);
  return { file, text, blocks };
}

function validateSourceRefs(blocks) {
  const seen = new Set();
  for (const block of blocks) {
    for (const reference of block.sourceRefs) {
      const key = `${reference.path}\0${reference.symbol}`;
      if (seen.has(key)) throw new Error(`duplicate Last Light source reference ${key}`);
      seen.add(key);
      const file = path.resolve(ROOT, reference.path);
      const relative = path.relative(ROOT, file);
      if (!fs.existsSync(file) || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) throw new Error(`missing or escaping source reference ${reference.path}`);
      const source = fs.readFileSync(file, "utf8");
      const occurrences = source.split(reference.symbol).length - 1;
      if (occurrences !== 1) throw new Error(`${reference.path} symbol ${JSON.stringify(reference.symbol)} occurs ${occurrences} times.`);
      reference.digest = digestFile(file);
    }
  }
}

function rootDisposition(files, blocks) {
  const byFile = new Map(files.map((file) => [path.relative(ATLAS, file.file).split(path.sep).join("/"), file]));
  const required = new Map([
    ["language.w", new Set(["module"])],
    ["execution.w", new Set(["module"])],
    ["build.w", new Set(["package", "workspace"])],
  ]);
  for (const [file, expected] of required) {
    const entry = byFile.get(file);
    if (!entry) throw new Error(`root atlas file ${file} is missing.`);
    for (const root of expected) if (!entry.blocks.some((block) => block.root === root)) throw new Error(`${file} must contain root=${root}.`);
    for (const block of entry.blocks) if (!expected.has(block.root)) throw new Error(`${file} has incompatible root=${block.root} on ${block.id}.`);
  }
}

function deriveSnapshot(manifestInput) {
  for (const name of VISIBLE_RULES_MUST_NOT_BE_INTERNAL) {
    if (INTERNAL_RULES.has(name)) throw new Error(`visible public rule ${name} cannot be waived as internal.`);
  }
  const expected = manifestInput ?? (fs.existsSync(MANIFEST) ? JSON.parse(fs.readFileSync(MANIFEST, "utf8")) : undefined);
  const metadata = new Map((expected?.blocks ?? []).map((block) => [block.id, block]));
  const files = walk(ATLAS).filter((file) => file.endsWith(".w")).sort().map(parseAtlasFile);
  const blocks = files.flatMap((file) => file.blocks);
  const ids = new Set();
  for (const block of blocks) {
    if (ids.has(block.id)) throw new Error(`duplicate atlas marker ${block.id}`);
    ids.add(block.id);
    const saved = metadata.get(block.id);
    if (!saved) throw new Error(`atlas marker ${block.id} is not listed in atlas-manifest.json.`);
    if (Object.hasOwn(saved, "implementationEvidence")) throw new Error(`legacy implementationEvidence field on ${block.id}; use evidenceStatus.`);
    Object.assign(block, {
      title: saved.title,
      family: saved.family,
      designStatus: saved.designStatus,
      evidenceStatus: saved.evidenceStatus,
      root: saved.root,
      designRefs: saved.designRefs ?? [],
      sourceRefs: saved.sourceRefs ?? [],
    });
    if (!block.family || !block.title || block.title.includes("_") || !STATUSES.has(block.designStatus) || !EVIDENCE.has(block.evidenceStatus) || !ROOT_KINDS.has(block.root)) throw new Error(`incomplete or non-human metadata for atlas marker ${block.id}`);
    if (block.designStatus !== "current") throw new Error(`non-current design status ${block.designStatus} cannot appear in a .w atlas source.`);
    for (const reference of block.sourceRefs) if (!reference.path || !reference.symbol) throw new Error(`${block.id} has an incomplete source reference.`);
  }
  if (metadata.size !== blocks.length) throw new Error("atlas-manifest.json contains a marker not present in the atlas sources.");
  const order = expected?.pedagogicalOrder;
  if (!Array.isArray(order) || order.length !== blocks.length || new Set(order).size !== blocks.length || order.some((id) => !ids.has(id))) throw new Error("pedagogicalOrder must list every atlas marker exactly once.");
  blocks.sort((left, right) => order.indexOf(left.id) - order.indexOf(right.id));
  validateSourceRefs(blocks);
  rootDisposition(files, blocks);
  const variants = expected?.variants;
  if (!Array.isArray(variants)) throw new Error("atlas-manifest.json must list accepted variants.");
  const variantIds = new Set();
  for (const variant of variants) {
    if (!variant || typeof variant.id !== "string" || variantIds.has(variant.id)) throw new Error("variant inventory contains a missing or duplicate ID.");
    variantIds.add(variant.id);
    if (Object.hasOwn(variant, "implementationEvidence")) throw new Error(`legacy implementationEvidence field on variant ${variant.id}; use evidenceStatus.`);
    if (!variant.block || !blocks.some((block) => block.id === variant.block)) throw new Error(`variant ${variant.id} points to a missing atlas block.`);
    if (variant.designStatus !== "current" || !EVIDENCE.has(variant.evidenceStatus) || !variant.construction || !variant.witness) throw new Error(`variant ${variant.id} has incomplete metadata.`);
    const block = blocks.find((entry) => entry.id === variant.block);
    if (!block.snippet.includes(variant.witness)) throw new Error(`variant ${variant.id} witness is absent from ${variant.block}.`);
  }
  for (const required of REQUIRED_VARIANT_IDS) if (!variantIds.has(required)) throw new Error(`required accepted variant ${required} is missing.`);
  const companions = expected?.companions;
  if (!Array.isArray(companions) || companions.length !== 2) throw new Error("atlas-manifest.json must list the two non-parseable companion files.");
  for (const companion of companions) {
    if (!companion || Object.hasOwn(companion, "implementationEvidence") || !["research", "rejected"].includes(companion.designStatus) || companion.evidenceStatus !== "not-parseable" || !companion.file || companion.file.endsWith(".w")) throw new Error(`invalid companion status for ${companion?.file ?? "unknown"}.`);
    const companionPath = path.join(ATLAS, companion.file);
    if (!fs.existsSync(companionPath)) throw new Error(`missing companion file ${companion.file}.`);
  }
  const allRules = grammarRules();
  const ruleSet = allRules.filter((name) => !name.startsWith("_"));
  const actualDigest = digestBytes(Buffer.from(`${ruleSet.join("\n")}\n`, "utf8"));
  if (actualDigest !== RULE_SET_DIGEST) throw new Error(`grammar public-rule inventory changed: expected ${RULE_SET_DIGEST}, got ${actualDigest}. Classify the new rule before updating the atlas.`);
  const ruleEntries = ruleSet.map((name) => {
    const classification = classifyRule(name);
    if (!classification) throw new Error(`public grammar rule ${name} is unclassified.`);
    const markers = classification.marker ? [classification.marker] : [];
    if (markers.length && !blocks.some((block) => block.id === markers[0])) throw new Error(`${name} points to missing marker ${markers[0]}.`);
    return { name, surface: classification.surface, reason: classification.reason, markers };
  });
  const markerRules = new Map(blocks.map((block) => [block.id, []]));
  for (const entry of ruleEntries) for (const marker of entry.markers) markerRules.get(marker).push(entry.name);
  for (const block of blocks) block.grammarRefs = markerRules.get(block.id).sort();
  const sourceFiles = files.map((file) => path.relative(ATLAS, file.file).split(path.sep).join("/"));
  const familyMap = new Map();
  for (const block of blocks) familyMap.set(block.family, [...(familyMap.get(block.family) ?? []), block.id]);
  return {
    sourceFiles,
    pedagogicalOrder: order,
    files,
    blocks,
    ruleEntries,
    families: [...familyMap.entries()].map(([family, blockIds]) => ({ family, blocks: blockIds })),
    variants,
    companions,
    grammarDigest: digestFile(GRAMMAR),
    ruleSetDigest: RULE_SET_DIGEST,
  };
}

function manifestBlocks(snapshot) {
  return snapshot.blocks.map((block, index) => ({
    id: block.id,
    file: block.file,
    order: index,
    line: block.line,
    title: block.title,
    family: block.family,
    designStatus: block.designStatus,
    evidenceStatus: block.evidenceStatus,
    root: block.root,
    grammarRefs: block.grammarRefs,
    designRefs: block.designRefs,
    sourceRefs: block.sourceRefs,
    snippetDigest: block.snippetDigest,
    snippetBytes: Buffer.byteLength(block.snippet, "utf8"),
  }));
}

function renderCoverage(snapshot) {
  const inlineCode = (value) => `\`${value.replaceAll("`", "\\`")}\``;
  const lines = [
    "# W syntax coverage",
    "",
    "> Generated from marked snippets in `language.w`, `execution.w`, `operators.w`, and `build.w`.",
    "> Do not edit this file. Run `bun tooling/syntax-atlas.mjs --write` after source changes.",
    "> Semantics: [operators, bits, and numeric policy](../../CHEATSHEET.md#operadores-bits-e-política-numérica). Performance: [performance and cost](../../CHEATSHEET.md#performance-e-custo).",
    "",
    "## Quick reference",
    "",
    "| Family | Blocks |",
    "| --- | --- |",
    ...snapshot.families.map((entry) => `| ${entry.family} | ${entry.blocks.map((id) => `\`${id}\``).join(", ")} |`),
    "",
    "| Variant | Syntax | Atlas block |",
    "| --- | --- | --- |",
    ...snapshot.variants.map((variant) => `| ${inlineCode(variant.id)} | ${inlineCode(variant.witness)} | ${inlineCode(variant.block)} |`),
    "",
    "## Full snippets",
    "",
  ];
  for (const block of snapshot.blocks) {
    lines.push(`<details>`, `<summary>${block.title} · ${block.family} · ${block.id}</summary>`, "", `**${block.designStatus}** · **${block.evidenceStatus}**`, "", "```w");
    lines.push(...block.snippet.replace(/\n$/u, "").split("\n"), "```", "");
    lines.push("</details>", "");
  }
  return `${lines.join("\n").replace(/\n+$/u, "")}\n`;
}

function buildManifest(snapshot, coverage) {
  return {
    $schema: SCHEMA,
    purpose: "Human syntax atlas for the current W Tree-sitter surface.",
    sourceOfTruth: "atlas-manifest.json metadata plus terse source markers and snippets in the atlas .w files. SYNTAX-COVERAGE.md is generated.",
    grammar: { path: "tooling/tree-sitter-w/grammar.js", digest: snapshot.grammarDigest, publicRuleSetDigest: snapshot.ruleSetDigest },
    sourceFiles: snapshot.sourceFiles,
    pedagogicalOrder: snapshot.pedagogicalOrder,
    rootForms: [
      { file: "language.w", root: "module", role: "main-reading-path" },
      { file: "execution.w", root: "module", role: "main-reading-path" },
      { file: "build.w", root: "build_manifest", role: "unified-root" },
    ],
    grammarRules: snapshot.ruleEntries,
    families: snapshot.families,
    variants: snapshot.variants,
    blocks: manifestBlocks(snapshot),
    companionFiles: ["reserved.w-reserved.txt", "rejected.w-rejected.txt"],
    companions: snapshot.companions,
    generated: { coverage: "SYNTAX-COVERAGE.md", coverageDigest: digestBytes(Buffer.from(coverage, "utf8")) },
  };
}

function stableJson(value) {
  return `${JSON.stringify(value, null, 2)}\n`;
}

function checkManifest(snapshot, expected) {
  const errors = [];
  if (!expected || expected.$schema !== SCHEMA) errors.push(`manifest must use ${SCHEMA}.`);
  if (expected?.blocks?.some((block) => Object.hasOwn(block, "implementationEvidence")) || expected?.variants?.some((variant) => Object.hasOwn(variant, "implementationEvidence")) || expected?.companions?.some((companion) => Object.hasOwn(companion, "implementationEvidence"))) errors.push("legacy implementationEvidence field is not accepted; use evidenceStatus.");
  if (expected?.grammar?.path !== "tooling/tree-sitter-w/grammar.js" || expected?.grammar?.digest !== snapshot.grammarDigest) errors.push("grammar digest or path is stale.");
  if (expected?.grammar?.publicRuleSetDigest !== snapshot.ruleSetDigest) errors.push("public grammar rule-set digest is stale.");
  const actualFiles = JSON.stringify(snapshot.sourceFiles);
  if (JSON.stringify(expected?.sourceFiles) !== actualFiles) errors.push("manifest sourceFiles are stale.");
  const expectedBlocks = expected?.blocks;
  if (!Array.isArray(expectedBlocks)) errors.push("manifest blocks are missing.");
  const byId = new Map((expectedBlocks ?? []).map((block) => [block.id, block]));
  if (byId.size !== (expectedBlocks ?? []).length) errors.push("manifest blocks contain duplicate IDs.");
  if (byId.size !== snapshot.blocks.length) errors.push("manifest block inventory is missing or has unlisted markers.");
  for (const [index, block] of snapshot.blocks.entries()) {
    const actual = manifestBlocks(snapshot)[index];
    const saved = byId.get(block.id);
    if (!saved) { errors.push(`unlisted or missing marker ${block.id}.`); continue; }
    if (JSON.stringify(saved) !== JSON.stringify(actual)) errors.push(`${block.id} marker, grammar refs, source ref, order, or snippet digest is stale.`);
  }
  const actualRules = snapshot.ruleEntries;
  if (JSON.stringify(expected?.grammarRules) !== JSON.stringify(actualRules)) errors.push("grammarRules inventory is stale or unclassified.");
  if (JSON.stringify(expected?.families) !== JSON.stringify(snapshot.families)) errors.push("family inventory is stale.");
  if (JSON.stringify(expected?.pedagogicalOrder) !== JSON.stringify(snapshot.pedagogicalOrder)) errors.push("pedagogicalOrder is stale.");
  if (JSON.stringify(expected?.variants) !== JSON.stringify(snapshot.variants)) errors.push("accepted variant inventory is stale or incomplete.");
  const coverage = renderCoverage(snapshot);
  if (expected?.generated?.coverage !== "SYNTAX-COVERAGE.md" || expected.generated.coverageDigest !== digestBytes(Buffer.from(coverage, "utf8"))) errors.push("syntax coverage digest is stale.");
  if (!fs.existsSync(SYNTAX_COVERAGE) || fs.readFileSync(SYNTAX_COVERAGE, "utf8") !== coverage) errors.push("SYNTAX-COVERAGE.md is stale or manually edited.");
  if (JSON.stringify(expected?.companionFiles) !== JSON.stringify(["reserved.w-reserved.txt", "rejected.w-rejected.txt"])) errors.push("companion file inventory is stale.");
  if (!Array.isArray(expected?.companions) || JSON.stringify(expected.companions) !== JSON.stringify(snapshot.companions)) errors.push("companion status inventory is stale.");
  if (JSON.stringify(expected?.rootForms) !== JSON.stringify(buildManifest(snapshot, coverage).rootForms)) errors.push("root disposition inventory is stale.");
  return errors;
}

function writeArtifacts(snapshot) {
  const coverage = renderCoverage(snapshot);
  const manifest = buildManifest(snapshot, coverage);
  fs.writeFileSync(MANIFEST, stableJson(manifest), "utf8");
  fs.writeFileSync(SYNTAX_COVERAGE, coverage, "utf8");
}

function main() {
  const mode = process.argv[2] ?? "--check";
  if (!fs.existsSync(ATLAS)) throw new Error("reference/syntax-atlas is missing.");
  const snapshot = deriveSnapshot();
  if (mode === "--write") {
    writeArtifacts(snapshot);
    return;
  }
  if (mode !== "--check") throw new Error("usage: bun tooling/syntax-atlas.mjs --write|--check");
  if (!fs.existsSync(MANIFEST)) throw new Error("atlas-manifest.json is missing. Run --write once.");
  const expected = JSON.parse(fs.readFileSync(MANIFEST, "utf8"));
  const errors = checkManifest(snapshot, expected);
  if (errors.length) throw new Error(errors.join("\n"));
}

if (import.meta.main) {
  try {
    main();
    console.log(`syntax-atlas ${process.argv[2] ?? "--check"}: ok`);
  } catch (error) {
    console.error(`syntax-atlas: ${error.message}`);
    process.exitCode = 1;
  }
}

export { buildManifest, deriveSnapshot, renderCoverage, checkManifest, VISIBLE_RULES_MUST_NOT_BE_INTERNAL, REQUIRED_VARIANT_IDS };
