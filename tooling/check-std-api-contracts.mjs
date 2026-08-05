import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const rootDirectory = path.dirname(toolingDirectory);
const catalogPath = path.join(toolingDirectory, "std-api-contracts.json");
const snapshotPath = path.join(toolingDirectory, "std-api-surface.snapshot.json");
const designPath = path.join(rootDirectory, "DESIGN.md");
const write = process.argv.includes("--write");

const catalog = JSON.parse(fs.readFileSync(catalogPath, "utf8"));
const design = fs.readFileSync(designPath, "utf8");
const errors = [];

const allowedTiers = new Set(["T0", "T1", "T2"]);
const allowedFailureModes = new Set(["none", "typed", "generic"]);
const allowedImplementationProviderKinds = new Set(["std-intrinsic"]);
const allowedImplementationProviderStatuses = new Set(["available", "missing"]);
const requiredProfileFields = [
  "capabilities",
  "effects",
  "failures",
  "bounds",
  "complexity",
];

function digest(value) {
  return crypto.createHash("sha256").update(value).digest("hex");
}

function recursiveFiles(directory) {
  return fs.readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const entryPath = path.join(directory, entry.name);
    if (entry.isDirectory()) return recursiveFiles(entryPath);
    return entry.isFile() && entry.name.endsWith(".w") ? [entryPath] : [];
  });
}

function normalizeHeader(lines) {
  return lines
    .join(" ")
    .replace(/\s+/g, " ")
    .replace(/\s+([,:>])/g, "$1")
    .replace(/([<])\s+/g, "$1")
    .trim()
    .replace(/\s*\{\s*$/, "");
}

function structuralCharacters(line) {
  let result = "";
  let quote = null;
  let escaped = false;

  for (let index = 0; index < line.length; index += 1) {
    const character = line[index];
    const next = line[index + 1];

    if (!quote && character === "/" && next === "/") break;
    if (quote) {
      if (escaped) {
        escaped = false;
      } else if (character === "\\") {
        escaped = true;
      } else if (character === quote) {
        quote = null;
      }
      continue;
    }
    if (character === '"' || character === "'") {
      quote = character;
      continue;
    }
    result += character;
  }

  return result;
}

function extractExports(source, sourcePath) {
  const lines = source.split(/\r?\n/);
  const declarations = [];

  for (let index = 0; index < lines.length; index += 1) {
    const match = lines[index].match(
      /^export\s+(?:(?:async|const)\s+)?(type|alias|struct|object|enum|protocol|fn|unit)\s+([A-Za-z_][A-Za-z0-9_]*)/,
    );
    if (!match) continue;

    const [, kind, symbol] = match;
    const header = [lines[index].trim()];
    const declarationLines = [lines[index]];

    if (!header[0].includes("{") && !["type", "alias"].includes(kind)) {
      let cursor = index + 1;
      while (cursor < lines.length && !header.at(-1).includes("{")) {
        if (lines[cursor].trim() === "") break;
        header.push(lines[cursor].trim());
        cursor += 1;
      }
    } else if (["type", "alias"].includes(kind) && /=$/.test(header[0])) {
      let cursor = index + 1;
      while (cursor < lines.length && lines[cursor].trim() !== "") {
        header.push(lines[cursor].trim());
        cursor += 1;
      }
    }

    if (!["type", "alias"].includes(kind)) {
      let depth = 0;
      let bodyStarted = false;
      let cursor = index;

      while (cursor < lines.length) {
        if (cursor > index) declarationLines.push(lines[cursor]);
        const structural = structuralCharacters(lines[cursor]);
        for (const character of structural) {
          if (character === "{") {
            depth += 1;
            bodyStarted = true;
          } else if (character === "}") {
            depth -= 1;
          }
        }
        if (bodyStarted && depth === 0) break;
        cursor += 1;
      }
    } else {
      declarationLines.splice(0, declarationLines.length, ...header);
    }

    declarations.push({
      symbol,
      kind,
      line: index + 1,
      signature: normalizeHeader(header),
      declarationDigest: digest(declarationLines.join("\n")),
      declarationLines,
      source: sourcePath,
    });
  }

  return declarations;
}

function extractExportedMembers(declaration) {
  const members = [];
  const lines = declaration.declarationLines;
  const exportedMemberPattern = new RegExp(
    "^\\s*export\\s+" +
      "(?:(?:(?:static|async|const|mut|take)\\s+)*" +
      "(?:(fn)\\s+([A-Za-z_][A-Za-z0-9_]*)|(init)\\b)|" +
      "(?:var\\s+)?([A-Za-z_][A-Za-z0-9_]*)\\s*:)",
  );

  for (let index = 1; index < lines.length; index += 1) {
    const match = lines[index].match(exportedMemberPattern);
    if (!match) continue;

    const symbol = match[3] ? "init" : match[2] ?? match[4];
    const kind = match[4] ? "property" : match[3] ? "initializer" : "method";
    const header = [lines[index].trim()];
    let cursor = index + 1;
    while (!header.at(-1).includes("{") && cursor < lines.length) {
      if (lines[cursor].trim() === "") break;
      header.push(lines[cursor].trim());
      cursor += 1;
    }

    members.push({
      symbol,
      kind,
      line: declaration.line + index,
      signature: normalizeHeader(header),
    });
  }

  return members;
}

function extractExtensions(source, owner) {
  const lines = source.split(/\r?\n/);
  const escapedOwner = owner.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const extensionPattern = new RegExp(
    `^extension(?:<[^{}]*>)?\\s+${escapedOwner}\\b`,
  );
  const declarations = [];

  for (let index = 0; index < lines.length; index += 1) {
    if (!/^extension\b/.test(lines[index])) continue;

    const header = [];
    let headerCursor = index;
    while (headerCursor < lines.length) {
      header.push(lines[headerCursor].trim());
      if (structuralCharacters(lines[headerCursor]).includes("{")) break;
      headerCursor += 1;
    }
    if (!extensionPattern.test(header.join(" ").replace(/\s+/g, " "))) continue;

    const declarationLines = [];
    let depth = 0;
    let bodyStarted = false;
    let cursor = index;

    while (cursor < lines.length) {
      declarationLines.push(lines[cursor]);
      const structural = structuralCharacters(lines[cursor]);
      for (const character of structural) {
        if (character === "{") {
          depth += 1;
          bodyStarted = true;
        } else if (character === "}") {
          depth -= 1;
        }
      }
      if (bodyStarted && depth === 0) break;
      cursor += 1;
    }

    declarations.push({ declarationLines, line: index + 1 });
    index = cursor;
  }

  return declarations;
}

function hasDesignAnchor(anchor) {
  const escaped = anchor.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  return new RegExp(`^#{3,4} ${escaped}(?:\\s|$)`, "m").test(design);
}

function hasToken(source, token) {
  const escaped = token.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  return new RegExp(`\\b${escaped}\\b`).test(source);
}

function resolveRequirement(requirement, moduleState) {
  if (requirement.kind === "symbol") {
    return moduleState.exports.has(requirement.surface);
  }

  if (requirement.kind === "member") {
    const [owner, member] = requirement.surface.split(".");
    if (!owner || !member || !moduleState.exports.has(owner)) return false;
    return new RegExp(`\\b(?:fn|init)\\s+${member}\\b`).test(moduleState.source);
  }

  errors.push(`${requirement.id}: kind must be symbol or member.`);
  return false;
}

if (catalog.$schema !== "w-std-api-contracts-1") {
  errors.push("std-api-contracts.json must use schema w-std-api-contracts-1.");
}
if (catalog.status !== "design-oracle-input") {
  errors.push("std-api-contracts.json must have status design-oracle-input.");
}

for (const [name, profile] of Object.entries(catalog.profiles ?? {})) {
  for (const field of requiredProfileFields) {
    if (!(field in profile)) errors.push(`profile ${name} is missing ${field}.`);
  }
  if (!Array.isArray(profile.capabilities) || !Array.isArray(profile.effects)) {
    errors.push(`profile ${name} capabilities and effects must be arrays.`);
  }
  if (!Array.isArray(profile.bounds) || profile.bounds.length === 0) {
    errors.push(`profile ${name} must declare at least one bound.`);
  }
  if (!allowedFailureModes.has(profile.failures?.mode)) {
    errors.push(`profile ${name} has an invalid failure mode.`);
  }
  if (!Array.isArray(profile.failures?.types)) {
    errors.push(`profile ${name} failures.types must be an array.`);
  }
  if (!profile.complexity?.time || !profile.complexity?.space) {
    errors.push(`profile ${name} must declare time and space complexity.`);
  }
}

const moduleIds = new Set();
const apiIds = new Set();
const moduleStates = new Map();
const snapshotModules = [];

for (const module of catalog.modules ?? []) {
  if (moduleIds.has(module.id)) errors.push(`duplicate module ${module.id}.`);
  moduleIds.add(module.id);

  if (!allowedTiers.has(module.tier)) {
    errors.push(`${module.id}: tier must be T0, T1, or T2.`);
  }
  if (!module.availability) errors.push(`${module.id}: availability is required.`);

  const absoluteSource = path.join(rootDirectory, module.source ?? "");
  if (!fs.existsSync(absoluteSource)) {
    errors.push(`${module.id}: source ${module.source} does not exist.`);
    continue;
  }

  const source = fs.readFileSync(absoluteSource, "utf8");
  const declarations = extractExports(source, module.source);
  const exports = new Map(declarations.map((declaration) => [declaration.symbol, declaration]));
  moduleStates.set(module.id, { source, exports });

  const implementationProvider = module.implementationProvider;
  if (implementationProvider) {
    if (!allowedImplementationProviderKinds.has(implementationProvider.kind)) {
      errors.push(`${module.id}: implementation provider kind is invalid.`);
    }
    if (!allowedImplementationProviderStatuses.has(implementationProvider.status)) {
      errors.push(`${module.id}: implementation provider status is invalid.`);
    }
    if (
      !/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*(?:\.[a-z][a-z0-9]*(?:-[a-z0-9]+)*)+@[1-9][0-9]*$/.test(
        implementationProvider.id ?? "",
      )
    ) {
      errors.push(`${module.id}: implementation provider ID must be namespace.name@major.`);
    }
    if (
      !Array.isArray(implementationProvider.gates) ||
      implementationProvider.gates.length === 0 ||
      new Set(implementationProvider.gates).size !== implementationProvider.gates.length ||
      implementationProvider.gates.some((gate) => typeof gate !== "string" || gate.length === 0)
    ) {
      errors.push(`${module.id}: implementation provider gates must be unique nonempty strings.`);
    }
    if (
      implementationProvider.kind === "std-intrinsic" &&
      !source.includes(`foreign intrinsic from "${implementationProvider.id}"`)
    ) {
      errors.push(
        `${module.id}: source does not declare intrinsic provider ${implementationProvider.id}.`,
      );
    }
  }

  for (const anchor of module.designAnchors ?? []) {
    if (!hasDesignAnchor(anchor)) {
      errors.push(`${module.id}: design anchor ${anchor} does not exist.`);
    }
  }
  if (!Array.isArray(module.designAnchors) || module.designAnchors.length === 0) {
    errors.push(`${module.id}: at least one design anchor is required.`);
  }

  const catalogSymbols = new Set();
  const snapshotApis = [];
  for (const api of module.apis ?? []) {
    const apiId = `${module.id}.${api.symbol}`;
    if (apiIds.has(apiId)) errors.push(`duplicate API ${apiId}.`);
    apiIds.add(apiId);
    catalogSymbols.add(api.symbol);

    const declaration = exports.get(api.symbol);
    if (!declaration) {
      errors.push(`${apiId}: no matching exported declaration exists.`);
      continue;
    }
    if (declaration.kind !== api.kind) {
      errors.push(`${apiId}: catalog kind ${api.kind} does not match ${declaration.kind}.`);
    }
    if (!catalog.profiles?.[api.profile]) {
      errors.push(`${apiId}: unknown profile ${api.profile}.`);
    }

    const extensions = extractExtensions(source, api.symbol);
    const exportedMembers = [
      ...extractExportedMembers(declaration),
      ...extensions.flatMap((extension) => extractExportedMembers(extension)),
    ];
    const declarationDigest = digest(
      [
        ...declaration.declarationLines,
        ...extensions.flatMap((extension) => extension.declarationLines),
      ].join("\n"),
    );
    const snapshotMembers = [];
    const catalogMembers = new Set();
    for (const member of api.members ?? []) {
      if (catalogMembers.has(member.symbol)) {
        errors.push(`${apiId}.${member.symbol}: duplicate member profile.`);
      }
      catalogMembers.add(member.symbol);

      if (!catalog.profiles?.[member.profile]) {
        errors.push(`${apiId}.${member.symbol}: unknown profile ${member.profile}.`);
      }

      const matches = exportedMembers.filter((candidate) => candidate.symbol === member.symbol);
      const expectedOverloads = member.overloads ?? 1;
      if (matches.length !== expectedOverloads) {
        errors.push(
          `${apiId}.${member.symbol}: expected ${expectedOverloads} exported overloads, ` +
            `found ${matches.length}.`,
        );
      }

      snapshotMembers.push({
        symbol: member.symbol,
        ...(matches[0]?.kind === "property" ? { kind: "property" } : {}),
        overloads: matches.map((match) => ({
          signature: match.signature,
          line: match.line,
        })),
        profile: member.profile,
      });
    }
    if (api.members) {
      for (const member of exportedMembers) {
        if (!catalogMembers.has(member.symbol)) {
          errors.push(`${apiId}.${member.symbol}: exported member has no profile.`);
        }
      }
    }

    snapshotApis.push({
      symbol: api.symbol,
      kind: declaration.kind,
      signature: declaration.signature,
      declarationDigest,
      line: declaration.line,
      profile: api.profile,
      ...(snapshotMembers.length > 0 ? { members: snapshotMembers } : {}),
    });
  }

  for (const symbol of exports.keys()) {
    if (!catalogSymbols.has(symbol)) {
      errors.push(`${module.id}.${symbol}: exported declaration is not cataloged.`);
    }
  }
  for (const symbol of catalogSymbols) {
    if (!exports.has(symbol)) {
      errors.push(`${module.id}.${symbol}: catalog entry has no exported declaration.`);
    }
  }

  const consumerPath = path.join(rootDirectory, module.consumer?.path ?? "");
  if (!fs.existsSync(consumerPath)) {
    errors.push(`${module.id}: consumer ${module.consumer?.path} does not exist.`);
  } else {
    const consumer = fs.readFileSync(consumerPath, "utf8");
    if (!Array.isArray(module.consumer?.symbols) || module.consumer.symbols.length === 0) {
      errors.push(`${module.id}: consumer must name at least one symbol.`);
    } else {
      for (const symbol of module.consumer.symbols) {
        if (!hasToken(consumer, symbol)) {
          errors.push(`${module.id}: consumer does not contain ${symbol}.`);
        }
      }
    }
  }

  const orderedApis = snapshotApis.sort((left, right) => left.symbol.localeCompare(right.symbol));
  snapshotModules.push({
    id: module.id,
    tier: module.tier,
    availability: module.availability,
    source: module.source,
    ...(implementationProvider
      ? {
          implementationProvider: {
            kind: implementationProvider.kind,
            id: implementationProvider.id,
            status: implementationProvider.status,
            gates: [...implementationProvider.gates].sort(),
          },
        }
      : {}),
    catalogedDeclarationsDigest: digest(JSON.stringify(orderedApis)),
    apis: orderedApis,
  });
}

const carrierRequirementIds = new Set();
const carrierRequirementStates = new Map();
const carrierRequirementsById = new Map();
const snapshotCarrierRequirements = [];
const declaredCarrierConsumers = new Set(
  (catalog.referenceRequirements ?? []).map(
    (requirement) => `${requirement.module}.${requirement.surface.split(".")[0]}`,
  ),
);
for (const requirement of catalog.carrierRequirements ?? []) {
  if (carrierRequirementIds.has(requirement.id)) {
    errors.push(`duplicate carrier requirement ${requirement.id}.`);
  }
  carrierRequirementIds.add(requirement.id);
  carrierRequirementsById.set(requirement.id, requirement);

  if (!catalog.profiles?.[requirement.profile]) {
    errors.push(`${requirement.id}: unknown profile ${requirement.profile}.`);
  }
  if (!["required", "profile-final"].includes(requirement.readiness)) {
    errors.push(`${requirement.id}: readiness must be required or profile-final.`);
  }
  if (!Array.isArray(requirement.consumers) || requirement.consumers.length === 0) {
    errors.push(`${requirement.id}: at least one consumer is required.`);
  } else {
    for (const consumer of requirement.consumers) {
      if (!declaredCarrierConsumers.has(consumer)) {
        errors.push(`${requirement.id}: unknown consumer ${consumer}.`);
      }
    }
  }

  const provider = requirement.providerModule
    ? moduleStates.get(requirement.providerModule)
    : undefined;
  const providerCatalogModule = requirement.providerModule
    ? catalog.modules.find((module) => module.id === requirement.providerModule)
    : undefined;
  const actualStatus = provider?.exports.has(requirement.surface) ? "draft" : "missing";
  carrierRequirementStates.set(requirement.id, actualStatus);

  if (requirement.status !== actualStatus) {
    errors.push(
      `${requirement.id}: expected status ${requirement.status}, actual status ${actualStatus}.`,
    );
  }

  snapshotCarrierRequirements.push({
    id: requirement.id,
    providerModule: requirement.providerModule ?? null,
    surface: requirement.surface,
    status: actualStatus,
    readiness: requirement.readiness,
    profile: requirement.profile,
    consumers: [...(requirement.consumers ?? [])].sort(),
    ...(providerCatalogModule?.implementationProvider
      ? {
          implementationProvider: {
            id: providerCatalogModule.implementationProvider.id,
            status: providerCatalogModule.implementationProvider.status,
          },
        }
      : {}),
  });
}

const requirementIds = new Set();
const requirementSurfaces = new Set();
const requirementStatusBySurface = new Map();
const referenceRequirementStates = new Map();
const readinessCarrierIds = new Set();
const snapshotRequirements = [];
for (const requirement of catalog.referenceRequirements ?? []) {
  if (requirementIds.has(requirement.id)) {
    errors.push(`duplicate reference requirement ${requirement.id}.`);
  }
  requirementIds.add(requirement.id);
  requirementSurfaces.add(`${requirement.module}:${requirement.surface}`);

  const moduleState = moduleStates.get(requirement.module);
  if (!moduleState) {
    errors.push(`${requirement.id}: unknown module ${requirement.module}.`);
    continue;
  }

  const consumerPath = path.join(rootDirectory, requirement.consumer ?? "");
  if (!fs.existsSync(consumerPath)) {
    errors.push(`${requirement.id}: consumer ${requirement.consumer} does not exist.`);
  } else {
    const consumer = fs.readFileSync(consumerPath, "utf8");
    const token = requirement.surface.split(".").at(-1);
    if (!hasToken(consumer, token)) {
      errors.push(`${requirement.id}: consumer does not contain ${token}.`);
    }
  }

  const present = resolveRequirement(requirement, moduleState);
  const requiredCarriers = requirement.requires ?? [];
  for (const carrier of requiredCarriers) {
    readinessCarrierIds.add(carrier);
    if (!carrierRequirementStates.has(carrier)) {
      errors.push(`${requirement.id}: unknown carrier requirement ${carrier}.`);
    } else if (carrierRequirementsById.get(carrier).readiness !== "required") {
      errors.push(`${requirement.id}: profile-final carrier ${carrier} cannot block draft readiness.`);
    }
  }
  const carriersReady = requiredCarriers.every(
    (carrier) => carrierRequirementStates.get(carrier) === "draft",
  );
  const actualStatus = present && carriersReady ? "draft" : "missing";
  referenceRequirementStates.set(requirement.id, actualStatus);
  requirementStatusBySurface.set(`${requirement.module}:${requirement.surface}`, actualStatus);
  if (requirement.status !== actualStatus) {
    errors.push(
      `${requirement.id}: expected status ${requirement.status}, actual status ${actualStatus}.`,
    );
  }

  if (requirement.profile && !catalog.profiles?.[requirement.profile]) {
    errors.push(`${requirement.id}: unknown profile ${requirement.profile}.`);
  }
  if (present && requirement.profile) {
    const owner = requirement.surface.split(".")[0];
    const module = catalog.modules.find((candidate) => candidate.id === requirement.module);
    const api = module?.apis.find((candidate) => candidate.symbol === owner);
    if (api?.profile !== requirement.profile) {
      errors.push(
        `${requirement.id}: requirement profile ${requirement.profile} does not match ` +
          `${owner} profile ${api?.profile ?? "missing"}.`,
      );
    }
  }

  snapshotRequirements.push({
    id: requirement.id,
    module: requirement.module,
    surface: requirement.surface,
    status: actualStatus,
    ...(requirement.profile ? { profile: requirement.profile } : {}),
    ...(requiredCarriers.length > 0 ? { requires: [...requiredCarriers].sort() } : {}),
    consumer: requirement.consumer,
  });
}

const readinessDependencyStates = new Map();
for (const [id, status] of carrierRequirementStates) {
  readinessDependencyStates.set(id, status);
}
for (const [id, status] of referenceRequirementStates) {
  if (readinessDependencyStates.has(id)) {
    errors.push(`${id}: readiness dependency ID is shared by a carrier and reference requirement.`);
  }
  readinessDependencyStates.set(id, status);
}

const catalogedApiStatusBySurface = new Map();
for (const snapshotModule of snapshotModules) {
  const catalogModule = catalog.modules.find((candidate) => candidate.id === snapshotModule.id);
  for (const snapshotApi of snapshotModule.apis) {
    const catalogApi = catalogModule?.apis.find((candidate) => candidate.symbol === snapshotApi.symbol);
    const requiredDependencies = catalogApi?.requires ?? [];
    if (!Array.isArray(requiredDependencies) || new Set(requiredDependencies).size !== requiredDependencies.length) {
      errors.push(`${snapshotModule.id}.${snapshotApi.symbol}: requires must be an array of unique IDs.`);
      continue;
    }

    for (const dependency of requiredDependencies) {
      if (!readinessDependencyStates.has(dependency)) {
        errors.push(
          `${snapshotModule.id}.${snapshotApi.symbol}: unknown readiness dependency ${dependency}.`,
        );
        continue;
      }
      if (carrierRequirementsById.has(dependency)) {
        readinessCarrierIds.add(dependency);
        if (carrierRequirementsById.get(dependency).readiness !== "required") {
          errors.push(
            `${snapshotModule.id}.${snapshotApi.symbol}: profile-final carrier ${dependency} ` +
              "cannot block API readiness.",
          );
        }
      }
    }

    const actualStatus = requiredDependencies.every(
      (dependency) => readinessDependencyStates.get(dependency) === "draft",
    ) ? "draft" : "missing";
    snapshotApi.status = actualStatus;
    if (requiredDependencies.length > 0) {
      snapshotApi.requires = [...requiredDependencies].sort();
    }
    catalogedApiStatusBySurface.set(
      `${snapshotModule.id}:${snapshotApi.symbol}`,
      actualStatus,
    );
  }
}

for (const requirement of catalog.carrierRequirements ?? []) {
  if (requirement.readiness === "required" && !readinessCarrierIds.has(requirement.id)) {
    errors.push(`${requirement.id}: required carrier does not participate in draft readiness.`);
  }
}

const referenceSources = recursiveFiles(path.join(rootDirectory, "reference", "last-light"));
const snapshotQualifiedReferences = [];
for (const scan of catalog.referenceScans ?? []) {
  const moduleState = moduleStates.get(scan.module);
  if (!moduleState) {
    errors.push(`reference scan ${scan.binding}: unknown module ${scan.module}.`);
    continue;
  }

  const escapedBinding = scan.binding.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const pattern = new RegExp(
    `(?<!\\.)\\b${escapedBinding}\\.([A-Za-z_][A-Za-z0-9_]*)`,
    "g",
  );
  const surfaces = new Map();

  for (const sourcePath of referenceSources) {
    const source = fs.readFileSync(sourcePath, "utf8");
    for (const match of source.matchAll(pattern)) {
      const consumers = surfaces.get(match[1]) ?? new Set();
      consumers.add(path.relative(rootDirectory, sourcePath).replaceAll("\\", "/"));
      surfaces.set(match[1], consumers);
    }
  }

  for (const [surface, consumers] of [...surfaces].sort(([left], [right]) =>
    left.localeCompare(right),
  )) {
    const status = requirementStatusBySurface.get(`${scan.module}:${surface}`)
      ?? catalogedApiStatusBySurface.get(`${scan.module}:${surface}`)
      ?? (moduleState.exports.has(surface) ? "draft" : "missing");
    if (status === "missing" && !requirementSurfaces.has(`${scan.module}:${surface}`)) {
      errors.push(
        `${scan.module}.${surface}: qualified Last Light use is missing and has no requirement.`,
      );
    }
    snapshotQualifiedReferences.push({
      module: scan.module,
      surface,
      status,
      consumers: [...consumers].sort(),
    });
  }
}

const apiCount = snapshotModules.reduce((total, module) => total + module.apis.length, 0);
const missingCatalogedApiCount = snapshotModules.reduce(
  (total, module) => total + module.apis.filter((api) => api.status === "missing").length,
  0,
);
const draftCatalogedApiCount = apiCount - missingCatalogedApiCount;
const missingCount = snapshotRequirements.filter((item) => item.status === "missing").length;
const contractedRequirementCount = snapshotRequirements.filter((item) => item.profile).length;
const missingCarrierCount = snapshotCarrierRequirements.filter(
  (item) => item.status === "missing",
).length;
const implementationProviderCount = snapshotModules.filter(
  (module) => module.implementationProvider,
).length;
const missingImplementationProviderCount = snapshotModules.filter(
  (module) => module.implementationProvider?.status === "missing",
).length;
const snapshot = {
  $schema: "w-std-api-surface-snapshot-1",
  status: "generated-design-projection",
  catalogDigest: digest(fs.readFileSync(catalogPath)),
  summary: {
    modules: snapshotModules.length,
    catalogedApis: apiCount,
    draftCatalogedApis: draftCatalogedApiCount,
    missingCatalogedApis: missingCatalogedApiCount,
    qualifiedReferenceSurfaces: snapshotQualifiedReferences.length,
    referenceRequirements: snapshotRequirements.length,
    contractedReferenceRequirements: contractedRequirementCount,
    missingReferenceRequirements: missingCount,
    carrierRequirements: snapshotCarrierRequirements.length,
    missingCarrierRequirements: missingCarrierCount,
    implementationProviders: implementationProviderCount,
    missingImplementationProviders: missingImplementationProviderCount,
  },
  modules: snapshotModules.sort((left, right) => left.id.localeCompare(right.id)),
  qualifiedReferences: snapshotQualifiedReferences.sort((left, right) =>
    `${left.module}.${left.surface}`.localeCompare(`${right.module}.${right.surface}`),
  ),
  carrierRequirements: snapshotCarrierRequirements.sort((left, right) =>
    left.id.localeCompare(right.id),
  ),
  referenceRequirements: snapshotRequirements.sort((left, right) => left.id.localeCompare(right.id)),
};

const serialized = `${JSON.stringify(snapshot, null, 2)}\n`;
if (write && errors.length === 0) {
  fs.writeFileSync(snapshotPath, serialized);
} else if (!fs.existsSync(snapshotPath)) {
  errors.push("std-api-surface.snapshot.json is missing; run the checker with --write.");
} else if (!write && fs.readFileSync(snapshotPath, "utf8") !== serialized) {
  errors.push("std-api-surface.snapshot.json is stale; run the checker with --write.");
}

if (errors.length > 0) {
  for (const error of errors) console.error(`- ${error}`);
  process.exitCode = 1;
} else {
  console.log(
    `Std API contracts: ${snapshotModules.length} modules, ${apiCount} cataloged APIs, ` +
      `${draftCatalogedApiCount} draft-ready and ${missingCatalogedApiCount} blocked, ` +
      `${snapshotQualifiedReferences.length} qualified Last Light surfaces, ` +
      `${contractedRequirementCount}/${snapshotRequirements.length} contracted requirements, ` +
      `${missingCount} missing drafts, ${missingCarrierCount}/${snapshotCarrierRequirements.length} ` +
      `missing carriers, ${missingImplementationProviderCount}/${implementationProviderCount} ` +
      `missing implementation providers.`,
  );
}
