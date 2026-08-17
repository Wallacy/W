import { describe, expect, test } from "bun:test";

const input = "Last Light";
const canonicalBytes = "6a4c617374204c69676874";

function oneLine(value) {
  return value;
}

function encodeString(value) {
  const bytes = [...new TextEncoder().encode(value)];
  return [0x60 + bytes.length, ...bytes];
}

describe("R1C0 WLO1 host oracle", () => {
  test("portable and native projections preserve one logical String", () => {
    const portable = oneLine(input);
    const native = oneLine(input);
    expect(portable).toBe(input);
    expect(native).toBe(input);
    expect(Buffer.from(encodeString(portable)).toString("hex")).toBe(canonicalBytes);
    expect(Buffer.from(encodeString(native)).toString("hex")).toBe(canonicalBytes);
  });

  test("empty String remains a bounded valid value", () => {
    expect(Buffer.from(encodeString("")).toString("hex")).toBe("60");
  });
});
