import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "url";
import { classes, loadData, validate } from "./oracle.mjs";

const directory = path.dirname(fileURLToPath(import.meta.url));
const study = JSON.parse(fs.readFileSync(path.join(directory, "study.json"), "utf8"));
const errors = [...validate(loadData()).errors];
if (study.id !== "LLM0" || study.decision !== "W-1475" || study.status !== "complete-design-study") errors.push("LLM0 study identity/status is invalid");
if (study.evidenceBoundary?.researchClosed !== true) errors.push("LLM0 research closure is absent");
if (study.defaultRecommendation !== "avoid core inflation") errors.push("LLM0 default recommendation drifted");
for (const key of ["frameworkImplemented", "kernelsImplemented", "providerImplemented", "performanceMeasured", "apiPromotion"]) {
  if (study.evidenceBoundary?.[key] !== false) errors.push(`LLM0 evidence boundary must keep ${key}=false`);
}
if (JSON.stringify(study.classificationClasses) !== JSON.stringify(classes)) errors.push("LLM0 classification list drifted");
if ((study.sources ?? []).length < 10) errors.push("LLM0 primary source list is incomplete");
if (errors.length) { console.error(errors.join("\n")); process.exitCode = 1; }
else console.log("LLM0 oracle/check: existing W coverage, 19 bounded gaps, two source-shaped workloads, and no implementation claims");
