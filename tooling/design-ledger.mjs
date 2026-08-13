import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const rootDirectory = path.resolve(toolingDirectory, "..");
const rationalePath = path.join(rootDirectory, "RATIONALE.md");

export const rationaleText = fs.readFileSync(rationalePath, "utf8");
const ledgerStart = rationaleText.indexOf("## 3. Ledger");
if (ledgerStart < 0) throw new Error("design ledger heading is missing");
export const ledgerRows = [...rationaleText.slice(ledgerStart).matchAll(/^\| (W-\d{3,}) \| ([^|]+) \|/gm)].map(
  (match) => ({ id: match[1], theme: match[2].trim() }),
);

if (ledgerRows.length === 0) {
  throw new Error("design ledger is empty");
}

export const ledgerIds = ledgerRows.map((row) => row.id);
export const ledgerIdSet = new Set(ledgerIds);
export const ledgerThemeById = new Map(ledgerRows.map((row) => [row.id, row.theme]));

if (ledgerIdSet.size !== ledgerIds.length) {
  throw new Error("design ledger contains duplicate IDs");
}

for (const [index, id] of ledgerIds.entries()) {
  const expected = `W-${String(index + 1).padStart(3, "0")}`;
  if (id !== expected) {
    throw new Error(`design ledger is not contiguous at ${id}; expected ${expected}`);
  }
}

export default {
  rationaleText,
  ledgerRows,
  ledgerIds,
  ledgerIdSet,
  ledgerThemeById,
};
