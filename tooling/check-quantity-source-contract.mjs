import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const rootDirectory = path.resolve(toolingDirectory, "..");
const files = {
  si: path.join(rootDirectory, "std", "si", "contracts.w"),
  iec: path.join(rootDirectory, "std", "iec", "contracts.w"),
  quantity: path.join(rootDirectory, "reference", "last-light", "quantity_oracle.w"),
  units: path.join(rootDirectory, "reference", "last-light", "units.w"),
  readme: path.join(rootDirectory, "reference", "last-light", "README.md"),
};
const errors = [];
const source = Object.fromEntries(Object.entries(files).map(([key, file]) => {
  if (!fs.existsSync(file)) errors.push(`missing ${key} source`);
  return [key, fs.existsSync(file) ? fs.readFileSync(file, "utf8") : ""];
}));

function requireText(key, text, label) {
  if (!source[key].includes(text)) errors.push(`${key} missing ${label}`);
}

function rejectPattern(key, pattern, label) {
  if (pattern.test(source[key])) errors.push(`${key} has forbidden ${label}`);
}

requireText("si", "export dimension TemperatureDelta", "TemperatureDelta dimension");
requireText("si", "export unit deltaK: TemperatureDelta", "deltaK reference");
requireText("si", "export dimension Temperature\nexport dimension TemperatureDelta", "distinct point/delta dimensions");
requireText("si", "export unit K: Temperature\nexport unit deltaK: TemperatureDelta", "distinct point/delta references");
requireText("iec", "export unit byte = 8<bit>", "byte reference conversion");
requireText("quantity", "  degC,", "explicit degC import");
requireText("quantity", "30<si.s>", "qualified seconds literal");
requireText("quantity", "0.5<si.min>", "qualified minutes literal");
requireText("quantity", "64<iec.KiB>", "qualified IEC literal");
requireText("quantity", "exactValue(in: iec.byte)", "qualified byte conversion");
requireText("quantity", "value(in: si.deltaK)", "qualified delta conversion");
requireText("units", "Quantity<si.TemperatureDelta, f64>", "TemperatureDelta alias");

rejectPattern("quantity", /(?<![.\w])<s>/, "ambient seconds unit");
rejectPattern("quantity", /(?<![.\w])<min>/, "ambient minutes unit");
rejectPattern("quantity", /(?<![.\w])<KiB>/, "ambient IEC unit");
rejectPattern("quantity", /(?<![.\w])<B>/, "ambient byte unit");
rejectPattern("quantity", /(?<![.\w])<degC>/, "ambient affine temperature unit");
rejectPattern("quantity", /exactValue\(in:\s*B\b/, "B byte alias");
rejectPattern("iec", /export\s+unit\s+B\b/, "ambient B alias");
rejectPattern("readme", /`30<s>`|`0\.5<min>`|`\d+<(?:KiB|B)>`/, "legacy unqualified quantity example");

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

process.stdout.write("Quantity source contract: qualified bindings and point/delta spellings are stable.\n");
