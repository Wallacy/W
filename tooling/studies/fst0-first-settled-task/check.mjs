import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadCases, validate } from "./oracle.mjs";

const directory = path.dirname(fileURLToPath(import.meta.url));
const study = JSON.parse(fs.readFileSync(path.join(directory, "study.json"), "utf8"));
const checked = validate(loadCases());
const errors = [...checked.errors];
if (study.id !== "FST0" || study.decision !== "W-1481" || study.status !== "complete-design-study") errors.push("FST0 identity/status is invalid");
if (study.evidenceBoundary?.researchClosed !== true || study.evidenceBoundary?.apiPromotion !== true) errors.push("FST0 promotion boundary is incomplete");
if ((study.sources ?? []).length < 4) errors.push("FST0 primary source set is incomplete");
const accepted = checked.results.filter((item) => item.status === "accepted");
const rejected = checked.results.filter((item) => item.status === "rejected");
if (accepted.length !== 6 || rejected.length !== 10) errors.push("FST0 route counts drifted");
if (errors.length) { console.error(errors.join("\n")); process.exitCode = 1; }
else console.log("FST0 first-settled: 6 current routes accepted, 10 implicit or unsafe routes rejected.");
