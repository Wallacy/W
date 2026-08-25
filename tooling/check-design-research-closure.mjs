import { deriveClosure, loadCorpus } from "./design-research-closure-machine.mjs";

const result = deriveClosure(loadCorpus());
if (result.errors.length > 0) {
  for (const error of result.errors) console.error(error);
  process.exitCode = 1;
} else {
  console.log("DRC0 design research closure: 4/4 stop conditions satisfied, Research=0, implementation boundaries preserved");
}
