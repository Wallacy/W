import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { states, loadData, validate } from "./oracle.mjs";

const directory = path.dirname(fileURLToPath(import.meta.url));
const study = JSON.parse(fs.readFileSync(path.join(directory, "study.json"), "utf8"));
const errors = [...validate(loadData()).errors];
if (study.id !== "SEA0" || study.decision !== "W-1474" || study.status !== "complete-design-study") errors.push("SEA0 study identity/status is invalid");
if (study.evidenceBoundary?.researchClosed !== true) errors.push("SEA0 research closure is absent");
if (JSON.stringify(study.states) !== JSON.stringify(states)) errors.push("SEA0 state list drifted");
if (study.evidenceBoundary?.simulationIsProviderProof !== false || study.evidenceBoundary?.rollbackPromised !== false || study.evidenceBoundary?.exactlyOncePromised !== false) errors.push("SEA0 evidence boundary makes an invalid promise");
if (study.testLanes?.length !== 4 || !JSON.stringify(study).includes("productionAndTestMachine")) errors.push("SEA0 test-infrastructure lane is incomplete");
const requiredSources = [
  "https://github.com/cloudflare/cloudflare-os",
  "https://apple.github.io/foundationdb/testing.html",
  "https://www.foundationdb.org/files/fdb-paper.pdf",
  "https://www.microsoft.com/en-us/research/publication/predictable-and-progressive-testing-of-multithreaded-code/",
];
const sourceUrls = new Set((study.sources ?? []).map((source) => source.url));
if (!requiredSources.every((url) => sourceUrls.has(url))) errors.push("SEA0 primary source list is incomplete");
if (errors.length) { console.error(errors.join("\n")); process.exitCode = 1; }
else console.log("SEA0 oracle/check: bounded approval machine, causal DAG, stale revalidation, unknown outcome, and test lanes");
