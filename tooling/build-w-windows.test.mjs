import { createHash } from "node:crypto"
import { readFileSync } from "node:fs"
import {
  lstat,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  rename,
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
  cmakeProfileArguments,
  createBuildReceipt,
  parseBuildArguments,
  parseMsvcCompilerVersion,
  probeMsvcReproducibility,
  profileRecipe,
  reproducibilityRecipe,
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

function sampleReceipt(binaryText = "sample w executable", options = {}) {
  const artifact = Buffer.from(binaryText)
  const profile = options.profile ?? "release"
  const reproducibility = options.reproducibility ?? {
    required: false,
    compilerFlags: [],
    linkerFlags: [],
    pathMapping: null,
    probes: null,
  }
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
    profile,
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
    reproducibility,
  })
}

async function writeValidOutput(directory, binaryText) {
  await mkdir(directory)
  await writeFile(join(directory, "w.exe"), binaryText)
  await writeFile(join(directory, RECEIPT_NAME), serializeReceipt(sampleReceipt(binaryText)))
}

async function outputBytes(directory) {
  return {
    binary: await readFile(join(directory, "w.exe"), "utf8"),
    receipt: await readFile(join(directory, RECEIPT_NAME), "utf8"),
  }
}

async function backupDirectories(workspace) {
  return (await readdir(workspace))
    .filter((entry) => entry.startsWith(".w-windows-backup-"))
    .map((entry) => join(workspace, entry))
}

function sampleBenchmarkReproducibility() {
  const recipe = reproducibilityRecipe(join(root, "build", "benchmark-probe"))
  return {
    required: true,
    compilerFlags: recipe.normalizedCompilerFlags,
    linkerFlags: recipe.normalizedLinkerFlags,
    pathMapping: recipe.pathMapping,
    probes: { compiler: "passed", linker: "passed" },
  }
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
    const forgedText = JSON.parse(serialized)
    forgedText.windowsSdk.version = "/WX"
    expect(validateReceipt(forgedText).join(";")).toContain("receipt must not contain absolute paths")
  })

  test("builds the complete benchmark recipe and quotes CMake path flags", () => {
    const workspace = resolve(tmpdir(), "w benchmark workspace with spaces")
    const buildDirectory = join(workspace, "w benchmark build with spaces")
    const recipe = reproducibilityRecipe(buildDirectory, workspace)
    expect(recipe.compilerFlags).toEqual([
      "/options:strict",
      "/Brepro",
      `/pathmap:${resolve(buildDirectory)}=B`,
      `/pathmap:${resolve(workspace)}=W`,
    ])
    expect(recipe.normalizedCompilerFlags).toEqual([
      "/options:strict",
      "/Brepro",
      "/pathmap:<build>=B",
      "/pathmap:<workspace>=W",
    ])
    expect(recipe.linkerFlags).toEqual(["/Brepro", "/WX"])
    const args = cmakeProfileArguments(
      profileRecipe("benchmark"),
      buildDirectory,
      workspace,
      recipe,
    )
    const cFlags = args.find((argument) => argument.startsWith("-DCMAKE_C_FLAGS="))
    const linkerFlags = args.find((argument) =>
      argument.startsWith("-DCMAKE_EXE_LINKER_FLAGS="))
    expect(cFlags).toBe(
      `-DCMAKE_C_FLAGS=/options:strict /Brepro ` +
      `"/pathmap:${resolve(buildDirectory)}=B" ` +
      `"/pathmap:${resolve(workspace)}=W"`,
    )
    expect(linkerFlags).toBe("-DCMAKE_EXE_LINKER_FLAGS=/Brepro /WX")
    expect(cFlags).not.toContain(";")
    expect(cFlags.indexOf(`/pathmap:${resolve(buildDirectory)}=B`))
      .toBeLessThan(cFlags.indexOf(`/pathmap:${resolve(workspace)}=W`))
  })

  test("accepts only the complete benchmark recipe in a receipt", () => {
    const receipt = sampleReceipt("sample w executable", {
      profile: "benchmark",
      reproducibility: sampleBenchmarkReproducibility(),
    })
    expect(validateReceipt(receipt)).toEqual([])
    const incomplete = JSON.parse(JSON.stringify(receipt))
    incomplete.reproducibility.compilerFlags = [
      "/Brepro",
      "/pathmap:<workspace>=W",
    ]
    incomplete.reproducibility.linkerFlags = ["/Brepro"]
    incomplete.reproducibility.pathMapping = "workspace-source-to-W"
    expect(validateReceipt(incomplete).join(";"))
      .toContain("benchmark receipt does not prove its reproducibility recipe")
  })

  test("fails a zero-exit compiler probe that reports D9002", async () => {
    const probeDirectory = await mkdtemp(join(tmpdir(), "w repro probe with spaces-"))
    try {
      const calls = []
      await expect(probeMsvcReproducibility("unused-vsdevcmd", probeDirectory, {
        run: (command, args, options) => {
          calls.push({ command, args, options })
          return {
            exitCode: 0,
            stdout: Buffer.alloc(0),
            stderr: Buffer.from(
              command === "cl.exe"
                ? "cl : warning D9002 : ignoring unknown option '/options:strict'\n"
                : "",
            ),
          }
        },
      })).rejects.toThrow("ignored a required option")
      expect(calls).toHaveLength(1)
      expect(calls[0].args).toContain("/options:strict")
      expect(calls[0].args).toContain(`/pathmap:${resolve(probeDirectory)}=B`)
    } finally {
      await rm(probeDirectory, { recursive: true, force: true })
    }
  })

  test("fails a zero-exit linker probe that emits a warning", async () => {
    const probeDirectory = await mkdtemp(join(tmpdir(), "w repro linker probe-"))
    try {
      await expect(probeMsvcReproducibility("unused-vsdevcmd", probeDirectory, {
        run: (command) => ({
          exitCode: 0,
          stdout: Buffer.alloc(0),
          stderr: Buffer.from(command === "link.exe" ? "LINK : warning LNK9999\n" : ""),
        }),
      })).rejects.toThrow("linker probe emitted a warning")
    } finally {
      await rm(probeDirectory, { recursive: true, force: true })
    }
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

  test("installs a validated output on the first install", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-build-windows-test-"))
    try {
      const target = join(workspace, "w-windows")
      const builtBinary = join(workspace, "built-w.exe")
      const unrelated = join(workspace, "unrelated.txt")
      await writeFile(builtBinary, "first install executable")
      await writeFile(unrelated, "keep this path")
      const stage = await stageOutput({
        builtBinary,
        stageParent: workspace,
        receipt: sampleReceipt("first install executable"),
      })

      expect(await atomicInstallOutput(stage, target)).toBe(target)
      await expect(validateOutputDirectory(target)).resolves.toMatchObject({
        profile: { selected: "release", cmakeBuildType: "Release" },
      })
      expect(await readFile(join(target, "w.exe"), "utf8"))
        .toBe("first install executable")
      expect(await readFile(unrelated, "utf8")).toBe("keep this path")
      expect(await backupDirectories(workspace)).toEqual([])
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })

  test("replaces the dedicated directory with exactly w.exe and receipt", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-build-windows-test-"))
    try {
      const target = join(workspace, "w-windows")
      const builtBinary = join(workspace, "built-w.exe")
      const unrelated = join(workspace, "unrelated.txt")
      await writeValidOutput(target, "old executable")
      await writeFile(unrelated, "keep this path")
      await writeFile(builtBinary, "new executable")
      const receipt = sampleReceipt("new executable")
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
      expect(await readFile(join(target, "w.exe"), "utf8")).toBe("new executable")
      expect(await readFile(unrelated, "utf8")).toBe("keep this path")
      expect(await backupDirectories(workspace)).toEqual([])
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })

  test("restores exact old bytes when the stage rename fails", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-build-windows-test-"))
    try {
      const target = join(workspace, "w-windows")
      const builtBinary = join(workspace, "built-w.exe")
      const unrelated = join(workspace, "unrelated.txt")
      await writeValidOutput(target, "old executable")
      await writeFile(unrelated, "keep this path")
      const oldBytes = await outputBytes(target)
      await writeFile(builtBinary, "new executable")
      const stage = await stageOutput({
        builtBinary,
        stageParent: workspace,
        receipt: sampleReceipt("new executable"),
      })
      const stagedBytes = await outputBytes(stage)
      let renameCount = 0
      const fs = {
        rename: async (...args) => {
          renameCount += 1
          if (renameCount === 2) throw new Error("injected stage rename failure")
          return rename(...args)
        },
      }

      await expect(atomicInstallOutput(stage, target, { fs }))
        .rejects.toThrow("injected stage rename failure")
      expect(await outputBytes(target)).toEqual(oldBytes)
      expect(await outputBytes(stage)).toEqual(stagedBytes)
      expect(await backupDirectories(workspace)).toEqual([])
      expect(await readFile(unrelated, "utf8")).toBe("keep this path")
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })

  test("leaves exact old bytes when the first rename fails", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-build-windows-test-"))
    try {
      const target = join(workspace, "w-windows")
      const builtBinary = join(workspace, "built-w.exe")
      const unrelated = join(workspace, "unrelated.txt")
      await writeValidOutput(target, "old executable")
      await writeFile(unrelated, "keep this path")
      const oldBytes = await outputBytes(target)
      await writeFile(builtBinary, "new executable")
      const stage = await stageOutput({
        builtBinary,
        stageParent: workspace,
        receipt: sampleReceipt("new executable"),
      })
      let renameCount = 0
      const fs = {
        rename: async (...args) => {
          renameCount += 1
          if (renameCount === 1) throw new Error("injected old output rename failure")
          return rename(...args)
        },
      }

      await expect(atomicInstallOutput(stage, target, { fs }))
        .rejects.toThrow("injected old output rename failure")
      expect(await outputBytes(target)).toEqual(oldBytes)
      expect(await lstat(stage)).toBeDefined()
      expect(await backupDirectories(workspace)).toEqual([])
      expect(await readFile(unrelated, "utf8")).toBe("keep this path")
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })

  test("preserves recovery paths when old output restoration fails", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-build-windows-test-"))
    try {
      const target = join(workspace, "w-windows")
      const builtBinary = join(workspace, "built-w.exe")
      const unrelated = join(workspace, "unrelated.txt")
      await writeValidOutput(target, "old executable")
      await writeFile(unrelated, "keep this path")
      const oldBytes = await outputBytes(target)
      await writeFile(builtBinary, "new executable")
      const stage = await stageOutput({
        builtBinary,
        stageParent: workspace,
        receipt: sampleReceipt("new executable"),
      })
      const stagedBytes = await outputBytes(stage)
      let renameCount = 0
      const fs = {
        rename: async (...args) => {
          renameCount += 1
          if (renameCount >= 2) throw new Error("injected swap or restore failure")
          return rename(...args)
        },
      }

      let installError
      try {
        await atomicInstallOutput(stage, target, { fs })
      } catch (error) {
        installError = error
      }
      expect(installError).toBeInstanceOf(Error)
      const backups = await backupDirectories(workspace)
      expect(backups).toHaveLength(1)
      expect(installError.message).toContain(stage)
      expect(installError.message).toContain(backups[0])
      expect(installError.message).toContain("caller-owned stage path")
      expect(installError.message).toContain("recoverable backup retained at")
      expect(await outputBytes(stage)).toEqual(stagedBytes)
      expect(await outputBytes(backups[0])).toEqual(oldBytes)
      await expect(lstat(target)).rejects.toMatchObject({ code: "ENOENT" })
      expect(await readFile(unrelated, "utf8")).toBe("keep this path")
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })

  test("keeps new output when backup deletion partially fails", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-build-windows-test-"))
    try {
      const target = join(workspace, "w-windows")
      const builtBinary = join(workspace, "built-w.exe")
      const unrelated = join(workspace, "unrelated.txt")
      await writeValidOutput(target, "old executable")
      await writeFile(unrelated, "keep this path")
      const oldBytes = await outputBytes(target)
      await writeFile(builtBinary, "new executable")
      const stage = await stageOutput({
        builtBinary,
        stageParent: workspace,
        receipt: sampleReceipt("new executable"),
      })
      const warnings = []
      const fs = {
        rm: async (directory, options) => {
          expect(options).toEqual({ recursive: true, force: false })
          await rm(join(directory, "w.exe"), { force: false })
          throw new Error("injected partial backup cleanup failure")
        },
      }

      expect(await atomicInstallOutput(stage, target, {
        fs,
        onWarning: (message) => warnings.push(message),
      })).toBe(target)
      await expect(validateOutputDirectory(target)).resolves.toBeDefined()
      expect(await readFile(join(target, "w.exe"), "utf8")).toBe("new executable")
      const backups = await backupDirectories(workspace)
      expect(backups).toHaveLength(1)
      expect(warnings).toHaveLength(1)
      expect(warnings[0]).toContain(backups[0])
      expect(warnings[0]).toContain("injected partial backup cleanup failure")
      expect(await readFile(join(backups[0], RECEIPT_NAME), "utf8"))
        .toBe(oldBytes.receipt)
      await expect(lstat(join(backups[0], "w.exe")))
        .rejects.toMatchObject({ code: "ENOENT" })
      expect(await readFile(unrelated, "utf8")).toBe("keep this path")
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })

  test("ignores a throwing backup cleanup warning callback", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-build-windows-test-"))
    try {
      const target = join(workspace, "w-windows")
      const builtBinary = join(workspace, "built-w.exe")
      await writeValidOutput(target, "old executable")
      await writeFile(builtBinary, "new executable")
      const stage = await stageOutput({
        builtBinary,
        stageParent: workspace,
        receipt: sampleReceipt("new executable"),
      })
      const fs = {
        rm: async (directory) => {
          await rm(join(directory, "w.exe"), { force: false })
          throw new Error("injected cleanup failure")
        },
      }

      expect(await atomicInstallOutput(stage, target, {
        fs,
        onWarning: () => {
          throw new Error("injected warning callback failure")
        },
      })).toBe(target)
      await expect(validateOutputDirectory(target)).resolves.toBeDefined()
      expect(await readFile(join(target, "w.exe"), "utf8")).toBe("new executable")
      expect(await backupDirectories(workspace)).toHaveLength(1)
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
