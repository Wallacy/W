import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const STORY = path.join(ROOT, "reference", "last-light", "story.json");
const DOCUMENT = path.join(ROOT, "reference", "last-light", "STORY.md");
const ATLAS = path.join(ROOT, "reference", "syntax-atlas", "atlas-manifest.json");
const SCHEMA = "w-last-light-story-1";

function contained(root, candidate) {
  const relative = path.relative(root, candidate);
  return relative === "" || (!relative.startsWith(`..${path.sep}`) && relative !== ".." && !path.isAbsolute(relative));
}

function load() {
  const story = JSON.parse(fs.readFileSync(STORY, "utf8"));
  const atlas = JSON.parse(fs.readFileSync(ATLAS, "utf8"));
  return { story, atlas };
}

function validate(story, atlas) {
  const errors = [];
  if (story?.$schema !== SCHEMA) errors.push(`story must use ${SCHEMA}`);
  if (typeof story?.title !== "string" || typeof story?.premise !== "string") errors.push("story title and premise are required");
  if (!Array.isArray(story?.acts) || story.acts.length < 3) errors.push("story needs at least three ordered acts");
  const actIds = new Set();
  const coveredBlocks = new Set();
  const knownBlocks = new Set((atlas.blocks ?? []).map((block) => block.id));
  const knownVariants = new Set((atlas.variants ?? []).map((variant) => variant.id));
  const sourceWitnesses = new Set();
  for (const act of story.acts ?? []) {
    if (!act?.id || actIds.has(act.id)) errors.push(`missing or duplicate act id ${String(act?.id)}`);
    actIds.add(act?.id);
    if (!act?.summary || !Array.isArray(act.acceptance) || act.acceptance.length === 0) errors.push(`act ${act?.id} lacks narrative or acceptance`);
    for (const block of act.atlasBlocks ?? []) {
      if (!knownBlocks.has(block)) errors.push(`act ${act.id} references unknown atlas block ${block}`);
      if (coveredBlocks.has(block)) errors.push(`atlas block ${block} belongs to more than one act`);
      coveredBlocks.add(block);
    }
    if (!Array.isArray(act.sourceRefs) || act.sourceRefs.length === 0) errors.push(`act ${act?.id} has no source witnesses`);
    for (const reference of act.sourceRefs ?? []) {
      const file = path.resolve(ROOT, reference.path ?? "");
      const witnessKey = `${reference.path}\0${reference.symbol}`;
      if (sourceWitnesses.has(witnessKey)) errors.push(`story source witness is assigned more than once: ${String(reference.path)} ${JSON.stringify(reference.symbol)}`);
      sourceWitnesses.add(witnessKey);
      if (!contained(ROOT, file) || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
        errors.push(`act ${act.id} has missing or escaping source ${String(reference.path)}`);
        continue;
      }
      const source = fs.readFileSync(file, "utf8");
      const occurrences = typeof reference.symbol === "string" ? source.split(reference.symbol).length - 1 : 0;
      if (occurrences !== 1) errors.push(`${reference.path} story symbol ${JSON.stringify(reference.symbol)} occurs ${occurrences} times`);
    }
  }
  const missingBlocks = [...knownBlocks].filter((block) => !coveredBlocks.has(block));
  if (missingBlocks.length) errors.push(`story does not assign atlas blocks: ${missingBlocks.join(", ")}`);
  const coveredVariants = new Set(
    (atlas.variants ?? [])
      .filter((variant) => coveredBlocks.has(variant.block))
      .map((variant) => variant.id),
  );
  const missingVariants = [...knownVariants].filter((variant) => !coveredVariants.has(variant));
  if (missingVariants.length) errors.push(`story does not assign atlas variants: ${missingVariants.join(", ")}`);
  if ((atlas.variants ?? []).length < 90) errors.push("story expects the expanded atomic atlas variant inventory");
  return errors;
}

function render(story, atlas) {
  const blockById = new Map(atlas.blocks.map((block) => [block.id, block]));
  const variantsByBlock = new Map();
  for (const variant of atlas.variants) {
    const variants = variantsByBlock.get(variant.block) ?? [];
    variants.push(variant.id);
    variantsByBlock.set(variant.block, variants);
  }
  const lines = [
    `# ${story.title}`,
    "",
    "> Generated from `story.json`. Edit the manifest, then run `bun tooling/last-light-story.mjs --write`.",
    "",
    story.premise,
    "",
    "The Syntax Atlas owns the complete atomic source-form inventory. Last Light owns the connected application story. A form may be parse-only until its compiler or provider exists, but it may not be represented by an invalid placeholder statement.",
    "",
    `Current inventory: ${atlas.grammarRules.length} public grammar rules, ${atlas.variants.length} accepted atomic variants, and ${atlas.blocks.length} story-assigned Atlas blocks. Mutually exclusive roots and entry defaults live in separate modules rather than being stacked into an impossible program.`,
    "",
  ];
  story.acts.forEach((act, index) => {
    lines.push(`## ${index + 1}. ${act.id.replaceAll("-", " ")}`, "", act.summary, "", "Source witnesses:", "");
    for (const reference of act.sourceRefs) {
      const symbol = reference.symbol.includes("\n") ? `${reference.symbol.split("\n")[0]} …` : reference.symbol;
      lines.push(`- \`${reference.path}\` — \`${symbol}\``);
    }
    lines.push("", "Acceptance:", "");
    for (const fact of act.acceptance) lines.push(`- ${fact}`);
    if (act.atlasBlocks.length) {
      lines.push("", "Atlas application families:", "", "| Block | Family | Atomic variants assigned to this act |", "| --- | --- | --- |");
      for (const id of act.atlasBlocks) {
        const block = blockById.get(id);
        const variants = (variantsByBlock.get(id) ?? []).map((variant) => `\`${variant}\``).join(", ") || "—";
        lines.push(`| \`${id}\` | ${block.family} | ${variants} |`);
      }
    }
    lines.push("");
  });
  lines.push(
    "## Evidence boundary",
    "",
    "The deterministic `LastLightSimulation` source is the first coherent execution target: quiet orbit, photon rush, and timeline collision already have explicit expected facts in `simulation.w`. The full service, UI, device, registry, and deployment products remain design sources until their corresponding compiler/runtime/provider gates exist.",
    "",
    "Completeness is collective, not a claim that one binary should exercise mutually exclusive package roots, entries, targets, or foreign adapters. The gate requires every Atlas block to belong to exactly one narrative act and every act to cite real, unique W source witnesses.",
    "",
  );
  return lines.join("\n");
}

function main() {
  const mode = process.argv[2] ?? "--check";
  const { story, atlas } = load();
  const errors = validate(story, atlas);
  if (errors.length) throw new Error(errors.join("\n"));
  const document = render(story, atlas);
  if (mode === "--write") fs.writeFileSync(DOCUMENT, document, "utf8");
  else if (mode !== "--check") throw new Error("usage: bun tooling/last-light-story.mjs --write|--check");
  else if (!fs.existsSync(DOCUMENT) || fs.readFileSync(DOCUMENT, "utf8") !== document) throw new Error("reference/last-light/STORY.md is stale");
  console.log(`last-light story ${mode}: ok (${atlas.blocks.length} blocks; ${atlas.variants.length} variants)`);
}

if (import.meta.main) {
  try { main(); } catch (error) { console.error(`last-light story: ${error.message}`); process.exitCode = 1; }
}

export { load, render, validate };
