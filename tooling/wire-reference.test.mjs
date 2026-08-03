import assert from "node:assert/strict";
import test from "node:test";

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
  let id;
  let urgent;

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

    if (lengthResult.value > data.length - cursor) {
      throw new WireDecodeError("truncatedBlock");
    }

    const block = data.slice(cursor, cursor + lengthResult.value);
    cursor += lengthResult.value;

    if (currentId === 1) {
      if (kind !== WIRE_KIND.u16 || block.length !== 2) {
        throw new WireDecodeError("invalidWireKind");
      }

      id = block[0] | (block[1] << 8);
    } else if (currentId === 2) {
      if (kind !== WIRE_KIND.bool || block.length !== 1) {
        throw new WireDecodeError("invalidWireKind");
      }

      if (block[0] > 1) {
        throw new WireDecodeError("invalidBool");
      }

      urgent = block[0] === 1;
    } else {
      validateUnknownScalar(kind, block);
    }

    previousId = currentId;
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
      0x02, 0x01, 0x03, 0x02, 0x2a, 0x00, 0x02, 0x0e, 0x02, 0xaa, 0xbb,
    ]),
    { id: 42, urgent: undefined },
  );
});

test("strict decoding rejects non-canonical and unsafe forms", () => {
  expectDecodeError(() => decodeExactMenuKey([0x02, 0x2a, 0x00]), "unusedPresenceBit");
  expectDecodeError(() => decodeExactMenuKey([0x00, 0x2a, 0x00, 0x00]), "trailingData");
  expectDecodeError(() => decodeCompatibleMenuKey([0x81, 0x00]), "nonMinimalControlInteger");
  expectDecodeError(
    () => decodeCompatibleMenuKey([0x02, 0x01, 0x03, 0x02, 0x2a, 0x00, 0x00, 0x01, 0x01, 0x01]),
    "duplicateFieldId",
  );
  expectDecodeError(
    () => decodeCompatibleMenuKey([0x02, 0x01, 0x03, 0x02, 0x2a, 0x00, 0x01, 0x01, 0x01, 0x02]),
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
    () => decodeCompatibleMenuKey([0x02, 0x01, 0x03, 0x02, 0x2a, 0x00, 0x02, 0x18, 0x00]),
    "unsupportedWireKind",
  );
});

export {
  WireDecodeError,
  decodeCompatibleMenuKey,
  decodeExactMenuKey,
  encodeCompatibleMenuKey,
  encodeExactMenuKey,
};
