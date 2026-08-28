import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const ROOT = path.resolve(import.meta.dirname, "..");
const CATALOG_PATH = path.join(ROOT, "tooling", "diagnostic-catalog.json");
const DESIGN_PATH = path.join(ROOT, "DESIGN.md");
const OUTPUT_PATH = path.join(ROOT, "DIAGNOSTICS.md");
const CODE = /^W-[A-Z]+-[0-9]{4}$/u;
const SCHEMA = "w-human-diagnostic-catalog-1";

function digestBytes(value) {
  return `sha256:${crypto.createHash("sha256").update(value).digest("hex")}`;
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function firstLiteralOffset(source, literal) {
  const pattern = new RegExp(
    `(?<![A-Za-z0-9_-])${escapeRegExp(literal)}(?![A-Za-z0-9_-])`,
    "u",
  );
  return source.search(pattern);
}

function normalizeLineEndings(source) {
  return source.replace(/\r\n?/gu, "\n");
}

function markdownSlug(text) {
  return text
    .replace(/<[^>]+>/g, "")
    .replace(/!\[([^\]]*)\]\([^)]*\)/g, "$1")
    .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1")
    .replace(/[`*_~]/g, "")
    .trim()
    .toLocaleLowerCase("en-US")
    .replace(/[^\p{L}\p{M}\p{N}\s_-]/gu, "")
    .replace(/\s+/g, "-");
}

function parseDesign(designText) {
  const source = normalizeLineEndings(designText);
  const lines = source.split("\n");
  const headings = [];
  const anchors = new Map();
  const errors = [];
  const slugCounts = new Map();
  let offset = 0;

  for (const [index, line] of lines.entries()) {
    const heading = /^(#{1,6})\s+(.+?)\s*#*\s*$/u.exec(line);
    if (heading) {
      const title = heading[2];
      const base = markdownSlug(title);
      if (!base) {
        errors.push(`DESIGN.md:${index + 1}: heading has no usable anchor.`);
      } else {
        const duplicate = slugCounts.get(base) ?? 0;
        const anchor = duplicate === 0 ? base : `${base}-${duplicate}`;
        slugCounts.set(base, duplicate + 1);
        const record = {
          anchor,
          base,
          level: heading[1].length,
          line: index + 1,
          offset,
          title,
        };
        headings.push(record);
        const records = anchors.get(anchor) ?? [];
        records.push({ kind: "heading", ...record });
        anchors.set(anchor, records);
      }
    }

    for (const explicit of line.matchAll(/<(?:a|span)\s+(?:id|name)=["']([^"']+)["']/giu)) {
      const anchor = explicit[1].toLocaleLowerCase("en-US");
      const records = anchors.get(anchor) ?? [];
      records.push({ kind: "explicit", line: index + 1, anchor });
      anchors.set(anchor, records);
    }

    offset += line.length + 1;
  }

  for (const [anchor, records] of anchors.entries()) {
    if (records.length > 1) {
      errors.push(`DESIGN.md: anchor #${anchor} is ambiguous (${records.length} targets).`);
    }
  }

  return { anchors, errors, headings };
}

function previousHeading(headings, offset) {
  let result = null;
  for (const heading of headings) {
    if (heading.offset > offset) break;
    result = heading;
  }
  return result;
}

function resolveDesignReferences(catalog, designText) {
  const source = normalizeLineEndings(designText);
  const parsed = parseDesign(source);
  const references = [];
  const errors = [...parsed.errors];

  for (const entry of catalog.codes ?? []) {
    const family = entry.code?.replace(/[0-9]{4}$/u, "*");
    const exactOffset = firstLiteralOffset(source, entry.code ?? "");
    const familyOffset = firstLiteralOffset(source, family ?? "");
    const mode = exactOffset >= 0 ? "exact" : familyOffset >= 0 ? "family" : null;
    const offset = exactOffset >= 0 ? exactOffset : familyOffset;

    if (!mode) {
      errors.push(`${entry.code ?? "<unknown>"}: DESIGN.md has no exact code or family wildcard reference.`);
      continue;
    }

    const heading = previousHeading(parsed.headings, offset);
    if (!heading) {
      errors.push(`${entry.code}: DESIGN.md reference has no preceding heading.`);
      continue;
    }

    const targets = parsed.anchors.get(heading.anchor) ?? [];
    if (targets.length !== 1 || targets[0].kind !== "heading") {
      errors.push(`${entry.code}: DESIGN.md heading anchor #${heading.anchor} is missing or ambiguous.`);
      continue;
    }

    references.push({
      code: entry.code,
      family,
      heading,
      mode,
      offset,
      token: mode === "exact" ? entry.code : family,
    });
  }

  return { anchors: parsed.anchors, errors, headings: parsed.headings, references };
}

function effectiveEntry(catalog, entry) {
  const profile = entry.profile ? catalog.profiles?.[entry.profile] : null;
  return {
    ...entry,
    defaultSeverity: entry.defaultSeverity ?? profile?.defaultSeverity,
    fixes: entry.fixes ?? profile?.fixes,
    labelRoles: entry.labelRoles ?? profile?.labelRoles,
    phase: entry.phase ?? profile?.phase,
    requiredFacts: entry.requiredFacts ?? profile?.requiredFacts,
  };
}

function validateCatalog(catalog) {
  const errors = [];
  if (!catalog || typeof catalog !== "object") return ["diagnostic catalog is not an object."];
  if (catalog.$schema !== "w-diagnostic-catalog-1") errors.push("diagnostic catalog has an unexpected schema.");
  if (!catalog.profiles || Array.isArray(catalog.profiles)) errors.push("diagnostic catalog has no profiles object.");
  if (!Array.isArray(catalog.codes) || catalog.codes.length === 0) {
    errors.push("diagnostic catalog has no codes.");
    return errors;
  }

  const codes = new Set();
  for (const [index, raw] of catalog.codes.entries()) {
    if (!raw || typeof raw !== "object") {
      errors.push(`catalog entry ${index + 1} is not an object.`);
      continue;
    }
    if (!CODE.test(raw.code ?? "")) errors.push(`catalog entry ${index + 1} has an invalid code ${JSON.stringify(raw.code)}.`);
    if (codes.has(raw.code)) errors.push(`catalog contains duplicate code ${raw.code}.`);
    codes.add(raw.code);
    if (!new Set(["active", "reserved", "retired"]).has(raw.state)) errors.push(`${raw.code} has an invalid state.`);
    if (raw.profile && !catalog.profiles?.[raw.profile]) errors.push(`${raw.code} uses an unknown profile ${raw.profile}.`);
    const entry = effectiveEntry(catalog, raw);
    for (const field of ["phase", "defaultSeverity", "requiredFacts", "labelRoles", "fixes"]) {
      if (entry[field] === undefined) errors.push(`${raw.code} has no resolved ${field}.`);
    }
    if (typeof raw.meaning !== "string" || raw.meaning.length === 0) errors.push(`${raw.code} has no meaning.`);
  }
  return errors;
}

function sortKeys(value) {
  return Object.keys(value ?? {}).sort((left, right) => Buffer.from(left).compare(Buffer.from(right)));
}

function formatLimit(value) {
  if (value === null || value === undefined) return "unbounded";
  return String(value);
}

function renderFacts(facts) {
  const keys = sortKeys(facts);
  if (keys.length === 0) return ["  - none"];
  return keys.map((key) => `  - \`${key}\`: \`${facts[key]}\``);
}

function renderRoles(roles) {
  const keys = sortKeys(roles);
  if (keys.length === 0) return ["  - none"];
  return keys.map((key) => {
    const role = roles[key];
    return `  - \`${key}\`: minimum \`${formatLimit(role.minimum)}\`, maximum \`${formatLimit(role.maximum)}\``;
  });
}

function renderFixes(fixes) {
  const keys = sortKeys(fixes);
  if (keys.length === 0) return ["  - none"];
  return keys.map((key) => `  - \`${key}\`: \`${fixes[key]}\``);
}

function countLabel(count, singular, plural) {
  return `${count} ${count === 1 ? singular : plural}`;
}

function headingLabel(heading) {
  return heading.title.replaceAll("[", "\\[").replaceAll("]", "\\]");
}

function familyEntries(entries) {
  const families = [];
  const byFamily = new Map();
  for (const entry of entries) {
    if (!byFamily.has(entry.family)) {
      const family = { family: entry.family, entries: [] };
      byFamily.set(entry.family, family);
      families.push(family);
    }
    byFamily.get(entry.family).entries.push(entry);
  }
  const compare = (left, right) => Buffer.from(left).compare(Buffer.from(right));
  families.sort((left, right) => compare(left.family, right.family));
  for (const family of families) family.entries.sort((left, right) => compare(left.code, right.code));
  return families;
}

function deriveProjection(catalog, designText, metadata = {}) {
  const catalogErrors = validateCatalog(catalog);
  if (catalogErrors.length > 0) throw new Error(catalogErrors.join("\n"));

  const design = resolveDesignReferences(catalog, designText);
  if (design.errors.length > 0) throw new Error(design.errors.join("\n"));

  const references = new Map(design.references.map((reference) => [reference.code, reference]));
  const entries = catalog.codes.map((raw, index) => {
    const entry = effectiveEntry(catalog, raw);
    return {
      ...entry,
      family: raw.code.replace(/-[0-9]{4}$/u, ""),
      index,
      reference: references.get(raw.code),
    };
  });
  const families = familyEntries(entries);
  const stateCounts = Object.fromEntries(
    [...new Set(entries.map((entry) => entry.state))].sort().map((state) => [state, entries.filter((entry) => entry.state === state).length]),
  );
  const modeCounts = Object.fromEntries(
    ["exact", "family"].map((mode) => [mode, entries.filter((entry) => entry.reference.mode === mode).length]),
  );

  return {
    catalog,
    catalogDigest: metadata.catalogDigest ?? null,
    design,
    entries,
    families,
    modeCounts,
    schema: SCHEMA,
    stateCounts,
  };
}

function renderDiagnostics(projection) {
  const catalogDigest = projection.catalogDigest ?? "sha256:unknown";
  const lines = [
    "# Catálogo humano de diagnostics W",
    "",
    "> Este arquivo é uma projeção humana gerada de `tooling/diagnostic-catalog.json`.",
    "> O JSON é a superfície de máquina para os metadados dos diagnostics.",
    "> O contrato e a semântica pertencem a `DESIGN.md`.",
    "> O gate verifica drift estrutural do catálogo, do output e dos vínculos de código, família e heading.",
    "> O gate não prova equivalência textual entre `meaning` e `DESIGN.md`.",
    "> `DESIGN.md` continua a autoridade semântica.",
    "> Este documento não cria uma autoridade nova.",
    "> Não edite este arquivo. Use `bun tooling/diagnostic-catalog.mjs --write`.",
    "",
    `- Catalog digest: \`${catalogDigest}\``,
    `- Entries: \`${projection.entries.length}\``,
    `- Families: \`${projection.families.length}\``,
    `- Design references: \`${projection.modeCounts.exact}\` exact, \`${projection.modeCounts.family}\` family`,
    `- States: ${Object.entries(projection.stateCounts).map(([state, count]) => `\`${state}\` ${count}`).join(", ")}`,
    "",
    "## Como ler",
    "",
    "Cada entrada mantém os nomes do JSON para facilitar a busca cruzada.",
    "",
    "- `state` identifica o estado de ciclo de vida do código.",
    "- `phase` e `severity` são os valores efetivos após a expansão de `profile`.",
    "- `meaning` vem do catálogo. O contrato normativo está no link de `DESIGN.md`.",
    "- O catálogo e seus oracles de design não provam que o compiler atual emite todos os códigos.",
    "- `requiredFacts`, `labelRoles` e `fixes` descrevem os dados exigidos, os papéis de label e a aplicabilidade do fix.",
    "- `exact` usa a primeira ocorrência literal do código em `DESIGN.md`.",
    "- `family` usa a primeira ocorrência literal de `W-FAMILY-*` quando o código não aparece literalmente.",
    "",
    "## Índice por família",
    "",
  ];

  for (const family of projection.families) {
    lines.push(`- [\`${family.family}\`](#${family.family.toLocaleLowerCase("en-US")}) — ${countLabel(family.entries.length, "entrada", "entradas")}`);
  }

  lines.push("", "## Entradas", "");
  for (const family of projection.families) {
    lines.push(`### ${family.family}`, "");
    for (const entry of family.entries) {
      const reference = entry.reference;
      lines.push(
        `#### ${entry.code}`,
        "",
        `- \`state\`: \`${entry.state}\``,
        `- \`phase\`: \`${entry.phase}\``,
        `- \`severity\`: \`${entry.defaultSeverity}\``,
      );
      if (entry.profile) lines.push(`- \`profile\`: \`${entry.profile}\``);
      lines.push(`- \`meaning\`: ${entry.meaning}`, "", "- `requiredFacts`:");
      lines.push(...renderFacts(entry.requiredFacts));
      lines.push("", "- `labelRoles`:");
      lines.push(...renderRoles(entry.labelRoles));
      lines.push("", "- `fixes`:");
      lines.push(...renderFixes(entry.fixes));
      lines.push(
        "",
        `- Design authority: \`${reference.mode}\` \`${reference.token}\` — [${headingLabel(reference.heading)}](DESIGN.md#${reference.heading.anchor})`,
        "",
      );
    }
  }

  return `${lines.join("\n").replace(/\n+$/u, "")}\n`;
}

function metadataValue(markdown, label) {
  const match = new RegExp("^- " + escapeRegExp(label) + ": `([^`]+)`$", "mu").exec(markdown);
  return match?.[1] ?? null;
}

function renderedCodes(markdown) {
  return [...markdown.matchAll(/^#### (W-[A-Z]+-[0-9]{4})\s*$/gmu)].map((match) => match[1]);
}

function renderedAuthority(markdown) {
  const records = new Map();
  const lines = markdown.split(/\r?\n/u);
  let code = null;
  for (const line of lines) {
    const heading = /^#### (W-[A-Z]+-[0-9]{4})\s*$/u.exec(line);
    if (heading) {
      code = heading[1];
      continue;
    }
    if (!code) continue;
    const authority = /^- Design authority: `([^`]+)` `([^`]+)` — \[[^\]]*\]\(DESIGN\.md#([^)]*)\)$/u.exec(line);
    if (authority) records.set(code, { mode: authority[1], token: authority[2], anchor: authority[3] });
  }
  return records;
}

function checkRenderedDiagnostics(markdown, projection) {
  const errors = [];
  if (typeof markdown !== "string") return ["DIAGNOSTICS.md is missing."];

  const expectedCodes = projection.families.flatMap((family) => family.entries.map((entry) => entry.code));
  const actualCodes = renderedCodes(markdown);
  const expectedSet = new Set(expectedCodes);
  const actualSet = new Set(actualCodes);
  const missing = expectedCodes.filter((code) => !actualSet.has(code));
  const extra = actualCodes.filter((code) => !expectedSet.has(code));
  const duplicate = actualCodes.filter((code, index) => actualCodes.indexOf(code) !== index);
  if (missing.length > 0) errors.push(`DIAGNOSTICS.md is missing codes: ${missing.join(", ")}.`);
  if (extra.length > 0) errors.push(`DIAGNOSTICS.md contains extra codes: ${extra.join(", ")}.`);
  if (duplicate.length > 0) errors.push(`DIAGNOSTICS.md contains duplicate code entries: ${[...new Set(duplicate)].join(", ")}.`);
  if (actualCodes.length === expectedCodes.length && actualCodes.some((code, index) => code !== expectedCodes[index])) {
    errors.push("DIAGNOSTICS.md code ordering is stale.");
  }

  const catalogDigest = metadataValue(markdown, "Catalog digest");
  if (catalogDigest !== projection.catalogDigest) errors.push("DIAGNOSTICS.md catalog digest is stale.");

  const authorities = renderedAuthority(markdown);
  for (const entry of projection.entries) {
    const expected = entry.reference;
    const actual = authorities.get(entry.code);
    if (!actual) {
      errors.push(`${entry.code}: DIAGNOSTICS.md has no authority link.`);
      continue;
    }
    if (actual.mode !== expected.mode || actual.token !== expected.token || actual.anchor !== expected.heading.anchor) {
      errors.push(`${entry.code}: DIAGNOSTICS.md authority link is stale or ambiguous.`);
    }
    const targets = projection.design.anchors.get(actual.anchor) ?? [];
    if (targets.length !== 1 || targets[0].kind !== "heading") {
      errors.push(`${entry.code}: DIAGNOSTICS.md authority anchor #${actual.anchor} is missing or ambiguous.`);
    }
  }

  const expectedText = renderDiagnostics(projection);
  if (markdown !== expectedText) errors.push("DIAGNOSTICS.md is stale or manually edited.");
  return [...new Set(errors)];
}

function readProjection() {
  const catalogBytes = fs.readFileSync(CATALOG_PATH);
  const designBytes = fs.readFileSync(DESIGN_PATH);
  const catalog = JSON.parse(catalogBytes.toString("utf8"));
  return deriveProjection(catalog, designBytes.toString("utf8"), {
    catalogDigest: digestBytes(catalogBytes),
  });
}

function main() {
  const mode = process.argv[2] ?? "--check";
  if (!new Set(["--write", "--check"]).has(mode)) throw new Error("usage: bun tooling/diagnostic-catalog.mjs --write|--check");
  const projection = readProjection();
  if (mode === "--write") {
    fs.writeFileSync(OUTPUT_PATH, renderDiagnostics(projection), "utf8");
    return;
  }
  const markdown = fs.existsSync(OUTPUT_PATH) ? fs.readFileSync(OUTPUT_PATH, "utf8") : null;
  const errors = checkRenderedDiagnostics(markdown, projection);
  if (errors.length > 0) throw new Error(errors.join("\n"));
}

if (import.meta.main) {
  try {
    main();
    console.log(`diagnostic-catalog ${process.argv[2] ?? "--check"}: ok`);
  } catch (error) {
    console.error(`diagnostic-catalog: ${error.message}`);
    process.exitCode = 1;
  }
}

export {
  checkRenderedDiagnostics,
  deriveProjection,
  effectiveEntry,
  markdownSlug,
  parseDesign,
  renderDiagnostics,
  resolveDesignReferences,
  validateCatalog,
};
