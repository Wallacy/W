import { deriveClosure, loadCorpus } from "./design-research-closure-machine.mjs";

const result = deriveClosure(loadCorpus());
if (result.errors.length > 0) {
  for (const error of result.errors) console.error(error);
  process.exitCode = 1;
} else {
  console.log("DRC0 design research closure: 4/4 stop conditions satisfied, historical Research=0 through W-1459; W-1486/W-1503 are historical post-snapshot provenance and active research gates are []; implementation boundaries preserved");
}
