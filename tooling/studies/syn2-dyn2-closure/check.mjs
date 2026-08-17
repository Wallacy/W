import { runSyn2Dyn2Closure } from "../../check-syn2-dyn2-closure.mjs";

const result = runSyn2Dyn2Closure({ write: process.argv.includes("--write") });
if (result.errors.length > 0) {
  process.stderr.write(`${result.errors.join("\n")}\n`);
  process.exit(1);
}
process.stdout.write(`SYN2/DYN2 nested check: ${result.summary.caseCount} cases, ${result.summary.currentContractCount} current, ${result.summary.rejectedCount} rejected.\n`);
