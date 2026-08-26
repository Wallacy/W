import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadCases, validate } from "./oracle.mjs";

const directory = path.dirname(fileURLToPath(import.meta.url));
const study = JSON.parse(fs.readFileSync(path.join(directory, "study.json"), "utf8"));
const checked = validate(loadCases());
const errors = [...checked.errors];

if (study.id !== "QOS0" || study.decision !== "W-1483") {
  errors.push("QOS0 identity is invalid");
}
if (study.status !== "complete-design-study") errors.push("QOS0 status is invalid");
if (study.classification !== "implementation-evidence-gap") {
  errors.push("QOS0 classification is invalid");
}
if (study.evidenceBoundary?.hostOnly !== true) errors.push("QOS0 must remain host-only");
if (study.evidenceBoundary?.apiPromotion !== false) errors.push("QOS0 must not promote a priority API");
if (study.evidenceBoundary?.researchClosed !== true) errors.push("QOS0 research boundary is incomplete");

const requiredMissing = [
  "semantic checker", "lowering", "runtime provider", "cross-target receipts",
  "fault liveness tests", "performance evidence", "human study", "model study",
];
for (const item of requiredMissing) {
  if (!study.evidenceBoundary?.missing?.includes(item)) errors.push(`QOS0 missing boundary omits ${item}`);
}
if ((study.sources ?? []).length !== 4) errors.push("QOS0 primary source set is incomplete");
if ((study.sources ?? []).some((source) => source.accessed !== "2026-08-26")) {
  errors.push("QOS0 source access date drifted");
}

const accepted = checked.results.filter((item) => item.status === "accepted");
const rejected = checked.results.filter((item) => item.status === "rejected");
const eligible = checked.results.filter((item) => item.status === "eligible");
if (accepted.length !== 7 || rejected.length !== 73 || eligible.length !== 1) {
  errors.push("QOS0 route counts drifted");
}

if (errors.length > 0) {
  console.error(errors.join("\n"));
  process.exitCode = 1;
} else {
  console.log("QOS0 scheduling boundary: 7 current routes accepted, 73 priority or rule-drift routes rejected, 1 complete research-reopen route eligible.");
}
