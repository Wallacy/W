import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { allowedClassifications, loadData, requiredFields, validate } from "./oracle.mjs";

const directory = path.dirname(fileURLToPath(import.meta.url));
const study = JSON.parse(fs.readFileSync(path.join(directory, "study.json"), "utf8"));
const errors = [...validate(loadData()).errors];
if (study.id !== "MEM0" || study.decision !== "W-1473" || study.status !== "complete-design-study") errors.push("MEM0 study identity/status is invalid");
if (study.evidenceBoundary?.researchClosed !== true) errors.push("MEM0 research closure is absent");
if (JSON.stringify(study).includes("Mapped<T>")) errors.push("MEM0 metadata must not invent a Mapped<T> API");
if (study.allowedClassifications?.join("|") !== [...allowedClassifications].join("|")) errors.push("MEM0 classification list drifted");
if (JSON.stringify(study.requiredFields) !== JSON.stringify(requiredFields)) errors.push("MEM0 required field list drifted");
if ((study.sources ?? []).length < 8 || study.evidenceBoundary?.apiPromotion !== false) errors.push("MEM0 evidence boundary is incomplete");
if (errors.length) { console.error(errors.join("\n")); process.exitCode = 1; }
else console.log("MEM0 oracle/check: 16+ mechanisms, 5 workloads, target evidence boundary, and no API promotion claims");
