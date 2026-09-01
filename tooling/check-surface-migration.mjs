import fs from "node:fs"
import path from "node:path"

const root = path.resolve(import.meta.dir, "..")

// Keep this audit bounded to current source/projection inputs. Historical
// ledgers, generated trees, rejected fixtures, and external comparisons have
// separate provenance rules and are intentionally not scanned here.
const activeFiles = [
  "compiler/seed-c/include/w_seed_parser.h",
  "compiler/seed-c/src/w_seed_frontend.c",
  "compiler/seed-c/src/w_seed_formatter.c",
  "compiler/seed-c/src/w_seed_lexer.c",
  "compiler/seed-c/src/w_seed_parser.c",
  "compiler/seed-c/tests/test_lexer.c",
  "compiler/seed-c/tests/test_parser.c",
  "compiler/seed-c/README.md",
  "tooling/check-seed-parser.mjs",
  "tooling/check-operator-surface.mjs",
  "tooling/formatter-cases.json",
  "tooling/formatter-diagnostics.snapshot.jsonl",
  "tooling/syntax-atlas.mjs",
  "reference/syntax-atlas/atlas-manifest.json",
  "reference/syntax-atlas/SYNTAX-COVERAGE.md",
  "tooling/boundary-effect-cases.json",
  "reference/last-light/transaction_oracle.w",
  "reference/last-light/README.md",
  "DESIGN.md",
  "DESIGN-INDEX.md",
  "STUDIES.md",
  "tooling/study-registry.json",
  "tooling/studies/gen2-stream-yield/README.md",
  "tooling/studies/qos0-scheduling-boundary/README.md",
  "tooling/studies/qos0-scheduling-boundary/cases.json",
  "tooling/studies/qos0-scheduling-boundary/study.json",
  "tooling/studies/qos0-scheduling-boundary/oracle.mjs",
  "tooling/studies/qos0-scheduling-boundary/oracle.test.mjs",
  "tooling/studies/tgm0-task-group-map-collect/README.md",
  "tooling/studies/tgm0-task-group-map-collect/study.json",
  "tooling/studies/tgm0-task-group-map-collect/oracle.test.mjs",
  "tooling/studies/tgm0-task-group-map-collect/check.mjs",
]

const banned = [
  { id: "removed CST kind", pattern: /W_SEED_CST_TRANSACTION_EXPRESSION/u },
  { id: "removed parser function", pattern: /parse_transaction_expression/u },
  { id: "legacy transaction syntax", pattern: /\btransaction\s+(?:identifier|tx)\s*=/u },
  { id: "legacy transaction contract syntax", pattern: /\btransaction\s*</u },
  { id: "active TaskGroup surface", pattern: /\bTaskGroup\b/u },
  { id: "unimplemented seed pipe claim", pattern: /(?:seed parser|pipe).*(?:does not|not|não).*(?:lower|implement|parse|reconhec)/iu },
  { id: "old formatter case id", pattern: /F0-structured-transaction/u },
  { id: "superseded formatter decision", pattern: /W-688/u },
  { id: "reduced tasks contract", pattern: /pipeline<tasks:\s*\.parallel\s*>/u },
]

// Exceptions are only rejected witnesses or explicit historical statements.
const allowlisted = new Map([
  ["compiler/seed-c/README.md", { legacy: /legada|não produz/u }],
  ["compiler/seed-c/tests/test_parser.c", { legacy: /legacy|transaction tx=provider/u }],
  ["tooling/check-seed-parser.mjs", { legacy: /legacy-transaction-rejected/u }],
  ["DESIGN.md", { "active TaskGroup surface": /proveniência histórica/u }],
])

function context(lines, index) {
  return lines.slice(Math.max(0, index - 2), Math.min(lines.length, index + 3)).join("\n")
}

const failures = []
const texts = new Map()
for (const relative of activeFiles) {
  const file = path.join(root, relative)
  if (!fs.existsSync(file)) {
    failures.push(`${relative}: active migration input is missing`)
    continue
  }
  const text = fs.readFileSync(file, "utf8")
  texts.set(relative, text)
  const lines = text.split(/\r?\n/u)
  for (const [lineIndex, line] of lines.entries()) {
    for (const rule of banned) {
      if (!rule.pattern.test(line)) continue
      const key = rule.id === "legacy transaction syntax" ||
        rule.id === "legacy transaction contract syntax" ? "legacy" : rule.id
      if (allowlisted.get(relative)?.[key]?.test(context(lines, lineIndex))) continue
      failures.push(`${relative}:${lineIndex + 1}: active surface contains ${rule.id}`)
    }
  }
}

function requireText(invariants, sourceMap, relative, needle, label = needle) {
  if (!sourceMap.get(relative)?.includes(needle)) {
    invariants.push(`${relative}: missing bounded invariant ${label}`)
  }
}

function requirePattern(invariants, sourceMap, relative, pattern, label) {
  if (!pattern.test(sourceMap.get(relative) ?? "")) {
    invariants.push(`${relative}: missing bounded invariant ${label}`)
  }
}

function positiveInvariants(sourceMap) {
  const invariants = []
  requireText(invariants, sourceMap, "compiler/seed-c/src/w_seed_lexer.c", '"|>"', "lexer token |>")
  requireText(invariants, sourceMap, "compiler/seed-c/tests/test_lexer.c", "|>", "lexer |> witness")
  requireText(invariants, sourceMap, "compiler/seed-c/include/w_seed_parser.h",
    "W_SEED_CST_PIPELINE_EXPRESSION", "PIPELINE CST declaration")
  requirePattern(invariants, sourceMap, "compiler/seed-c/src/w_seed_parser.c",
    /\{"\|>",\s*2,\s*false\}/u, "parser |> precedence 2, left associative")
  requireText(invariants, sourceMap, "compiler/seed-c/src/w_seed_parser.c",
    "W_SEED_CST_PIPELINE_EXPRESSION", "PIPELINE CST parser owner")
  requireText(invariants, sourceMap, "compiler/seed-c/src/w_seed_parser.c",
    "current_is_text(parser, \"#\")", "postfix facet marker")
  requireText(invariants, sourceMap, "compiler/seed-c/src/w_seed_parser.c",
    "parse_contract_envelope(parser, parser->last_token_end, false)",
    "attached contextual contract")
  requireText(invariants, sourceMap, "compiler/seed-c/tests/test_parser.c",
    "value#core.version", "qualified facet witness")
  requireText(invariants, sourceMap, "compiler/seed-c/tests/test_parser.c",
    '#\\"raw # path\\"#', "raw-string facet noncollision witness")

  const taskWitness = "pipeline<tasks:.parallel<.compute>,limit:16,ordering:.input,errors:.collect> each item in source{commit item}"
  requireText(invariants, sourceMap, "compiler/seed-c/tests/test_parser.c", taskWitness,
    "canonical tasks witness bytes")
  requireText(invariants, sourceMap, "tooling/check-seed-parser.mjs", taskWitness,
    "canonical tasks checker bytes")
  requireText(invariants, sourceMap, "tooling/check-seed-parser.mjs",
    "pipeline-dependent-block", "dependent pipeline witness")
  requireText(invariants, sourceMap, "tooling/check-seed-parser.mjs",
    "pipeline-short-chain", "short-chain pipeline witness")
  requireText(invariants, sourceMap, "tooling/check-seed-parser.mjs",
    "pipeline-transaction", "transaction pipeline witness")
  requireText(invariants, sourceMap, "compiler/seed-c/README.md",
    "pipeline<tasks: ...>", "current tasks documentation")
  requireText(invariants, sourceMap, "compiler/seed-c/README.md",
    "pipeline<transaction: { ... }>", "current transaction documentation")

  requireText(invariants, sourceMap, "compiler/seed-c/src/w_seed_frontend.c",
    "first_direct_kind(doc, expression_node, W_SEED_CST_PIPELINE_EXPRESSION)",
    "frontend PIPELINE recognition")
  requireText(invariants, sourceMap, "compiler/seed-c/src/w_seed_frontend.c",
    "W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE", "frontend unsupported fact")
  requireText(invariants, sourceMap, "compiler/seed-c/src/w_seed_formatter.c",
    'token_text(writer, previous, "pipeline")', "formatter pipeline head")
  requireText(invariants, sourceMap, "tooling/formatter-cases.json",
    '"id": "F0-structured-pipeline-transaction"', "formatter W-1511 case id")
  requireText(invariants, sourceMap, "tooling/formatter-cases.json",
    '"id": "F0-facet-path-spacing"', "formatter W-1509 facet case id")
  requireText(invariants, sourceMap, "tooling/formatter-cases.json",
    '"id": "F0-pipe-chain-spacing"', "formatter W-1510 pipe case id")
  requireText(invariants, sourceMap, "tooling/formatter-cases.json",
    '"id": "F0-structured-pipeline-tasks"', "formatter W-1511 tasks case id")
  requireText(invariants, sourceMap, "tooling/formatter-cases.json",
    '"decisions": ["W-705", "W-708", "W-775", "W-1511"]',
    "formatter current decisions")
  requireText(invariants, sourceMap, "tooling/formatter-cases.json",
    "pipeline<transaction:", "formatter current pipeline spelling")
  requireText(invariants, sourceMap, "tooling/formatter-cases.json",
    "attitude.yaw#degrees.reset()", "formatter canonical facet postfix spelling")
  requireText(invariants, sourceMap, "tooling/formatter-cases.json",
    "source |> first() |> second()", "formatter pipe chain spelling")
  requireText(invariants, sourceMap, "tooling/formatter-cases.json",
    "pipeline<tasks: .parallel<.compute>, limit: 16, ordering: .input, errors: .collect>",
    "formatter canonical tasks spelling")
  requireText(invariants, sourceMap, "tooling/formatter-diagnostics.snapshot.jsonl",
    "F0-structured-pipeline-transaction.w", "formatter snapshot source path")

  requireText(invariants, sourceMap, "tooling/check-operator-surface.mjs",
    "const parserInventory =", "operator parser inventory")
  requireText(invariants, sourceMap, "tooling/check-operator-surface.mjs",
    '...assignment, "|>"', "operator pipe inventory")
  requireText(invariants, sourceMap, "tooling/check-operator-surface.mjs",
    "new Map([...parserTable.entries()].map(([form, entry]) => [form, entry.precedence]))",
    "operator parser precedence derivation")
  requireText(invariants, sourceMap, "tooling/check-operator-surface.mjs",
    'parserValues.set("is", 10)', "operator relation precedence")
  requireText(invariants, sourceMap, "tooling/check-operator-surface.mjs",
    'parserValues.set("in", 10)', "operator membership precedence")
  requireText(invariants, sourceMap, "tooling/studies/qos0-scheduling-boundary/study.json",
    "pipeline<tasks: .parallel<.compute>, ...>", "QOS0 current facet spelling")
  if (sourceMap.get("tooling/check-operator-surface.mjs")?.includes('parserValues.set("|>"')) {
    invariants.push("tooling/check-operator-surface.mjs: pipe precedence must come from parser table")
  }
  return invariants
}

failures.push(...positiveInvariants(texts))

function assertMutationFails(label, relative, mutate) {
  const mutated = new Map(texts)
  const original = mutated.get(relative)
  if (original === undefined) {
    failures.push(`mutation ${label}: source input is missing`)
    return
  }
  mutated.set(relative, mutate(original))
  if (positiveInvariants(mutated).length === 0) {
    failures.push(`mutation ${label}: positive invariant did not detect regression`)
  }
}

// These mutations exist only in memory. They prove that the positive checks
// fail for loss of each critical migrated family, instead of passing vacuously.
assertMutationFails("lexer drops pipe token", "compiler/seed-c/src/w_seed_lexer.c",
  (text) => text.replace('"|>"', '"|~"'))
assertMutationFails("parser pipe precedence drifts", "compiler/seed-c/src/w_seed_parser.c",
  (text) => text.replace('{"|>", 2, false}', '{"|>", 3, false}'))
assertMutationFails("PIPELINE CST regresses", "compiler/seed-c/src/w_seed_parser.c",
  (text) => text.replaceAll("W_SEED_CST_PIPELINE_EXPRESSION", "W_SEED_CST_TRANSACTION_EXPRESSION"))
assertMutationFails("frontend loses unsupported fact", "compiler/seed-c/src/w_seed_frontend.c",
  (text) => text.replaceAll("W_SEED_FRONTEND_FACT_UNSUPPORTED_NODE", "W_SEED_FRONTEND_FACT_UNSUPPORTED_EXPRESSION"))
assertMutationFails("formatter id regresses", "tooling/formatter-cases.json",
  (text) => text.replace("F0-structured-pipeline-transaction", "F0-structured-transaction"))
assertMutationFails("formatter facet spacing regresses", "tooling/formatter-cases.json",
  (text) => text.replaceAll("attitude.yaw#degrees.reset()", "attitude.yaw # degrees.reset()"))
assertMutationFails("tasks witness loses contract fields", "compiler/seed-c/tests/test_parser.c",
  (text) => text.replace("errors:.collect", "errors"))

if (failures.length > 0) {
  console.error(failures.join("\n"))
  process.exitCode = 1
} else {
  console.log(`Surface migration: ${activeFiles.length} bounded current inputs checked; positive invariants and mutation guards passed.`)
}
