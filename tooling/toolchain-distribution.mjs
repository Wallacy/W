import fs from "node:fs"
import path from "node:path"

const root = path.resolve(import.meta.dirname, "..")
const manifestPath = path.join(import.meta.dirname, "toolchain-distribution.json")
const documentPath = path.join(root, "TOOLCHAIN.md")
const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"))
const errors = []

function error(message) {
  errors.push(message)
}

function exact(actual, expected, label) {
  if (JSON.stringify(actual) !== JSON.stringify(expected))
    error(`${label} must be ${JSON.stringify(expected)}`)
}

function hasAll(actual, expected, label) {
  if (!Array.isArray(actual) || expected.some((item) => !actual.includes(item)))
    error(`${label} must include ${expected.join(", ")}`)
}

function localFile(relativePath, label) {
  if (typeof relativePath !== "string" || path.isAbsolute(relativePath) ||
      relativePath.split(/[\\/]+/u).includes("..")) {
    error(`${label} must be a repository-relative path`)
    return
  }
  if (!fs.existsSync(path.join(root, relativePath))) error(`${label} is missing: ${relativePath}`)
}

function validate() {
  if (manifest.$schema !== "w-toolchain-distribution-1") error("schema is invalid")
  if (manifest.version !== 1) error("version must be 1")
  if (manifest.status !== "design-oracle-input") error("status must be design-oracle-input")
  if (manifest.decision !== "W-1533") error("decision must be W-1533")
  exact(manifest.networkBoundary, {
    compilerInvocation: "forbidden-silent-toolchain-acquisition",
    toolchainAcquisition: "explicit-opt-in-tooling-only",
    registryDependencyFetch: "separate-authorized-policy",
  }, "network boundary")

  const profiles = manifest.profileContract
  exact(profiles?.wProgram?.map((profile) => profile.id),
    ["debug", "release", "benchmark"], "W program profile ids")
  exact(profiles?.toolchain?.map((profile) => profile.id),
    ["development", "release", "benchmark", "size-experimental"], "toolchain profile ids")
  if ((profiles?.wProgram ?? []).some((profile) => Object.hasOwn(profile, "default")))
    error("W program profiles must not invent a default selection")
  const wProgramProfiles = new Map((profiles?.wProgram ?? []).map((profile) => [profile.id, profile]))
  const toolchainProfiles = new Map((profiles?.toolchain ?? []).map((profile) => [profile.id, profile]))
  if (wProgramProfiles.get("debug")?.purpose !== "iteration-and-diagnostics" ||
      wProgramProfiles.get("debug")?.status !== "existing-design-profile")
    error("W program debug profile must remain iteration-and-diagnostics")
  if (wProgramProfiles.get("release")?.purpose !== "performance-first" ||
      wProgramProfiles.get("release")?.status !== "existing-design-profile")
    error("W program release profile must remain the performance-first existing profile")
  if (wProgramProfiles.get("benchmark")?.purpose !== "reproducible-pinned" ||
      wProgramProfiles.get("benchmark")?.status !== "existing-design-profile")
    error("W program benchmark profile must remain reproducible and pinned")
  if (toolchainProfiles.get("development")?.purpose !== "toolchain-iteration-speed" ||
      toolchainProfiles.get("development")?.default !== false ||
      toolchainProfiles.get("development")?.status !== "contract-only" ||
      toolchainProfiles.get("release")?.purpose !== "performance-first" ||
      toolchainProfiles.get("release")?.default !== true ||
      toolchainProfiles.get("release")?.status !== "contract-only" ||
      toolchainProfiles.get("benchmark")?.purpose !== "reproducible-pinned" ||
      toolchainProfiles.get("benchmark")?.default !== false ||
      toolchainProfiles.get("benchmark")?.status !== "contract-only" ||
      toolchainProfiles.get("size-experimental")?.purpose !== "size-comparison-only" ||
      toolchainProfiles.get("size-experimental")?.default !== false ||
      toolchainProfiles.get("size-experimental")?.status !== "opt-in-only")
    error("toolchain profiles must separate Release, benchmark, and opt-in size comparison")
  if (profiles?.namespaceRule !==
      "W program profiles and toolchain build/distribution profiles are separate; no implicit inheritance.")
    error("profile namespaces must remain separate")
  if (profiles?.harmonization !==
      "debug is the canonical W profile; dev is an informal naming opportunity only, not an alias or syntax.")
    error("profile harmonization must remain explicit and syntax-free")

  const windowsBuilder = manifest.windowsBuilder
  if (windowsBuilder?.decision !== "W-1534" ||
      windowsBuilder?.status !== "bounded-local-evidence" ||
      windowsBuilder?.script !== "tooling/build-w-windows.mjs" ||
      windowsBuilder?.network !== "forbidden" ||
      windowsBuilder?.defaultProfile !== "release")
    error("Windows builder boundary must be the bounded W-1534 offline builder")
  exact(windowsBuilder?.profiles?.map((profile) => profile.id),
    ["development", "release", "benchmark", "size-experimental"],
    "Windows builder profile ids")
  const windowsProfiles = new Map((windowsBuilder?.profiles ?? [])
    .map((profile) => [profile.id, profile]))
  for (const [id, buildType, purpose, defaultValue] of [
    ["development", "Debug", "toolchain-iteration-and-diagnostics", false],
    ["release", "Release", "performance-first", true],
    ["benchmark", "Release", "reproducible-pinned", false],
    ["size-experimental", "MinSizeRel", "size-comparison-only", false],
  ]) {
    const profile = windowsProfiles.get(id)
    if (profile?.cmakeBuildType !== buildType || profile?.purpose !== purpose ||
        profile?.default !== defaultValue)
      error(`Windows builder profile ${id} is invalid`)
  }
  exact(windowsBuilder?.cStandard, {
    primary: "23",
    recovery: "11",
    recoveryOption: "--c11-recovery",
    implicitFallback: false,
  }, "Windows builder C standard policy")
  exact(windowsBuilder?.benchmarkRecipe, {
    requiresCleanGit: true,
    compilerFlags: ["/Brepro", "/pathmap:<workspace>=W"],
    linkerFlags: ["/Brepro"],
    probe: "required-before-build",
    headRecord: "required",
  }, "Windows benchmark recipe")
  exact(windowsBuilder?.output, {
    directory: "build/w-windows",
    entries: ["w.exe", "receipt.json"],
    install: "validated-staged-directory-rename",
    failurePreservesPrevious: true,
  }, "Windows builder output")
  exact(windowsBuilder?.receipt, {
    file: "receipt.json",
    schema: "w-seed-windows-build-receipt-1",
    status: "local-evidence-only",
    determinism: "stable-key-order-final-newline",
    claimBoundary: "not-a-package-budget-or-performance-proof",
  }, "Windows builder receipt")
  exact(windowsBuilder?.smoke, {
    runner: "staged-w.exe",
    fixtures: [
      "compiler/seed-c/fixtures/hlo0-hello.w",
      "compiler/seed-c/fixtures/restaurant-if.w",
    ],
    fixtureSha256: "required-content-sha256-read-before-staged-run",
    stdout: "exact-bytes",
    outcome: "required",
  }, "Windows builder smoke")
  if (windowsBuilder?.benchmarkDisposition !== "compiler-lifecycle")
    error("Windows builder benchmark disposition must be compiler-lifecycle")
  const opportunity = manifest.optimizationBacklog?.items?.find((item) =>
    item?.id === "hello-windows-pe-under-1kib")
  if (manifest.optimizationBacklog?.status !== "opportunity-only" ||
      opportunity?.status !== "opportunity" ||
      opportunity?.scope !== "build:w-windows@W-1532" ||
      !Number.isInteger(opportunity?.currentObservationBytes) ||
      opportunity.currentObservationBytes <= 0 ||
      !Number.isInteger(opportunity?.targetBytes) || opportunity.targetBytes <= 0 ||
      opportunity.metric !== "fileContainerBytes" ||
      opportunity.gate !== "benchmark-no-regression" ||
      opportunity.defaultAction !== "none")
    error("optimization backlog must record Hello PE size as a non-gating opportunity")

  const layers = manifest.layers
  exact(layers?.map((layer) => layer.id),
    ["development-cache", "release-builder", "end-user-package"], "layers")
  const development = layers?.[0]
  if (development?.distributed !== false || development?.networkAtRuntime !== "forbidden" ||
      development?.location !== "external-user-cache" ||
      development?.wProvenance !== "external-evaluation-only")
    error("development cache must remain external, non-distributed, and runtime-offline")
  localFile(development?.sourceManifest, "development cache sourceManifest")
  hasAll(development?.includes, ["mlir-opt", "mlir-translate", "llc", "lld-link",
    "headers", "MLIR/LLVM/LLD development static libraries", "debug-and-text-tools"], "development cache includes")

  const builder = layers?.[1]
  if (builder?.distributed !== false || builder?.networkAtBuild !== "forbidden" ||
      builder?.toolchainCopy !== false)
    error("release builder must be hermetic and must not copy the development toolchain")
  exact(builder?.route, ["verified HIR", "in-process MLIR APIs and W pass subset",
    "LLVM target machine and object", "LLD library", "target executable"],
  "release builder route")
  exact(builder?.llvmConfiguration?.targets, ["X86", "AArch64"],
    "release builder LLVM targets")
  if (builder?.llvmConfiguration?.buildType !== "Release" ||
      builder?.llvmConfiguration?.optimizationPriority !== "performance-first" ||
      builder?.llvmConfiguration?.lto !== "conditional-after-benchmark-no-regression" ||
      builder?.llvmConfiguration?.sectionGarbageCollection !== "conditional-after-benchmark-no-regression" ||
      builder?.llvmConfiguration?.deadStripping !== "conditional-after-benchmark-no-regression" ||
      builder?.llvmConfiguration?.minSizeRel !== "experimental-comparison-only" ||
      builder?.llvmConfiguration?.cliTools !== "excluded")
    error("release builder LLVM configuration is invalid")

  const packageLayer = layers?.[2]
  if (packageLayer?.distributed !== true || packageLayer?.networkAtRuntime !== "forbidden")
    error("end-user package must be distributed and runtime-offline")
  if (packageLayer?.role !== "compact-hermetic-toolchain")
    error("end-user package role must be compact-hermetic-toolchain")
  hasAll(packageLayer?.excludes, ["mlir-opt", "mlir-translate", "llc", "lld-link",
    "Clang CLI", "generic MLIR textual parser", "headers", "MLIR/LLVM/LLD development static libraries", "debug files"],
  "end-user package exclusions")
  const packPolicy = packageLayer?.targetPackPolicy
  if (packPolicy?.signed !== true || packPolicy?.versioned !== true ||
      packPolicy?.shipInStandardPackage !== true ||
      packPolicy?.separateInstallOption !== "only-after-measured-budget-failure" ||
      packPolicy?.silentDownload !== false)
    error("target pack policy must be signed, versioned, standard-package, and non-silent")

  const matrix = manifest.primaryCrossMatrix
  exact(matrix?.architectures, ["x86_64", "aarch64"], "matrix architectures")
  exact(matrix?.platforms, ["windows", "linux", "macos"], "matrix platforms")
  exact(matrix?.hosts, ["host-windows-x86_64-native", "host-linux-x86_64-native",
    "host-macos-aarch64-native"], "matrix hosts")
  exact(matrix?.targetFamilies?.map((family) => family.platform),
    ["windows", "linux", "macos"], "matrix target families")
  for (const family of matrix?.targetFamilies ?? []) {
    exact(family.triples, family.platform === "windows"
      ? ["x86_64-pc-windows-msvc", "aarch64-pc-windows-msvc"]
      : family.platform === "linux"
        ? ["x86_64-unknown-linux-gnu", "aarch64-unknown-linux-gnu"]
        : ["x86_64-apple-darwin", "aarch64-apple-darwin"],
    `${family.platform} target triples`)
  }
  const edges = matrix?.edges
  if (!Array.isArray(edges) || edges.length !== 9) error("matrix must contain nine edges")
  const edgeKeys = new Set()
  for (const edge of edges ?? []) {
    const key = `${edge.host}->${edge.target}`
    if (edgeKeys.has(key)) error(`matrix edge repeats ${key}`)
    edgeKeys.add(key)
    if (!["windows", "linux", "macos"].includes(edge.host) ||
        !["windows", "linux", "macos"].includes(edge.target) ||
        edge.status !== "future-candidate") error(`matrix edge is invalid: ${key}`)
  }
  if (edgeKeys.size !== 9 || matrix?.crossToolchainDownload !== "forbidden" ||
      matrix?.implementation !== "gap")
    error("matrix cross-compilation policy is invalid")

  if (manifest.platformRequirements?.apple !== "Apple SDK and license evidence is a blocker")
    error("Apple SDK/license blocker must remain explicit")
  if (manifest.budget?.goalCompressedMiB !== 50 ||
      manifest.budget?.provisionalGateCompressedMiB !== 64 ||
      manifest.budget?.measurement !== "required" ||
      manifest.budget?.failurePolicy !== "visible-failure-and-explicit-review" ||
      manifest.budget?.observation?.recipe !== "build:w-windows@W-1532" ||
      !Number.isInteger(manifest.budget?.observation?.wExecutableBytes) ||
      manifest.budget.observation.wExecutableBytes <= 0 ||
      manifest.budget?.observation?.wExecutableIncludesBackend !== false ||
      !Number.isInteger(manifest.budget?.observation?.helloWindowsPeBytes) ||
      manifest.budget.observation.helloWindowsPeBytes <= 0 ||
      manifest.budget?.observation?.helloWindowsPeHasCrt !== false ||
      manifest.budget?.observation?.isBudgetProof !== false)
    error("distribution budget policy is invalid")
  exact(manifest.requiredMetrics, ["compressedArtifactBytes", "installedFootprintBytes",
    "mainExecutableBytes", "perTargetPackBytes", "coldCompileStartupMs",
    "coldHelloBuildMs", "warmCompileThroughput", "toolchainStartupMs",
    "artifactRuntimeMs", "fileContainerBytes", "sectionBytes", "codeBytes",
    "importBytes", "benchmarkVsBaseline", "unexpectedDynamicDependencies",
    "sbom"], "required metrics")
  const zig = manifest.comparison?.zig
  if (zig?.official !== true || zig?.version !== "0.16" ||
      zig?.source !== "https://ziglang.org/download/0.16.0/" ||
      zig?.use !== "context-only; not a W support or performance claim")
    error("Zig comparison must remain official and context-only")
  exact(zig?.observedCompressedMiB?.["linux-macos"], [49, 55], "Zig Linux/macOS range")
  exact(zig?.observedCompressedMiB?.windows, [89, 94], "Zig Windows range")
  if (manifest.backendDirection?.releaseCoverage !==
      "integrated LLVM backend with minimal components" ||
      manifest.backendDirection?.futureNativeBackend !== "research-only fast path" ||
      manifest.backendDirection?.mlirReplacement !== false)
    error("backend direction is invalid")
  exact(manifest.implementationStatus, {
    w1533Policy: "source-backed-by-manifest-and-offline-checker",
    w1534WindowsBuilder: "bounded-local-evidence",
    releaseBuilder: "gap",
    endUserPackage: "gap",
    crossCompilation: "gap",
    budgetEvidence: "gap",
  }, "implementation status")
}

function render() {
  const matrix = manifest.primaryCrossMatrix
  const windowsBuilder = manifest.windowsBuilder
  const tick = String.fromCharCode(96)
  const edges = matrix.edges.map((edge) =>
    `| ${edge.host} | ${edge.target} | ${edge.status} |`).join("\n")
  return `# Toolchain and distribution

<!-- Generated by tooling/toolchain-distribution.mjs. Edit tooling/toolchain-distribution.json. -->

This document projects W-1533. It does not claim a released W package.

## Three layers

| Layer | Role | Distributed | Boundary |
| --- | --- | ---: | --- |
| development-cache | development and release cache | no | external, heavy, runtime-offline |
| release-builder | hermetic minimal builder | no | in-process APIs, no CLI tool copy |
| end-user-package | compact hermetic toolchain | yes | W executable and signed target packs |

The development cache may contain ${tick}mlir-opt${tick}, ${tick}mlir-translate${tick}, ${tick}llc${tick}, ${tick}lld-link${tick},
headers, MLIR/LLVM/LLD development static libraries, debug files, and text tools. It is not the W package.
The current Windows cache is described by
[tooling/mlir0-windows-toolchain.json](tooling/mlir0-windows-toolchain.json).

Network is forbidden during compiler invocation for silent toolchain acquisition.
Registry dependency fetch remains a separately authorized policy and is not silently
performed by this toolchain boundary.

## Profile boundary

W program profiles are ${tick}${manifest.profileContract.wProgram.map((profile) => profile.id).join(", ")}${tick}.
${tick}debug${tick} is for iteration and diagnostics, ${tick}release${tick} is performance-first,
and ${tick}benchmark${tick} is reproducible and pinned. Toolchain
profiles are ${tick}${manifest.profileContract.toolchain.map((profile) => profile.id).join(", ")}${tick};
they are separate from W program profiles and do not inherit them. The size
profile is opt-in and experimental only. ${tick}dev${tick} is an informal naming
opportunity only, not an alias or syntax; the canonical W profile is
${tick}debug${tick}. This manifest adds no CLI syntax.

## Bounded Windows builder (W-1534)

The current native Windows builder is bounded local evidence. It uses
${tick}${windowsBuilder.script}${tick} with ${tick}${windowsBuilder.network}${tick} network access.
The default toolchain profile is ${tick}${windowsBuilder.defaultProfile}${tick}.

| Toolchain profile | CMake build type | Purpose | Default |
| --- | --- | --- | ---: |
${windowsBuilder.profiles.map((profile) =>
    `| ${profile.id} | ${profile.cmakeBuildType} | ${profile.purpose} | ${profile.default ? "yes" : "no"} |`).join("\n")}

The primary C standard is ${tick}${windowsBuilder.cStandard.primary}${tick}.
The explicit recovery standard is ${tick}${windowsBuilder.cStandard.recovery}${tick}.
The recovery option is ${tick}${windowsBuilder.cStandard.recoveryOption}${tick}.
The builder does not select recovery implicitly.
The ${tick}benchmark${tick} profile is a constrained, probed recipe. It requires
a clean Git worktree, records HEAD, and probes
${tick}${windowsBuilder.benchmarkRecipe.compilerFlags.join(", ")}${tick} and
${tick}${windowsBuilder.benchmarkRecipe.linkerFlags.join(", ")}${tick} before the build.
This bounded evidence does not claim a reproducible binary or a double-build result.

The persistent output directory is ${tick}${windowsBuilder.output.directory}${tick}.
It contains only ${windowsBuilder.output.entries.map((entry) => `${tick}${entry}${tick}`).join(" and ")}.
The builder validates a staged directory before an atomic rename. A pre-commit
failure preserves the previous output directory.

The receipt is ${tick}${windowsBuilder.receipt.file}${tick} with schema
${tick}${windowsBuilder.receipt.schema}${tick}. It uses stable key order and a final newline.
It records local evidence only. It is not a package, budget, or performance proof.
The builder reads each fixture before execution, records its SHA-256, and runs
exact-byte ${windowsBuilder.smoke.fixtures.map((fixture) => `${tick}${fixture}${tick}`).join(" and ")}
smokes from the staged executable.

The future release builder uses this route:

${tick}verified HIR → in-process MLIR APIs/pass subset → LLVM target machine/object → LLD library → executable${tick}.

It excludes MLIR, LLVM, LLD, and Clang command-line tools from the builder output.
The future LLVM build is ${tick}Release${tick} and performance-first. LTO, section
garbage collection, and dead stripping are conditional on benchmark no-regression;
${tick}MinSizeRel${tick} is experimental comparison only. It uses only X86 and AArch64
primary targets.

The end-user package excludes ${tick}mlir-opt${tick}, ${tick}mlir-translate${tick}, ${tick}llc${tick}, ${tick}lld-link${tick},
Clang CLI, generic MLIR textual parsers, headers, MLIR/LLVM/LLD development static libraries, and debug files.
Target packs are W-signed, versioned, and included in the standard package.
A separately signed install option requires measured budget failure and explicit review.
The package never performs a silent download.

## Primary cross matrix

The policy has three native hosts, three target platform families, and nine host-to-target edges.
Each platform family has X86 and AArch64 target packs.

| Host | Target family | Status |
| --- | --- | --- |
${edges}

The matrix is a future candidate. It does not prove cross-compilation today.
The standard package must cross-compile to Windows, Linux, and macOS without downloading
a cross toolchain. SDK, import library, runtime, signing, and provenance facts remain explicit.
Apple SDK and license evidence is a blocker.

## Budget and measurement

The initial compressed package goal is ${tick}${manifest.budget.goalCompressedMiB} MiB${tick}.
The provisional host gate is ${tick}<= ${manifest.budget.provisionalGateCompressedMiB} MiB${tick}.
Failure must be visible and receive explicit review. It must not remove target packs silently.

Every release measurement records:

- compressed artifact bytes;
- installed footprint bytes;
- main executable bytes;
- bytes for each target pack;
- cold compile startup;
- cold Hello build;
- warm compile throughput;
- toolchain startup;
- artifact runtime;
- file/container, section, code, and import bytes;
- benchmark versus baseline;
- unexpected dynamic dependencies;
- SBOM.

The recipe-scoped ${tick}${manifest.budget.observation.recipe}${tick} observation records
${manifest.budget.observation.wExecutableBytes} bytes for ${tick}w.exe${tick} and
${manifest.budget.observation.helloWindowsPeBytes} file/container bytes for the Hello PE.
The Hello PE has no CRT in this observation. Section, code, and import bytes are
not measured here. These are recipe-local observations, not a portable minimum,
performance result, or package-size proof.

The optimization backlog records the Hello PE target below 1 KiB as an
${tick}${manifest.optimizationBacklog.status}${tick} opportunity. It is not a gate or a
default action; any size change requires the performance benchmark gate.

For context only, the official [Zig 0.16 download page](https://ziglang.org/download/0.16.0/)
records the comparison snapshot used by this ledger: Linux/macOS about 49–55 MiB and
Windows about 89–94 MiB. This is not a W support or performance claim.

## Backend direction and status

The release direction uses an integrated LLVM backend with minimal components.
A fast native backend remains research-only and does not replace MLIR.
W-1533 policy is source-backed by the manifest and offline checker.
The release builder, end-user package, cross-compilation, and budget evidence remain gaps.
`
}

validate()
if (errors.length > 0) {
  console.error(errors.join("\n"))
  process.exitCode = 1
} else if (process.argv.includes("--write")) {
  fs.writeFileSync(documentPath, render())
  console.log(`Toolchain distribution projection written: ${documentPath}`)
} else if (process.argv.includes("--check")) {
  const actual = fs.readFileSync(documentPath, "utf8")
  if (actual !== render()) {
    console.error("TOOLCHAIN.md is stale. Run bun tooling/toolchain-distribution.mjs --write")
    process.exitCode = 1
  } else {
    console.log("Toolchain distribution: offline manifest and projection checks passed")
  }
} else {
  console.log(render())
}
