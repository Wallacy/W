import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { deriveSec0, validateSec0 } from "../../sec0-security-model-machine.mjs";
import { validateSec0StudyManifest } from "../../sec0-security-model-manifest.mjs";

const studyDirectory = import.meta.dir;
const toolingDirectory = path.resolve(studyDirectory, "../..");
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "sec0-security-model-cases.json"), "utf8"));
const study = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
const bundle = JSON.parse(fs.readFileSync(path.join(studyDirectory, "bundle.json"), "utf8"));
const results = deriveSec0(corpus);
const result = (id) => results.find((entry) => entry.caseId === id);

describe("SEC0 study oracle", () => {
  test("keeps the broad security axes closed and measurable", () => {
    expect(validateSec0(corpus).errors).toEqual([]);
    expect(validateSec0StudyManifest(study, { studyDirectory, repositoryRoot })).toEqual([]);
    expect(study.profiles).toEqual([
      "trusted-native-cpu",
      "sandboxed-native-process",
      "wasm-component",
      "multi-tenant-isolate",
      "embedded-freestanding",
      "fpga-asic-hardware",
    ]);
    expect(bundle.variants.find((entry) => entry.id === "rejected-ambient-security")).toMatchObject({ role: "rejected-witness" });
  });

  test("preserves safe W and explicit unsafe boundaries", () => {
    expect(result("SEC0-safe-baseline")).toMatchObject({ status: "accepted", code: "safeProofsPreserved", safe: true });
    expect(result("SEC0-safe-explicit-unsafe")).toMatchObject({ status: "accepted", code: "explicitUnsafeIsland", safe: false });
    expect(result("SEC0-safe-unproven-elision").code).toBe("checkElisionUnproven");
    expect(result("SEC0-safe-unchecked").code).toBe("uncheckedOperationRejected");
    expect(result("SEC0-safe-ambient").code).toBe("safeAuthorityRejected");
  });

  test("separates capabilities, resources, secrets, and supply chain", () => {
    expect(result("SEC0-api-baseline").code).toBe("apiMediated");
    expect(result("SEC0-api-wider").code).toBe("capabilityAmplificationRejected");
    expect(result("SEC0-input-baseline").code).toBe("boundedInputResourceAndSupplyChain");
    expect(result("SEC0-input-secret-log").code).toBe("securityEvidenceMissing");
    expect(result("SEC0-supply-baseline").code).toBe("supplyChainAdmitted");
  });

  test("covers every physical profile and receipt rule", () => {
    for (const id of ["SEC0-profile-native", "SEC0-profile-process", "SEC0-profile-wasm", "SEC0-profile-isolate", "SEC0-profile-embedded", "SEC0-profile-hardware"]) {
      expect(result(id).code).toBe("profileAdmitted");
    }
    expect(result("SEC0-profile-omission").code).toBe("protectionReceiptMissing");
    expect(result("SEC0-profile-deployment-floor").code).toBe("deploymentWeakensProductMinimum");
    expect(result("SEC0-profile-semantic-drift").code).toBe("physicalChangeMutatesSemanticInterface");
    expect(result("SEC0-profile-missing-receipt").code).toBe("profileIdentityInvalid");
    expect(result("SEC0-profile-static-proof").route).toBe("research");
    expect(result("SEC0-profile-external-boundary").route).toBe("research");
    expect(result("SEC0-profile-na-memory").code).toBe("runtimeProtectionBasisInvalid");
    expect(result("SEC0-profile-na-without-marker").code).toBe("threatModelExceptionInvalid");
    expect(result("SEC0-profile-process-minimum").code).toBe("profileMinimumMissing");
    expect(result("SEC0-profile-hardware-minimum").code).toBe("profileMinimumMissing");
    expect(result("SEC0-profile-wasm-common-memory").code).toBe("profileMinimumMissing");
    expect(result("SEC0-profile-hardware-common-supply").code).toBe("profileMinimumMissing");
    expect(result("SEC0-profile-receipt-issuer").code).toBe("protectionReceiptInvalid");
    expect(result("SEC0-profile-receipt-stage").code).toBe("protectionReceiptInvalid");
    expect(result("SEC0-profile-receipt-digest").code).toBe("protectionReceiptInvalid");
    expect(result("SEC0-profile-receipt-scope").code).toBe("protectionReceiptScopeInvalid");
    expect(result("SEC0-profile-receipt-verified").code).toBe("callerEchoRejected");
    expect(result("SEC0-profile-runtime-present-static").code).toBe("runtimeProtectionBasisInvalid");
    expect(result("SEC0-profile-runtime-omitted-runtime").code).toBe("runtimeProtectionBasisInvalid");
    expect(result("SEC0-profile-receipt-target").code).toBe("protectionReceiptScopeInvalid");
    expect(result("SEC0-profile-receipt-artifact").code).toBe("protectionReceiptScopeInvalid");
    expect(result("SEC0-profile-target-unknown").code).toBe("profileTargetUnknown");
    expect(result("SEC0-profile-available-echo").code).toBe("callerEchoRejected");
    expect(result("SEC0-profile-threat-pollution").code).toBe("profileThreatUnknown");
  });

  test("models residual side-channel risk, FFI, tenants, and patches", () => {
    expect(result("SEC0-side-baseline").code).toBe("sideChannelBudgeted");
    expect(result("SEC0-side-universal").code).toBe("universalSideChannelClaimRejected");
    expect(result("SEC0-ffi-baseline").code).toBe("mediatedFfi");
    expect(result("SEC0-ffi-explicit-unsafe").code).toBe("explicitUnsafeFfi");
    expect(result("SEC0-ffi-ub").code).toBe("undefinedBehaviorRejected");
    expect(result("SEC0-tenant-cross-capability").code).toBe("tenantBoundaryRejected");
    expect(result("SEC0-patch-baseline").receipt).toMatch(/^sha256:[0-9a-f]{64}$/u);
    expect(result("SEC0-patch-reordered").code).toBe("patchAttestationOrderInvalid");
    expect(result("SEC0-patch-bad-digest").code).toBe("patchEventFactInvalid");
    expect(result("SEC0-supply-bad-digest").code).toBe("supplyChainFactInvalid");
  });

  test("keeps AVF0 runtime configuration security-neutral", () => {
    expect(result("SEC0-feature-baseline")).toMatchObject({ status: "accepted", code: "featureSecurityNeutral", authority: "unchanged" });
    expect(result("SEC0-feature-capability").code).toBe("featureSecurityAuthorityRejected");
    expect(result("SEC0-feature-dependency").code).toBe("featureSecurityAuthorityRejected");
    expect(result("SEC0-feature-abi").code).toBe("featureSecurityAuthorityRejected");
    expect(result("SEC0-feature-echo").code).toBe("callerEchoRejected");
  });

  test("rejects caller-owned outcome and open schemas", () => {
    const echo = structuredClone(corpus.fixtures.safe);
    echo.status = "accepted";
    expect(deriveSec0({ cases: [{ id: "echo", fixture: "x", expected: { status: "rejected", code: "callerEchoRejected" } }], fixtures: { x: echo } })[0].code).toBe("callerEchoRejected");
    const open = structuredClone(corpus.fixtures.api);
    open.extra = true;
    expect(deriveSec0({ cases: [{ id: "open", fixture: "x", expected: { status: "rejected", code: "apiMediationSchemaClosed" } }], fixtures: { x: open } })[0].code).toBe("apiMediationSchemaClosed");
  });
});
