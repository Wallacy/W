import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadCases, validate } from "./oracle.mjs";

const directory = path.dirname(fileURLToPath(import.meta.url));
const study = JSON.parse(fs.readFileSync(path.join(directory, "study.json"), "utf8"));
const checked = validate(loadCases());
const errors = [...checked.errors];
if (study.id !== "SVC0" || study.decision !== "W-1480" || study.status !== "complete-design-study") errors.push("SVC0 identity/status is invalid");
if (study.evidenceBoundary?.researchClosed !== true || study.evidenceBoundary?.apiPromotion !== true) errors.push("SVC0 promotion boundary is incomplete");
if ((study.sources ?? []).length < 3) errors.push("SVC0 primary source set is incomplete");
const accepted = checked.results.filter((item) => item.status === "accepted");
const rejected = checked.results.filter((item) => item.status === "rejected");
if (accepted.length !== 4 || rejected.length !== 10) errors.push("SVC0 route counts drifted");
if (errors.length) { console.error(errors.join("\n")); process.exitCode = 1; }
else console.log("SVC0 service streams: 4 directional topologies accepted, 10 unsafe or implicit routes rejected.");
