import { describe, expect, test } from "bun:test";
import {
  checkRenderedDiagnostics,
  deriveProjection,
  renderDiagnostics,
} from "./diagnostic-catalog.mjs";

const catalog = {
  $schema: "w-diagnostic-catalog-1",
  status: "projection-seed",
  profiles: {},
  codes: [
    {
      code: "W-ALPHA-0001",
      state: "active",
      phase: "semantic.type",
      defaultSeverity: "error",
      meaning: "an alpha condition is invalid",
      requiredFacts: { actual: "string" },
      labelRoles: {},
      fixes: {},
    },
    {
      code: "W-ALPHA-0002",
      state: "active",
      phase: "semantic.type",
      defaultSeverity: "error",
      meaning: "a second alpha condition is invalid",
      requiredFacts: {},
      labelRoles: {},
      fixes: {},
    },
    {
      code: "W-BETA-0001",
      state: "reserved",
      phase: "semantic.type",
      defaultSeverity: "error",
      meaning: "a beta condition is reserved",
      requiredFacts: {},
      labelRoles: {},
      fixes: {},
    },
  ],
};

const design = [
  "# Design",
  "",
  "## Alpha contract",
  "",
  "W-ALPHA-0002",
  "W-ALPHA-0001",
  "",
  "## Beta contract",
  "",
  "W-BETA-*",
  "",
].join("\n");

const metadata = { catalogDigest: `sha256:${"a".repeat(64)}` };
const projection = deriveProjection(catalog, design, metadata);
const markdown = renderDiagnostics(projection);

function replaceOnce(source, from, to) {
  expect(source.includes(from)).toBe(true);
  return source.replace(from, to);
}

describe("human diagnostic catalog projection", () => {
  test("resolves first exact and family references and renders compact ordered entries", () => {
    expect(projection.modeCounts).toEqual({ exact: 2, family: 1 });
    expect(projection.families.map((family) => family.family)).toEqual(["W-ALPHA", "W-BETA"]);
    expect(projection.families[0].entries.map((entry) => entry.code)).toEqual([
      "W-ALPHA-0001",
      "W-ALPHA-0002",
    ]);
    expect(projection.entries.find((entry) => entry.code === "W-BETA-0001").reference.mode).toBe("family");
    expect(projection.entries.find((entry) => entry.code === "W-BETA-0001").reference.token).toBe("W-BETA-*");
    expect(markdown).toContain("- [`W-ALPHA`](#w-alpha) — 2 entradas");
    expect(markdown).not.toContain("  - [`W-ALPHA-0001`](#w-alpha-0001)");
    expect(markdown).toContain("- `requiredFacts`:\n  - `actual`: `string`");
    expect(checkRenderedDiagnostics(markdown, projection)).toEqual([]);
  });

  test("rejects missing and extra code entries", () => {
    const missing = replaceOnce(markdown, "#### W-ALPHA-0001\n", "");
    expect(checkRenderedDiagnostics(missing, projection).join("\n")).toContain("missing codes: W-ALPHA-0001");

    const extra = replaceOnce(markdown, "## Entradas\n", "## Entradas\n\n#### W-EXTRA-0001\n\n");
    expect(checkRenderedDiagnostics(extra, projection).join("\n")).toContain("extra codes: W-EXTRA-0001");
  });

  test("rejects stale digest, content, ordering, and manual drift", () => {
    const staleDigest = replaceOnce(markdown, "sha256:" + "a".repeat(64), "sha256:" + "b".repeat(64));
    expect(checkRenderedDiagnostics(staleDigest, projection).join("\n")).toContain("catalog digest is stale");

    const staleMeaning = replaceOnce(markdown, "an alpha condition is invalid", "a changed alpha condition is invalid");
    expect(checkRenderedDiagnostics(staleMeaning, projection).join("\n")).toContain("stale or manually edited");

    const reordered = replaceOnce(
      replaceOnce(markdown, "#### W-ALPHA-0001", "#### W-TEMP-0001"),
      "#### W-ALPHA-0002",
      "#### W-ALPHA-0001",
    ).replace("#### W-TEMP-0001", "#### W-ALPHA-0002");
    expect(checkRenderedDiagnostics(reordered, projection).join("\n")).toContain("code ordering is stale");
  });

  test("rejects broken or ambiguous authority links", () => {
    const broken = replaceOnce(markdown, "DESIGN.md#alpha-contract", "DESIGN.md#missing");
    expect(checkRenderedDiagnostics(broken, projection).join("\n")).toContain("authority anchor #missing is missing or ambiguous");

    const ambiguousDesign = design.replace("## Alpha contract", "<a id=\"alpha-contract\"></a>\n\n## Alpha contract");
    expect(() => deriveProjection(catalog, ambiguousDesign, metadata)).toThrow("anchor #alpha-contract is ambiguous");
  });

  test("ignores unrelated DESIGN edits and follows an effective heading change", () => {
    const unrelated = deriveProjection(catalog, `${design}\n## Unrelated note\n\nNo diagnostic reference.\n`, metadata);
    expect(renderDiagnostics(unrelated)).toBe(markdown);

    const renamed = deriveProjection(catalog, design.replace("## Beta contract", "## Renamed beta contract"), metadata);
    const errors = checkRenderedDiagnostics(markdown, renamed).join("\n");
    expect(errors).toContain("W-BETA-0001: DIAGNOSTICS.md authority link is stale or ambiguous");
    expect(errors).toContain("DIAGNOSTICS.md is stale or manually edited");
  });

  test("keeps exact and family links on the immediately preceding Unicode heading", () => {
    const unicodeDesign = [
      "# Design",
      "",
      "## Preface — café 🧪 Привет مرحبا",
      "",
      "## Exact target — São Paulo 🚲",
      "",
      "W-ALPHA-0001",
      "W-ALPHA-0002",
      "",
      "## Family target — données naïves 🛰️",
      "",
      "W-BETA-*",
      "",
    ].join("\r\n");
    const unicodeProjection = deriveProjection(catalog, unicodeDesign, metadata);
    expect(unicodeProjection.entries.find((entry) => entry.code === "W-ALPHA-0001").reference.heading.title).toBe("Exact target — São Paulo 🚲");
    expect(unicodeProjection.entries.find((entry) => entry.code === "W-BETA-0001").reference.heading.title).toBe("Family target — données naïves 🛰️");
  });

  test("rejects a code without an exact or family reference", () => {
    expect(() => deriveProjection(catalog, design.replace("W-BETA-*", "no beta reference"), metadata)).toThrow(
      "W-BETA-0001: DESIGN.md has no exact code or family wildcard reference",
    );
  });
});
