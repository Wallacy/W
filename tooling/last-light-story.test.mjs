import { describe, expect, test } from "bun:test";
import { load, render, validate } from "./last-light-story.mjs";

const { story, atlas } = load();

describe("Last Light connected story", () => {
  test("covers every Atlas block exactly once", () => {
    expect(validate(story, atlas)).toEqual([]);
    const assigned = story.acts.flatMap((act) => act.atlasBlocks);
    expect(new Set(assigned).size).toBe(atlas.blocks.length);
    expect(assigned).toHaveLength(atlas.blocks.length);
    const assignedBlocks = new Set(assigned);
    const assignedVariants = atlas.variants.filter((variant) => assignedBlocks.has(variant.block));
    expect(new Set(assignedVariants.map((variant) => variant.id)).size).toBe(atlas.variants.length);
  });

  test("rejects a silent syntax-family omission", () => {
    const candidate = structuredClone(story);
    candidate.acts[0].atlasBlocks.pop();
    expect(validate(candidate, atlas).some((error) => error.includes("does not assign atlas blocks"))).toBe(true);
  });

  test("rejects assigning one syntax block to two scenes", () => {
    const candidate = structuredClone(story);
    candidate.acts[1].atlasBlocks.push(candidate.acts[0].atlasBlocks[0]);
    expect(validate(candidate, atlas).some((error) => error.includes("more than one act"))).toBe(true);
  });

  test("rejects reusing one source witness across scenes", () => {
    const candidate = structuredClone(story);
    candidate.acts[1].sourceRefs.push(structuredClone(candidate.acts[0].sourceRefs[0]));
    expect(validate(candidate, atlas).some((error) => error.includes("source witness is assigned more than once"))).toBe(true);
  });

  test("publishes the deterministic outcomes and evidence boundary", () => {
    const document = render(story, atlas);
    expect(document).toContain("quiet orbit completes 3 and loses 0");
    expect(document).toContain("timeline collision completes 1 and exposes 3 departures");
    expect(document).toContain("photon rush replays the same observable history");
    expect(document).toContain("Mutually exclusive roots and entry defaults live in separate modules");
    expect(document).toContain("`behavior-storage`");
    expect(document).toContain("`entry-default-body`");
  });
});
