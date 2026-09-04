import { readFile } from "node:fs/promises"
import { isAbsolute, relative, resolve, sep } from "node:path"
import { describe, expect, test } from "bun:test"
import {
  defaultCacheDirectory,
  installedSizeForTree,
  normalizeArchivePath,
  validateArchiveEntries,
  validateManifest,
} from "./acquire-mlir0-windows.mjs"

const root = resolve(import.meta.dir, "..")
const manifest = JSON.parse(await readFile(
  resolve(import.meta.dir, "mlir0-windows-toolchain.json"), "utf8"))

function outsideRepository(pathValue) {
  const relativePath = relative(root, resolve(pathValue))
  return isAbsolute(relativePath) || relativePath === ".." ||
    relativePath.startsWith(`..${sep}`)
}

describe("native Windows MLIR0 acquisition contract", () => {
  test("checked-in pin validates without network access", () => {
    expect(validateManifest(manifest)).toEqual([])
    expect(manifest.toolchain).toEqual({ mlir: "23.1.0", llvm: "23.1.0", lld: "23.1.0" })
    expect(manifest.tools.required).toEqual([
      "mlir-opt.exe", "mlir-translate.exe", "llc.exe", "lld-link.exe",
    ])
    expect(manifest.tools.optional).toContain("clang.exe")
    expect(manifest.tools.optional).toContain("clang-cl.exe")
    expect(manifest.tools.required).not.toContain("clang.exe")
    expect(manifest.runtimeBoundary).toMatchObject({
      distributionRole: "development-and-release-only",
      bundledWithW: false,
      extractedSizeIsWBudget: false,
      futureRuntime: "minimal-hermetic-components",
      futureCrossTargetPlatforms: ["windows", "linux", "macos"],
    })
    expect(manifest.buildBoundary).toMatchObject({
      script: "tooling/build-w-windows.mjs",
      output: "build/w-windows/w.exe",
      network: "forbidden",
      toolchainCopy: false,
      visualStudioDiscovery: "vswhere-with-explicit-VsDevCmd",
      sdkDiscovery: "explicit-kernel32-x64-probe",
    })
    expect(manifest.buildBoundary.configuration.cStandard).toBe("23")
    expect(manifest.buildBoundary.configuration.recoveryCStandard).toBe("11")
    expect(manifest.buildBoundary.configuration.makeProgram).toBe("explicit-ninja-path")
    expect(manifest.buildBoundary.configuration.cStandardPolicy)
      .toBe("C23-primary; C11-explicit-recovery-only")
    expect(manifest.buildBoundary.configuration.toolchainRoles).toEqual([
      "mlir-opt", "mlir-translate", "llc", "lld-link",
    ])
  })

  test("default cache is deterministic and outside the repository", () => {
    expect(defaultCacheDirectory()).toContain(
      "portable-mlir-toolchain\\2026.08.31\\x86_64-pc-windows-msvc",
    )
    expect(outsideRepository(defaultCacheDirectory())).toBe(true)
    expect(outsideRepository(resolve(root, "tooling"))).toBe(false)
    expect(outsideRepository(root)).toBe(false)
  })

  test("archive paths reject traversal and absolute forms", () => {
    expect(normalizeArchivePath("llvm/bin/mlir-opt.exe")).toBe(
      "llvm/bin/mlir-opt.exe",
    )
    expect(normalizeArchivePath("./llvm/bin/mlir-opt.exe")).toBe(
      "llvm/bin/mlir-opt.exe",
    )
    for (const pathValue of [
      "../outside",
      "llvm/../outside",
      "/absolute",
      "C:/absolute",
      "llvm\\bin\\tool.exe",
      "llvm//tool.exe",
      "llvm/./tool.exe",
    ]) {
      expect(() => normalizeArchivePath(pathValue)).toThrow()
    }
  })

  test("archive entries reject links, duplicates, and file parents", () => {
    expect(() => validateArchiveEntries([
      { path: "llvm/bin/tool", type: "symlink" },
    ])).toThrow()
    expect(() => validateArchiveEntries([
      { path: "llvm/bin/tool", type: "file" },
      { path: "llvm/bin/tool", type: "file" },
    ])).toThrow()
    expect(() => validateArchiveEntries([
      { path: "llvm", type: "file" },
      { path: "llvm/bin/tool", type: "file" },
    ])).toThrow()
  })

  test("installed size excludes the generated materialized manifest", () => {
    expect(installedSizeForTree([
      { path: "bin/mlir-opt.exe", stats: { size: 41 } },
      { path: "bin/clang.exe", stats: { size: 59 } },
      { path: "w-mlir0-windows-materialized.json", stats: { size: 1000 } },
    ])).toBe(100)
  })
})
