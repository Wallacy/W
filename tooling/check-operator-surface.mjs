import crypto from "node:crypto";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawnSync } from "node:child_process";

const ROOT = path.resolve(import.meta.dirname, "..");
const paths = {
  design: path.join(ROOT, "DESIGN.md"),
  grammar: path.join(ROOT, "tooling", "tree-sitter-w", "grammar.js"),
  seedLexer: path.join(ROOT, "compiler", "seed-c", "src", "w_seed_lexer.c"),
  seedParser: path.join(ROOT, "compiler", "seed-c", "src", "w_seed_parser.c"),
  rootCheatsheet: path.join(ROOT, "CHEATSHEET.md"),
  atlasSource: path.join(ROOT, "reference", "syntax-atlas", "operators.w"),
  atlasManifest: path.join(ROOT, "reference", "syntax-atlas", "atlas-manifest.json"),
  syntaxCoverage: path.join(ROOT, "reference", "syntax-atlas", "SYNTAX-COVERAGE.md"),
  lastLightNumerics: path.join(ROOT, "reference", "last-light", "numerics.w"),
};

const assignment = ["=", "+=", "-=", "*=", "/=", "%=", "**=", "<<=", ">>=", "&=", "^=", "|="];
const groups = [
  { name: "assignment", forms: assignment, associativity: "not-chainable" },
  { name: "pipe-forward", forms: ["|>"], associativity: "left" },
  { name: "coalescing", forms: ["??"], associativity: "right" },
  { name: "logical-or", forms: ["||"], associativity: "left" },
  { name: "logical-and", forms: ["&&"], associativity: "left" },
  { name: "bitwise-or", forms: ["|"], associativity: "left" },
  { name: "bitwise-xor", forms: ["^"], associativity: "left" },
  { name: "bitwise-and", forms: ["&"], associativity: "left" },
  { name: "equality", forms: ["==", "!="], associativity: "not-chainable" },
  { name: "relation", forms: ["<", "<=", ">", ">=", "is", "in"], associativity: "not-chainable" },
  { name: "range", forms: ["...", "..<", ">..", ">..<"], associativity: "not-chainable" },
  { name: "shift", forms: ["<<", ">>"], associativity: "left" },
  { name: "additive", forms: ["+", "-"], associativity: "left" },
  { name: "multiplicative", forms: ["*", "/", "%", "@"], associativity: "left" },
  { name: "prefix", forms: ["!", "~", "-", "try", "try?", "await", "copy", "take", "pin", "inout", "ref"], associativity: "right" },
  { name: "power", forms: ["**"], associativity: "right" },
  { name: "postfix", forms: ["call", "member", "index", "?"], associativity: "left" },
];

const namedNumeric = [
  "checkedAdd", "checkedSubtract", "checkedMultiply", "checkedNegate", "checkedDivide", "checkedRemainder", "euclideanDivide", "euclideanRemainder", "checkedPower",
  "checkedShiftLeft", "checkedShiftRight", "wrappingAdd", "wrappingSubtract", "wrappingMultiply", "wrappingNegate", "wrappingPower", "wrappingShiftLeft",
  "saturatingAdd", "saturatingSubtract", "saturatingMultiply", "saturatingNegate", "saturatingPower",
  "overflowingAdd", "overflowingSubtract", "overflowingMultiply", "overflowingNegate", "overflowingPower",
  "carryingAdd", "borrowingSubtract", "fullMultiply", "maskedShiftLeft", "maskedShiftRight",
  "logicalShiftRight", "rotatedLeft", "rotatedRight", "toBits", "fromBits", "toBytes", "fromBytes",
];
const namedBitPrimitives = [
  "bitWidth", "countOnes", "countZeros", "countLeadingZeros", "countTrailingZeros", "reversedBits", "reversedBytes",
];
namedNumeric.push(...namedBitPrimitives);

const rejectedNumeric = [
  "wrappingDivide", "saturatingDivide", "overflowingDivide",
  "wrappingRemainder", "saturatingRemainder", "overflowingRemainder",
  "saturatingShiftLeft", "overflowingShiftLeft", "wrappingShiftRight",
  "saturatingShiftRight", "overflowingShiftRight",
];
const policySignatures = [
  ["checkedAdd", "Result<T, ArithmeticError>"], ["wrappingAdd", "T"], ["saturatingAdd", "T"], ["overflowingAdd", "(T, Bool)"],
  ["checkedSubtract", "Result<T, ArithmeticError>"], ["wrappingSubtract", "T"], ["saturatingSubtract", "T"], ["overflowingSubtract", "(T, Bool)"],
  ["checkedMultiply", "Result<T, ArithmeticError>"], ["wrappingMultiply", "T"], ["saturatingMultiply", "T"], ["overflowingMultiply", "(T, Bool)"],
  ["checkedNegate", "Result<T, ArithmeticError>"], ["wrappingNegate", "T"], ["saturatingNegate", "T"], ["overflowingNegate", "(T, Bool)"],
  ["checkedPower", "Result<T, ArithmeticError>"], ["wrappingPower", "T"], ["saturatingPower", "T"], ["overflowingPower", "(T, Bool)"],
  ["checkedDivide", "Result<T, ArithmeticError>"], ["checkedRemainder", "Result<T, ArithmeticError>"], ["euclideanDivide", "T"], ["euclideanRemainder", "T"],
  ["checkedShiftLeft", "Result<T, ArithmeticError>"], ["wrappingShiftLeft", "T"],
  ["checkedShiftRight", "Result<T, ArithmeticError>"], ["maskedShiftLeft", "T"],
  ["maskedShiftRight", "T"], ["logicalShiftRight", "T"], ["rotatedLeft", "T"], ["rotatedRight", "T"],
];

const rejected = ["custom/user operators", "unary +", "++", "--", "postfix force unwrap", "&&=", "||=", "??=", "@="];
const lexicalOperators = [
  ">..<", "...", "..<", ">..", "**=", "<<=", ">>=", "|>", "?.", "??", "=>", "==", "!=", "<=", ">=", "&&", "||",
  "+=", "-=", "*=", "/=", "%=", "&=", "^=", "|=", "**", "<<", ">>",
];
const binaryInventory = [
  "@", "*", "/", "%", "+", "-", "<<", ">>", "<", "<=", ">", ">=", "is", "in", "==", "!=", "&", "^", "|", "&&", "||",
];
const parserInventory = [
  ...assignment, "|>", "??", "||", "&&", "|", "^", "&", "==", "!=", "<", "<=", ">", ">=", "...", "..<", ">..", ">..<", "<<", ">>", "+", "-", "*", "/", "%", "@", "**",
];

function read(file) {
  return fs.readFileSync(file, "utf8");
}

function fail(errors, message) {
  errors.push(message);
}

function codeForm(form) {
  return form.replaceAll("|", "\\|");
}

function requireCodeForms(errors, text, label, forms) {
  for (const form of forms) {
    const needle = `\`${codeForm(form)}\``;
    if (!text.includes(needle)) fail(errors, `${label} is missing ${form}`);
  }
}

function normalizeBits(value, width) {
  return BigInt.asUintN(width, BigInt(value));
}

function countOnes(value) {
  let count = 0n;
  let remaining = value;
  while (remaining !== 0n) {
    count += remaining & 1n;
    remaining >>= 1n;
  }
  return count;
}

function countLeadingZeros(value, width) {
  if (value === 0n) return BigInt(width);
  let count = 0n;
  for (let bit = width - 1; bit >= 0; bit -= 1) {
    if (((value >> BigInt(bit)) & 1n) !== 0n) break;
    count += 1n;
  }
  return count;
}

function countTrailingZeros(value, width) {
  if (value === 0n) return BigInt(width);
  let count = 0n;
  for (let bit = 0; bit < width; bit += 1) {
    if (((value >> BigInt(bit)) & 1n) !== 0n) break;
    count += 1n;
  }
  return count;
}

function reverseBits(value, width) {
  let result = 0n;
  for (let bit = 0; bit < width; bit += 1) {
    if (((value >> BigInt(bit)) & 1n) !== 0n) result |= 1n << BigInt(width - bit - 1);
  }
  return result;
}

function reverseBytes(value, width) {
  let result = 0n;
  const bytes = width / 8;
  for (let index = 0; index < bytes; index += 1) {
    const byte = (value >> BigInt(index * 8)) & 0xffn;
    result |= byte << BigInt((bytes - index - 1) * 8);
  }
  return result;
}

function checkBitPrimitiveOracle(errors) {
  const vectors = [
    {
      label: "u8 zero", width: 8, value: 0n,
      ones: 0n, zeros: 8n, leading: 8n, trailing: 8n, bits: 0n, bytes: 0n,
    },
    {
      label: "u8 0b0010_1000", width: 8, value: 0b0010_1000n,
      ones: 2n, zeros: 6n, leading: 2n, trailing: 3n, bits: 0b0001_0100n, bytes: 0b0010_1000n,
    },
    {
      label: "u16 0x1234", width: 16, value: 0x1234n,
      ones: 5n, zeros: 11n, leading: 3n, trailing: 2n, bits: 0x2c48n, bytes: 0x3412n,
    },
    {
      label: "signed i8 -2 pattern 0xfe", width: 8, value: -2n,
      ones: 7n, zeros: 1n, leading: 0n, trailing: 1n, bits: 0x7fn, bytes: 0xfen,
    },
  ];
  for (const vector of vectors) {
    const value = normalizeBits(vector.value, vector.width);
    const actual = {
      width: BigInt(vector.width),
      ones: countOnes(value),
      zeros: BigInt(vector.width) - countOnes(value),
      leading: countLeadingZeros(value, vector.width),
      trailing: countTrailingZeros(value, vector.width),
      bits: reverseBits(value, vector.width),
      bytes: reverseBytes(value, vector.width),
    };
    const expected = {
      width: BigInt(vector.width), ones: vector.ones, zeros: vector.zeros,
      leading: vector.leading, trailing: vector.trailing, bits: vector.bits, bytes: vector.bytes,
    };
    for (const key of Object.keys(expected)) {
      if (actual[key] !== expected[key]) fail(errors, `bit primitive oracle mismatch for ${vector.label}: ${key}`);
    }
  }
}

function euclideanPair(left, right, bits, signed) {
  if (right === 0n) throw new RangeError("division by zero");
  const minimum = -(1n << BigInt(bits - 1));
  if (signed && left === minimum && right === -1n) throw new RangeError("division overflow");
  let quotient = left / right;
  let remainder = left % right;
  if (remainder < 0n) {
    remainder += right < 0n ? -right : right;
    quotient += right < 0n ? 1n : -1n;
  }
  return { quotient, remainder };
}

function euclideanRemainderOracle(left, right, bits, signed) {
  if (right === 0n) throw new RangeError("division by zero");
  const minimum = -(1n << BigInt(bits - 1));
  if (signed && left === minimum && right === -1n) return 0n;
  const remainder = left % right;
  const magnitude = right < 0n ? -right : right;
  return remainder < 0n ? remainder + magnitude : remainder;
}

function checkEuclideanOracle(errors) {
  for (const [left, right] of [[-7n, 3n], [-7n, -3n], [7n, 3n], [7n, -3n]]) {
    const pair = euclideanPair(left, right, 8, true);
    if (left !== right * pair.quotient + pair.remainder || pair.remainder < 0n || pair.remainder >= (right < 0n ? -right : right)) {
      fail(errors, `euclidean oracle invariant failed for ${left}/${right}`);
    }
  }
  if (euclideanRemainderOracle(-128n, -1n, 8, true) !== 0n) fail(errors, "euclidean remainder min/-1 must be zero");
  try {
    euclideanPair(-128n, -1n, 8, true);
    fail(errors, "euclidean divide min/-1 must reject");
  } catch (error) {
    if (!(error instanceof RangeError)) fail(errors, "euclidean divide min/-1 raised the wrong error");
  }
  for (const right of [0n]) {
    try {
      euclideanPair(1n, right, 8, true);
      fail(errors, "euclidean divide zero must reject");
    } catch (error) {
      if (!(error instanceof RangeError)) fail(errors, "euclidean divide zero raised the wrong error");
    }
  }
}

function extractBlock(source, start, end) {
  const begin = source.indexOf(start);
  if (begin < 0) return "";
  const finish = source.indexOf(end, begin + start.length);
  return finish < 0 ? "" : source.slice(begin, finish);
}

function extractQuoted(block) {
  return [...block.matchAll(/"([^"\\]*(?:\\.[^"\\]*)*)"/gu)].map((match) => match[1]);
}

function parseGrammarOperators(source) {
  const binaryBlock = source.match(/const BINARY_OPERATORS = \[(.*?)\];/su)?.[1] ?? "";
  const assignmentsBlock = source.match(/const ASSIGNMENT_OPERATORS = \[(.*?)\];/su)?.[1] ?? "";
  const binary = new Map([...binaryBlock.matchAll(/\["([^"\\]+)",\s*(\d+)\]/gu)].map((match) => [match[1], Number(match[2])]));
  return { binary, assignments: extractQuoted(assignmentsBlock) };
}

function parseSeedParserTable(source) {
  const block = extractBlock(source, "  } operators[] = {", "  };\n  for (size_t index");
  const table = new Map();
  for (const match of block.matchAll(/\{"([^"\\]+)",\s*(\d+),\s*(true|false)\}/gu)) {
    table.set(match[1], { precedence: Number(match[2]), rightAssociative: match[3] === "true" });
  }
  return table;
}

function tableLabels(text, heading) {
  const section = text.match(new RegExp(`${heading}[\\s\\S]*?(?=\\n### |\\n## |$)`, "u"))?.[0] ?? "";
  return [...section.matchAll(/^\|\s*([^|]+?)\s*\|/gmu)].map((match) => match[1].trim()).filter((label) => !["Grupo", "Família", "---"].includes(label));
}

function assertRelativePrecedence(errors, values, label) {
  const expected = [
    ["assignment", ["="], "pipe-forward", ["|>"]],
    ["pipe-forward", ["|>"], "coalescing", ["??"]],
    ["coalescing", ["??"], "logical-or", ["||"]],
    ["logical-or", ["||"], "logical-and", ["&&"]],
    ["logical-and", ["&&"], "bitwise-or", ["|"]],
    ["bitwise-or", ["|"], "bitwise-xor", ["^"]],
    ["bitwise-xor", ["^"], "bitwise-and", ["&"]],
    ["bitwise-and", ["&"], "equality", ["=="]],
    ["equality", ["=="], "relation", ["<"]],
    ["relation", ["<"], "range", ["..."]],
    ["range", ["..."], "shift", ["<<"]],
    ["shift", ["<<"], "additive", ["+"]],
    ["additive", ["+"], "multiplicative", ["*"]],
    ["multiplicative", ["*"], "power", ["**"]],
  ];
  for (const [lowerName, lowerForms, higherName, higherForms] of expected) {
    const lower = values.get(lowerForms[0]);
    const higher = values.get(higherForms[0]);
    if (lower === undefined || higher === undefined || lower >= higher) {
      fail(errors, `${label} precedence drift: ${lowerName} must be weaker than ${higherName}`);
    }
  }
}

function assertAssociativity(errors, grammar, seed) {
  for (const form of ["**"]) {
    if (grammar.binary.get(form) === undefined || seed.get(form)?.rightAssociative !== true) fail(errors, `${form} must be right-associative in grammar and seed parser`);
  }
  for (const form of ["??", ...assignment]) {
    const seedEntry = seed.get(form);
    if (form === "??" && seedEntry?.rightAssociative !== true) fail(errors, `${form} must be right-associative in seed parser`);
    if (form !== "??" && seedEntry?.rightAssociative !== true) fail(errors, `${form} must be right-associative in seed parser`);
  }
}

function findTreeSitter() {
  const candidate = process.platform === "win32"
    ? path.join(ROOT, "tooling", "tree-sitter-w", "node_modules", ".bin", "tree-sitter.cmd")
    : path.join(ROOT, "tooling", "tree-sitter-w", "node_modules", ".bin", "tree-sitter");
  return fs.existsSync(candidate) ? candidate : "tree-sitter";
}

function parseProbe(source, label, errors, mode = "required") {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), "w-operator-surface-"));
  const file = path.join(directory, "probe.w");
  fs.writeFileSync(file, `${source.trim()}\n`, "utf8");
  try {
    const result = spawnSync(findTreeSitter(), ["parse", "--grammar-path", path.dirname(paths.grammar), "--xml", "--no-ranges", file], {
      cwd: path.dirname(paths.grammar), encoding: "utf8", windowsHide: true, shell: process.platform === "win32",
    });
    const output = `${result.stdout ?? ""}\n${result.stderr ?? ""}`;
    const failed = result.status !== 0 || /\b(?:ERROR|MISSING)\b/u.test(output);
    if (failed) {
      if (mode === "gap") return { output, parsed: false };
      fail(errors, `${label} parse probe failed`);
      return { output: "", parsed: false };
    }
    return { output, parsed: true };
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
}

function checkProbes(errors) {
  const gaps = [];
  const probes = [
    {
      label: "bitwise precedence (parser/grammar-only)",
      source: "module probe\nfn f() { let result = left | middle ^ right & mask }",
      pattern: /\|\s*<binary_expression[\s\S]*\^\s*<binary_expression[\s\S]*&amp;/u,
    },
    {
      label: "power right-association (parser/grammar-only)",
      source: "module probe\nfn f() { let result = left ** middle ** right }",
      pattern: /\*\*\s*<binary_expression[\s\S]*\*\*/u,
    },
    {
      label: "unary power witness -2 ** 2 (parser/grammar-only)",
      source: "module probe\nfn f() { let result = -2 ** 2 }",
      pattern: /<unary_expression[\s\S]*<binary_expression[\s\S]*\*\*/u,
    },
    {
      label: "shift nesting (parser/grammar-only)",
      source: "module probe\nfn f() { let result = left << middle >> right }",
      pattern: /<binary_expression[\s\S]*<binary_expression[\s\S]*&lt;&lt;[\s\S]*&gt;&gt;/u,
    },
    {
      label: "one-sided range (parser/grammar-only)",
      source: "module probe\nfn f() { let result = select(..<upper) }",
      pattern: /<one_sided_range_expression[\s\S]*\.\.&lt;[\s\S]*<identifier/u,
    },
    {
      label: "one-sided >.. direct Tree-sitter gap",
      source: "module probe\nfn f() { let result = select(>..upper) }",
      mode: "gap",
    },
    {
      label: "one-sided >..< direct Tree-sitter gap",
      source: "module probe\nfn f() { let result = select(>..<upper) }",
      mode: "gap",
    },
    {
      label: "compound place once shape (parser/grammar-only)",
      source: "module probe\nfn f() { registers[index()] <<= 1 }",
      pattern: /<assignment_expression[\s\S]*<index_expression field="left"[\s\S]*&lt;&lt;=/u,
    },
    {
      label: "assignment chain frontend conformance gap",
      source: "module probe\nfn f() { let result = a = b = c }",
      pattern: /<assignment_expression[\s\S]*<assignment_expression[\s\S]*/u,
      mode: "semantic-gap",
    },
  ];
  for (const probe of probes) {
    const result = parseProbe(probe.source, probe.label, errors, probe.mode === "gap" ? "gap" : "required");
    if (probe.mode === "gap") {
      if (result.parsed) fail(errors, `${probe.label} unexpectedly parses`);
      else gaps.push(probe.label);
      continue;
    }
    if (!result.parsed) continue;
    if (!probe.pattern.test(result.output)) fail(errors, `${probe.label} witness shape is absent`);
    if (probe.mode === "semantic-gap") gaps.push(probe.label);
  }
  return gaps;
}

function check() {
  const errors = [];
  const design = read(paths.design);
  const grammarSource = read(paths.grammar);
  const lexerSource = read(paths.seedLexer);
  const parserSource = read(paths.seedParser);
  const rootCheatsheet = read(paths.rootCheatsheet);
  const atlasSource = read(paths.atlasSource);
  const syntaxCoverage = read(paths.syntaxCoverage);
  const lastLightNumerics = read(paths.lastLightNumerics);
  let manifest;
  try {
    manifest = JSON.parse(read(paths.atlasManifest));
  } catch (error) {
    fail(errors, `atlas manifest is not valid JSON: ${error.message}`);
    manifest = {};
  }

  const grammar = parseGrammarOperators(grammarSource);
  grammar.binary.set("??", 1);
  grammar.binary.set("**", 14);
  for (const form of ["...", "..<", ">..", ">..<", "is", "in"]) grammar.binary.set(form, form === "is" || form === "in" ? 8 : 9);
  const parserTable = parseSeedParserTable(parserSource);
  const lexerBlock = extractBlock(lexerSource, "static const char *const operators[] = {", "  };\n  const size_t start");
  const lexerOperators = extractQuoted(lexerBlock);

  const designOrder = ["assignment", "pipe-forward", "coalescing", "logical OR", "logical AND", "bitwise", "equality", "relation", "range", "shift", "additive", "multiplicative", "prefix", "power", "postfix"];
  const designLabels = tableLabels(design, "### 5.6 Operadores");
  if (JSON.stringify(designLabels.slice(0, designOrder.length)) !== JSON.stringify(designOrder)) fail(errors, "DESIGN operator table order or labels drifted");

  for (const group of groups) {
    requireCodeForms(errors, design, `DESIGN ${group.name}`, group.forms.filter((form) => !["call", "member", "index"].includes(form)));
    requireCodeForms(errors, rootCheatsheet, `root CHEATSHEET ${group.name}`, group.forms.filter((form) => !["call", "member", "index"].includes(form)));
  }
  if (!rootCheatsheet.includes("`?.member`")) fail(errors, "root CHEATSHEET postfix optional member is missing");
  for (const form of assignment) {
    if (!grammar.assignments.includes(form)) fail(errors, `grammar assignment table is missing ${form}`);
    if (!parserTable.has(form)) fail(errors, `seed parser table is missing ${form}`);
  }
  if (grammar.assignments.length !== assignment.length || grammar.assignments.some((form, index) => form !== assignment[index])) fail(errors, "grammar assignment inventory drifted");
  for (const form of binaryInventory) if (!grammar.binary.has(form)) fail(errors, `grammar binary inventory is missing ${form}`);
  for (const form of grammar.binary.keys()) if (![...binaryInventory, "??", "**", "...", "..<", ">..", ">..<"].includes(form)) fail(errors, `grammar binary inventory has unlisted ${form}`);
  for (const form of parserInventory) if (!parserTable.has(form)) fail(errors, `seed parser inventory is missing ${form}`);
  for (const form of parserTable.keys()) if (!parserInventory.includes(form)) fail(errors, `seed parser inventory has unlisted ${form}`);
  for (const keyword of ["is", "in"]) if (!parserSource.includes(`current_is_text(parser, "${keyword}")`)) fail(errors, `seed parser keyword operator path is missing ${keyword}`);
  for (const [form, precedence] of grammar.binary) {
    if (form !== "@" && !groups.some((group) => group.forms.includes(form))) fail(errors, `grammar has unlisted binary operator ${form}`);
    if (form !== "@" && parserTable.get(form)?.precedence === undefined && !["is", "in"].includes(form)) fail(errors, `seed parser table is missing ${form}`);
    if (form !== "@" && parserTable.get(form)?.precedence !== undefined && !Number.isInteger(precedence)) fail(errors, `grammar precedence is not numeric for ${form}`);
  }
  const grammarValues = new Map([...grammar.binary.entries(), ...assignment.map((form) => [form, 0])]);
  // The pipe has a dedicated grammar rule, not a seed binary-operator entry.
  // Keep its conceptual slot between assignment (-1) and coalescing (1).
  grammarValues.set("=", -1);
  grammarValues.set("|>", 0);
  const parserValues = new Map([...parserTable.entries()].map(([form, entry]) => [form, entry.precedence]));
  parserValues.set("is", 10);
  parserValues.set("in", 10);
  assertRelativePrecedence(errors, grammarValues, "grammar");
  assertRelativePrecedence(errors, parserValues, "seed parser");
  assertAssociativity(errors, grammar, parserTable);

  for (const form of lexicalOperators) if (!lexerOperators.includes(form)) fail(errors, `seed lexer longest-match table is missing ${form}`);
  for (let index = 0; index < lexerOperators.length; index += 1) {
    for (let next = 0; next < lexerOperators.length; next += 1) {
      if (index === next) continue;
      const longer = lexerOperators[index];
      const shorter = lexerOperators[next];
      if (longer.length > shorter.length && longer.startsWith(shorter) && index > next) {
        fail(errors, `seed lexer longest-match order places ${shorter} before ${longer}`);
      }
    }
  }

  for (const form of namedNumeric) {
    if (!design.includes(form)) fail(errors, `DESIGN numeric policy is missing ${form}`);
    if (!atlasSource.includes(form)) fail(errors, `operators.w is missing ${form}`);
  }
  const designPolicyMatrix = extractBlock(design, "As policies numéricas têm um inventário fechado.", "`overflowingX` devolve");
  const rootPolicyMatrix = extractBlock(rootCheatsheet, "#### Matriz fechada de policies integer", "| Qual forma usar");
  for (const [name, returnType] of policySignatures) {
    const signature = `\`${name} -> ${returnType}\``;
    if (!designPolicyMatrix.includes(signature)) fail(errors, `DESIGN closed policy matrix is missing ${signature}`);
    if (!rootPolicyMatrix.includes(signature)) fail(errors, `root CHEATSHEET closed policy matrix is missing ${signature}`);
  }
  for (const form of rejectedNumeric) {
    if (designPolicyMatrix.includes(`\`${form}`)) fail(errors, `DESIGN closed policy matrix presents rejected numeric policy ${form}`);
    if (rootPolicyMatrix.includes(`\`${form}`)) fail(errors, `root CHEATSHEET closed policy matrix presents rejected numeric policy ${form}`);
  }
  for (const form of rejectedNumeric) {
    if (atlasSource.includes(form)) fail(errors, `operators.w presents rejected numeric policy ${form}`);
  }
  if (design.includes("`signed.min % -1` produz `0`") === false ||
      !design.includes("checkedRemainder`\nrejeita somente divisor zero")) {
    fail(errors, "remainder must reject only zero divisor and define signed.min % -1 as zero");
  }
  for (const form of namedBitPrimitives) {
    if (!rootCheatsheet.includes(form)) fail(errors, `root CHEATSHEET bit primitive inventory is missing ${form}`);
    if (!lastLightNumerics.includes(form)) fail(errors, `Last Light numerics is missing ${form}`);
    if (grammar.binary.has(form) || grammar.assignments.includes(form) || parserTable.has(form) || lexerOperators.includes(form)) {
      fail(errors, `named bit primitive ${form} was treated as a token/operator`);
    }
  }
  for (const vector of ["0b0010_1000", "0x1234", "0x3412", "0x2c48", "0xfe", "0x7f"]) {
    if (!lastLightNumerics.includes(vector)) fail(errors, `Last Light numerics is missing bit primitive vector ${vector}`);
  }
  for (const form of [...assignment, "|>", "??", "|", "^", "&", "<<", ">>", "**", "in", "is", "...", "..<", "@", "?."]) {
    if (!atlasSource.includes(form)) fail(errors, `operators.w is missing a ${form} witness`);
  }
  for (const term of ["checked*", "wrapping*", "saturating*", "overflowing*"]) {
    if (!rootCheatsheet.includes(term)) fail(errors, `root CHEATSHEET numeric policy is missing ${term}`);
  }
  for (const term of ["carryingAdd", "borrowingSubtract", "fullMultiply", "toBits", "fromBits", "toBytes", "fromBytes", "tensor.matmul", "tensor.contract", "isSameInstance", "place uma vez"]) {
    if (!design.includes(term) && !rootCheatsheet.includes(term)) fail(errors, `operator policy text is missing ${term}`);
  }
  for (const term of rejected) {
    const normalized = `${design}\n${rootCheatsheet}`.toLocaleLowerCase();
    const present = term === "custom/user operators"
      ? normalized.includes("custom/user operators") || normalized.includes("operadores definidos pelo usuário")
      : term === "unary +"
        ? normalized.includes("unary `+`") || normalized.includes("unary +")
        : term === "postfix force unwrap"
          ? normalized.includes("postfix force unwrap")
          : normalized.includes(term.toLocaleLowerCase());
    if (!present) fail(errors, `rejected-form inventory is missing ${term}`);
    if (grammarSource.includes(`"${term}"`) || lexerSource.includes(`"${term}"`) || parserSource.includes(`{"${term}"`)) fail(errors, `rejected form ${term} appears in an accepted operator table`);
  }
  if (!design.includes("Assignment produz `()`") || !rootCheatsheet.includes("Assignment composta preserva")) fail(errors, "assignment result/place-once contract is missing");
  if (!design.includes("isSameInstance") || !rootCheatsheet.includes("isSameInstance")) fail(errors, "identity API distinction is missing");
  if (!grammarSource.includes("optional_member_expression") || !grammarSource.includes('token.immediate("?")')) fail(errors, "grammar optional postfix/member surface is missing");

  const block = manifest.blocks?.find((entry) => entry.id === "operators");
  if (!manifest.sourceFiles?.includes("operators.w") || !manifest.pedagogicalOrder?.includes("operators") || !block) fail(errors, "atlas manifest does not inventory operators.w");
  if (block && (block.file !== "operators.w" || block.root !== "module" || block.designStatus !== "current" || !block.designRefs?.includes("5.6"))) fail(errors, "operators atlas block metadata is incomplete");
  if (!syntaxCoverage.includes("<summary>Operators and numeric policies")) fail(errors, "generated syntax coverage is missing operators block");
  if (!syntaxCoverage.includes("let power = -2 ** 2") || !syntaxCoverage.includes("u16.checkedAdd")) fail(errors, "generated syntax coverage lost operator witnesses");
  const digest = `sha256:${crypto.createHash("sha256").update(read(paths.grammar)).digest("hex")}`;
  if (manifest.grammar?.digest !== digest) fail(errors, "atlas manifest grammar digest is stale");

  const gaps = checkProbes(errors);
  if (parserTable.get("=")?.rightAssociative !== true) fail(errors, "seed parser assignment associativity changed without a conformance decision");
  if (!design.includes("Assignment encadeada fica inválida") || !design.includes("Assignment produz `()`")) fail(errors, "DESIGN assignment chain rejection contract is missing");
  checkBitPrimitiveOracle(errors);
  checkEuclideanOracle(errors);
  if (errors.length > 0) throw new Error(errors.join("\n"));
  console.log(`operator-surface: ok (${groups.length} groups, ${assignment.length} assignments, ${namedNumeric.length} named numeric APIs; probes parser/grammar-only; gaps: ${gaps.join(", ")})`);
}

if (import.meta.main) {
  try {
    check();
  } catch (error) {
    console.error(`operator-surface: ${error.message}`);
    process.exitCode = 1;
  }
}

export { check, groups, assignment, namedNumeric, namedBitPrimitives, rejected, rejectedNumeric, policySignatures };
