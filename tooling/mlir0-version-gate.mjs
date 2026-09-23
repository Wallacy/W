const SEMVER_PATTERN = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/u

function assertVersion(value, label) {
  if (typeof value !== "string" || !SEMVER_PATTERN.test(value)) {
    throw new Error(`${label} must be an exact stable X.Y.Z version`)
  }
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/gu, "\\$&")
}

function exactVersionSource(value) {
  return `(?<![0-9A-Za-z.+-])${escapeRegExp(value)}(?![0-9A-Za-z.+-])`
}

export function mlir0VersionRequirement({ pinnedVersion,
  candidateVersion = undefined, developmentPatchCompatibility = true }) {
  assertVersion(pinnedVersion, "pinned MLIR0 toolchain version")
  if (candidateVersion !== undefined)
    assertVersion(candidateVersion, "MLIR0 acceptance candidate")

  if (candidateVersion !== undefined) {
    return {
      pattern: new RegExp(exactVersionSource(candidateVersion), "u"),
      description: `exact candidate ${candidateVersion}`,
    }
  }

  if (!developmentPatchCompatibility) {
    return {
      pattern: new RegExp(exactVersionSource(pinnedVersion), "u"),
      description: pinnedVersion,
    }
  }

  const [major, minor] = pinnedVersion.split(".")
  return {
    pattern: new RegExp(
      `(?<![0-9A-Za-z.+-])${major}\\.${minor}\\.\\d+(?![0-9A-Za-z.+-])`, "u"),
    description: `the ${major}.${minor}.x development line`,
  }
}
