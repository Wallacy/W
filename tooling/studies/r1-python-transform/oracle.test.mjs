import { describe, expect, test } from "bun:test";

function pipeline(tickets, limit) {
  const urgent = [];
  let inspected = 0;
  if (limit === 0) return { values: urgent, inspected };

  for (const ticket of tickets) {
    inspected += 1;
    if (ticket.priority !== 1) continue;
    urgent.push(ticket.orderId);
    if (urgent.length === limit) break;
  }

  return { values: urgent, inspected };
}

function explicitLoop(tickets, limit) {
  const urgent = [];
  let inspected = 0;
  if (limit === 0) return { values: urgent, inspected };

  for (const ticket of tickets) {
    inspected += 1;
    if (ticket.priority !== 1) continue;
    urgent.push(ticket.orderId);
    if (urgent.length === limit) break;
  }

  return { values: urgent, inspected };
}

describe("R1 Python-transform host oracle", () => {
  test("pipeline and loop preserve the primary bounded outcome", () => {
    const tickets = [
      { orderId: 7, priority: 1 },
      { orderId: 9, priority: 1 },
      { orderId: 11, priority: 0 },
    ];

    const expected = { values: [7], inspected: 1 };
    expect(pipeline(tickets, 1)).toEqual(expected);
    expect(explicitLoop(tickets, 1)).toEqual(expected);
  });

  test("zero limit and no match stay empty", () => {
    const noMatches = [
      { orderId: 21, priority: 0 },
      { orderId: 22, priority: 0 },
    ];

    expect(pipeline(noMatches, 0)).toEqual({ values: [], inspected: 0 });
    expect(explicitLoop(noMatches, 0)).toEqual({ values: [], inspected: 0 });
    expect(pipeline(noMatches, 3)).toEqual({ values: [], inspected: 2 });
    expect(explicitLoop(noMatches, 3)).toEqual({ values: [], inspected: 2 });
  });
});
