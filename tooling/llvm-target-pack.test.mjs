import { afterEach, describe, expect, test } from "bun:test";
import { mkdtemp, mkdir, readFile, rm, symlink, writeFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { tmpdir } from "node:os";
import { join } from "node:path";
import {
  cmakeConfigureArguments,
  createSha256Inventory,
  validateInputs,
  verifyBuiltTargets,
  writeDeterministicTar,
} from "./llvm-target-pack.mjs";

const manifest = JSON.parse(await readFile(new URL("./llvm-target-pack.json", import.meta.url), "utf8"));
const workflow = await readFile(new URL("../.github/workflows/build-llvm-target-pack.yml", import.meta.url), "utf8");

const temporaryDirectories = [];

afterEach(async () => {
  await Promise.all(temporaryDirectories.splice(0).map((path) => rm(path, { recursive: true, force: true })));
});

async function temporaryDirectory() {
  const path = await mkdtemp(join(tmpdir(), "w-llvm-target-pack-test-"));
  temporaryDirectories.push(path);
  return path;
}

describe("LLVM target-pack bootstrap contract", () => {
  test("requires a stable upstream tag, full commit, source digest, and known host", () => {
    const valid = {
      host: "linux-x86_64",
      tag: "llvmorg-23.1.1",
      commit: "6dfe1677ab8dffbc6ec13d53a1e0215d75147689",
      sourceSha256: "a".repeat(64),
    };
    expect(validateInputs(valid).version).toBe("23.1.1");
    expect(() => validateInputs({ ...valid, host: "linux-arm64" })).toThrow("Unsupported host");
    expect(() => validateInputs({ ...valid, tag: "llvmorg-23.1.1-rc1" })).toThrow("exact stable");
    expect(() => validateInputs({ ...valid, commit: "deadbeef" })).toThrow("full hexadecimal");
    expect(() => validateInputs({ ...valid, sourceSha256: "not-a-digest" })).toThrow("64 hexadecimal");
  });

  test("selects the runner-specific target set and focused build distribution", () => {
    const macosHost = validateInputs({
      host: "macos-arm64",
      tag: "llvmorg-23.1.1",
      commit: "a".repeat(40),
      sourceSha256: "b".repeat(64),
    }).host;
    expect(macosHost.targets).toEqual(["X86", "AArch64", "ARM", "RISCV", "WebAssembly"]);
    expect(macosHost.omittedForRunnerCapacity).toEqual(["NVPTX", "AMDGPU"]);
    const windowsHost = validateInputs({ host: "windows-x86_64", tag: "llvmorg-23.1.1", commit: "a".repeat(40), sourceSha256: "b".repeat(64) }).host;
    const windowsArgs = cmakeConfigureArguments({
      sourceDir: "source",
      buildDir: "build",
      installDir: "stage",
      host: windowsHost,
    });
    expect(windowsHost.generator).toBe("Ninja");
    expect(windowsArgs).toContain("-G");
    expect(windowsArgs[windowsArgs.indexOf("-G") + 1]).toBe("Ninja");
    expect(windowsArgs.some((argument) => argument.includes("Visual Studio"))).toBe(false);
    expect(windowsArgs).toContain("-DLLVM_TARGETS_TO_BUILD=X86;AArch64;ARM;RISCV;WebAssembly;NVPTX;AMDGPU");
    expect(windowsArgs).toContain("-DLLVM_ENABLE_PROJECTS=clang;lld;mlir");
    expect(windowsArgs.some((argument) => argument.includes("clang-tools-extra"))).toBe(false);
    expect(windowsArgs).toContain(`-DLLVM_DISTRIBUTION_COMPONENTS=${[
      ...manifest.distributionComponents,
      ...manifest.developmentComponents,
    ].join(";")}`);
    expect(manifest.tools).toContain("llvm-nm");
    expect(manifest.distributionComponents).toContain("clang");
    expect(manifest.distributionComponents).toContain("lld");
    expect(manifest.distributionComponents).not.toContain("clang-cl");
    expect(manifest.distributionComponents).not.toContain("ld.lld");
    expect(manifest.distributionComponents).not.toContain("lld-link");
    expect(manifest.developmentComponents).toContain("clang-resource-headers");
    expect(manifest.developmentComponents).not.toContain("lld-cmake-exports");
  });

  test("keeps the workflow manual-only, least-privileged, and commit-pinned", () => {
    expect(workflow).toMatch(/^on:\r?$/mu);
    expect(workflow).toMatch(/^  workflow_dispatch:\r?$/mu);
    expect(workflow).not.toMatch(/^\s*(push|pull_request|schedule):/mu);
    expect(workflow).toMatch(/^permissions:\r?\n  contents: read\r?$/mu);
    expect(workflow).toMatch(/^    timeout-minutes: 360$/mu);
    for (const [host, runner] of Object.entries({
      "linux-x86_64": "ubuntu-24.04",
      "windows-x86_64": "windows-2025",
      "macos-arm64": "macos-15",
    })) {
      expect(workflow).toContain(`host: ${host}\n            runner: ${runner}`);
    }
    for (const input of ["llvm_tag", "expected_commit", "expected_source_sha256"]) {
      expect(workflow).toMatch(new RegExp(`^      ${input}:\\r?\\n        description: .+\\r?\\n        required: true\\r?\\n        type: string$`, "mu"));
    }
    expect(workflow).toContain("W_LLVM_TAG: ${{ inputs.llvm_tag }}");
    expect(workflow).toContain("W_LLVM_EXPECTED_COMMIT: ${{ inputs.expected_commit }}");
    expect(workflow).toContain("W_LLVM_EXPECTED_SOURCE_SHA256: ${{ inputs.expected_source_sha256 }}");
    const references = [...workflow.matchAll(/^\s+uses:\s+([^\s#]+)/gmu)].map((match) => match[1]);
    expect(references.length).toBeGreaterThan(0);
    for (const reference of references) expect(reference.split("@")[1]).toMatch(/^[0-9a-f]{40}$/iu);
    for (const [action, commit] of Object.entries(manifest.actionPins)) expect(workflow).toContain(`${action}@${commit}`);
  });

  test("checks the target backend list reported by llvm-config", () => {
    expect(verifyBuiltTargets("AArch64 AMDGPU ARM NVPTX RISCV WebAssembly X86", ["X86", "AArch64", "ARM", "RISCV", "WebAssembly", "NVPTX", "AMDGPU"]))
      .toEqual(["AArch64", "AMDGPU", "ARM", "NVPTX", "RISCV", "WebAssembly", "X86"]);
    expect(() => verifyBuiltTargets("X86 ARM", ["X86", "AArch64"])).toThrow("AArch64");
  });
});

describe("deterministic pack metadata", () => {
  test("hash inventory is sorted, content-addressed, and excludes itself", async () => {
    const directory = await temporaryDirectory();
    await mkdir(join(directory, "bin"));
    await writeFile(join(directory, "bin", "opt"), "tool bytes");
    await writeFile(join(directory, "receipt.json"), "{}\n");
    await writeFile(join(directory, "sha256-inventory.json"), "not-included");
    const inventory = await createSha256Inventory(directory);
    expect(inventory.map((entry) => entry.path)).toEqual(["bin/opt", "receipt.json"]);
    expect(inventory[0].sha256).toBe(createHash("sha256").update("tool bytes").digest("hex"));
  });

  test("writes byte-identical ustar archives with normalized metadata", async () => {
    const temporaryRoot = await temporaryDirectory();
    const directory = join(temporaryRoot, "stage");
    const archives = join(temporaryRoot, "archives");
    await mkdir(join(directory, "bin"), { recursive: true });
    await mkdir(archives);
    await writeFile(join(directory, "bin", "opt"), "stable payload\n");
    await writeFile(join(directory, "receipt.json"), "{\"ok\":true}\n");
    if (process.platform !== "win32") await symlink("opt", join(directory, "bin", "opt-alias"));
    const first = join(archives, "first.tar");
    const second = join(archives, "second.tar");
    await writeDeterministicTar(directory, first);
    await writeDeterministicTar(directory, second);
    expect(await readFile(first)).toEqual(await readFile(second));
  });
});
