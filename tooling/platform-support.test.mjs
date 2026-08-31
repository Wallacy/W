import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import {
  CROSS_COMPILATION_STATES,
  HOST_AXES,
  PLATFORM_SUPPORT_SCHEMA,
  PRIMARY_HOST_REFS,
  PRIMARY_TARGET_REFS,
  TARGET_AXES,
  loadPlatformSupport,
  renderPlatformSupport,
  validatePlatformSupport,
} from "./platform-support.mjs";

const rootDirectory = path.resolve(import.meta.dir, "..");
const source = loadPlatformSupport();

function copySource() {
  return structuredClone(source);
}

function errorsAfter(mutator) {
  const value = copySource();
  mutator(value);
  return validatePlatformSupport(value).errors;
}

function expectError(errors, text) {
  expect(errors.some((error) => error.includes(text))).toBe(true);
}

describe("platform support catalog", () => {
  test("validates the checked-in record and generated document", () => {
    const result = validatePlatformSupport(source);
    expect(result.errors).toEqual([]);
    expect(source.$schema).toBe(PLATFORM_SUPPORT_SCHEMA);
    expect(source.targets.filter((target) => target.state === "supported")).toHaveLength(0);
    expect(source.targets.filter((target) => target.state === "evidence")).toHaveLength(1);
    expect(source.compilerHosts).toHaveLength(7);
    expect(source.compilerHosts[0].axes.nativeToolchain.status).toBe("partial");
    expect(source.compilerHosts[0].blockers).toContain("nativeToolchain");
    expect(source.nativeToolchainPlans.map((plan) => plan.platform)).toEqual(["windows", "macos"]);
    expect(source.nativeToolchainPlans.map((plan) => plan.id)).toEqual([
      "plan-windows-native-llvm-pending-audit",
      "plan-macos-native-llvm-pending-audit",
    ]);
    expect(source.nativeToolchainPlans.every((plan) => plan.source.tag === "pending-audit")).toBe(true);
    expect(source.nativeToolchainPlans.every((plan) => plan.gaps.includes("repository-dependency-currency-audit"))).toBe(true);
    expect(source.nativeToolchainPlans.every((plan) => Array.isArray(plan.configuration.linkerDrivers))).toBe(true);
    expect(source.nativeToolchainPlans.every((plan) => plan.configuration.linkerDrivers.join(",") === "lld-link,ld.lld,ld64.lld")).toBe(true);
    expect(source.nativeToolchainPlans.every((plan) => plan.outputs.artifacts.includes("lld"))).toBe(true);
    expect(source.externalToolchainCandidates).toHaveLength(1);
    expect(source.externalToolchainCandidates[0].id).toBe("portable-mlir-toolchain");
    expect(source.externalToolchainCandidates[0].status).toBe("evaluation-only");
    expect(source.externalToolchainCandidates[0].publishedHostTriples).toHaveLength(6);
    expect(source.externalToolchainCandidates[0].evidenceCapabilities).toEqual([
      "per-platform-build-scripts",
      "sha256-release-assets",
      "release-attestation",
      "verified-release-commit",
    ]);
    expect(source.crossCompilation.baselineHosts).toEqual(PRIMARY_HOST_REFS);
    expect(source.crossCompilation.baselineTargets).toEqual(PRIMARY_TARGET_REFS);
    expect(source.crossCompilation.edges).toHaveLength(9);
    expect(source.crossCompilation.developmentEvidence).toHaveLength(1);
    expect(source.crossCompilation.edges.every((edge) => edge.state === CROSS_COMPILATION_STATES[0])).toBe(true);
    expect(source.crossCompilation.developmentEvidence[0].evidence.map((evidence) => evidence.role)).toEqual([
      "development",
      "toolchain",
      "build",
      "execution",
    ]);
    expect(source.crossCompilation.developmentEvidence[0].evidence.at(-1).executionMode).toBe("local");
    expect(source.crossCompilation.developmentEvidence[0].blockers).toEqual([
      "nativeHost",
      "hostEndpoint",
      "targetEndpoint",
      "toolchainSysrootLinkerPackaging",
    ]);
    expect(source.crossChecks.mlir0Toolchain.currencyStatus).toBe("update-required");
    expect(source.policy.dependencyCurrency).toEqual({
      currentEvidenceVersion: "20.1.2",
      currentEvidenceCurrencyStatus: "update-required",
      futureNativePlanPolicy: "latest-stable-exact-pin-after-currency-audit",
      pendingAuditTag: "pending-audit",
      auditBlocker: "repository-dependency-currency-audit",
    });
    expect(renderPlatformSupport(source, { root: rootDirectory })).toBe(
      fs.readFileSync(path.join(rootDirectory, "PLATFORM-SUPPORT.md"), "utf8"),
    );
  });

  test("rejects duplicate target IDs", () => {
    const errors = errorsAfter((value) => {
      value.targets[1].id = value.targets[0].id;
    });
    expectError(errors, "targets contains duplicate id");
  });

  test("rejects a supported target with a missing axis", () => {
    const errors = errorsAfter((value) => {
      const target = value.targets[0];
      target.state = "supported";
      target.verificationLevel = "experimental";
      delete target.axes.backend;
    });
    expectError(errors, "target \"target-x86_64-unknown-linux-gnu\" is missing required axis \"backend\"");
  });

  test("rejects a supported target with a partial axis", () => {
    const errors = errorsAfter((value) => {
      const target = value.targets[0];
      target.state = "supported";
      target.verificationLevel = "experimental";
      target.axes.backend.status = "partial";
      target.blockers = ["backend", ...target.blockers];
    });
    expectError(errors, "supported target \"target-x86_64-unknown-linux-gnu\" requires axis \"backend\" status pass");
  });

  test("rejects a supported pass axis with empty evidence", () => {
    const errors = errorsAfter((value) => {
      const target = value.targets[0];
      target.state = "supported";
      target.verificationLevel = "experimental";
      target.axes.backend.evidence = [];
      target.blockers = ["backend", ...target.blockers];
    });
    expectError(errors, "target \"target-x86_64-unknown-linux-gnu\" axis \"backend\" with status pass requires evidence");
  });

  test("rejects divergent target blockers", () => {
    const errors = errorsAfter((value) => {
      value.targets[1].blockers = [];
    });
    expectError(errors, "target \"target-aarch64-unknown-linux-gnu\" blockers must equal non-pass axes");
  });

  test("rejects triple-only target evidence", () => {
    const errors = errorsAfter((value) => {
      delete value.targets[0].axes;
    });
    expectError(errors, "LLVM triple alone is not support evidence");
  });

  test("rejects WSL marked as native Windows", () => {
    const errors = errorsAfter((value) => {
      value.compilerHosts[0].nativeForOuterHost = true;
    });
    expectError(errors, "WSL is never native for a Windows outer host");
  });

  test("rejects a non-native composite host with a pass native toolchain", () => {
    const errors = errorsAfter((value) => {
      value.compilerHosts[0].axes.nativeToolchain.status = "pass";
    });
    expectError(errors, "current non-native composite compiler host nativeToolchain must not have status pass");
  });

  test("rejects host and target row conflation", () => {
    const errors = errorsAfter((value) => {
      value.compilerHosts[0].triple = value.targets[0].triple;
    });
    expectError(errors, "compiler host \"host-x86_64-pc-windows-msvc-wsl2\" must not contain emitted-target fields");
  });

  test("rejects imported Rust tiers", () => {
    const errors = errorsAfter((value) => {
      value.policy.referenceBreadth.importsRustTiers = true;
    });
    expectError(errors, "policy.referenceBreadth.importsRustTiers must be false");
  });

  test("rejects an MLIR0 manifest cross-check mismatch", () => {
    const errors = errorsAfter((value) => {
      value.crossChecks.mlir0Toolchain.hostEvidence = "native-windows";
    });
    expectError(errors, "mlir0 toolchain manifest mismatch: hostEvidence must be wsl-linux");
  });

  test("rejects an unknown target state", () => {
    const errors = errorsAfter((value) => {
      value.targets[1].state = "future";
    });
    expectError(errors, "target \"target-aarch64-unknown-linux-gnu\" state must be one of candidate, evidence, supported, deprecated, removed");
  });

  test("rejects an unknown supported verification level", () => {
    const errors = errorsAfter((value) => {
      const target = value.targets[0];
      target.state = "supported";
      target.verificationLevel = "level-0";
      target.claim = "unsupported claim";
      for (const axis of TARGET_AXES) {
        target.axes[axis].status = "pass";
        if (target.axes[axis].evidence.length === 0) {
          target.axes[axis].evidence = [{
            kind: "check",
            path: "tooling/check-mlir0.mjs",
            symbol: "const products = [",
            claim: "A bounded focal check provides local evidence.",
          }];
        }
      }
      target.blockers = [];
    });
    expectError(errors, "target \"target-x86_64-unknown-linux-gnu\".verificationLevel must be one of experimental, level-3, level-2, level-1, long-term");
  });

  test("rejects a candidate claim", () => {
    const errors = errorsAfter((value) => {
      value.targets[1].claim = "LLVM accepts this triple";
    });
    expectError(errors, "candidate target \"target-aarch64-unknown-linux-gnu\" must not declare a claim");
  });

  test("keeps host and target axis sets distinct", () => {
    expect(TARGET_AXES).not.toEqual(HOST_AXES);
    expect(TARGET_AXES).toContain("linkerSysrootPackaging");
    expect(HOST_AXES).toContain("nativeToolchain");
  });

  test("rejects a missing baseline cross-compilation edge", () => {
    const errors = errorsAfter((value) => {
      value.crossCompilation.edges.pop();
    });
    expectError(errors, "crossCompilation.edges is missing baseline edge");
  });

  test("rejects duplicate baseline cross-compilation edges", () => {
    const errors = errorsAfter((value) => {
      const duplicate = value.crossCompilation.edges[0];
      value.crossCompilation.edges[1] = structuredClone(duplicate);
    });
    expectError(errors, "crossCompilation.edges contains duplicate hostRef/targetRef pair");
  });

  test("rejects an only-self cross-compilation matrix", () => {
    const hostForTarget = {
      "target-x86_64-unknown-linux-gnu": "host-linux-x86_64-native",
      "target-x86_64-pc-windows-msvc": "host-windows-x86_64-native",
      "target-aarch64-apple-darwin": "host-macos-aarch64-native",
    };
    const errors = errorsAfter((value) => {
      for (const edge of value.crossCompilation.edges) {
        edge.hostRef = hostForTarget[edge.targetRef];
        edge.id = `edge-${edge.hostRef}-to-${edge.targetRef}`;
      }
    });
    expectError(errors, "crossCompilation.edges must include cross-host or cross-target edges");
  });

  test("rejects undefined cross-compilation endpoint references", () => {
    const errors = errorsAfter((value) => {
      value.crossCompilation.edges[0].hostRef = "host-missing";
    });
    expectError(errors, "crossCompilation.edges[0].hostRef must reference a primary native compiler host");
    expectError(errors, "crossCompilation.edges[0].hostRef does not resolve to a compiler host row");
  });

  test("rejects a WSL evidence edge marked as native", () => {
    const errors = errorsAfter((value) => {
      value.crossCompilation.developmentEvidence[0].nativeHost = true;
    });
    expectError(errors, "crossCompilation.developmentEvidence[0].nativeHost must be false for WSL evidence");
  });

  test("rejects an evidence symbol absent from its source file", () => {
    const errors = errorsAfter((value) => {
      value.crossCompilation.developmentEvidence[0].evidence[2].symbol = "notDeclared";
    });
    expectError(errors, "crossCompilation.developmentEvidence[0].evidence[2].symbol must identify a declared symbol in tooling/check-mlir0.mjs");
  });

  test("requires manifest evidence symbols to be top-level keys", () => {
    const errors = errorsAfter((value) => {
      value.crossCompilation.developmentEvidence[0].evidence[0].symbol = "notTopLevel";
    });
    expectError(errors, "crossCompilation.developmentEvidence[0].evidence[0].symbol must identify a declared symbol in tooling/mlir0-toolchain.json");
  });

  test("requires development build and execution evidence with derived blockers", () => {
    const errors = errorsAfter((value) => {
      const edge = value.crossCompilation.developmentEvidence[0];
      edge.evidence = edge.evidence.filter((evidence) => !["build", "execution"].includes(evidence.role));
    });
    expectError(errors, "crossCompilation.developmentEvidence[0].blockers must equal nativeHost, hostEndpoint, targetEndpoint, toolchainSysrootLinkerPackaging, buildExecution");
  });

  test("requires a distinct development evidence role", () => {
    const errors = errorsAfter((value) => {
      const edge = value.crossCompilation.developmentEvidence[0];
      edge.evidence = edge.evidence.filter((evidence) => evidence.role !== "development");
    });
    expectError(errors, "crossCompilation.developmentEvidence[0].evidence must include development role");
  });

  test("rejects host-target conflation in the baseline matrix", () => {
    const errors = errorsAfter((value) => {
      const edge = value.crossCompilation.edges[0];
      edge.hostRef = edge.targetRef;
      edge.id = `edge-${edge.hostRef}-to-${edge.targetRef}`;
    });
    expectError(errors, "crossCompilation.edges[0].hostRef must reference a primary native compiler host");
  });

  test("rejects supported cross-compilation edges without endpoints and gates", () => {
    const errors = errorsAfter((value) => {
      const edge = value.crossCompilation.edges[0];
      edge.state = "supported";
      edge.blockers = [];
    });
    expectError(errors, "supported cross-compilation edge");
    expectError(errors, "requires a supported compiler host endpoint");
    expectError(errors, "requires a supported emitted target endpoint");
    expectError(errors, "requires build and execution evidence");
  });

  test("requires LLD output and linker drivers in native plans", () => {
    const errors = errorsAfter((value) => {
      const plan = value.nativeToolchainPlans[0];
      plan.configuration.linkerDrivers = [];
      plan.outputs.artifacts = plan.outputs.artifacts.filter((artifact) => artifact !== "lld");
    });
    expectError(errors, "configuration must use Release, Ninja, X86, AArch64, and explicit LLD linker drivers");
    expectError(errors, "outputs must require mlir-opt, mlir-translate, clang, lld, llvm-config");
  });

  test("rejects a future native plan pinned to current evidence", () => {
    const errors = errorsAfter((value) => {
      value.nativeToolchainPlans[0].source.tag = "llvmorg-20.1.2";
    });
    expectError(errors, "source.tag must remain the closed pending-audit sentinel until repository-dependency-currency-audit");
  });

  test("rejects floating or nightly future native plan versions", () => {
    const errors = errorsAfter((value) => {
      value.nativeToolchainPlans[0].source.tag = "latest";
      value.nativeToolchainPlans[1].source.tag = "nightly";
    });
    expect(errors.filter((error) => error.includes("source.tag must remain the closed pending-audit sentinel until repository-dependency-currency-audit"))).toHaveLength(2);
  });

  test("keeps external toolchain candidates evaluation-only", () => {
    const errors = errorsAfter((value) => {
      const candidate = value.externalToolchainCandidates[0];
      candidate.status = "supported";
      candidate.claim = "portable support";
    });
    expectError(errors, "externalToolchainCandidates[0].status must be evaluation-only; promotion and support claims are forbidden");
    expectError(errors, "externalToolchainCandidates[0] contains unknown field(s): claim");
  });
});
