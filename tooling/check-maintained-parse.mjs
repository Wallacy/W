import assert from "node:assert/strict"
import fs from "node:fs"
import path from "node:path"

const root = path.resolve(import.meta.dirname, "..")
const treePackagePath = path.join(root, "tooling", "tree-sitter-w", "package.json")
const suitesPath = path.join(root, "tooling", "check-suites.json")
const treePackage = JSON.parse(fs.readFileSync(treePackagePath, "utf8"))
const suites = JSON.parse(fs.readFileSync(suitesPath, "utf8"))

assert.match(treePackage.scripts["parse:reference"], /reference\/last-light\/\*\.w/)
assert.match(treePackage.scripts["parse:std"], /std\/.*\.w/)

const treeCheckParseSteps = suites.suites["tree-check"].steps.filter(
  (step) => step.package === "tree" && (step.script === "parse:reference" || step.script === "parse:std"),
)
assert.deepEqual(
  treeCheckParseSteps.map((step) => step.script),
  ["parse:reference", "parse:std"],
  "tree-check must keep both maintained-source parse gates exactly once and in source order",
)

function reservedTypeUses(source) {
  return source.split(/\r?\n/).flatMap((line, index) => {
    if (!/\btype\s*:/.test(line)) return []
    // A contextual computed property is the one permitted surface. Its
    // accessor body starts on the declaration line, unlike a label or binding.
    if (/^\s*(?:export\s+)?(?:let|var)\s+type\s*:\s*[^{}\r\n]+\{\s*$/.test(line)) return []
    return [`${index + 1}: ${line.trim()}`]
  })
}

const sourceRules = [
  {
    path: "reference/last-light/web_bodies.w",
    required: ["mediaType:", ".type"],
    forbidden: [
      /\b(?:let|var|const)\s+type\b/g,
      /\b(?:take|copy|ref|view|shared|weak)\s+type\b/g,
    ],
  },
  {
    path: "std/blob/contracts.w",
    required: ["mediaType:", "export let type:", ".type"],
    forbidden: [
      /\b(?:var|const)\s+type\b/g,
      /\b(?:take|copy|ref|view|shared|weak)\s+type\b/g,
      /normalizeMediaType\(type\)/g,
    ],
  },
  {
    path: "std/data/contracts.w",
    required: ["decimalType:", "logicalType:"],
    forbidden: [
      /\b(?:let|var|const)\s+type\b/g,
      /\b(?:take|copy|ref|view|shared|weak)\s+type\b/g,
      /\bnamed type\b/g,
    ],
  },
]

for (const rule of sourceRules) {
  const source = fs.readFileSync(path.join(root, rule.path), "utf8")
  for (const required of rule.required) assert.ok(source.includes(required), `${rule.path} lost ${required}`)
  assert.deepEqual(reservedTypeUses(source), [], `${rule.path} reintroduced type as a parameter or call label`)
  for (const forbidden of rule.forbidden) {
    forbidden.lastIndex = 0
    assert.equal(forbidden.test(source), false, `${rule.path} reintroduced reserved type surface: ${forbidden}`)
  }
}

console.log("maintained parse contract: reference/std gates integrated; reserved type labels absent; explicit contextual Web property present")
