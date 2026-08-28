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

console.log("type-query highlighting: portal contextual tokens and TextMate pattern are valid");
