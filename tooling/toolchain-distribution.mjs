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
    ["debug", "release"], "W program base profile ids")
  exact(profiles?.toolchain?.map((profile) => profile.id),
    ["development", "release", "benchmark", "size-experimental"], "toolchain profile ids")
  if (profiles?.wProgramDefault !== "release")
    error("release must be the default W program base profile")
  const wProgramProfiles = new Map((profiles?.wProgram ?? []).map((profile) => [profile.id, profile]))
  const toolchainProfiles = new Map((profiles?.toolchain ?? []).map((profile) => [profile.id, profile]))
  if (wProgramProfiles.get("debug")?.purpose !== "iteration-and-diagnostics" ||
      wProgramProfiles.get("debug")?.status !== "selected-base-profile")
    error("W program debug profile must remain iteration-and-diagnostics")
  exact(wProgramProfiles.get("debug")?.defaults, {
    optimize: "none",
    checks: "full",
  }, "debug base-profile defaults")
  if (wProgramProfiles.get("release")?.purpose !== "performance-first" ||
      wProgramProfiles.get("release")?.status !== "selected-base-profile")
    error("W program release profile must remain the performance-first existing profile")
  exact(wProgramProfiles.get("release")?.defaults, {
    optimize: "speed",
    checks: "safe",
  }, "release base-profile defaults")
  exact(profiles?.fieldDefaults, {
    size: "performance",
    debug: "not-requested-and-selected-independently",
    pie: "target-aware-elf-static-pie-when-supported-platform-equivalent-elsewhere",
    relro: "full-when-supported",
    hardening: "target-required",
    strip: "debug-profile-preserves-primary-symbols-release-primary-is-stripped",
    auditability: "ordinary-local-output-not-a-full-audit-package",
    wrt: "static-reachable-only-from-signed-target-pack",
    crt: "auto-declared-requirements",
    target: "explicit-product-target-no-host-inference",
    cpuPolicy: "portable-versioned-target-baseline",
    deterministicBuild: "required-contract-not-yet-implemented",
  }, "W program field defaults")
  exact(profiles?.orthogonalAxes, [
    { id: "benchmark", kind: "recipe", default: "off", rule: "pinned-reproducible-recipe-over-debug-or-release-base" },
    { id: "size", kind: "preset", default: "performance", values: ["performance", "compact"], rule: "compact-changes-unspecified-optimize-strip-and-link-defaults-explicit-fields-win" },
    { id: "proof", kind: "assurance", default: "not-requested", rule: "adds-proof-obligations-and-cannot-weaken-checks" },
    { id: "distribution", kind: "admission", default: "local-only", rule: "requires-proof-policy-closed-dependencies-signed-target-provenance-audit-package-and-receipt" },
    { id: "sanitizer", kind: "instrumentation", default: "off", rule: "instrumentation-only-with-explicit-training-runtime" },
    { id: "pgo", kind: "instrumentation", default: "off", rule: "explicit-generate-and-use-recipes-final-use-reproves-closure" },
  ], "orthogonal recipe and assurance axes")
  exact(profiles?.resolutionPrecedence, [
    "explicit-product-target-abi-and-platform-contract-bound-by-compatible-signed-target-pack",
    "selected-debug-or-release-base-profile-establishes-optimize-and-check-defaults",
    "selected-size-preset-overlays-only-unspecified-defaults-owned-by-that-preset",
    "explicit-build.w-field-values-override-profile-and-preset-defaults-within-target-constraints",
    "remaining-platform-recipe-assurance-and-instrumentation-defaults-fill-unspecified-fields",
    "required-safety-target-hardening-runtime-closure-and-admission-gates-are-non-overridable-and-conflicts-fail-closed",
  ], "profile and axis resolution precedence")
  exact(profiles?.fieldRules, {
    optimize: "debug defaults to none and release to speed; size compact may choose a smaller default, while an explicit optimize value wins",
    checks: "debug defaults to full and release to safe; proof may add obligations, never remove required safety checks",
    debug: "not requested by default and independently selectable for either base profile; distribution admission requires a complete audit package with separately inventoried debug information",
    size: "performance by default; compact overlays size-oriented defaults only where fields are unspecified; explicit fields win and safety, hardening, and runtime closure cannot be weakened",
    pie: "ELF-specific: static PIE where the selected ELF target supports it; PE/Windows resolves target hardening such as ASLR and DEP rather than ELF PIE",
    relro: "full RELRO by default on supporting ELF targets; other formats use their target hardening contract; disabling RELRO is a named non-default ELF size/hardening experiment",
    hardening: "target-required hardening is a floor, not an optimization preference",
    strip: "debug keeps symbols in the primary by default; release strips the primary by default; debug sidecars are independently selectable",
    auditability: "ordinary local outputs do not require a full audit bundle; distribution admission requires an audit package and receipt, which may inventory a stripped primary and separate sidecar",
    wrt: "static and reachability-closed from the selected signed target pack by default; shared providers require exact target, ABI, version, linkage, and digest",
    crt: "auto resolves none without a declared transitive requirement, otherwise one exact target/ABI-compatible offer or failure; C ABI and unsafe do not imply CRT",
    target: "selected explicitly from build.w and never inferred from the compiler host",
    cpuPolicy: "portable uses the versioned target baseline; explicit CPU/features are pinned recipe inputs and do not create a native-host default",
    deterministicBuild: "required as a product contract, but deterministic output and receipt generation are not implemented or evidenced yet",
    proof: "no proof claim unless an assurance policy requests one; requested proofs add obligations and cannot alter program semantics or weaken required checks",
    distribution: "local-only by default; admission requires proof policy, closed dependencies, signed target provenance, a complete audit package and receipt, and explicit release policy",
  }, "W program field precedence rules")
  if (profiles?.configurationSource !== "build.w-only")
    error("build.w must remain the only human-authored build configuration")
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
      "debug names the iteration-and-diagnostics base profile; dev is an informal naming opportunity only, not an alias or syntax.")
    error("profile harmonization must remain explicit and syntax-free")

  exact(manifest.buildReceiptContract, {
    schema: "w.build-receipt/1",
    encoding: "canonical-deterministic-cbor",
    humanAuthoredConfiguration: "build.w-only",
    recipeIdentity: {
      digest: "sha256-of-canonical-normalized-recipe-inputs",
      includes: [
        "build.w-and-resolution-digests",
        "source-and-generated-input-digests",
        "debug-or-release-base-profile-and-orthogonal-axis-selections",
        "exact-target-cpu-features-abi-and-platform-contract",
        "compiler-toolchain-sdk-and-provider-identities-and-digests",
        "runtime-closure-and-deterministic-environment",
      ],
      excludes: ["output-identity", "external-signature"],
    },
    outputIdentity: {
      digest: "sha256-of-canonical-output-inventory-and-exact-output-bytes",
      includes: ["sorted-relative-output-paths", "output-roles", "per-output-sha256"],
      excludes: ["build-receipt-itself", "external-signature"],
    },
    payload: "recipe-identity-and-output-identity-remain-distinct",
    signing: "external-dsse-signs-exact-canonical-cbor-payload",
    json: "display-projection-only-never-authoritative-or-identity-bearing",
    status: "future-contract-not-implemented",
  }, "W build receipt contract")
  exact(manifest.sizeHardeningExperiments, {
    status: "exact-reconstruction-non-ranking-not-productized",
    defaultAction: "none",
    gate: "public-recipe-and-product-identity-before-ranking-plus-independent-audit-package-and-dependency-closure-review",
    experiments: [
      {
        id: "linux-hello-784-pie-relro",
        outputBytes: 784,
        pie: true,
        relro: "full",
        change: "strip-elf-section-headers-and-nonloaded-section-data",
        classification: "exact-reconstruction-non-ranking-size-hardening-experiment",
        artifactEvidence: "ET_DYN-no-PT_INTERP-no-relocations-no-DT_NEEDED-no-undefined-dynamic-symbols-NX-stack-PT_GNU_RELRO",
        sourceToolchainAndOutputHashes: "pinned-in-reconstruction-evidence",
        publicProductRecipeIdentity: "not-defined-in-build.w",
        wBuildReceipt: "not-implemented",
        promotion: "not-a-product-recipe-or-ranking-cell-until-public-recipe-and-output-identity-exist",
      },
      {
        id: "linux-hello-720-pie-no-relro",
        outputBytes: 720,
        pie: true,
        relro: "disabled",
        change: "strip-elf-section-headers-and-nonloaded-section-data",
        additionalRequirement: "final-artifact-proves-no-interpreter-imports-or-relocations",
        classification: "exact-reconstruction-non-ranking-size-hardening-experiment",
        artifactEvidence: "ET_DYN-no-PT_INTERP-no-relocations-no-DT_NEEDED-no-undefined-dynamic-symbols-NX-stack-no-PT_GNU_RELRO",
        sourceToolchainAndOutputHashes: "pinned-in-reconstruction-evidence",
        publicProductRecipeIdentity: "not-defined-in-build.w",
        wBuildReceipt: "not-implemented",
        promotion: "not-a-product-recipe-or-ranking-cell-until-public-recipe-and-output-identity-exist",
      },
    ],
  }, "size and hardening experiments")

  const closure = manifest.runtimeClosureContract
  if (closure?.default !== "freestanding")
    error("runtime closure must default to freestanding")
  exact(closure?.axes, {
    wrt: {
      default: "static-reachability-closed",
      shared: "explicit-exact-provider-and-abi",
      none: "only-when-no-wrt-operation-is-reachable",
    },
    crt: {
      default: "auto-declared-requirements",
      noRequirement: "none",
      strictNone: "reject-any-declared-crt-requirement",
      provider: "explicit-target-abi-version-link-mode",
      auto: "one-exact-compatible-declared-offer-or-error",
      cAbiImpliesCrt: false,
      unsafeImpliesCrt: false,
      optimizerMayAuthorizeCrt: false,
    },
    targetEnvironment: "explicit-target-contract",
  }, "runtime closure selection axes")
  exact(closure?.modes?.map((mode) => mode.id),
    ["freestanding", "hosted-crt"], "runtime closure modes")
  const closureModes = new Map((closure?.modes ?? []).map((mode) => [mode.id, mode]))
  exact(closureModes.get("freestanding"), {
    id: "freestanding",
    status: "selected-default",
    explicitOptIn: false,
    allowedExternalClasses: ["wrt", "target-sdk", "explicit-provider"],
    implicitLibcCrtCompilerRt: false,
  }, "freestanding runtime closure")
  exact(closureModes.get("hosted-crt"), {
    id: "hosted-crt",
    status: "future-requirement-backed-capability",
    explicitRequirement: true,
    allowedExternalClasses: ["wrt", "target-sdk", "explicit-provider", "receipted-crt-libc-libm"],
    receiptFields: ["target", "abi", "provider", "version", "link-mode", "imports", "digest"],
  }, "hosted CRT runtime closure")
  if (closure?.profileOrthogonality !==
      "program and toolchain profiles, recipes, size presets, sanitizer and PGO lanes never widen runtime closure implicitly")
    error("runtime closure must remain orthogonal to optimization and instrumentation")
  exact(closure?.optimizerBoundary, {
    postOptIr: "external-declaration-allowlist",
    object: "undefined-symbol-allowlist-after-codegen",
    finalArtifact: "import-and-dynamic-dependency-closure",
    linkSuccessOnly: false,
    functionLocalLibcallGuard: "no-builtin-strlen-and-wcslen-on-argument-scan",
    globalSimplifyLibcallsDisabled: false,
  }, "optimizer dependency boundary")
  exact(closure?.instrumentation, {
    modes: ["none", "sanitizer", "pgo-generate", "pgo-use"],
    trainingRuntime: "explicit-build-only",
    finalPgoUseClosure: "revalidated-from-scratch",
    mayGrantHostedCrt: false,
  }, "instrumentation closure")
  exact(closure?.benchmarkLanes,
    ["freestanding", "hosted-crt", "instrumentation-only"],
    "runtime closure benchmark lanes")
  if (closure?.benchmarkPooling !== "forbidden")
    error("runtime closure benchmark lanes must not be pooled")
  exact(closure?.implementationStatus, {
    freestandingSeed: "bounded-current",
    postOptIrAllowlist: "gap",
    objectUndefinedSymbolAllowlist: "gap",
    finalDependencyValidation: "bounded-current",
    productClosureReceipt: "gap",
    benchmarkRuntimeClosureAxis: "bounded-current-recipe-class-unverified",
    unusedProcessInputElision: "gap",
    helperReachabilityPartitioning: "gap",
    wrtSharedLinkage: "gap",
    declaredCrtAutoResolution: "gap",
    hostedCrtProduct: "gap",
    pgoClosure: "gap",
  }, "runtime closure implementation status")

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
    primaryLane: "c23-msvc-preview",
    primaryMode: "/std:clatest",
    primaryDisclosure: "correctness-only; not a final C23 result",
    recovery: "11",
    recoveryLane: "c11-recovery",
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
      "compiler/seed-c/fixtures/if.w",
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
  hasAll(development?.includes, ["mlir-opt", "mlir-translate", "opt", "llc", "lld-link",
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
  hasAll(packageLayer?.excludes, ["mlir-opt", "mlir-translate", "opt", "llc", "lld-link",
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
  if (manifest.budget?.goalCompressedMiB !== 64 ||
      manifest.budget?.optimizationGoalCompressedMiB !== 50 ||
      manifest.budget?.provisionalGateCompressedMiB !== 64 ||
      manifest.budget?.priority !== "performance-first-no-size-tradeoff" ||
      manifest.budget?.measurement !== "required" ||
      manifest.budget?.failurePolicy !== "visible-failure-and-explicit-review" ||
      manifest.budget?.observation?.recipe !== "build:w-windows@W-1532" ||
      !Number.isInteger(manifest.budget?.observation?.wExecutableBytes) ||
      manifest.budget.observation.wExecutableBytes <= 0 ||
      manifest.budget?.observation?.wExecutableIncludesBackend !== false ||
      !Number.isInteger(manifest.budget?.observation?.helloWindowsPeBytes) ||
      manifest.budget.observation.helloWindowsPeBytes <= 0 ||
      !Number.isInteger(manifest.budget?.observation?.helloLinuxWslPieBytes) ||
      manifest.budget.observation.helloLinuxWslPieBytes <= 0 ||
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
  const profiles = manifest.profileContract
  const tick = String.fromCharCode(96)
  const edges = matrix.edges.map((edge) =>
    `| ${edge.host} | ${edge.target} | ${edge.status} |`).join("\n")
  const baseProfileRows = profiles.wProgram.map((profile) =>
    `| ${tick}${profile.id}${tick} | ${profile.purpose} | ${tick}${profile.defaults.optimize}${tick} | ${tick}${profile.defaults.checks}${tick} |`).join("\n")
  const fieldRows = Object.entries(profiles.fieldDefaults).map(([field, value]) =>
    `| ${tick}${field}${tick} | ${tick}${value}${tick} | ${profiles.fieldRules[field]} |`).join("\n")
  const axisRows = profiles.orthogonalAxes.map((axis) =>
    `| ${tick}${axis.id}${tick} | ${axis.kind} | ${tick}${axis.default}${tick} | ${axis.rule} |`).join("\n")
  const precedenceRows = profiles.resolutionPrecedence.map((rule) => `- ${rule}`).join("\n")
  return `# Toolchain and distribution

<!-- Generated by tooling/toolchain-distribution.mjs. Edit tooling/toolchain-distribution.json. -->

This document projects W-1533. It does not claim a released W package.

## Three layers

| Layer | Role | Distributed | Boundary |
| --- | --- | ---: | --- |
| development-cache | development and release cache | no | external, heavy, runtime-offline |
| release-builder | hermetic minimal builder | no | in-process APIs, no CLI tool copy |
| end-user-package | compact hermetic toolchain | yes | W executable and signed target packs |

The development cache may contain ${tick}mlir-opt${tick}, ${tick}mlir-translate${tick}, ${tick}opt${tick}, ${tick}llc${tick}, ${tick}lld-link${tick},
headers, MLIR/LLVM/LLD development static libraries, debug files, and text tools. It is not the W package.
The current Windows cache is described by
[tooling/mlir0-windows-toolchain.json](tooling/mlir0-windows-toolchain.json).

Network is forbidden during compiler invocation for silent toolchain acquisition.
Registry dependency fetch remains a separately authorized policy and is not silently
performed by this toolchain boundary.

## Contract boundary

Development and bootstrap may use a C23 compiler, CMake, Bun, and external
MLIR/LLVM tools. These tools belong to the development host and cache. They are
not target-machine requirements for the public package.

The public ${tick}w${tick} distribution must be self-contained. A target machine needs
none of the C23 compiler, CMake, Bun, or MLIR/LLVM command-line tools. The
package contains the W executable, signed target packs, and required runtime
components. This is a distribution contract, not a released-package claim.

The native route is ${tick}W source → verified HIR → MLIR → LLVM IR → LLVM optimization → object → link → target executable${tick}.
It never lowers W source to C. The legacy Linux ${tick}clang -x ir${tick} gate uses
Clang only as a temporary link-driver bridge for generated LLVM IR.

## Profile boundary

W programs have exactly two base profiles: ${tick}${profiles.wProgram.map((profile) => profile.id).join(", ")}${tick}.
${tick}${profiles.wProgramDefault}${tick} is the default. Benchmark is a recipe;
${tick}size${tick} is a defaults preset; proof is an assurance policy; distribution is an
admission policy; sanitizer and PGO are instrumentation lanes. None is another
base profile. Toolchain profiles are
${tick}${profiles.toolchain.map((profile) => profile.id).join(", ")}${tick}; they belong to the
toolchain builder and do not inherit from W program profiles. ${tick}debug${tick}
names the iteration-and-diagnostics base profile; ${tick}dev${tick} is only an informal naming
opportunity, not an alias or syntax. The only human-authored build configuration is ${tick}build.w${tick}.
This policy does not add compiler CLI syntax.

| W base profile | Purpose | Optimize | Checks |
| --- | --- | --- | --- |
${baseProfileRows}

The ${tick}size${tick} preset defaults to ${tick}performance${tick}. ${tick}compact${tick} overlays only the
defaults it owns, such as optimize/strip/link choices; explicit per-field values
in ${tick}build.w${tick} take precedence. Required language checks, target hardening, and
runtime closure cannot be weakened by the preset.

Output defaults and orthogonal-axis rules:

| Field | Default | Rule |
| --- | --- | --- |
${fieldRows}

| Orthogonal axis | Class | Default | Rule |
| --- | --- | --- | --- |
${axisRows}

Resolution precedence is fixed:

${precedenceRows}

The release primary is stripped for efficient execution; debug information is
an independently selected sidecar rather than a universal release dependency.
Ordinary local builds do not require the distribution audit package. Distribution
admission requires a stronger audit package and receipt. A proof policy may add
checks but never remove required language safety checks. Contradictory target,
hardening, runtime-closure, or admission requirements fail closed instead of
silently falling back.

${tick}pie${tick} is ELF-specific: static PIE applies only where the selected ELF target
supports it. PE/Windows uses its target hardening contract (for example ASLR and
DEP), not ELF PIE. Full RELRO is likewise an ELF target default where supported.
Deterministic output is required by the future product contract, but no current
compiler/build/receipt path proves it.

## Runtime closure boundary

Runtime closure is independent from program, toolchain, size, sanitizer, and
PGO modes. The default is ${tick}${manifest.runtimeClosureContract.default}${tick}:
only reachability-closed WRT code plus explicit target-SDK and provider leaves
are permitted. WRT uses reachability-closed static linkage by default; a shared
WRT requires an exact provider and ABI, and no WRT requires proof that no WRT
operation is reachable. CRT resolution defaults to declared-requirements-only
auto: an empty requirement set resolves to none, while a unique exact target/ABI
offer is required when a dependency declares CRT. ${tick}unsafe${tick} and C ABI
alone do not imply CRT. ${tick}hosted-crt${tick} is a future requirement-backed
capability, not a faster Release profile; it must bind target, ABI, provider,
version, link mode, imports, and digest in its receipt. This contract adds no
CLI spelling.

The complete native-route contract requires checking externals after LLVM
optimization, undefined symbols after object emission, and final imports or
dynamic dependencies after linking.
A successful link alone is not closure evidence. The seed process-argument
scan now uses function-local guards against LLVM synthesizing
${tick}strlen${tick}/${tick}wcslen${tick}; the global
${tick}--disable-simplify-libcalls${tick} flag is removed. This is bounded
closure protection, not proof of optimal lowering. Sanitizer and PGO-generate
runtimes are explicit build-only
dependencies; the final PGO-use product revalidates its own closure and cannot
inherit hosted authority. Freestanding, hosted-CRT, and instrumentation-only
measurements remain separate benchmark lanes.

## Canonical build receipt

The future target-product receipt uses schema ${tick}${manifest.buildReceiptContract.schema}${tick}
and ${manifest.buildReceiptContract.encoding} encoding. Its recipe identity is a digest of
normalized ${tick}build.w${tick}, source, target/CPU/ABI, toolchain/provider, and closure inputs;
its output identity is a separate digest of the sorted output inventory and
exact bytes. Each identity excludes the other, and output identity excludes
the receipt itself. External DSSE signs the exact canonical CBOR payload.
JSON is a display projection only, never the authoritative configuration,
receipt encoding, or identity input. This future product receipt does not
replace W-1534's existing local ${tick}receipt.json${tick}.

Current implementation evidence is bounded to the freestanding seed and final
PE/ELF dependency checks. Post-opt and object-level allowlists, a target-product
closure receipt, unused process-input
elision, helper partitioning, shared WRT, declared CRT auto-resolution, a
hosted-CRT product, and PGO closure remain gaps.
The executable catalog now carries a runtime-closure class as a comparability
axis, but its recipe-derived classes remain unverified until artifact-level
dependency receipts exist.

## Bounded Windows builder (W-1534)

The current native Windows builder is bounded local evidence. It uses
${tick}${windowsBuilder.script}${tick} with ${tick}${windowsBuilder.network}${tick} network access.
The default toolchain profile is ${tick}${windowsBuilder.defaultProfile}${tick}.

| Toolchain profile | CMake build type | Purpose | Default |
| --- | --- | --- | ---: |
${windowsBuilder.profiles.map((profile) =>
    `| ${profile.id} | ${profile.cmakeBuildType} | ${profile.purpose} | ${profile.default ? "yes" : "no"} |`).join("\n")}

The primary C standard request is ${tick}${windowsBuilder.cStandard.primary}${tick}.
The MSVC request lane is ${tick}${windowsBuilder.cStandard.primaryLane}${tick} via
${tick}${windowsBuilder.cStandard.primaryMode}${tick}. It is
${windowsBuilder.cStandard.primaryDisclosure}.
The explicit recovery standard is ${tick}${windowsBuilder.cStandard.recovery}${tick}
in lane ${tick}${windowsBuilder.cStandard.recoveryLane}${tick}.
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

The end-user package excludes ${tick}mlir-opt${tick}, ${tick}mlir-translate${tick}, ${tick}opt${tick}, ${tick}llc${tick}, ${tick}lld-link${tick},
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

The initial compressed package budget is ${tick}<= ${manifest.budget.goalCompressedMiB} MiB${tick}.
The ${tick}${manifest.budget.optimizationGoalCompressedMiB} MiB${tick} value is an optimization goal.
Release performance has priority over package size.
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
${manifest.budget.observation.helloWindowsPeBytes} bytes for the Windows Hello PE.
The live Linux/WSL Hello PIE is ${manifest.budget.observation.helloLinuxWslPieBytes} bytes.
The Hello PE has no CRT in this observation. Section, code, and import bytes are
not measured here. These are recipe-local observations, not a portable minimum,
performance result, or package-size proof.

The optimization backlog records the Hello PE target below 1 KiB as an
${tick}${manifest.optimizationBacklog.status}${tick} opportunity. It is not a gate or a
default action; any size change requires the performance benchmark gate.

The exact reconstructed ${manifest.sizeHardeningExperiments.experiments[0].outputBytes}-byte PIE+RELRO Hello and
${manifest.sizeHardeningExperiments.experiments[1].outputBytes}-byte no-RELRO Hello are separate, non-default
size/hardening experiments, not product recipes or ranking cells. Source,
toolchain, and output hashes are pinned in the reconstruction evidence, but no
public ${tick}build.w${tick} recipe or product receipt binds those outputs yet. Both omit ELF
section headers and non-loaded section data; the latter additionally requires
final-artifact proof of no interpreter, imports, or relocations. Neither
changes the default hardening or auditability policy or provides current
product reproducibility evidence.

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
