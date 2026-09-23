import { describe, expect, test } from "bun:test"
import { mlir0VersionRequirement } from "./mlir0-version-gate.mjs"

describe("MLIR0 toolchain version acceptance", () => {
  test("accepts only the explicit stable candidate without changing the pin", () => {
    const requirement = mlir0VersionRequirement({
      pinnedVersion: "23.1.1",
      candidateVersion: "24.1.0",
    })

    expect(requirement.description).toBe("exact candidate 24.1.0")
    expect(requirement.pattern.test("clang version 24.1.0")).toBe(true)
    expect(requirement.pattern.test("clang version 23.1.1")).toBe(false)
    expect(requirement.pattern.test("clang version 24.1.1")).toBe(false)
    expect(requirement.pattern.test("clang version 24.1.0-rc1")).toBe(false)
  })

  test("keeps the default patch-compatibility line derived from the pin", () => {
    const requirement = mlir0VersionRequirement({ pinnedVersion: "23.1.1" })

    expect(requirement.description).toBe("the 23.1.x development line")
    expect(requirement.pattern.test("Ubuntu LLVM 23.1.2")).toBe(true)
    expect(requirement.pattern.test("Ubuntu LLVM 23.1.10")).toBe(true)
    expect(requirement.pattern.test("Ubuntu LLVM 23.2.0")).toBe(false)
    expect(requirement.pattern.test("Ubuntu LLVM 24.1.0")).toBe(false)
    expect(requirement.pattern.test("Ubuntu LLVM 23.1.2-rc1")).toBe(false)
  })

  test("can still require the exact selected pin", () => {
    const requirement = mlir0VersionRequirement({
      pinnedVersion: "23.1.1",
      developmentPatchCompatibility: false,
    })

    expect(requirement.description).toBe("23.1.1")
    expect(requirement.pattern.test("Ubuntu LLVM 23.1.1")).toBe(true)
    expect(requirement.pattern.test("Ubuntu LLVM 23.1.2")).toBe(false)
  })

  test("rejects ranges, prereleases, and malformed version inputs", () => {
    for (const candidateVersion of ["24.1", "24.1.x", "24.01.0", "24.1.0-rc1"]) {
      expect(() => mlir0VersionRequirement({
        pinnedVersion: "23.1.1",
        candidateVersion,
      })).toThrow("exact stable X.Y.Z")
    }
  })
})
