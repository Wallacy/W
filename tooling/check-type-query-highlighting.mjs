import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import vm from "node:vm";

const root = path.resolve(import.meta.dir, "..");

const portalSource = fs.readFileSync(path.join(root, "portal/w-syntax.js"), "utf8");
const context = { window: {} };
vm.runInNewContext(portalSource, context, { filename: "portal/w-syntax.js" });
const syntax = context.window.WSyntax;
assert.ok(syntax, "portal fallback must expose WSyntax");
assert.equal(syntax.keywords.has("info"), false, "info must not be a global portal keyword");
assert.equal(syntax.keywords.has("of"), false, "of must not be a global portal keyword");
assert.equal(syntax.keywords.has("typeof"), false, "typeof must not be a global portal keyword");

const portalTokens = (source) => JSON.parse(JSON.stringify(syntax.scan(source).tokens.map(({ value, kind }) => ({ value, kind }))));
assert.deepEqual(portalTokens("let info = 1\nlet of = 2\nlet typeof = 3"), [
  { value: "let", kind: "keyword" },
  { value: "info", kind: "identifier" },
  { value: "=", kind: "operator" },
  { value: "1", kind: "number" },
  { value: "let", kind: "keyword" },
  { value: "of", kind: "identifier" },
  { value: "=", kind: "operator" },
  { value: "2", kind: "number" },
  { value: "let", kind: "keyword" },
  { value: "typeof", kind: "identifier" },
  { value: "=", kind: "operator" },
  { value: "3", kind: "number" }
], "bare contextual words must remain identifiers");
assert.deepEqual(portalTokens("type of Ticket\ninfo of value\ninfo(value)"), [
  { value: "type", kind: "keyword" },
  { value: "of", kind: "keyword" },
  { value: "Ticket", kind: "identifier" },
  { value: "info", kind: "keyword" },
  { value: "of", kind: "keyword" },
  { value: "value", kind: "identifier" },
  { value: "info", kind: "identifier" },
  { value: "(", kind: "delimiter" },
  { value: "value", kind: "identifier" },
  { value: ")", kind: "delimiter" }
], "only query phrases promote info/of");
assert.deepEqual(portalTokens("type of = Int"), [
  { value: "type", kind: "keyword" },
  { value: "of", kind: "identifier" },
  { value: "=", kind: "operator" },
  { value: "Int", kind: "identifier" }
], "declaration-like type of must not promote of");
assert.deepEqual(portalTokens("place#version.mutationEpoch\nvalue |> try json.decode<Document>()"), [
  { value: "place", kind: "identifier" },
  { value: "#", kind: "operator" },
  { value: "version", kind: "identifier" },
  { value: ".", kind: "punctuation" },
  { value: "mutationEpoch", kind: "identifier" },
  { value: "value", kind: "identifier" },
  { value: "|>", kind: "operator" },
  { value: "try", kind: "keyword" },
  { value: "json", kind: "identifier" },
  { value: ".", kind: "punctuation" },
  { value: "decode", kind: "identifier" },
  { value: "<", kind: "operator" },
  { value: "Document", kind: "identifier" },
  { value: ">", kind: "operator" },
  { value: "(", kind: "delimiter" },
  { value: ")", kind: "delimiter" },
], "facet and pipe-forward tokens must remain visible");
const transactionTokens = portalTokens("transaction\npipeline<transaction: {}>");
assert.equal(transactionTokens.find((token) => token.value === "transaction")?.kind, "identifier", "bare transaction must remain an identifier");
assert.equal(transactionTokens.filter((token) => token.value === "transaction")[1]?.kind, "keyword", "pipeline transaction label must be contextual");
const rawTokens = portalTokens("let raw = #\"place#facet\"#");
assert.equal(rawTokens.at(-1)?.kind, "string", "raw hash strings must remain strings");

const tmPath = path.join(root, "tooling/vscode-w/syntaxes/w.tmLanguage.json");
const tm = JSON.parse(fs.readFileSync(tmPath, "utf8"));
const keywordPatterns = tm.repository?.keywords?.patterns ?? [];
const queryPattern = keywordPatterns.find((pattern) => pattern.name === "meta.type-query.w");
assert.ok(queryPattern, "TextMate must define a contextual type-query pattern");
assert.match(queryPattern.match, /type\|info/u);
assert.match(queryPattern.match, /of/u);
assert.equal(queryPattern.captures?.["1"]?.name, "keyword.operator.type-query.w");
assert.equal(queryPattern.captures?.["3"]?.name, "keyword.operator.type-query.w");
const queryRegex = new RegExp(queryPattern.match, "u");
assert.equal(queryRegex.test("type of Ticket"), true, "TextMate must match an actual query");
assert.equal(queryRegex.test("type of = Int"), false, "TextMate must not match declaration-like type of");
const globalKeywordText = keywordPatterns
  .filter((pattern) => pattern !== queryPattern)
  .map((pattern) => pattern.match ?? "")
  .join("\n");
assert.doesNotMatch(globalKeywordText, /\binfo\b/u, "TextMate must not reserve info globally");
assert.doesNotMatch(globalKeywordText, /\bof\b/u, "TextMate must not reserve of globally");
assert.doesNotMatch(globalKeywordText, /\btransaction\b/u, "TextMate must not reserve transaction globally");
for (const hook of ["willSet", "didSet", "willModify", "didModify"]) {
  assert.doesNotMatch(globalKeywordText, new RegExp(`\\b${hook}\\b`, "u"), `TextMate must not reserve ${hook} globally`);
}
const pipelineContractPattern = tm.repository?.["pipeline-contract"]?.patterns?.[0];
assert.ok(pipelineContractPattern, "TextMate must define a structural pipeline contract context");
assert.match(pipelineContractPattern.begin, /pipeline/u);
assert.match(pipelineContractPattern.patterns?.find((pattern) => pattern.match?.includes("transaction"))?.match ?? "", /transaction/u);
const behaviorBodyPattern = tm.repository?.["behavior-bodies"]?.patterns?.[0];
assert.ok(behaviorBodyPattern, "TextMate must define a behavior body context");
assert.match(behaviorBodyPattern.begin, /behavior/u);
assert.match(behaviorBodyPattern.patterns?.find((pattern) => pattern.match?.includes("willSet"))?.match ?? "", /didModify/u);
const operatorPattern = tm.repository?.operators?.patterns?.find((pattern) => pattern.name === "keyword.operator.w");
assert.ok(operatorPattern, "TextMate must define the operator pattern");
const operatorRegex = new RegExp(operatorPattern.match, "u");
assert.equal(operatorRegex.test("|>"), true, "TextMate must highlight pipe-forward");
assert.equal(operatorRegex.test("#facet"), true, "TextMate must highlight facet projection");
assert.equal(operatorRegex.test("#\"raw\"#"), false, "TextMate must not steal raw-string delimiters");

console.log("type-query highlighting: portal contextual tokens and TextMate pattern are valid");
