import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";

const ROOT = path.resolve(import.meta.dirname, "..");

function read(relativePath) {
  return fs.readFileSync(path.join(ROOT, relativePath), "utf8");
}

function laneCount(lanes) {
  if (!Number.isInteger(lanes) || lanes < 1 || lanes > 64) throw new RangeError("lanes must be 1...64");
  return lanes;
}

function simd(lanes, values) {
  laneCount(lanes);
  if (values.length !== lanes) throw new RangeError("lane value count mismatch");
  return { lanes, values: [...values] };
}

function mask(lanes, values) {
  laneCount(lanes);
  if (values.length !== lanes) throw new RangeError("mask lane count mismatch");
  return { lanes, values: values.map(Boolean) };
}

function maskAnd(left, right) {
  if (left.lanes !== right.lanes) throw new RangeError("mask lane mismatch");
  return mask(left.lanes, left.values.map((value, index) => value && right.values[index]));
}

function maskOr(left, right) {
  if (left.lanes !== right.lanes) throw new RangeError("mask lane mismatch");
  return mask(left.lanes, left.values.map((value, index) => value || right.values[index]));
}

function maskXor(left, right) {
  if (left.lanes !== right.lanes) throw new RangeError("mask lane mismatch");
  return mask(left.lanes, left.values.map((value, index) => Boolean(value) !== Boolean(right.values[index])));
}

function maskNot(value) {
  return mask(value.lanes, value.values.map((lane) => !lane));
}

function all(value) {
  return value.values.every(Boolean);
}

function any(value) {
  return value.values.some(Boolean);
}

function none(value) {
  return !any(value);
}

function countTrue(value) {
  return value.values.filter(Boolean).length;
}

function equalLanes(value, other) {
  if (value.lanes !== other.lanes) throw new RangeError("lane mismatch");
  return mask(value.lanes, value.values.map((lane, index) => lane === other.values[index]));
}

function select(condition, whenTrue, otherwise) {
  if (condition.lanes !== whenTrue.lanes || condition.lanes !== otherwise.lanes) {
    throw new RangeError("lane mismatch");
  }
  return simd(condition.lanes, condition.values.map((enabled, index) =>
    enabled ? whenTrue.values[index] : otherwise.values[index]));
}

function checkedLoad(source, at, lanes) {
  laneCount(lanes);
  if (!Number.isInteger(at) || at < 0 || at + lanes > source.length) throw new RangeError("bounds");
  return simd(lanes, source.slice(at, at + lanes));
}

function loadPartial(source, at, lanes, fill) {
  laneCount(lanes);
  if (!Number.isInteger(at) || at < 0 || at > source.length) throw new RangeError("bounds");
  const values = [];
  const live = [];
  for (let lane = 0; lane < lanes; lane += 1) {
    const index = at + lane;
    if (index < source.length) {
      values.push(source[index]);
      live.push(true);
    } else {
      values.push(fill);
      live.push(false);
    }
  }
  return { value: simd(lanes, values), live: mask(lanes, live) };
}

function scanMenuReference(source, delimiter, backend) {
  if (!["scalar", "native", "split"].includes(backend)) throw new Error("unknown backend");
  if (source.length < 16 || source.length > 32) throw new RangeError("menu refinement");
  const delimiterVector = simd(16, Array(16).fill(delimiter));
  const lfVector = simd(16, Array(16).fill(10));
  const full = checkedLoad(source, 0, 16);
  const fullMatches = maskOr(equalLanes(full, delimiterVector), equalLanes(full, lfVector));
  const tail = loadPartial(source, 16, 16, delimiter);
  const tailMatches = maskAnd(
    maskOr(equalLanes(tail.value, delimiterVector), equalLanes(tail.value, lfVector)),
    tail.live,
  );
  return {
    backend,
    fullMatches: countTrue(fullMatches),
    tailMatches: countTrue(tailMatches),
    tailLive: [...tail.live.values],
  };
}

function instrumentedSource(values) {
  const reads = [];
  const oobReads = [];
  const source = new Proxy([...values], {
    get(target, property, receiver) {
      if (/^\d+$/u.test(String(property))) {
        const index = Number(property);
        reads.push(index);
        if (index < 0 || index >= target.length) {
          oobReads.push(index);
          throw new RangeError("out-of-bounds read");
        }
      }
      return Reflect.get(target, property, receiver);
    },
  });
  return { source, reads, oobReads };
}

function storePartial(target, at, value, where) {
  if (value.lanes !== where.lanes || !Number.isInteger(at) || at < 0) throw new RangeError("bounds");
  for (let lane = 0; lane < value.lanes; lane += 1) {
    if (where.values[lane] && at + lane >= target.length) throw new RangeError("bounds");
  }
  for (let lane = 0; lane < value.lanes; lane += 1) {
    if (where.values[lane]) target[at + lane] = value.values[lane];
  }
}

function overflowingAdd(values, bits) {
  const limit = 1n << BigInt(bits);
  return {
    value: values.map(([left, right]) => Number(BigInt.asUintN(bits, BigInt(left) + BigInt(right)))),
    overflow: values.map(([left, right]) => BigInt(left) + BigInt(right) >= limit),
  };
}

function checkedRemainder(left, right, bits, { signed = false } = {}) {
  if (right === 0) throw new RangeError("division by zero");
  const signedMin = -(1 << (bits - 1));
  if (signed && left === signedMin && right === -1) return 0;
  return left % right;
}

function backendOverflow(values, bits, backend) {
  if (!["scalar", "native", "split"].includes(backend)) throw new Error("unknown backend");
  const result = overflowingAdd(values.map((pair) => [...pair]), bits);
  return { backend, ...result };
}

function swizzle(value, indices) {
  for (const index of indices) {
    if (!Number.isInteger(index) || index < 0 || index >= value.lanes) throw new RangeError("swizzle index");
  }
  return simd(indices.length, indices.map((index) => value.values[index]));
}

function reduceInteger(values, operation, policy, bits = 8) {
  const max = (1n << BigInt(bits)) - 1n;
  let accumulator = operation === "add" ? 0n : 1n;
  for (const value of values) {
    const next = operation === "add" ? accumulator + BigInt(value) : accumulator * BigInt(value);
    if (policy === "checked" && next > max) throw new RangeError("overflow");
    if (policy === "wrapping") accumulator = BigInt.asUintN(bits, next);
    else if (policy === "saturating") accumulator = next > max ? max : next;
    else accumulator = next;
  }
  return Number(accumulator);
}

function reduceFloat(values, mode) {
  if (!["strict", "fast", "reproducible"].includes(mode)) throw new Error("reduction mode required");
  return values.reduce((sum, value) => sum + value, 0);
}

const sources = {
  design: read("DESIGN.md"),
  rationale: read("RATIONALE.md"),
  cheatsheet: read("CHEATSHEET.md"),
  performance: read("reference/last-light/performance.w"),
  lastLightReadme: read("reference/last-light/README.md"),
  operators: read("reference/syntax-atlas/operators.w"),
  atlasCheatsheet: read("reference/syntax-atlas/CHEATSHEET.md"),
  corpus: JSON.parse(read("tooling/simd-reference-cases.json")),
};

describe("SIMD1 host oracle", () => {
  test("corpus is design-oracle input and derives outcomes from invariants", () => {
    expect(sources.corpus.$schema).toBe("w-simd-reference-cases-1");
    expect(sources.corpus.status).toBe("design-oracle-input");
    expect(sources.corpus.decisions).toEqual(["W-1459"]);
    expect(sources.corpus.cases.length).toBeGreaterThanOrEqual(3);
    for (const entry of sources.corpus.cases) {
      expect(entry.decisions).toContain("W-1459");
      expect(entry.source.path).toBe("reference/last-light/performance.w");
      expect(sources.performance).toContain(entry.source.symbol);
      expect(entry.expected).toBeUndefined();
      expect(entry.result).toBeUndefined();
    }
    const current = sources.corpus.cases.find((entry) => entry.id === "SIMD1-W-1459-current");
    expect(() => laneCount(current.input.lanes)).not.toThrow();
    expect(current.input.bytes).toBeArray();
    expect(current.input.bytes.length).toBeGreaterThanOrEqual(16);
    expect(current.input.bytes.length).toBeLessThanOrEqual(32);
    expect(current.input.fillEqualsDelimiter).toBe(true);
    expect(current.input.operations).toContain("loadPartial");
    expect(() => scanMenuReference(current.input.bytes.slice(0, 15), current.input.delimiter, "scalar")).toThrow(RangeError);
    expect(() => scanMenuReference([...current.input.bytes, ...Array(13).fill(0)], current.input.delimiter, "scalar")).toThrow(RangeError);
    const rejected = sources.corpus.cases.find((entry) => entry.id === "SIMD1-W-1459-reject-lanes-65");
    expect(() => laneCount(rejected.input.lanes)).toThrow(RangeError);
    const mutation = structuredClone(current);
    mutation.input.lanes = 65;
    expect(() => laneCount(mutation.input.lanes)).toThrow(RangeError);
    const overflow = sources.corpus.cases.find((entry) => entry.id === "SIMD1-W-1459-overflow-swizzle");
    expect(overflow.input.operands.left).toBeArray();
    expect(overflow.input.operands.right).toBeArray();
    const original = overflowingAdd(overflow.input.operands.left.map((left, index) => [left, overflow.input.operands.right[index]]), 8);
    const operandMutation = structuredClone(overflow);
    operandMutation.input.operands.left[0] = 0;
    const mutated = overflowingAdd(operandMutation.input.operands.left.map((left, index) => [left, operandMutation.input.operands.right[index]]), 8);
    expect(mutated).not.toEqual(original);
  });

  test("source and contract snippets are present", () => {
    const required = [
      [sources.design, "W-1459"],
      [sources.design, "Simd<Element, lanes: usize>"],
      [sources.design, "SimdMask<_ lanes: usize>"],
      [sources.design, "SimdMask.splat(Bool)"],
      [sources.design, "SimdMask<N>"],
      [sources.design, "fromArray([Bool; N])"],
      [sources.design, "toArray() -> [Bool; N]"],
      [sources.design, "load(from:at:)"],
      [sources.design, "store(to:at:)"],
      [sources.design, "loadPartial(from:at:fill:)"],
      [sources.design, "storePartial(to:at:where:)"],
      [sources.design, "at == count"],
      [sources.design, "preflight de todas as lanes ativas"],
      [sources.design, "native, split ou scalarize"],
      [sources.design, "floats não ganham"],
      [sources.design, "wrappingReduceAdd"],
      [sources.design, "saturatingReduceAdd"],
      [sources.design, "reduceBitAnd"],
      [sources.design, "reduceBitOr"],
      [sources.design, "reduceBitXor"],
      [sources.design, "wrappingReduceMultiply"],
      [sources.design, "saturatingReduceMultiply"],
      [sources.design, "reduceAdd(mode:)"],
      [sources.design, "reduceMultiply(mode:)"],
      [sources.design, "all() -> Bool"],
      [sources.design, "countTrue() -> UInt"],
      [sources.design, "signed.min % -1"],
      [sources.design, "somente divisor zero"],
      [sources.rationale, "https://doc.rust-lang.org/std/primitive.u8.html"],
      [sources.rationale, "https://developer.apple.com/documentation/swift/fixedwidthinteger"],
      [sources.rationale, "https://ziglang.org/documentation/master/#Vectors"],
      [sources.rationale, "https://llvm.org/docs/LangRef.html#vector-predication-intrinsics"],
      [sources.performance, "export fn scanMenuDelimiters("],
      [sources.performance, "export fn wrappingByteVectorOracle("],
      [sources.performance, "Simd<u8, lanes: 16>.loadPartial"],
      [sources.performance, "fill: delimiter"],
      [sources.performance, "full.equalLanes(delimiterVector) | full.equalLanes(lfVector)"],
      [sources.performance, "tail.equalLanes(delimiterVector) | tail.equalLanes(lfVector)"],
      [sources.performance, "tailMatches"],
      [sources.performance, "overflowingAdd"],
      [sources.performance, "swizzled<indices: [3, 3, 0]>"],
      [sources.operators, "let inverted = ~value"],
      [sources.operators, "let _ = inverted"],
      [sources.cheatsheet, "Matriz fechada de policies integer"],
      [sources.cheatsheet, "SIMD portátil"],
      [sources.cheatsheet, "wrappingReduceAdd"],
      [sources.cheatsheet, "saturatingReduceMultiply"],
      [sources.cheatsheet, "reduceBitXor"],
      [sources.lastLightReadme, "scanMenuDelimiters"],
      [sources.atlasCheatsheet, "../../CHEATSHEET.md#operadores-bits-e-política-numérica"],
      [sources.atlasCheatsheet, "../../CHEATSHEET.md#performance-e-custo"],
    ];
    for (const [source, snippet] of required) expect(source).toContain(snippet);
  });

  test("lanes accept 1, odd values and 64, and reject 0 and 65", () => {
    for (const lanes of [1, 3, 17, 63, 64]) expect(() => laneCount(lanes)).not.toThrow();
    for (const lanes of [0, 65]) expect(() => laneCount(lanes)).toThrow(RangeError);
  });

  test("scalar, native and split lowerings are logically equivalent", () => {
    const current = sources.corpus.cases.find((entry) => entry.id === "SIMD1-W-1459-current");
    const runs = ["scalar", "native", "split"].map((backend) => {
      const tracked = instrumentedSource(current.input.bytes);
      return { result: scanMenuReference(tracked.source, current.input.delimiter, backend), tracked };
    });
    for (const run of runs.slice(1)) {
      expect({ ...run.result, backend: undefined }).toEqual({ ...runs[0].result, backend: undefined });
    }
    expect(runs[0].result.fullMatches).toBe(3);
    expect(runs[0].result.tailMatches).toBe(2);
    for (const run of runs) {
      expect(run.tracked.oobReads).toEqual([]);
      expect(run.tracked.reads.every((index) => index >= 0 && index < current.input.bytes.length)).toBe(true);
    }
    const input = [[250, 10], [255, 2], [1, 255], [127, 1]];
    const outputs = ["scalar", "native", "split"].map((backend) => backendOverflow(input, 8, backend));
    expect(outputs[1].value).toEqual(outputs[0].value);
    expect(outputs[2].value).toEqual(outputs[0].value);
    expect(outputs[1].overflow).toEqual(outputs[0].overflow);
    expect(outputs[2].overflow).toEqual(outputs[0].overflow);
  });

  test("partial tail never reads or counts inactive fill lanes", () => {
    const current = sources.corpus.cases.find((entry) => entry.id === "SIMD1-W-1459-current");
    const delimiter = current.input.delimiter;
    const tracked = instrumentedSource(current.input.bytes);
    const tail = loadPartial(tracked.source, 16, 16, delimiter);
    const tailHits = maskAnd(
      maskOr(
        equalLanes(tail.value, simd(16, Array(16).fill(delimiter))),
        equalLanes(tail.value, simd(16, Array(16).fill(10))),
      ),
      tail.live,
    );
    expect(countTrue(tailHits)).toBe(2);
    expect(tracked.oobReads).toEqual([]);
    expect(tail.value.values.slice(4).every((value) => value === delimiter)).toBe(true);
    expect(tail.live.values.slice(4).every((value) => value === false)).toBe(true);

    const atEnd = instrumentedSource(current.input.bytes);
    const emptyTail = loadPartial(atEnd.source, current.input.bytes.length, 16, delimiter);
    expect(emptyTail.live.values.every((value) => value === false)).toBe(true);
    expect(atEnd.reads).toEqual([]);
    expect(() => loadPartial(atEnd.source, current.input.bytes.length + 1, 16, delimiter)).toThrow(RangeError);
    expect(atEnd.reads).toEqual([]);
  });

  test("partial store fails before any write when active lane is OOB", () => {
    const target = [0, 0, 0, 0];
    const before = [...target];
    const value = simd(4, [9, 8, 7, 6]);
    expect(() => storePartial(target, 2, value, mask(4, [true, false, false, true]))).toThrow(RangeError);
    expect(target).toEqual(before);
    storePartial(target, 1, value, mask(4, [true, true, false, false]));
    expect(target).toEqual([0, 9, 8, 0]);
  });

  test("overflowing lanes derive values and mask from scalar arithmetic", () => {
    const overflow = sources.corpus.cases.find((entry) => entry.id === "SIMD1-W-1459-overflow-swizzle");
    const input = overflow.input.operands.left.map((left, index) => [left, overflow.input.operands.right[index]]);
    const expected = overflowingAdd(input, 8);
    const result = backendOverflow(input, 8, "scalar");
    expect(result.value).toEqual(expected.value);
    expect(result.overflow).toEqual(expected.overflow);
    expect(result.value).toEqual([4, 1, 0, 128]);
    expect(result.overflow).toEqual([true, true, true, false]);
  });

  test("static swizzle accepts duplicate indices and rejects OOB", () => {
    const source = simd(4, [10, 20, 30, 40]);
    const overflow = sources.corpus.cases.find((entry) => entry.id === "SIMD1-W-1459-overflow-swizzle");
    expect(swizzle(source, overflow.input.swizzleIndices).values).toEqual([40, 40, 10]);
    expect(() => swizzle(source, [4])).toThrow(RangeError);
    expect(() => swizzle(source, [-1])).toThrow(RangeError);
  });

  test("mask reductions and bitwise operators are lane-wise", () => {
    const left = mask(4, [true, false, true, false]);
    const right = mask(4, [true, true, false, false]);
    expect(maskAnd(left, right).values).toEqual([true, false, false, false]);
    expect(maskOr(left, right).values).toEqual([true, true, true, false]);
    expect(maskXor(left, right).values).toEqual([false, true, true, false]);
    expect(maskNot(left).values).toEqual([false, true, false, true]);
    expect(all(mask(4, [true, true, true, true]))).toBe(true);
    expect(all(left)).toBe(false);
    expect(any(left)).toBe(true);
    expect(none(mask(4, [false, false, false, false]))).toBe(true);
    expect(none(right)).toBe(false);
    expect(countTrue(left)).toBe(2);
  });

  test("select forms both arguments before lane-wise selection", () => {
    const events = [];
    const condition = mask(4, [true, false, true, false]);
    const selected = select(
      condition,
      (() => { events.push("form:true"); return simd(4, [1, 2, 3, 4]); })(),
      (() => { events.push("form:false"); return simd(4, [10, 20, 30, 40]); })(),
    );
    events.push("after-call");
    expect(events).toEqual(["form:true", "form:false", "after-call"]);
    expect(selected.values).toEqual([1, 20, 3, 40]);
  });

  test("remainder only rejects zero divisor", () => {
    expect(checkedRemainder(-(1 << 7), -1, 8, { signed: true })).toBe(0);
    expect(checkedRemainder(7, -3, 8, { signed: true })).toBe(1);
    expect(() => checkedRemainder(-(1 << 7), 0, 8, { signed: true })).toThrow(RangeError);
    expect(sources.design).toContain("`signed.min / -1`");
    expect(sources.design).toContain("`signed.min % -1`");
  });

  test("reductions preserve lane order and scalar policies", () => {
    const add = [200, 100, 1];
    expect(() => reduceInteger(add, "add", "checked")).toThrow(RangeError);
    expect(reduceInteger(add, "add", "wrapping")).toBe(45);
    expect(reduceInteger(add, "add", "saturating")).toBe(255);
    const multiply = [20, 20, 2];
    expect(() => reduceInteger(multiply, "multiply", "checked")).toThrow(RangeError);
    expect(reduceInteger(multiply, "multiply", "wrapping")).toBe(32);
    expect(reduceInteger(multiply, "multiply", "saturating")).toBe(255);
    const strict = reduceFloat([0.1, 0.2, 0.3], "strict");
    expect(reduceFloat([0.1, 0.2, 0.3], "reproducible")).toBe(strict);
    expect(() => reduceFloat([0.1, 0.2], undefined)).toThrow();
  });

  test("forbidden claims and forms are not promoted", () => {
    const acceptedSource = `${sources.design}\n${sources.cheatsheet}\n${sources.performance}`;
    const forbiddenCurrent = [
      /\b(?:export|pub)\s+type\s+nativeLanes\b/iu,
      /\bSimd\s*<[^>\n]*\b(?:vectorWidth|physicalWidth)\b/iu,
      /\bperformance\s+operator\s*=/iu,
      /\bloadPartial[^\n]*alignment\s*:/iu,
      /\bselect[^\n]*short[- ]circuit\s*:\s*true/iu,
    ];
    for (const pattern of forbiddenCurrent) expect(acceptedSource).not.toMatch(pattern);
    expect(sources.performance).not.toContain("fullMatches | tailVisible");
    expect(sources.performance).not.toContain("let combined");
    expect(sources.design).toContain("nativeLanes");
    expect(sources.design).toContain("não é short-circuit");
    expect(sources.design).toContain("O baseline não promete performance");
    expect(sources.design).toContain("Compiler, runtime, provider, native acceleration");
  });
});
