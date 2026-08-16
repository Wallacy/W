import { describe, expect, test } from "bun:test";
import { deriveSec0, validateSec0, PROFILES } from "./sec0-security-model-machine.mjs";
import corpus from "./sec0-security-model-cases.json" with { type: "json" };

const results = deriveSec0(corpus);
const result = (id) => results.find((entry) => entry.caseId === id);

describe("SEC0 security model oracle", () => {
  test("validates the closed corpus and all target profiles", () => {
    expect(validateSec0(corpus).errors).toEqual([]);
    expect(PROFILES).toHaveLength(6);
    expect(results).toHaveLength(101);
    expect(results.filter((entry) => entry.status === "accepted")).toHaveLength(24);
    expect(results.filter((entry) => entry.status === "rejected")).toHaveLength(77);
    expect(results.filter((entry) => entry.route === "research" && entry.status === "accepted")).toHaveLength(13);
  });

  test("keeps static safety and explicit unsafe separate", () => {
    expect(result("SEC0-safe-baseline").code).toBe("safeProofsPreserved");
    expect(result("SEC0-safe-explicit-unsafe").code).toBe("explicitUnsafeIsland");
    expect(result("SEC0-safe-unproven-elision").code).toBe("checkElisionUnproven");
  });

  test("rejects authority amplification and deployment downgrade", () => {
    expect(result("SEC0-api-wider").code).toBe("capabilityAmplificationRejected");
    expect(result("SEC0-feature-capability").code).toBe("featureSecurityAuthorityRejected");
    expect(result("SEC0-profile-deployment-floor").code).toBe("deploymentWeakensProductMinimum");
    expect(result("SEC0-tenant-cross-capability").code).toBe("tenantBoundaryRejected");
  });

  test("requires residual risk and ordered attestations", () => {
    expect(result("SEC0-side-baseline").code).toBe("sideChannelBudgeted");
    expect(result("SEC0-side-universal").code).toBe("universalSideChannelClaimRejected");
    expect(result("SEC0-patch-baseline").receipt).toMatch(/^sha256:[0-9a-f]{64}$/u);
    expect(result("SEC0-profile-static-proof").route).toBe("research");
    expect(result("SEC0-patch-reordered").code).toBe("patchAttestationOrderInvalid");
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
    expect(result("SEC0-profile-wasm-common-memory").code).toBe("profileMinimumMissing");
    expect(result("SEC0-profile-hardware-common-supply").code).toBe("profileMinimumMissing");
    expect(result("SEC0-profile-threat-pollution").code).toBe("profileThreatUnknown");
  });
});
