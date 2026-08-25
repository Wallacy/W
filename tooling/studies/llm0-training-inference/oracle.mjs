import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const directory = path.dirname(fileURLToPath(import.meta.url));
export const classes = ["core language", "std/API", "typed IR/compiler", "runtime/provider", "tooling/evidence", "application framework"];
export function loadData() { return JSON.parse(fs.readFileSync(path.join(directory, "cases.json"), "utf8")); }
export function validate(data = loadData()) {
  const errors = [];
  if (data.study !== "LLM0" || !Array.isArray(data.existingCoverage) || data.existingCoverage.length < 10) errors.push("LLM0 existing coverage inventory is incomplete");
  const ids = new Set();
  for (const gap of data.gaps ?? []) {
    if (ids.has(gap.id)) errors.push(`duplicate gap ${gap.id}`);
    ids.add(gap.id);
    if (!classes.includes(gap.ownerClass)) errors.push(`${gap.id} has invalid owner class`);
    if (gap.default !== "avoid core inflation") errors.push(`${gap.id} lacks the default core boundary`);
    if (!gap.topic || !gap.evidence) errors.push(`${gap.id} lacks topic/evidence`);
  }
  for (const workload of data.workloads ?? []) if (!workload.id || !workload.sourceShape || (workload.oracleChecks ?? []).length < 4) errors.push(`workload ${workload.id ?? "?"} is not structured`);
  for (const adversarial of data.adversarialCases ?? []) if (!adversarial) errors.push("empty adversarial case");
  if (!data.gaps.some((gap) => gap.ownerClass === "core language")) errors.push("core-language classification must be explicit even when rejected");
  if (!data.gaps.some((gap) => gap.ownerClass === "tooling/evidence")) errors.push("tooling/evidence classification is absent");
  return { errors, gaps: data.gaps ?? [], workloads: data.workloads ?? [] };
}
