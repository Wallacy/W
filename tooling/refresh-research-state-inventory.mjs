import fs from "node:fs";
import path from "node:path";
import {
  loadResearchStateInventory,
  RESEARCH_STATE_INVENTORY_PATH,
  repositoryRoot,
  researchStateArtifactsDigest,
  sortResearchStateArtifacts,
  validateResearchStateInventory,
} from "./final-research-closure-machine.mjs";

const inventory = loadResearchStateInventory();
for (const family of inventory.families ?? []) {
  family.artifacts = sortResearchStateArtifacts(family.artifacts ?? []);
  family.artifactsDigest = researchStateArtifactsDigest(family.artifacts);
}

const validationErrors = validateResearchStateInventory(inventory);
if (validationErrors.length > 0) {
  process.stderr.write(`${validationErrors.join("\n")}\n`);
  process.exitCode = 1;
} else {
  const target = path.join(repositoryRoot, RESEARCH_STATE_INVENTORY_PATH);
  const temporary = `${target}.tmp-${process.pid}-${Date.now()}`;
  try {
    fs.writeFileSync(temporary, `${JSON.stringify(inventory, null, 2)}\n`, "utf8");
    fs.renameSync(temporary, target);
    process.stdout.write(`Research-state inventory refreshed: ${inventory.families?.length ?? 0} families.\n`);
  } finally {
    if (fs.existsSync(temporary)) fs.rmSync(temporary, { force: true });
  }
}
