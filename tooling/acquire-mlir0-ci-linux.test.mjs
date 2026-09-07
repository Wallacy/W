import {
  chmod, mkdir, mkdtemp, readFile, readdir, realpath, rm, symlink, writeFile,
} from "node:fs/promises"
import { tmpdir } from "node:os"
import { join } from "node:path"
import { describe, expect, test } from "bun:test"
import {
  extractVerifiedArchive, hashFile, resolveExecutable,
} from "./acquire-mlir0-ci-linux.mjs"

const linuxTest = test.skipIf(process.platform !== "linux")

function run(command, args) {
  const result = Bun.spawnSync([command, ...args])
  expect(result.exitCode, Buffer.from(result.stderr).toString()).toBe(0)
}

async function archiveFixture(workspace, linkTarget) {
  const source = join(workspace, "source")
  const bin = join(source, "bin")
  await mkdir(bin, { recursive: true })
  await mkdir(join(source, "lib"))
  await writeFile(join(source, "lib", "mlir-opt-real"), "#!/bin/sh\nexit 0\n")
  await chmod(join(source, "lib", "mlir-opt-real"), 0o755)
  await symlink(linkTarget, join(bin, "mlir-opt"))
  const archivePath = join(workspace, "archive.tar.zst")
  const rawTarPath = join(workspace, "archive.tar")
  run("tar", ["-cf", rawTarPath, "-C", source, "."])
  run("zstd", ["--quiet", rawTarPath, "-o", archivePath])
  return { archivePath, expected: await hashFile(archivePath) }
}

describe("verified Linux MLIR archive acquisition", () => {
  test("rejects wrong size or SHA-256 before archive parsing", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-acquire-digest-test-"))
    try {
      const archivePath = join(workspace, "invalid.tar.zst")
      await writeFile(archivePath, "not a Zstandard archive")
      const actual = await hashFile(archivePath)
      for (const expected of [
        { ...actual, sizeBytes: actual.sizeBytes + 1 },
        { ...actual, sha256: "0".repeat(64) },
      ]) {
        const destination = join(workspace, "destination")
        await expect(extractVerifiedArchive(archivePath, destination, expected))
          .rejects.toThrow("does not match pinned size/SHA-256")
        expect(await readdir(workspace)).toEqual(["invalid.tar.zst"])
      }
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })

  linuxTest("extracts an authenticated archive with an internal ../ tool link", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-acquire-positive-test-"))
    try {
      const { archivePath, expected } = await archiveFixture(workspace,
        "../lib/mlir-opt-real")
      const destination = join(workspace, "destination")
      expect(await extractVerifiedArchive(archivePath, destination, expected))
        .toEqual(expected)
      const tool = await resolveExecutable(join(destination, "bin", "mlir-opt"),
        destination)
      expect(tool).toBe(await realpath(join(destination, "lib", "mlir-opt-real")))
      expect(await readFile(tool, "utf8")).toBe("#!/bin/sh\nexit 0\n")
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })

  for (const target of ["../../escaped", "/tmp/escaped"]) {
    linuxTest(`rejects archive link ${target} before extraction`, async () => {
      const workspace = await mkdtemp(join(tmpdir(), "w-acquire-escape-test-"))
      try {
        const { archivePath, expected } = await archiveFixture(workspace, target)
        const destination = join(workspace, "destination")
        await expect(extractVerifiedArchive(archivePath, destination, expected))
          .rejects.toThrow(/archive link target (escapes|is absolute)/u)
        expect(await readdir(destination)).toEqual([])
      } finally {
        await rm(workspace, { recursive: true, force: true })
      }
    })
  }

  linuxTest("rejects a cyclic link in an authenticated archive", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-acquire-cycle-test-"))
    try {
      const { archivePath, expected } = await archiveFixture(workspace, "mlir-opt")
      await expect(extractVerifiedArchive(archivePath,
        join(workspace, "destination"), expected))
        .rejects.toThrow("symbolic-link cycle")
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })

  linuxTest("tool resolution rejects escaped, cyclic, and nonexecutable links", async () => {
    const workspace = await mkdtemp(join(tmpdir(), "w-acquire-tool-test-"))
    try {
      const treeRoot = join(workspace, "tree")
      await mkdir(treeRoot)
      const outside = join(workspace, "outside")
      await writeFile(outside, "#!/bin/sh\n")
      await chmod(outside, 0o755)
      await symlink("../outside", join(treeRoot, "escape"))
      await expect(resolveExecutable(join(treeRoot, "escape"), treeRoot))
        .rejects.toThrow("escapes its extraction root")
      await symlink("cycle", join(treeRoot, "cycle"))
      await expect(resolveExecutable(join(treeRoot, "cycle"), treeRoot))
        .rejects.toThrow("cannot be resolved")
      await writeFile(join(treeRoot, "noexec"), "not executable")
      await chmod(join(treeRoot, "noexec"), 0o644)
      await symlink("noexec", join(treeRoot, "link"))
      await expect(resolveExecutable(join(treeRoot, "link"), treeRoot))
        .rejects.toThrow("not an executable regular file")
    } finally {
      await rm(workspace, { recursive: true, force: true })
    }
  })
})
