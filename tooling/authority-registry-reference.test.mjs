import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import {
  authorityOriginEqual,
  buildAuthoritySnapshot,
  deriveAuthorityCase,
  packageIdentity,
  registryFixtureInput,
  verifyRegistry,
} from "./authority-registry-machine.mjs";

const corpusPath = path.join(import.meta.dir, "authority-registry-cases.json");
const corpusText = fs.readFileSync(corpusPath, "utf8");
const corpus = JSON.parse(corpusText);
const snapshotPath = path.join(import.meta.dir, "authority-registry-results.snapshot.jsonl");
const snapshot = JSON.parse(fs.readFileSync(snapshotPath, "utf8").trim());
const textEncoder = new TextEncoder();

function oracleBytes(...parts) {
  const arrays = parts.map((part) => part instanceof Uint8Array ? part : Uint8Array.from(part));
  const result = new Uint8Array(arrays.reduce((total, part) => total + part.length, 0));
  let offset = 0;
  for (const part of arrays) {
    result.set(part, offset);
    offset += part.length;
  }
  return result;
}

function oracleU32(value) {
  return Uint8Array.of((value >>> 24) & 0xff, (value >>> 16) & 0xff,
    (value >>> 8) & 0xff, value & 0xff);
}

function oracleText(value) {
  const bytes = textEncoder.encode(value);
  return oracleBytes(oracleU32(bytes.length), bytes);
}

function oracleFrame(bytes) {
  return oracleBytes(oracleU32(bytes.length), bytes);
}

function oracleCompareBytes(left, right) {
  const length = Math.min(left.length, right.length);
  for (let index = 0; index < length; index += 1) {
    if (left[index] !== right[index]) return left[index] - right[index];
  }
  return left.length - right.length;
}

function oracleHex(value) {
  return Uint8Array.from(Buffer.from(value, "hex"));
}

function oracleSha256(value) {
  return `sha256:${crypto.createHash("sha256").update(value).digest("hex")}`;
}

function oracleHexText(value) {
  return Buffer.from(value).toString("hex");
}

function oracleRootPayload(root) {
  const keys = [...root.keys].sort((left, right) =>
    oracleCompareBytes(textEncoder.encode(left.id), textEncoder.encode(right.id)));
  return oracleBytes(
    textEncoder.encode("w-authority-root-payload-1"),
    Uint8Array.of(0x50, 0x52), oracleText(root.lineage), oracleU32(root.version),
    Uint8Array.of(root.threshold), oracleU32(keys.length),
    ...keys.map((key) => oracleBytes(
      Uint8Array.of(0x4b), oracleText(key.id), Uint8Array.of(1),
      oracleFrame(oracleHex(key.publicKey)),
    )),
  );
}

function oracleAuthorityOrigin(fixture) {
  const genesisPayload = oracleRootPayload(fixture.roots[0]);
  return oracleBytes(
    textEncoder.encode("w-authority-origin-1"),
    Uint8Array.of(0x41, 1, 0x50), oracleText(fixture.provider),
    Uint8Array.of(0x4c), oracleText(fixture.lineage),
    Uint8Array.of(0x47), oracleFrame(genesisPayload),
  );
}

function oraclePackageIdentity(origin, packageName) {
  if (!(origin instanceof Uint8Array) ||
      !/^[a-z][a-z0-9-]{0,62}\/[a-z][a-z0-9-]{0,62}$/u.test(packageName))
    throw new Error("packageNameInvalid");
  return oracleBytes(
    textEncoder.encode("w-package-identity-1"), Uint8Array.of(0x50),
    oracleFrame(origin), oracleText(packageName),
  );
}

function equalBytes(left, right) {
  return left.length === right.length && left.every((value, index) => value === right[index]);
}

function fullByteReferenceMatches(actual, reference) {
  return actual instanceof Uint8Array && reference?.bytes instanceof Uint8Array &&
    actual.length === reference.length && equalBytes(actual, reference.bytes) &&
    oracleSha256(actual) === reference.digest;
}

const outcomeOracle = new Map([
  ["AUL0-W-1469-genesis-2-of-3", ["accepted", "authorityLineageVerified"]],
  ["AUL0-W-1469-rotation-valid", ["accepted", "authorityLineageVerified"]],
  ["AUL0-W-1469-alias-evidence-only", ["accepted", "authorityLineageVerified"]],
  ["AUL0-W-1469-mirror-evidence-only", ["accepted", "authorityLineageVerified"]],
  ["AUL0-W-1469-checkpoint-v2-stable", ["accepted", "authorityLineageVerified"]],
  ["AUL0-W-1469-checkpoint-v2-replay-v1", ["rejected", "authorityRootRollback"]],
  ["AUL0-W-1469-checkpoint-v2-replay-v2", ["rejected", "authorityRootRollback"]],
  ["AUL0-W-1469-checkpoint-corrupt-bytes", ["rejected", "authorityCheckpointDigestMismatch"]],
  ["AUL0-W-1469-checkpoint-corrupt-digest", ["rejected", "authorityCheckpointDigestMismatch"]],
  ["AUL0-W-1469-checkpoint-corrupt-length", ["rejected", "authorityCheckpointLengthMismatch"]],
  ["AUL0-W-1469-checkpoint-corrupt-origin", ["rejected", "authorityCheckpointBytesMismatch"]],
  ["AUL0-W-1469-checkpoint-crosswire-root", ["rejected", "authorityCheckpointBytesMismatch"]],
  ["AUL0-W-1469-checkpoint-crosswire-origin", ["rejected", "authorityCheckpointBytesMismatch"]],
  ["AUL0-W-1469-genesis-different", ["accepted", "authorityLineageVerified"]],
  ["AUL0-W-1469-genesis-fabricated-untrusted", ["rejected", "authorityCheckpointGenesisMismatch"]],
  ["AUL0-W-1469-genesis-same-public-fields-different-signatures", ["accepted", "authorityLineageVerified"]],
  ["AUL0-W-1469-insufficient-old-threshold", ["rejected", "authorityOldThresholdInsufficient"]],
  ["AUL0-W-1469-insufficient-new-threshold", ["rejected", "authorityNewThresholdInsufficient"]],
  ["AUL0-W-1469-duplicate-signer", ["rejected", "authorityDuplicateSigner"]],
  ["AUL0-W-1469-rollback", ["rejected", "authorityRootRollback"]],
  ["AUL0-W-1469-gap", ["rejected", "authorityRootGap"]],
  ["AUL0-W-1469-wrong-lineage", ["rejected", "authorityWrongLineage"]],
  ["AUL0-W-1469-tamper", ["rejected", "authorityOldThresholdInsufficient"]],
  ["AUL0-W-1469-malformed-key", ["rejected", "authorityPublicKeyMalformed"]],
  ["AUL0-W-1469-malformed-signature", ["rejected", "authoritySignatureMalformed"]],
  ["AUL0-W-1469-key-id-mismatch", ["rejected", "authorityKeyIdMismatch"]],
  ["AUL0-W-1469-unsupported-algorithm", ["rejected", "authorityAlgorithmUnsupported"]],
  ["AUL0-W-1469-over-limit", ["rejected", "authorityUpdateCountExceeded"]],
  ["AUL0-W-1469-over-limit-keys", ["rejected", "authorityKeyCountExceeded"]],
  ["AUL0-W-1469-over-limit-signatures", ["rejected", "authoritySignatureCountExceeded"]],
  ["AUL0-W-1469-local-release-rejected", ["rejected", "authorityManifestKindRejected"]],
  ["AUL0-W-1469-git-snapshot-not-authority", ["rejected", "authorityManifestKindRejected"]],
  ["AUL0-W-1469-current", ["accepted", "authorityLineageVerified"]],
]);

const derived = buildAuthoritySnapshot(corpus, oracleSha256(textEncoder.encode(corpusText)));
const byId = (id) => derived.results.find((entry) => entry.caseId === id);

describe("AUL0 bounded signed registry authority oracle", () => {
  test("derives outcomes without corpus expected-result echo", () => {
    expect(derived.metrics.caseCount).toBe(outcomeOracle.size);
    expect(derived).toEqual(snapshot);
    for (const [caseId, expected] of outcomeOracle)
      expect([byId(caseId)?.status, byId(caseId)?.code]).toEqual(expected);
    expect(corpus.cases.every((entry) => !Object.hasOwn(entry, "expected"))).toBe(true);
  });

  test("reconstructs genesis anchor, origin, and package identity independently", () => {
    for (const fixture of Object.values(corpus.fixtures)) {
      const genesisPayload = oracleRootPayload(fixture.roots[0]);
      expect(oracleHexText(genesisPayload)).toBe(fixture.trustedGenesis.bytes);
      expect(oracleSha256(genesisPayload)).toBe(fixture.trustedGenesis.digest);
      expect(genesisPayload.length).toBe(fixture.trustedGenesis.length);
      const actual = verifyRegistry(registryFixtureInput(fixture));
      expect(actual.status).toBe("accepted");
      const origin = oracleAuthorityOrigin(fixture);
      expect(oracleHexText(actual.origin)).toBe(oracleHexText(origin));
      expect(actual.originDigest).toBe(oracleSha256(origin));
    }
    const base = verifyRegistry(registryFixtureInput(corpus.fixtures.registry));
    const expectedPackage = oraclePackageIdentity(base.origin, "last-light/restaurant");
    expect(oracleHexText(packageIdentity(base.origin, "last-light/restaurant").bytes)).toBe(
      oracleHexText(expectedPackage));
  });

  test("requires trusted genesis and verifies old/new thresholds on rotation", () => {
    expect(byId("AUL0-W-1469-genesis-2-of-3")).toMatchObject({ status: "accepted", code: "authorityLineageVerified" });
    expect(byId("AUL0-W-1469-rotation-valid")).toMatchObject({ status: "accepted", code: "authorityLineageVerified" });
    expect(byId("AUL0-W-1469-genesis-fabricated-untrusted").code).toBe("authorityCheckpointGenesisMismatch");
    expect(byId("AUL0-W-1469-insufficient-old-threshold").code).toBe("authorityOldThresholdInsufficient");
    expect(byId("AUL0-W-1469-insufficient-new-threshold").code).toBe("authorityNewThresholdInsufficient");
    expect(byId("AUL0-W-1469-duplicate-signer").code).toBe("authorityDuplicateSigner");
    expect(byId("AUL0-W-1469-checkpoint-v2-replay-v1").code).toBe("authorityRootRollback");
    expect(byId("AUL0-W-1469-checkpoint-v2-replay-v2").code).toBe("authorityRootRollback");
  });

  test("persists the next checkpoint without changing identity origin", () => {
    const resultFor = (id) => deriveAuthorityCase(
      corpus.cases.find((entry) => entry.id === id), corpus,
    ).result;
    const bootstrap = resultFor("AUL0-W-1469-genesis-2-of-3");
    const rotation = resultFor("AUL0-W-1469-rotation-valid");
    const stable = resultFor("AUL0-W-1469-checkpoint-v2-stable");
    expect(bootstrap.route).toBe("authorityLineageVerified");
    expect(rotation.route).toBe("authorityLineageVerified");
    expect(stable.route).toBe("authorityLineageVerified");
    expect(bootstrap.nextCheckpoint.root.version).toBe(1);
    expect(rotation.nextCheckpoint.root.version).toBe(2);
    expect(stable.nextCheckpoint.root.version).toBe(2);
    expect(rotation.continuity.updates).toHaveLength(1);
    expect(stable.continuity.updates).toHaveLength(0);
    expect(rotation.originDigest).toBe(bootstrap.originDigest);
    expect(stable.originDigest).toBe(bootstrap.originDigest);
    expect(rotation.nextCheckpoint.view.bytes).toMatch(/^[0-9a-f]+$/u);
    expect(stable.nextCheckpoint.view.bytes).toMatch(/^[0-9a-f]+$/u);
    expect(rotation.nextCheckpoint.view.digest).toBe(stable.nextCheckpoint.view.digest);
    expect(rotation.nextCheckpoint.view.length).toBe(stable.nextCheckpoint.view.length);
  });

  test("round-trips the external next checkpoint as a trusted checkpoint", () => {
    const sourceInput = registryFixtureInput(corpus.fixtures.registry);
    const first = verifyRegistry(sourceInput);
    expect(first.status).toBe("accepted");
    expect(first.nextCheckpoint.root.version).toBe(2);
    const secondInput = {
      ...sourceInput,
      trustedCheckpoint: first.nextCheckpoint,
      continuity: {
        ...sourceInput.continuity,
        checkpointVersion: first.nextCheckpoint.root.version,
        checkpointDigest: first.nextCheckpoint.view.digest,
        updates: [],
        observedRootVersion: first.nextCheckpoint.root.version,
        observedRootDigest: first.continuity.observedRootDigest,
      },
    };
    const second = verifyRegistry(secondInput);
    expect(second.status).toBe("accepted");
    expect(second.code).toBe("authorityLineageVerified");
    expect(second.continuity.updates).toHaveLength(0);
    expect(second.originDigest).toBe(first.originDigest);
  });

  test("keeps locator, mirror, checkpoint, and signature evidence outside origin", () => {
    const base = byId("AUL0-W-1469-rotation-valid");
    for (const id of [
      "AUL0-W-1469-genesis-2-of-3",
      "AUL0-W-1469-alias-evidence-only",
      "AUL0-W-1469-mirror-evidence-only",
      "AUL0-W-1469-checkpoint-v2-stable",
      "AUL0-W-1469-genesis-same-public-fields-different-signatures",
    ]) {
      expect(byId(id).originDigest).toBe(base.originDigest);
      expect(byId(id).originLength).toBe(base.originLength);
    }
    expect(byId("AUL0-W-1469-alias-evidence-only").receiptDigest).not.toBe(base.receiptDigest);
    expect(byId("AUL0-W-1469-mirror-evidence-only").receiptDigest).not.toBe(base.receiptDigest);
    expect(byId("AUL0-W-1469-genesis-same-public-fields-different-signatures").receiptDigest)
      .not.toBe(byId("AUL0-W-1469-genesis-2-of-3").receiptDigest);
    const signatureVariant = deriveAuthorityCase(
      corpus.cases.find((entry) => entry.id === "AUL0-W-1469-genesis-same-public-fields-different-signatures"), corpus,
    ).result;
    const baseRotation = deriveAuthorityCase(
      corpus.cases.find((entry) => entry.id === "AUL0-W-1469-rotation-valid"), corpus,
    ).result;
    expect(signatureVariant.nextCheckpoint.view.digest).toBe(baseRotation.nextCheckpoint.view.digest);
    expect(byId("AUL0-W-1469-genesis-different").originDigest).not.toBe(base.originDigest);
  });

  test("rejects malformed signatures, key IDs, algorithms, lineage, and ceilings", () => {
    expect(byId("AUL0-W-1469-rollback").code).toBe("authorityRootRollback");
    expect(byId("AUL0-W-1469-gap").code).toBe("authorityRootGap");
    expect(byId("AUL0-W-1469-wrong-lineage").code).toBe("authorityWrongLineage");
    expect(byId("AUL0-W-1469-tamper").code).toBe("authorityOldThresholdInsufficient");
    expect(byId("AUL0-W-1469-malformed-key").code).toBe("authorityPublicKeyMalformed");
    expect(byId("AUL0-W-1469-malformed-signature").code).toBe("authoritySignatureMalformed");
    expect(byId("AUL0-W-1469-key-id-mismatch").code).toBe("authorityKeyIdMismatch");
    expect(byId("AUL0-W-1469-unsupported-algorithm").code).toBe("authorityAlgorithmUnsupported");
    expect(byId("AUL0-W-1469-over-limit").code).toBe("authorityUpdateCountExceeded");
    expect(byId("AUL0-W-1469-over-limit-keys").code).toBe("authorityKeyCountExceeded");
    expect(byId("AUL0-W-1469-over-limit-signatures").code).toBe("authoritySignatureCountExceeded");
  });

  test("separates local/git manifest forms from registry authority", () => {
    expect(byId("AUL0-W-1469-local-release-rejected").code).toBe("authorityManifestKindRejected");
    expect(byId("AUL0-W-1469-git-snapshot-not-authority").code).toBe("authorityManifestKindRejected");
  });

  test("canonicalizes byte order and keeps full-byte equality", () => {
    const fixture = structuredClone(corpus.fixtures.registry);
    fixture.roots[0].keys.reverse();
    fixture.roots[0].signatures.reverse();
    fixture.roots[1].keys.reverse();
    fixture.roots[1].signatures.reverse();
    const reordered = verifyRegistry(registryFixtureInput(fixture));
    const base = verifyRegistry(registryFixtureInput(corpus.fixtures.registry));
    expect(reordered.originDigest).toBe(base.originDigest);
    expect(reordered.receiptDigest).toBe(base.receiptDigest);
    const altered = new Uint8Array(base.origin);
    altered[altered.length - 1] ^= 1;
    expect(authorityOriginEqual(base.origin, altered)).toBe(false);
    expect(authorityOriginEqual(base.origin, new Uint8Array(base.origin))).toBe(true);
  });

  test("validates scoped package grammar and rejects digest-only identity", () => {
    const base = verifyRegistry(registryFixtureInput(corpus.fixtures.registry));
    const first = packageIdentity(base.origin, "last-light/restaurant");
    const second = packageIdentity(base.origin, "other/restaurant");
    expect(first.digest).not.toBe(second.digest);
    for (const invalid of ["restaurant", "Last-light/restaurant", "a/b/c", "a/", "a./b", "a/β", "a/\ud800"])
      expect(() => packageIdentity(base.origin, invalid)).toThrow("packageNameInvalid");
    const digestOnly = new Uint8Array(base.origin);
    digestOnly[0] ^= 1;
    expect(authorityOriginEqual(base.origin, digestOnly)).toBe(false);
  });

  test("covers explicit lock/reference, digest-only, and forced-collision adversaries", () => {
    expect(new Set(corpus.identityCases.map((entry) => entry.id))).toEqual(new Set([
      "AUL0-W-1469-wrong-authority-lock-reference",
      "AUL0-W-1469-digest-only-substitution",
      "AUL0-W-1469-forced-digest-collision-full-byte",
      "AUL0-W-1469-package-scoped-grammar",
    ]));
    const base = verifyRegistry(registryFixtureInput(corpus.fixtures.registry));
    const alternate = verifyRegistry(registryFixtureInput(corpus.fixtures["registry-alt"]));
    const baseReference = { bytes: base.record, digest: base.recordDigest, length: base.record.length };
    expect(fullByteReferenceMatches(base.record, baseReference)).toBe(true);
    expect(fullByteReferenceMatches(alternate.record, baseReference)).toBe(false);
    const digestOnly = { bytes: new Uint8Array(alternate.origin), digest: base.originDigest, length: base.originLength };
    expect(fullByteReferenceMatches(base.origin, digestOnly)).toBe(false);
    const forcedCollision = { bytes: new Uint8Array(alternate.origin), digest: base.originDigest, length: alternate.origin.length };
    expect(fullByteReferenceMatches(base.origin, forcedCollision)).toBe(false);
  });

  test("covers text canonicality and trusted-anchor ceiling", () => {
    const malformedDisplay = structuredClone(corpus.fixtures.registry);
    malformedDisplay.manifest.display = "W\ud800";
    expect(verifyRegistry(registryFixtureInput(malformedDisplay)).code).toBe("authorityDisplayInvalid");
    const malformedLineage = registryFixtureInput(structuredClone(corpus.fixtures.registry));
    malformedLineage.lineage = "w\ud800";
    expect(verifyRegistry(malformedLineage).code).toBe("authorityLineageInvalid");
    const oversizedAnchor = registryFixtureInput(structuredClone(corpus.fixtures.registry));
    oversizedAnchor.trustedGenesis = {
      bytes: "00".repeat(8193),
      digest: "sha256:" + "00".repeat(32),
      length: 8193,
    };
    expect(verifyRegistry(oversizedAnchor).code).toBe("authorityTrustedGenesisInvalid");
  });

  test("rejects checkpoint corruption before update replay", () => {
    expect(byId("AUL0-W-1469-checkpoint-corrupt-bytes").status).toBe("rejected");
    expect(byId("AUL0-W-1469-checkpoint-corrupt-digest").status).toBe("rejected");
    expect(byId("AUL0-W-1469-checkpoint-corrupt-length").status).toBe("rejected");
    expect(byId("AUL0-W-1469-checkpoint-corrupt-origin").status).toBe("rejected");
    expect(byId("AUL0-W-1469-checkpoint-crosswire-root").status).toBe("rejected");
    expect(byId("AUL0-W-1469-checkpoint-crosswire-origin").status).toBe("rejected");
  });

  test("does not accept a caller result assertion", () => {
    const candidate = structuredClone(corpus.cases[0]);
    candidate.expected = { status: "rejected" };
    expect(() => deriveAuthorityCase(candidate, corpus)).toThrow("callerResultEcho");
  });
});
