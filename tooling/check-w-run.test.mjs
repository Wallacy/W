import {
  mkdir,
  mkdtemp,
  readFile,
  rm,
  writeFile,
} from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"
import { describe, expect, test } from "bun:test"
import { parseArguments, validateManifest } from "./check-w-run.mjs"

const root = resolve(import.meta.dir, "..")
const seedDirectory = resolve(root, "compiler", "seed-c")
const helloFixture = resolve(seedDirectory, "fixtures", "hlo0-hello.w")
const workflowPath = resolve(root, ".github", "workflows", "validate.yml")
const workflowText = await readFile(workflowPath, "utf8")
const ciManifest = JSON.parse(await readFile(
  resolve(import.meta.dir, "mlir0-ci-toolchain.json"), "utf8"))
const isWindows = process.platform === "win32"
const hasLinuxEvidenceHost = process.platform === "linux" ||
  (isWindows && Bun.which("wsl.exe") !== null)

function linuxPath(path) {
  if (!isWindows) return path
  const result = Bun.spawnSync({
    cmd: ["wsl.exe", "-d", "Ubuntu", "--", "wslpath", "-a",
      path.replaceAll("\\", "/")],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
  })
  if (result.exitCode !== 0)
    throw new Error(Buffer.from(result.stderr).toString("utf8"))
  return Buffer.from(result.stdout).toString("utf8").trim()
}

function runLinux(command, args) {
  return Bun.spawnSync({
    cmd: isWindows
      ? ["wsl.exe", "-d", "Ubuntu", "--", command, ...args]
      : [command, ...args],
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
  })
}

function output(result) {
  return `${Buffer.from(result.stdout)}\n${Buffer.from(result.stderr)}`
}

function normalizedOutput(result) {
  return output(result).replaceAll(/\s+/gu, " ").trim()
}

describe("W RUN native CI contract", () => {
  test("accepts one --ci flag and rejects every other argument", () => {
    expect(parseArguments([])).toEqual({ ci: false })
    expect(parseArguments(["--ci"])).toEqual({ ci: true })
    expect(() => parseArguments(["--ci", "--ci"])).toThrow(
      "unknown option: --ci",
    )
    expect(() => parseArguments(["--offline"])).toThrow(
      "unknown option: --offline",
    )
  })

  test("validates the separate pinned CI manifest", () => {
    expect(validateManifest(ciManifest, true)).toEqual({
      mlirOpt: "mlir-opt",
      mlirTranslate: "mlir-translate",
      llvmConfig: "llvm-config",
      llc: "llc",
      linkDriver: "/usr/bin/cc",
    })
  })

  test("rejects non-PIC objects, hidden link targets, and IR-to-C-driver recipes", () => {
    for (const mutate of [
      (manifest) => { manifest.pipeline[2].args[2] = "-relocation-model=static" },
      (manifest) => { manifest.pipeline[2].tool = "clang" },
      (manifest) => { manifest.pipeline[3].args[0] = "-no-pie" },
      (manifest) => { manifest.hostLink.targetFamily = "aarch64-linux-gnu" },
      (manifest) => { manifest.commands.linkDriver.linux = "cc" },
    ]) {
      const changed = structuredClone(ciManifest)
      mutate(changed)
      expect(() => validateManifest(changed, true)).toThrow()
    }
  })

  test("rejects a CI manifest that points at a local absolute tool", () => {
    const changed = structuredClone(ciManifest)
    changed.commands.mlirOpt.linux = "/usr/bin/mlir-opt-20"
    expect(() => validateManifest(changed, true)).toThrow(
      "toolchain command mlirOpt is not the pinned absolute command",
    )
  })

  test("wires Linux CI through the verified archive helper", () => {
    const workflow = Bun.YAML.parse(workflowText)
    const linux = workflow.jobs["native-linux"]
    expect(linux["runs-on"]).toBe("ubuntu-24.04")
    expect(linux.steps).toContainEqual({
      name: "Acquire and verify the pinned Linux MLIR toolchain",
      run: "bun tooling/acquire-mlir0-ci-linux.mjs --download",
    })
    expect(linux.steps.some((step) =>
      String(step.uses ?? "").includes("setup-mlir"))).toBe(false)
  })

  test.skipIf(!hasLinuxEvidenceHost)(
    "proves disabled Linux runner behavior and real CMake path validation",
    async () => {
      const buildDirectory = await mkdtemp(
        join(tmpdir(), "w-run-linux-config-test-"))
      const spyDirectory =
        `/tmp/w-run-linux-config-spy-${Date.now()}-${process.pid}`
      const spyPath = `${spyDirectory}/tool`
      const markerPath = `${spyDirectory}/invoked`
      try {
        expect(runLinux("mkdir", ["-p", spyDirectory]).exitCode).toBe(0)
        const createSpy = runLinux("sh", ["-c",
          `printf '%s\\n' '#!/bin/sh' 'printf invoked >> ${markerPath}' > ${spyPath}`])
        expect(createSpy.exitCode, output(createSpy)).toBe(0)
        expect(runLinux("chmod", ["700", spyPath]).exitCode).toBe(0)

        const quoteToolPath = `${spyDirectory}/tool"quote`
        const controlToolPath = `${spyDirectory}/tool\tcontrol`
        for (const destination of [quoteToolPath, controlToolPath]) {
          const link = runLinux("ln", ["-s", spyPath, destination])
          expect(link.exitCode, output(link)).toBe(0)
        }
        const missingToolPath = `${spyDirectory}/missing`
        const nonExecutablePath = `${spyDirectory}/non-executable`
        const nonExecutable = runLinux("sh", ["-c",
          `: > ${nonExecutablePath}; chmod 600 ${nonExecutablePath}`])
        expect(nonExecutable.exitCode, output(nonExecutable)).toBe(0)

        const buildPath = linuxPath(buildDirectory)
        const sourcePath = linuxPath(seedDirectory)
        const configure = [
          "-S", sourcePath, "-B", buildPath, "-G", "Ninja",
          "-DCMAKE_BUILD_TYPE=Release",
          "-DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc",
        ]
        const disabled = runLinux("cmake", [
          ...configure,
          "-DW_SEED_ENABLE_LINUX_NATIVE_RUN=OFF",
          `-DW_MLIR0_LINUX_MLIR_OPT:FILEPATH=${quoteToolPath}`,
          `-DW_MLIR0_LINUX_MLIR_TRANSLATE:FILEPATH=${controlToolPath}`,
          `-DW_MLIR0_LINUX_LLVM_CONFIG:FILEPATH=${missingToolPath}`,
          `-DW_MLIR0_LINUX_LLC:FILEPATH=${spyDirectory}`,
          `-DW_MLIR0_LINUX_LINK_DRIVER:FILEPATH=${spyPath}`,
        ])
        expect(disabled.exitCode, output(disabled)).toBe(0)

        const header = await readFile(join(
          buildDirectory, "generated", "w_seed_linux_config.h"), "utf8")
        expect(header).toContain("#define W_SEED_LINUX_NATIVE_RUN_ENABLED 0")
        expect(header).toContain("#define W_SEED_LINUX_MLIR_OPT_PATH \"\"")
        expect(header).toContain(
          "#define W_SEED_LINUX_MLIR_TRANSLATE_PATH \"\"")
        expect(header).toContain("#define W_SEED_LINUX_LLVM_CONFIG_PATH \"\"")
        expect(header).toContain("#define W_SEED_LINUX_LLC_PATH \"\"")
        expect(header).toContain("#define W_SEED_LINUX_LINK_DRIVER_PATH \"\"")

        const built = runLinux("cmake", [
          "--build", buildPath, "--target", "w", "--", "-j", "2",
        ])
        expect(built.exitCode).toBe(0)
        const binary = `${buildPath}/w`
        const execution = runLinux(binary, ["run", linuxPath(helloFixture)])
        expect(execution.exitCode).toBe(2)
        expect(Buffer.from(execution.stdout)).toEqual(Buffer.alloc(0))
        expect(Buffer.from(execution.stderr)).toEqual(Buffer.alloc(0))
        const marker = runLinux("test", ["!", "-e", markerPath])
        expect(marker.exitCode, output(marker)).toBe(0)

        const relative = runLinux("cmake", [
          ...configure,
          "-DW_SEED_ENABLE_LINUX_NATIVE_RUN=ON",
          "-DW_MLIR0_LINUX_MLIR_OPT=relative",
          "-DW_MLIR0_LINUX_MLIR_TRANSLATE:FILEPATH=/usr/bin/mlir-translate-20",
          "-DW_MLIR0_LINUX_LLVM_CONFIG:FILEPATH=/usr/bin/llvm-config-20",
          "-DW_MLIR0_LINUX_LLC:FILEPATH=/usr/bin/llc-20",
          "-DW_MLIR0_LINUX_LINK_DRIVER:FILEPATH=/usr/bin/cc",
        ])
        expect(relative.exitCode, output(relative)).not.toBe(0)
        expect(output(relative)).toContain(
          "W_MLIR0_LINUX_MLIR_OPT_RAW must be supplied as an absolute path")

        const invalidCases = [
          ["missing", missingToolPath,
            "W_MLIR0_LINUX_MLIR_OPT must be an existing absolute executable file"],
          ["directory", spyDirectory,
            "W_MLIR0_LINUX_MLIR_OPT must be an existing absolute executable file"],
          ["non-executable", nonExecutablePath,
            "W_MLIR0_LINUX_MLIR_OPT must be an existing absolute executable file"],
          ["quote", quoteToolPath,
            "W_MLIR0_LINUX_MLIR_OPT contains a character unsafe for the generated C header"],
          ["control", controlToolPath,
            "W_MLIR0_LINUX_MLIR_OPT contains a character unsafe for the generated C header"],
        ]
        for (const [label, invalidPath, diagnostic] of invalidCases) {
          const invalid = runLinux("cmake", [
            ...configure,
            `-DW_SEED_ENABLE_LINUX_NATIVE_RUN=ON`,
            `-DW_MLIR0_LINUX_MLIR_OPT:FILEPATH=${invalidPath}`,
            `-DW_MLIR0_LINUX_MLIR_TRANSLATE:FILEPATH=${spyPath}`,
            `-DW_MLIR0_LINUX_LLVM_CONFIG:FILEPATH=${spyPath}`,
            `-DW_MLIR0_LINUX_LLC:FILEPATH=${spyPath}`,
            `-DW_MLIR0_LINUX_LINK_DRIVER:FILEPATH=${spyPath}`,
          ])
          expect(invalid.exitCode, `${label}: ${output(invalid)}`).not.toBe(0)
          expect(normalizedOutput(invalid), label).toContain(
            diagnostic.replaceAll(/\s+/gu, " ").trim())
        }

        const validPaths = [
          ...configure, "-DW_SEED_ENABLE_LINUX_NATIVE_RUN=ON",
          `-DW_MLIR0_LINUX_MLIR_OPT:FILEPATH=${spyPath}`,
          `-DW_MLIR0_LINUX_MLIR_TRANSLATE:FILEPATH=${spyPath}`,
          `-DW_MLIR0_LINUX_LLVM_CONFIG:FILEPATH=${spyPath}`,
          `-DW_MLIR0_LINUX_LLC:FILEPATH=${spyPath}`,
          "-DW_MLIR0_LINUX_LINK_DRIVER:FILEPATH=/usr/bin/cc",
        ]
        for (const role of ["LLC", "LINK_DRIVER"]) {
          const invalid = runLinux("cmake", [...validPaths,
            `-DW_MLIR0_LINUX_${role}:FILEPATH=${missingToolPath}`])
          expect(invalid.exitCode, output(invalid)).not.toBe(0)
          expect(normalizedOutput(invalid)).toContain(
            `W_MLIR0_LINUX_${role} must be an existing absolute executable file`)
        }
        const targetDriver = join(buildDirectory, "target-driver")
        const targetDriverPath = linuxPath(targetDriver)
        for (const target of ["aarch64-linux-gnu", "x86_64-linux-musl",
          "x86_64-w64-mingw32", "x86_64-linux-gnux32", ""]) {
          await writeFile(targetDriver, `#!/bin/sh\nprintf '%s\\n' '${target}'\n`)
          expect(runLinux("chmod", ["700", targetDriverPath]).exitCode).toBe(0)
          const invalid = runLinux("cmake", [...validPaths,
            `-DW_MLIR0_LINUX_LINK_DRIVER:FILEPATH=${targetDriverPath}`])
          expect(invalid.exitCode, output(invalid)).not.toBe(0)
          expect(normalizedOutput(invalid)).toContain(
            "must report a native x86_64 Linux GNU target with -dumpmachine")
        }
        for (const target of ["x86_64-linux-gnu", "x86_64-unknown-linux-gnu"]) {
          await writeFile(targetDriver, `#!/bin/sh\nprintf '%s\\n' '${target}'\n`)
          const accepted = runLinux("cmake", [...validPaths,
            `-DW_MLIR0_LINUX_LINK_DRIVER:FILEPATH=${targetDriverPath}`])
          expect(accepted.exitCode, output(accepted)).toBe(0)
        }
      } finally {
        const cleanupSpy = runLinux("rm", ["-rf", "--", spyDirectory])
        expect(cleanupSpy.exitCode, output(cleanupSpy)).toBe(0)
        await rm(buildDirectory, { recursive: true, force: true })
      }
    },
    120_000,
  )
})
