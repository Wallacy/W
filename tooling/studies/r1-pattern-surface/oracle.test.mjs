import { describe, expect, test } from "bun:test";
import bundle from "./bundle.json" with { type: "json" };

const cases = bundle.inputs.map(({ expected, ...input }) => ({ input, expected }));

function validateOrder(order) {
  if (!order || !Number.isSafeInteger(order.id) || order.id < 0) throw new Error("id");
  if (!order.guest || typeof order.guest.name !== "string") throw new Error("guest");
  if (!Number.isSafeInteger(order.guests) || order.guests < 0) throw new Error("guests");
  if (typeof order.stage !== "string" || typeof order.route !== "string") throw new Error("route");
  if (typeof order.newField !== "string" || typeof order.newFieldValue !== "string") throw new Error("newField");
}

function party(stage, guests) {
  if (stage === "accepted" && guests >= 1 && guests <= 4) return "intimate";
  return "regular";
}

function run(operation, order) {
  validateOrder(order);
  if (operation === "nominal-open") {
    const route = order.route === "menu" ? "show" : "reject";
    return {
      name: order.guest.name,
      party: party(order.stage, order.guests),
      route,
      consumed: ["id", "guest", "stage", "guests", "route", "newField"],
      ownerConsumed: true,
      openRest: { [order.newField]: order.newFieldValue },
    };
  }
  if (operation === "closed-route") {
    if (order.route === "menu") return { route: "show", consumed: ["route"], effects: [] };
    return { route: "reject", consumed: ["route"], effects: [] };
  }
  if (operation === "unknown-route") {
    return { accepted: false, cause: "unknown-route", effects: [] };
  }
  if (operation === "structural-pattern") {
    return { accepted: false, cause: "structural-pattern", effects: [] };
  }
  if (operation === "multi-subject-switch") {
    return { accepted: false, cause: "multi-subject-switch", effects: [] };
  }
  if (operation === "implicit-open-pattern") {
    return { accepted: false, cause: "implicit-open-pattern", effects: [] };
  }
  if (operation === "custom-pattern-dispatch") {
    return { accepted: false, cause: "custom-pattern-dispatch", effects: [] };
  }
  throw new Error("unknown operation");
}

describe("R1 pattern-surface host oracle", () => {
  for (const { input, expected } of cases) {
    test(`derives exact outcome for ${input.id}`, () => {
      expect(run(input.operation, input.order)).toEqual(expected);
    });
  }

  test("malformed patterns fail before consumption or effects", () => {
    expect(() => run("nominal-open", { ...cases[0].input.order, newField: 7 })).toThrow("newField");
  });
});
