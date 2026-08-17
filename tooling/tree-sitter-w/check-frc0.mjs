import { main } from "../check-final-research-closure.mjs";
import path from "node:path";

if (!main()) process.exit(1);

const grammarRoot = path.resolve(import.meta.dir);
const treeSitter = path.join(grammarRoot, "node_modules", "tree-sitter-cli", "tree-sitter.exe");
const fixtures = [
  path.resolve(grammarRoot, "..", "studies", "final-research-closure", "current.w"),
  path.resolve(grammarRoot, "..", "studies", "final-research-closure", "adversarial.w"),
];
const parse = Bun.spawnSync([
  treeSitter,
  "parse",
  "--grammar-path",
  ".",
  "--quiet",
  "--stat",
  ...fixtures.map((fixture) => path.relative(grammarRoot, fixture)),
], { cwd: grammarRoot, stdout: "pipe", stderr: "pipe" });
if (parse.exitCode !== 0) {
  process.stderr.write(new TextDecoder().decode(parse.stderr));
  process.exit(1);
}
process.stdout.write("FRC0 nested tree-sitter parse: current.w and adversarial.w accepted.\n");
