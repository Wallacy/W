import assert from "node:assert/strict";
import test from "node:test";

const wireDiagnosticCases = await Bun.file(
  new URL("./wire-diagnostic-cases.json", import.meta.url),
).json();
const diagnosticCatalog = await Bun.file(
  new URL("./diagnostic-catalog.json", import.meta.url),
).json();

const WIRE_KIND = Object.freeze({
  bool: 1,
  u16: 3,
  bytes: 14,
  string: 15,
});

const MAX_U32 = 0xffff_ffff;
const MAX_FIELDS = 64;

class WireDecodeError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

function asBytes(value) {
  return value instanceof Uint8Array ? value : Uint8Array.from(value);
}

function joinBytes(...parts) {
  const size = parts.reduce((total, part) => total + part.length, 0);
  const result = new Uint8Array(size);
  let offset = 0;

  for (const part of parts) {
    result.set(part, offset);
    offset += part.length;
  }

  return result;
}

function equalBytes(left, right) {
  if (left.length !== right.length) {
    return false;
  }

  return left.every((value, index) => value === right[index]);
}

function encodeLeb128(value) {
  if (!Number.isInteger(value) || value < 0 || value > MAX_U32) {
    throw new RangeError("u32 LEB128 value is out of range");
  }

  const bytes = [];
  let remaining = value;

  do {
    let byte = remaining % 128;
    remaining = Math.floor(remaining / 128);

    if (remaining !== 0) {
      byte |= 0x80;
    }

    bytes.push(byte);
  } while (remaining !== 0);

  return Uint8Array.from(bytes);
}

function readLeb128(data, start) {
  let value = 0;
  let offset = start;

  for (let index = 0; index < 5; index += 1) {
    if (offset >= data.length) {
      throw new WireDecodeError("truncatedBlock");
    }

    const byte = data[offset];
    offset += 1;

    if (index === 4 && ((byte & 0x7f) > 0x0f || (byte & 0x80) !== 0)) {
      throw new WireDecodeError("controlIntegerOverflow");
    }

    value += (byte & 0x7f) * 2 ** (index * 7);

    if ((byte & 0x80) === 0) {
      const encoded = encodeLeb128(value);
      const actual = data.slice(start, offset);

      if (!equalBytes(encoded, actual)) {
        throw new WireDecodeError("nonMinimalControlInteger");
      }

      return { value, offset };
    }
  }

  throw new WireDecodeError("controlIntegerOverflow");
}

function encodeU16(value) {
  if (!Number.isInteger(value) || value < 0 || value > 0xffff) {
    throw new RangeError("u16 value is out of range");
  }

  return Uint8Array.from([value & 0xff, value >>> 8]);
}

function decodeExactMenuKey(input) {
  const data = asBytes(input);

  if (data.length < 3) {
    throw new WireDecodeError("truncatedBlock");
  }

  const presence = data[0];

  if ((presence & ~0x01) !== 0) {
    throw new WireDecodeError("unusedPresenceBit");
  }

  const result = {
    id: data[1] | (data[2] << 8),
    urgent: undefined,
  };

  if (presence === 0) {
    if (data.length !== 3) {
      throw new WireDecodeError("trailingData");
    }

    return result;
  }

  if (data.length < 4) {
    throw new WireDecodeError("truncatedBlock");
  }

  if (data.length !== 4) {
    throw new WireDecodeError("trailingData");
  }

  if (data[3] > 1) {
    throw new WireDecodeError("invalidBool");
  }

  result.urgent = data[3] === 1;
  return result;
}

function encodeExactMenuKey({ id, urgent }) {
  const fixed = encodeU16(id);
  const presence = urgent === undefined ? 0 : 1;

  if (urgent !== undefined && typeof urgent !== "boolean") {
    throw new TypeError("urgent must be a boolean or undefined");
  }

  return urgent === undefined
    ? joinBytes(Uint8Array.from([presence]), fixed)
    : joinBytes(
        Uint8Array.from([presence]),
        fixed,
        Uint8Array.from([urgent ? 1 : 0]),
      );
}

function validateUnknownScalar(kind, block) {
  if (
    ![WIRE_KIND.bool, WIRE_KIND.u16, WIRE_KIND.bytes, WIRE_KIND.string].includes(
      kind,
    )
  ) {
    throw new WireDecodeError("unsupportedWireKind");
  }

  if (kind === WIRE_KIND.bool && block.length !== 1) {
    throw new WireDecodeError("invalidWireKind");
  }

  if (kind === WIRE_KIND.u16 && block.length !== 2) {
    throw new WireDecodeError("invalidWireKind");
  }
}

function decodeCompatibleMenuKey(input) {
  const data = asBytes(input);
  let cursor = 0;
  const countResult = readLeb128(data, cursor);
  cursor = countResult.offset;

  if (countResult.value > MAX_FIELDS) {
    throw new WireDecodeError("countOverflow");
  }

  let previousId = 0;
  const entries = [];

  for (let index = 0; index < countResult.value; index += 1) {
    const deltaResult = readLeb128(data, cursor);
    cursor = deltaResult.offset;

    if (deltaResult.value === 0) {
      throw new WireDecodeError("duplicateFieldId");
    }

    if (previousId > MAX_U32 - deltaResult.value) {
      throw new WireDecodeError("fieldIdOverflow");
    }

    const currentId = previousId + deltaResult.value;

    if (cursor >= data.length) {
      throw new WireDecodeError("truncatedBlock");
    }

    const kind = data[cursor];
    cursor += 1;
    const lengthResult = readLeb128(data, cursor);
    cursor = lengthResult.offset;
    entries.push({ id: currentId, kind, length: lengthResult.value });
    previousId = currentId;
  }

  let id;
  let urgent;

  for (const entry of entries) {
    if (entry.length > data.length - cursor) {
      throw new WireDecodeError("truncatedBlock");
    }

    const block = data.slice(cursor, cursor + entry.length);
    cursor += entry.length;

    if (entry.id === 1) {
      if (entry.kind !== WIRE_KIND.u16 || block.length !== 2) {
        throw new WireDecodeError("invalidWireKind");
      }

      id = block[0] | (block[1] << 8);
    } else if (entry.id === 2) {
      if (entry.kind !== WIRE_KIND.bool || block.length !== 1) {
        throw new WireDecodeError("invalidWireKind");
      }

      if (block[0] > 1) {
        throw new WireDecodeError("invalidBool");
      }

      urgent = block[0] === 1;
    } else {
      validateUnknownScalar(entry.kind, block);
    }
  }

  if (id === undefined) {
    throw new WireDecodeError("missingRequiredField");
  }

  if (cursor !== data.length) {
    throw new WireDecodeError("trailingData");
  }

  return { id, urgent };
}

function encodeCompatibleMenuKey({ id, urgent }) {
  const fields = [
    { id: 1, kind: WIRE_KIND.u16, block: encodeU16(id) },
  ];

  if (urgent !== undefined) {
    if (typeof urgent !== "boolean") {
      throw new TypeError("urgent must be a boolean or undefined");
    }

    fields.push({
      id: 2,
      kind: WIRE_KIND.bool,
      block: Uint8Array.from([urgent ? 1 : 0]),
    });
  }

  const directory = [encodeLeb128(fields.length)];
  const blocks = [];
  let previousId = 0;

  for (const field of fields) {
    directory.push(encodeLeb128(field.id - previousId));
    directory.push(Uint8Array.from([field.kind]));
    directory.push(encodeLeb128(field.block.length));
    blocks.push(field.block);
    previousId = field.id;
  }

  return joinBytes(...directory, ...blocks);
}

function expectDecodeError(fn, code) {
  assert.throws(fn, (error) => error instanceof WireDecodeError && error.code === code);
}

function resolveSelector(source, selector) {
  const matches = [];
  let offset = 0;

  while (offset <= source.length) {
    const found = source.indexOf(selector.text, offset);

    if (found < 0) {
      break;
    }

    matches.push(found);
    offset = found + Math.max(selector.text.length, 1);
  }

  const occurrence = selector.occurrence ?? 0;
  assert.ok(occurrence >= 0 && occurrence < matches.length);

  if (selector.occurrence === undefined) {
    assert.equal(matches.length, 1);
  }

  const start = matches[occurrence];
  return {
    startByte: Buffer.byteLength(source.slice(0, start), "utf8"),
    endByte: Buffer.byteLength(source.slice(0, start + selector.text.length), "utf8"),
  };
}

function mutateKnownBytes(input, offsets, rounds) {
  const source = asBytes(input);
  const mutations = [];
  const masks = [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80];

  for (let round = 0; round < rounds; round += 1) {
    const mutation = source.slice();
    const offset = offsets[round % offsets.length];
    mutation[offset] ^= masks[round % masks.length];
    mutations.push(mutation);
  }

  return mutations;
}

test("MenuKey exact vectors round-trip", () => {
  assert.deepEqual(
    Array.from(encodeExactMenuKey({ id: 42, urgent: undefined })),
    [0x00, 0x2a, 0x00],
  );
  assert.deepEqual(
    Array.from(encodeExactMenuKey({ id: 42, urgent: true })),
    [0x01, 0x2a, 0x00, 0x01],
  );
  assert.deepEqual(decodeExactMenuKey([0x00, 0x2a, 0x00]), {
    id: 42,
    urgent: undefined,
  });
  assert.deepEqual(decodeExactMenuKey([0x01, 0x2a, 0x00, 0x01]), {
    id: 42,
    urgent: true,
  });
});

test("MenuKey compatible vectors round-trip and skip a scalar unknown", () => {
  assert.deepEqual(
    Array.from(encodeCompatibleMenuKey({ id: 42, urgent: undefined })),
    [0x01, 0x01, 0x03, 0x02, 0x2a, 0x00],
  );
  assert.deepEqual(
    Array.from(encodeCompatibleMenuKey({ id: 42, urgent: true })),
    [0x02, 0x01, 0x03, 0x02, 0x01, 0x01, 0x01, 0x2a, 0x00, 0x01],
  );
  assert.deepEqual(
    decodeCompatibleMenuKey([
      0x02, 0x01, 0x03, 0x02, 0x02, 0x0e, 0x02, 0x2a, 0x00, 0xaa, 0xbb,
    ]),
    { id: 42, urgent: undefined },
  );
});

test("strict decoding rejects non-canonical and unsafe forms", () => {
  expectDecodeError(() => decodeExactMenuKey([0x02, 0x2a, 0x00]), "unusedPresenceBit");
  expectDecodeError(() => decodeExactMenuKey([0x00, 0x2a, 0x00, 0x00]), "trailingData");
  expectDecodeError(() => decodeCompatibleMenuKey([0x81, 0x00]), "nonMinimalControlInteger");
  expectDecodeError(
    () => decodeCompatibleMenuKey([0x02, 0x01, 0x03, 0x02, 0x00, 0x01, 0x01, 0x2a, 0x00, 0x01]),
    "duplicateFieldId",
  );
  expectDecodeError(
    () => decodeCompatibleMenuKey([0x02, 0x01, 0x03, 0x02, 0x01, 0x01, 0x01, 0x2a, 0x00, 0x02]),
    "invalidBool",
  );
  expectDecodeError(() => decodeCompatibleMenuKey([0x00]), "missingRequiredField");
  expectDecodeError(
    () => decodeCompatibleMenuKey([0x01, 0x01, 0x03, 0x02, 0x2a]),
    "truncatedBlock",
  );
  expectDecodeError(
    () => decodeCompatibleMenuKey([0x01, 0x01, 0x03, 0x02, 0x2a, 0x00, 0x00]),
    "trailingData",
  );
  expectDecodeError(
    () => decodeCompatibleMenuKey([0x02, 0x01, 0x03, 0x02, 0x02, 0x18, 0x00, 0x2a, 0x00]),
    "unsupportedWireKind",
  );
});

test("deterministic mutations preserve canonical acceptance", () => {
  const cases = [
    {
      decode: decodeExactMenuKey,
      encode: encodeExactMenuKey,
      value: { id: 42, urgent: undefined },
      bytes: [0x00, 0x2a, 0x00],
      offsets: [1, 2],
    },
    {
      decode: decodeExactMenuKey,
      encode: encodeExactMenuKey,
      value: { id: 42, urgent: true },
      bytes: [0x01, 0x2a, 0x00, 0x01],
      offsets: [1, 2, 3],
    },
    {
      decode: decodeCompatibleMenuKey,
      encode: encodeCompatibleMenuKey,
      value: { id: 42, urgent: undefined },
      bytes: [0x01, 0x01, 0x03, 0x02, 0x2a, 0x00],
      offsets: [4, 5],
    },
    {
      decode: decodeCompatibleMenuKey,
      encode: encodeCompatibleMenuKey,
      value: { id: 42, urgent: true },
      bytes: [0x02, 0x01, 0x03, 0x02, 0x01, 0x01, 0x01, 0x2a, 0x00, 0x01],
      offsets: [7, 8, 9],
    },
  ];

  for (const candidate of cases) {
    const canonical = asBytes(candidate.bytes);
    assert.deepEqual(candidate.decode(canonical), candidate.value);

    for (const mutation of mutateKnownBytes(canonical, candidate.offsets, 32)) {
      try {
        const decoded = candidate.decode(mutation);
        assert.deepEqual(candidate.encode(decoded), mutation);
      } catch (error) {
        assert.ok(error instanceof WireDecodeError);
      }
    }
  }
});

test("wire eligibility diagnostic preserves its boundary evidence", () => {
  assert.equal(wireDiagnosticCases.$schema, "w-wire-diagnostic-cases-1");
  assert.equal(wireDiagnosticCases.status, "design-oracle-input");
  assert.equal(wireDiagnosticCases.cases.length, 2);

  const [positive, negative] = wireDiagnosticCases.cases;
  assert.equal(positive.id, "W0-POS-portable-duration");
  assert.equal(positive.kind, "positive");
  assert.equal(positive.expect.eligibility, "data");
  assert.deepEqual(positive.expect.diagnostics, []);
  assert.equal(negative.baseline, positive.id);
  assert.equal(negative.kind, "negative");
  assert.equal(negative.expect.eligibility, "rejected");
  assert.equal(negative.expect.diagnostics.length, 1);

  const diagnostic = negative.expect.diagnostics[0];
  const catalogEntry = diagnosticCatalog.codes.find((entry) => entry.code === diagnostic.code);
  assert.ok(catalogEntry);
  assert.equal(catalogEntry.state, "active");
  assert.equal(diagnostic.phase, catalogEntry.phase);
  assert.equal(diagnostic.severity, catalogEntry.defaultSeverity);
  assert.deepEqual(Object.keys(diagnostic.facts).sort(), Object.keys(catalogEntry.requiredFacts).sort());
  assert.deepEqual(Object.keys(diagnostic.facts), [
    "alternatives",
    "reason",
    "requiredProfiles",
    "type",
    "typePath",
  ]);
  assert.equal(diagnostic.facts.typePath, negative.typePath);

  const source = `${negative.source.join("\n")}\n`;
  const primary = resolveSelector(source, diagnostic.primary);
  assert.equal(
    Buffer.from(source, "utf8").subarray(primary.startByte, primary.endByte).toString("utf8"),
    "Instant",
  );

  const roles = diagnostic.labels.map((label) => label.role).sort();
  assert.deepEqual(roles, ["wire-boundary", "wire-member"]);

  for (const label of diagnostic.labels) {
    resolveSelector(source, label.selector);
  }
});

export {
  WireDecodeError,
  decodeCompatibleMenuKey,
  decodeExactMenuKey,
  encodeCompatibleMenuKey,
  encodeExactMenuKey,
};
