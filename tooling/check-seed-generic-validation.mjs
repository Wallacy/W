import { mkdtemp, rm } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const executableSuffix = process.platform === "win32" ? ".exe" : ""

function fail(message) {
  throw new Error(`seed generic validation: ${message}`)
}

function run(command, args, options = {}) {
  const result = Bun.spawnSync({
    cmd: [command, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
    ...options,
  })
  if (result.exitCode !== 0) {
    const stderr = result.stderr.toString().trim()
    fail(`${command} ${args.join(" ")} failed${stderr ? `: ${stderr}` : ""}`)
  }
  return result.stdout.toString()
}

function fragment(source, startMarker, endMarker, label) {
  const start = source.indexOf(startMarker)
  const end = source.indexOf(endMarker, start + startMarker.length)
  const duplicateStart = source.indexOf(startMarker, start + 1)
  const duplicateEnd = source.indexOf(endMarker, end + 1)
  if (start < 0 || end < 0 || end <= start || duplicateStart >= 0 || duplicateEnd >= 0)
    fail(`${label} markers are missing or duplicated`)
  return source.slice(start, end)
}

function parseProbe(output) {
  const resultLine = output.split(/\r?\n/u).find((line) => line.startsWith("GENERIC_RESULT "))
  if (!resultLine) fail("probe has no GENERIC_RESULT line")
  const resultMatch = /^GENERIC_RESULT applications=(\d+) frontend=(\d+) constir=(\d+)$/u.exec(resultLine)
  if (!resultMatch) fail(`invalid result line: ${resultLine}`)
  const lines = output.split(/\r?\n/u).filter((line) => line.startsWith("GENERIC app="))
  const records = lines.map((line) => {
    const match = /^GENERIC app=(\d+) state=(\w+) failure=([a-z:-]+) diagnostic=(\d+) predicates=(\d+) receipts=(\d+) steps=(\d+)$/u.exec(line)
    if (!match) fail(`invalid application line: ${line}`)
    return {
      application: Number(match[1]), state: match[2], failure: match[3],
      diagnostic: Number(match[4]), predicates: Number(match[5]),
      receipts: Number(match[6]), steps: Number(match[7]),
    }
  })
  return {
    applications: Number(resultMatch[1]), frontend: Number(resultMatch[2]),
    constir: Number(resultMatch[3]), records,
  }
}

const domain = await Bun.file(resolve(root, "reference/last-light/domain.w")).text()
const orderId = fragment(domain, "export type OrderId = u64", "export type GuestCount", "OrderId")
const serviceStage = fragment(domain, "export enum ServiceStage {", "export alias CancelledStage", "ServiceStage")
const canMove = fragment(domain, "export const fn canMove", "export const fn isValidStagePath", "canMove")
const isValidStagePath = fragment(domain, "export const fn isValidStagePath", "export enum PartySize", "isValidStagePath")
const stagePath = fragment(domain, "export struct StagePath", "export fn standardStagePath", "StagePath")
const standardStagePath = fragment(domain, "export fn standardStagePath", "export struct Guest", "standardStagePath")
const standardPath = /StagePath<(\[[^\]]+\])>/u.exec(standardStagePath)?.[1]
if (!standardPath) fail("domain.w has no standard StagePath path")
const useSource = `struct Use {
  standard: StagePath<${standardPath}>
  empty: StagePath<[]>
  skipped: StagePath<[.accepted, .completed]>
  duplicate: StagePath<[.accepted, .reserving, .reserving]>
}
`
const witness = `${orderId}\n${serviceStage}\n${canMove}\n${isValidStagePath}\n${stagePath}\n${useSource}`

const build = await mkdtemp(join(tmpdir(), "w-seed-generic-validation-check-"))
const witnessPath = join(build, "domain-generic-witness.w")
try {
  await Bun.write(witnessPath, witness)
  run("cmake", ["-S", seedDirectory, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
  run("cmake", ["--build", build, "--target", "w_seed_generic_validation_tests", "--", "-j", "2"])
  run(join(build, `w_seed_generic_validation_tests${executableSuffix}`), [])
  const executable = join(build, `w_seed_generic_validation_tests${executableSuffix}`)
  const firstOutput = run(executable, ["--domain-witness", witnessPath])
  const secondOutput = run(executable, ["--domain-witness", witnessPath])
  if (firstOutput !== secondOutput) fail("domain witness output is not deterministic")
  const parsed = parseProbe(firstOutput)
  if (parsed.frontend !== 0 || parsed.constir !== 0 || parsed.records.length !== 4)
    fail("domain witness did not produce four clean StagePath applications")
  const expected = ["VERIFIED", "REJECTED", "REJECTED", "REJECTED"]
  for (const [index, record] of parsed.records.entries()) {
    if (record.state !== expected[index] || record.predicates !== 1 || record.receipts !== 1 ||
        record.steps === 0 || (record.state === "REJECTED" &&
          (record.failure !== "predicate:false" || record.diagnostic !== 4)) ||
        (record.state === "VERIFIED" && (record.failure !== "none" || record.diagnostic !== 0)))
      fail(`domain witness application ${index} has wrong state, facts, or counters`)
  }
} finally {
  await rm(build, { recursive: true, force: true })
}

console.log("seed generic validation: ok")
