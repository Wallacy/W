import { createHash } from "node:crypto"
import { readFileSync } from "node:fs"
import {
  lstat,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  rm,
  writeFile,
} from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"
import { describe, expect, test } from "bun:test"
import {
  RECEIPT_NAME,
  SMOKE_CASES,
  assertStableGitHead,
  atomicInstallOutput,
  createBuildReceipt,
  parseBuildArguments,
  parseMsvcCompilerVersion,
  profileRecipe,
  serializeReceipt,
  stageOutput,
  validateOutputDirectory,
  validateReceipt,
} from "./build-w-windows.mjs"

const root = resolve(import.meta.dir, "..")
const manifest = JSON.parse(await readFile(
  resolve(import.meta.dir, "mlir0-windows-toolchain.json"), "utf8"))

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest("hex")
}

function sampleReceipt() {
  const artifact = Buffer.from("sample w executable")
  const materializedTools = {}
  for (const name of manifest.tools.required) {
    materializedTools[name] = {
      version: manifest.tools.expectedVersion,
      sizeBytes: 17,
      sha256: "1".repeat(64),
    }
  }
  const smoke = SMOKE_CASES.map((smokeCase) => {
    const stdout = Buffer.from(smokeCase.expectedStdout, "utf8")
    return {
      id: smokeCase.id,
      fixture: smokeCase.fixture,
      fixtureSha256: sha256(readFileSync(smokeCase.absoluteFixture)),
      outcome: "pass",
      exitCode: 0,
      stdoutBytes: stdout.byteLength,
      stdoutSha256: sha256(stdout),
      stderrBytes: 0,
    }
  })
  return createBuildReceipt({
    profile: "release",
    cStandard: "11",
    gitState: { head: "a".repeat(40), dirty: false },
    manifest,
    materialized: { tools: materializedTools },
    manifestSha256: "2".repeat(64),
    sdk: { version: "10.0.26100.1" },
    compiler: {
      identity: "Microsoft Visual C++ compiler",
      executable: "cl.exe",
      version: "19.51.36256",
    },
    artifact: { sizeBytes: artifact.byteLength, sha256: sha256(artifact) },
    smoke,
    reproducibility: {
      required: false,
      compilerFlags: [],
      linkerFlags: [],
      pathMapping: null,
      probes: null,
    },
  })
}

describe("native Windows builder profiles and receipt", () => {
  test("maps the four toolchain profiles without changing W profiles", () => {
    expect(parseBuildArguments([])).toMatchObject({
      profile: "release",
      cStandard: "23",
      c11Recovery: false,
    })
    expect(profileRecipe("development").cmakeBuildType).toBe("Debug")
    expect(profileRecipe("release").cmakeBuildType).toBe("Release")
    expect(profileRecipe("benchmark").cmakeBuildType).toBe("Release")
    expect(profileRecipe("benchmark").reproducible).toBe(true)
    expect(profileRecipe("size-experimental").cmakeBuildType).toBe("MinSizeRel")
    expect(parseBuildArguments([
      "--c11-recovery", "--profile", "size-experimental",
    ])).toMatchObject({ profile: "size-experimental", cStandard: "11" })
  })

  test("rejects missing, unknown, duplicate, and joined profile options", () => {
    for (const argv of [
      ["--profile"],
      ["--profile", "--c11-recovery"],
      ["--profile", "unknown"],
      ["--profile", "release", "--profile", "debug"],
      ["--profile=release"],
      ["--c11-recovery", "--c11-recovery"],
      ["--unknown"],
    ]) {
      expect(() => parseBuildArguments(argv)).toThrow()
    }
  })

  test("parses help without selecting a build side effect", () => {
    expect(parseBuildArguments(["--help"])).toMatchObject({ help: true, profile: "release" })
    const result = Bun.spawnSync({
      cmd: ["bun", "tooling/build-w-windows.mjs", "--help"],
      cwd: root,
      stdout: "pipe",
      stderr: "pipe",
      windowsHide: true,
    })
    expect(result.exitCode).toBe(0)
    expect(Buffer.from(result.stderr).toString()).toBe("")
    expect(Buffer.from(result.stdout).toString()).toContain("default profile: release")
  })

  test("parses an MSVC identity only from a compiler version line", () => {
    expect(parseMsvcCompilerVersion(
      "Microsoft (R) C/C++ Optimizing Compiler Version 19.51.36256 for x64\n",
    )).toBe("19.51.36256")
    expect(parseMsvcCompilerVersion("c1.dll: Versão 19.51.36256.0\n"))
      .toBe("19.51.36256.0")
    expect(() => parseMsvcCompilerVersion("compiler probe failed")).toThrow()
  })

  test("rejects Git HEAD drift for every profile", () => {
    expect(() => assertStableGitHead({ head: "a" }, { head: "b" }))
      .toThrow("source HEAD changed during build")
    expect(() => assertStableGitHead({ head: "a" }, { head: "a" })).not.toThrow()
  })

  test("serializes a bounded receipt with stable key order and no absolute paths", () => {
    const receipt = sampleReceipt()
    const serialized = serializeReceipt(receipt)
    expect(serialized.endsWith("\n")).toBe(true)
    expect(serialized.indexOf('"$schema"')).toBeLessThan(serialized.indexOf('"version"'))
    expect(serializeReceipt(JSON.parse(serialized))).toBe(serialized)
    expect(validateReceipt(JSON.parse(serialized))).toEqual([])
    const forged = JSON.parse(serialized)
    forged.smoke[0].fixture = resolve(root, forged.smoke[0].fixture)
    expect(validateReceipt(forged).join(";")).toContain("absolute paths")
    const forgedFixtureHash = JSON.parse(serialized)
    forgedFixtureHash.smoke[0].fixtureSha256 = "0".repeat(64)
    expect(validateReceipt(forgedFixtureHash).join(";")).toContain("smoke outcome is invalid")
    const forgedDigest = JSON.parse(serialized)
    forgedDigest.toolchain.llvm.digest = "3".repeat(64)
    expect(validateReceipt(forgedDigest).join(";")).toContain("LLVM identity")
    const forgedRecipe = JSON.parse(serialized)
    forgedRecipe.reproducibility = {
      required: true,
      compilerFlags: ["/Brepro"],
      linkerFlags: ["/Brepro"],
      pathMapping: "workspace-source-to-W",
      probes: { compiler: "passed", linker: "passed" },
    }
    expect(validateReceipt(forgedRecipe).join(";")).toContain("nonbenchmark receipt")
    const forgedProfile = JSON.parse(serialized)
    forgedProfile.profile.selected = "toString"
    expect(validateReceipt(forgedProfile).join(";")).toContain("selected profile is invalid")
  })

  test("rejects an output stage before it can replace the previous directory", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-build-windows-test-"))
    try {
      const target = join(workspace, "w-windows")
      const stage = join(workspace, "invalid-stage")
      await mkdir(target)
      await writeFile(join(target, "w.exe"), "old executable")
      await writeFile(join(target, RECEIPT_NAME), "old receipt")
      await mkdir(stage)
      await writeFile(join(stage, "w.exe"), "new executable")
      await writeFile(join(stage, RECEIPT_NAME), serializeReceipt(sampleReceipt()))
      await writeFile(join(stage, "unexpected.tmp"), "must reject")

      await expect(atomicInstallOutput(stage, target)).rejects.toThrow()
      expect(await readFile(join(target, "w.exe"), "utf8")).toBe("old executable")
      expect(await readFile(join(target, RECEIPT_NAME), "utf8")).toBe("old receipt")
      expect(await lstat(stage)).toBeDefined()
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })

  test("replaces the dedicated directory with exactly w.exe and receipt", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-build-windows-test-"))
    try {
      const target = join(workspace, "w-windows")
      const builtBinary = join(workspace, "built-w.exe")
      await writeFile(builtBinary, "sample w executable")
      const receipt = sampleReceipt()
      const stage = await stageOutput({
        builtBinary,
        stageParent: workspace,
        receipt,
      })
      await atomicInstallOutput(stage, target)
      expect(await readdir(target)).toEqual([RECEIPT_NAME, "w.exe"])
      expect(await validateOutputDirectory(target)).toMatchObject({
        profile: { selected: "release", cmakeBuildType: "Release" },
      })
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })

  test("rejects an empty staged binary without touching the destination", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-build-windows-test-"))
    try {
      const builtBinary = join(workspace, "empty-w.exe")
      await writeFile(builtBinary, "")
      await expect(stageOutput({
        builtBinary,
        stageParent: workspace,
        receipt: sampleReceipt(),
      })).rejects.toThrow()
      expect((await readdir(workspace)).sort()).toEqual(["empty-w.exe"])
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })
})
