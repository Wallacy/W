import assert from "node:assert/strict"
import test from "node:test"
import fs from "node:fs"
import os from "node:os"
import path from "node:path"
import {
  corpusGuardErrors,
  digestMatches,
  duplicateValues,
  isExpectedEcho,
  symbolOccurrenceCount,
} from "./frontend-freeze-guards.mjs"

const corpus = await Bun.file(new URL("./frontend-freeze-cases.json", import.meta.url)).json()
const checker = path.resolve(import.meta.dir, "check-frontend-freeze.mjs")

function runPreflight(mutator) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), "w-fz0-mutation-"))
  try {
    const mutated = structuredClone(corpus)
    mutator(mutated)
    const corpusPath = path.join(directory, "frontend-freeze-cases.json")
    fs.writeFileSync(corpusPath, JSON.stringify(mutated, null, 2) + "\n", "utf8")
    const result = Bun.spawnSync({
      cmd: [process.execPath, checker, "--corpus", corpusPath, "--preflight-only"],
      cwd: path.resolve(import.meta.dir, ".."),
      stdout: "pipe",
      stderr: "pipe",
    })
    return {
      exitCode: result.exitCode,
      stdout: result.stdout.toString(),
      stderr: result.stderr.toString(),
    }
  } finally {
    fs.rmSync(directory, { recursive: true, force: true })
  }
}

function expectPreflightFailure(mutator, message) {
  const result = runPreflight(mutator)
  assert.notEqual(result.exitCode, 0, message + " should fail preflight")
  assert.match(result.stderr, new RegExp(message.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")), message)
}

test("frontend freeze guards reject missing and incoherent family entries", () => {
  const mutated = structuredClone(corpus)
  mutated.families.pop()
  assert.ok(corpusGuardErrors(mutated).some((error) => error.includes("exactly one entry")))

  const incoherent = structuredClone(corpus)
  incoherent.families[0].id = "FZ0-G5-not-g0"
  incoherent.families[0].construction = ""
  const errors = corpusGuardErrors(incoherent)
  assert.ok(errors.some((error) => error.includes("agree with its family")))
  assert.ok(errors.some((error) => error.includes("construction must be non-empty")))
})

test("frontend freeze guards reject stale and duplicate references", () => {
  assert.equal(digestMatches("sha256:current", "sha256:stale"), false)
  assert.equal(symbolOccurrenceCount("fixed: while\nfixed: while", "fixed: while"), 2)
  assert.equal(symbolOccurrenceCount("fixed: while", "fixed: until"), 0)
  assert.deepEqual(duplicateValues(["source:a", "source:b", "source:a"]), ["source:a"])

  const mutated = structuredClone(corpus)
  mutated.families[1].id = mutated.families[0].id
  assert.ok(corpusGuardErrors(mutated).some((error) => error.includes("duplicate family id")))
})

test("frontend freeze guards reject expected echo", () => {
  assert.equal(isExpectedEcho("canonical source", "canonical source"), true)
  assert.equal(isExpectedEcho("source mutation", "canonical source"), false)
})

test("real FZ0 preflight rejects missing and incoherent families", () => {
  expectPreflightFailure((mutated) => mutated.families.pop(), "families must contain exactly one entry")
  expectPreflightFailure((mutated) => {
    mutated.families[0].id = "FZ0-G5-not-g0"
  }, "id must agree with its family")
})

test("real FZ0 preflight rejects stale and duplicate source refs", () => {
  expectPreflightFailure((mutated) => {
    mutated.families[0].sourceRef.digest = "sha256:stale"
  }, "digest is stale")
  expectPreflightFailure((mutated) => {
    mutated.families[1].sourceRef = structuredClone(mutated.families[0].sourceRef)
  }, "duplicates source reference")
})

test("real FZ0 preflight rejects duplicate formatter and semantic refs", () => {
  expectPreflightFailure((mutated) => {
    mutated.families[1].positive.formatterCase = "F0-labeled-repeat-loop"
  }, "duplicates formatter case")
  expectPreflightFailure((mutated) => {
    mutated.families[2].positive.semanticCase = "S0-POS-pattern-capture-modality"
  }, "duplicates semantic case")
})

test("real FZ0 preflight rejects stale semantic evidence and workflow outcomes", () => {
  expectPreflightFailure((mutated) => {
    mutated.families[2].adversarial.baseline = "S0-POS-pattern-capture-modality"
  }, "adversarial baseline is stale")
  expectPreflightFailure((mutated) => {
    mutated.families[2].adversarial.failureEvidence.baselineValueDigest = "sha256:stale"
  }, "adversarial failureEvidence is stale")
  expectPreflightFailure((mutated) => {
    mutated.families[1].adversarial.workflowOutcomes[0].code = "staleWorkflowCode"
  }, "waiver outcome does not match RU0 module-run")
})
