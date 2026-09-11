import { readFile } from "node:fs/promises"
import { isAbsolute, relative, resolve, sep } from "node:path"
import { describe, expect, test } from "bun:test"
import {
  defaultCacheDirectory,
  installedSizeForTree,
  normalizeArchivePath,
  parseArguments,
  validateArchiveEntries,
  validateManifest,
  ZSTD_WINDOW_BYTES,
  ZSTD_WINDOW_LOG,
  zstdCommandArguments,
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
    expect(manifest.toolchain).toEqual({ mlir: "23.1.1", llvm: "23.1.1", lld: "23.1.1" })
    expect(manifest.source).toEqual(expect.objectContaining({
      tag: "llvmorg-23.1.1",
      tagObject: "e7ce3600b55034ddf819638f395e3c475fad5be2",
      commit: "6dfe1677ab8dffbc6ec13d53a1e0215d75147689",
      providerBuild: "portable-mlir-toolchain",
      provenance: "not-established",
    }))
    expect(manifest.asset).toEqual(expect.objectContaining({
      provider: "portable-mlir-toolchain",
      release: "2026.09.11",
      llvmTag: "llvmorg-23.1.1",
      fileName: "llvm-mlir_llvmorg-23.1.1_x86_64-pc-windows-msvc.tar.zst",
      sizeBytes: 415415081,
      sha256: "47aba6d8e7a0cfdc60105f1869c431ee0a77185ddb54a8f87ea668ee3b268f93",
    }))
    expect(manifest.decompression).toEqual({
      format: "zstd",
      mode: "pinned-bootstrap-or-explicit-path-for-large-window",
      command: "zstd",
      args: ["-d", "-c", "--long=30", "<archive>"],
      windowLog: ZSTD_WINDOW_LOG,
      windowBytes: ZSTD_WINDOW_BYTES,
      implicitPathSearch: false,
      unboundedAlternative: false,
      bootstrap: {
        provider: "portable-mlir-toolchain",
        release: "2026.09.11",
        version: "1.5.7",
        archiveFormat: "tar.gz",
        fileName: "zstd-1.5.7_x86_64-pc-windows-msvc.tar.gz",
        url: "https://github.com/munich-quantum-software/portable-mlir-toolchain/releases/download/2026.09.11/zstd-1.5.7_x86_64-pc-windows-msvc.tar.gz",
        sizeBytes: 302543,
        sha256: "df7846b47ae5c6f6ba489b4f0599b52810423858617b32fe4868109212496545",
        entry: "zstd.exe",
      },
    })
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
      .toBe("C23-requested; MSVC-clatest-preview-correctness-only; C11-explicit-recovery-only")
    expect(manifest.buildBoundary.configuration.toolchainRoles).toEqual([
      "mlir-opt", "mlir-translate", "llc", "lld-link",
    ])
  })

  test("default cache is deterministic and outside the repository", () => {
    expect(defaultCacheDirectory()).toContain(
      "portable-mlir-toolchain\\2026.09.11\\x86_64-pc-windows-msvc",
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

  test("accepts one explicit zstd decompressor and pipes archive bytes to stdout", () => {
    expect(parseArguments([
      "--archive", "archive.tar.zst", "--destination", "cache",
      "--zstd", "C:/tools/zstd.exe",
    ])).toEqual({
      archive: "archive.tar.zst",
      destination: "cache",
      download: false,
      zstd: "C:/tools/zstd.exe",
      help: false,
    })
    expect(zstdCommandArguments("archive.tar.zst")).toEqual([
      "-d", "-c", `--long=${ZSTD_WINDOW_LOG}`, "archive.tar.zst",
    ])
    expect(ZSTD_WINDOW_BYTES).toBe(1_073_741_824)
    expect(() => parseArguments(["--zstd"])).toThrow("--zstd requires a path")
    expect(() => parseArguments([
      "--zstd", "first", "--zstd", "second",
    ])).toThrow("--zstd may be used only once")
  })
})
