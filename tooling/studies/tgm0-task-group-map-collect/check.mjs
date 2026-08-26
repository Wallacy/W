import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadCases, validate } from "./oracle.mjs";

const directory = path.dirname(fileURLToPath(import.meta.url));
const study = JSON.parse(fs.readFileSync(path.join(directory, "study.json"), "utf8"));
const checked = validate(loadCases());
const errors = [...checked.errors];
if (study.id !== "TGM0" || study.decision !== "W-1482" || study.status !== "complete-design-study") errors.push("TGM0 identity/status is invalid");
if (study.evidenceBoundary?.researchClosed !== true || study.evidenceBoundary?.apiPromotion !== true) errors.push("TGM0 promotion boundary is incomplete");
if ((study.sources ?? []).length < 4) errors.push("TGM0 primary source set is incomplete");
const accepted = checked.results.filter((item) => item.status === "accepted");
const rejected = checked.results.filter((item) => item.status === "rejected");
if (accepted.length !== 9 || rejected.length !== 17) errors.push("TGM0 route counts drifted");
if (errors.length) { console.error(errors.join("\n")); process.exitCode = 1; }
else console.log("TGM0 TaskGroup: 9 current routes accepted, 17 implicit or unsafe routes rejected.");
