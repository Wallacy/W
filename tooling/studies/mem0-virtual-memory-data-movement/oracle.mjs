import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const directory = path.dirname(fileURLToPath(import.meta.url));
export const allowedClassifications = new Set([
  "portable semantic owner/API candidate",
  "provider capability/receipt",
  "compiler optimization",
  "unsafe target adapter",
  "rejected universal promise",
]);
export const requiredFields = ["ownerMoveOnly", "boundedExtent", "permissions", "addressSpace", "provenance", "deterministicDrop", "liveViewExclusion", "externalInterference", "targetEvidence"];
export function loadData() {
  return JSON.parse(fs.readFileSync(path.join(directory, "cases.json"), "utf8"));
}
export function validate(data = loadData()) {
  const errors = [];
  if (data.study !== "MEM0" || !Array.isArray(data.mechanisms) || !Array.isArray(data.workloads)) errors.push("MEM0 data identity or arrays are invalid");
  const ids = new Set();
  for (const mechanism of data.mechanisms ?? []) {
    if (ids.has(mechanism.id)) errors.push(`duplicate mechanism ${mechanism.id}`);
    ids.add(mechanism.id);
    if (!allowedClassifications.has(mechanism.classification)) errors.push(`${mechanism.id} has an invalid classification`);
    for (const field of requiredFields) if (mechanism[field] === undefined || mechanism[field] === "") errors.push(`${mechanism.id} omits ${field}`);
    if (mechanism.ownerMoveOnly !== true || mechanism.boundedExtent !== true || mechanism.liveViewExclusion !== true) errors.push(`${mechanism.id} weakens ownership, bounds, or live-view exclusion`);
    if (mechanism.id === "mapped-universal-wrapper" && !String(mechanism.decision).includes("Mapped<T>")) errors.push("universal mapping rejection is not explicit");
  }
  const requiredIds = ["file-backed-immutable", "anonymous-reserve-commit-decommit", "private-copy-on-write", "shared-mapping", "mapped-device-mmio-boundary", "protection-changes", "access-advice-prefault-discard", "huge-pages", "numa-placement", "locked-pinned-host", "device-unified-transfer-prefetch", "vectored-io", "sendfile-zero-copy", "alignment-cache-prefetch", "non-temporal-operations", "allocator-arena-fixed-ipc1"];
  for (const id of requiredIds) if (!ids.has(id)) errors.push(`required mechanism is absent: ${id}`);
  for (const workload of data.workloads ?? []) if (!workload.id || !Array.isArray(workload.requirements) || workload.requirements.length < 2) errors.push(`workload ${workload.id ?? "?"} is not bounded`);
  for (const id of data.adversarialCases ?? []) if (!ids.has(id)) errors.push(`adversarial case ${id} is missing`);
  if (!data.mechanisms.some((item) => item.classification === "portable semantic owner/API candidate")) errors.push("portable owner route is absent");
  if (!data.mechanisms.some((item) => item.classification === "provider capability/receipt")) errors.push("provider route is absent");
  if (!data.mechanisms.some((item) => item.classification === "compiler optimization")) errors.push("compiler route is absent");
  if (!data.mechanisms.some((item) => item.classification === "unsafe target adapter")) errors.push("unsafe adapter route is absent");
  if (!data.mechanisms.some((item) => item.classification === "rejected universal promise")) errors.push("rejected route is absent");
  return { errors, mechanisms: data.mechanisms };
}
