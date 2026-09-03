import { readdir, readFile } from "node:fs/promises"
import { dirname, join, relative, resolve } from "node:path"
import { fileURLToPath } from "node:url"

import {
  acceptsCallContract,
  acceptsCallShape,
  acceptsPipeCallContract,
  acceptsPipeCallShape,
  deriveExecutionErgonomics,
} from "./execution-ergonomics-machine.mjs"

const toolingRoot = dirname(fileURLToPath(import.meta.url))
const repositoryRoot = resolve(toolingRoot, "..")
const sourceRoots = [
  join(repositoryRoot, "reference", "last-light"),
  join(repositoryRoot, "std"),
]

async function sourceFiles(directory) {
  const entries = await readdir(directory, { withFileTypes: true })
  const files = []
  for (const entry of entries.sort((left, right) => left.name.localeCompare(right.name))) {
    const path = join(directory, entry.name)
    if (entry.isDirectory()) files.push(...await sourceFiles(path))
    else if (entry.isFile() && entry.name.endsWith(".w")) files.push(path)
  }
  return files
}

const diagnostics = []
let declarations = 0
let directCallCandidates = 0
let memberCallCandidates = 0
const files = (await Promise.all(sourceRoots.map(sourceFiles))).flat()
const analyses = []

for (const path of files) {
  const result = deriveExecutionErgonomics(await readFile(path, "utf8"))
  analyses.push({ path, result })
  declarations += result.labels.declarations.length
  directCallCandidates += result.labels.calls.filter((call) => !call.member).length
  memberCallCandidates += result.labels.calls.filter((call) => call.member).length
  for (const diagnostic of result.labels.diagnostics) {
    diagnostics.push({
      file: relative(repositoryRoot, path).replaceAll("\\", "/"),
      ...diagnostic,
    })
  }
}

const declarationsByName = new Map()
for (const { path, result } of analyses) {
  for (const declaration of result.labels.declarations) {
    const candidates = declarationsByName.get(declaration.name) ?? []
    candidates.push({ path, ...declaration })
    declarationsByName.set(declaration.name, candidates)
  }
}

for (const { path, result } of analyses) {
  const localNames = new Set(result.labels.declarations
    .filter((declaration) => declaration.scope === "module")
    .map((declaration) => declaration.name))
  for (const call of result.labels.calls.filter((candidate) => !candidate.member)) {
    if (localNames.has(call.callee)) continue
    const candidates = (declarationsByName.get(call.callee) ?? [])
      .filter((candidate) => candidate.scope === "module")
    const matchingShapes = candidates.filter((candidate) =>
      call.pipe
        ? acceptsPipeCallShape(candidate.params, call.forms)
        : acceptsCallShape(candidate.params, call.forms))
    if (matchingShapes.length === 0
      || matchingShapes.some((candidate) => candidate.boundary === "foreign")
      || matchingShapes.some((candidate) =>
        call.pipe
          ? acceptsPipeCallContract(candidate.params, call.arguments)
          : acceptsCallContract(candidate.params, call.arguments))) {
      continue
    }
    diagnostics.push({
      file: relative(repositoryRoot, path).replaceAll("\\", "/"),
      line: call.line,
      code: "W-OWNERSHIP-0017",
      declaration: call.callee,
      expectedContracts: matchingShapes.map((candidate) =>
        candidate.params.map((parameter) => parameter.contractMode)),
      suppliedOperations: call.arguments.map((argument) => argument.operation),
      reason: "imported-call-operation-does-not-match-parameter-contract",
    })
  }
}

if (diagnostics.length > 0) {
  for (const diagnostic of diagnostics) console.error(JSON.stringify(diagnostic))
  console.error(`Source call shapes: ${diagnostics.length} diagnostic(s).`)
  process.exit(1)
}

console.log(
  `Source call shapes: ${files.length} files, ${declarations} declarations, `
  + `${directCallCandidates} direct-call candidates, ${memberCallCandidates} member-call candidates, `
  + "0 diagnostics.",
)
