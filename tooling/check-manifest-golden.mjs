import { createHash } from "node:crypto";

const ascii = (text) => Buffer.from(text, "ascii");
const utf8 = (text) => Buffer.from(text, "utf8");
const bytes = (...parts) => Buffer.concat(parts);

function u8(value) {
  return Buffer.from([value]);
}

function u32(value) {
  const output = Buffer.alloc(4);
  output.writeUInt32BE(value);
  return output;
}

function u64(value) {
  const output = Buffer.alloc(8);
  output.writeBigUInt64BE(BigInt(value));
  return output;
}

function frame(tag, payload) {
  const tagBytes = ascii(tag);
  return bytes(u32(tagBytes.length), tagBytes, u64(payload.length), payload);
}

function sequence(tag, items) {
  return frame(
    tag,
    bytes(u32(items.length), ...items.map((item) => frame("item", item))),
  );
}

function digest(tag, payload) {
  return createHash("sha256").update(frame(tag, payload)).digest();
}

const schema = frame("schema", ascii("w-seed-man0-1"));
const zeroDigest = Buffer.alloc(32);

const limits = Object.freeze({
  maxDocumentBytes: 1 * 1024 * 1024,
  maxAggregateBytes: 16 * 1024 * 1024,
  maxNesting: 256,
  maxStructuralNodes: 262_144,
  maxRootsPerDocument: 2,
  maxDocuments: 256,
  maxScalarSourceBytes: 1 * 1024 * 1024,
  maxNumberDigits: 1 * 1024 * 1024,
  maxDecodedScalarBytes: 1 * 1024 * 1024,
  maxCanonicalBytes: 16 * 1024 * 1024,
  maxWorkUnits: 67_108_864,
});

const limitsWire = bytes(
  u32(limits.maxDocumentBytes),
  u32(limits.maxAggregateBytes),
  u32(limits.maxNesting),
  u32(limits.maxStructuralNodes),
  u32(limits.maxRootsPerDocument),
  u32(limits.maxDocuments),
  u32(limits.maxScalarSourceBytes),
  u32(limits.maxNumberDigits),
  u32(limits.maxDecodedScalarBytes),
  u32(limits.maxCanonicalBytes),
  u64(limits.maxWorkUnits),
);

function candidateRef(generation, directoryOrdinal, candidateIndex) {
  return frame(
    "candidate-ref",
    bytes(u64(generation), u32(directoryOrdinal), u32(candidateIndex)),
  );
}

const bindingNone = bytes(
  u8(0),
  u64(0),
  candidateRef(0, 0, 0),
  zeroDigest,
  zeroDigest,
);

const record = (fields) => ({ kind: "record", fields });
const list = (...items) => ({ kind: "list", items });
const string = (value) => ({ kind: "string", value: utf8(value) });
const decimal = (coefficient, exponent, suffix = "") => ({
  kind: "number",
  radix: 10,
  digits: "",
  coefficient,
  exponent,
  suffix,
});
const integer = (radix, digits, suffix = "") => ({
  kind: "number",
  radix,
  digits,
  coefficient: "",
  exponent: "",
  suffix,
});

function numberCanonical(value) {
  return bytes(
    u8(value.radix),
    frame("digits", ascii(value.digits)),
    frame("coefficient", ascii(value.coefficient)),
    frame("exponent", ascii(value.exponent)),
    frame("suffix", ascii(value.suffix)),
  );
}

function semanticValue(value) {
  switch (value.kind) {
    case "record": {
      const fields = Object.entries(value.fields)
        .sort(([left], [right]) => Buffer.compare(utf8(left), utf8(right)))
        .map(([name, child]) =>
          frame(
            "field",
            bytes(frame("name", utf8(name)), semanticValue(child)),
          ),
        );
      return frame("value", bytes(u8(0), sequence("fields", fields)));
    }
    case "list":
      return frame(
        "value",
        bytes(u8(1), sequence("items", value.items.map(semanticValue))),
      );
    case "string":
      return frame("value", bytes(u8(4), frame("scalar", value.value)));
    case "number":
      return frame(
        "value",
        bytes(u8(5), frame("scalar", numberCanonical(value))),
      );
    default:
      throw new Error(`unsupported oracle value ${value.kind}`);
  }
}

function valueStats(value) {
  switch (value.kind) {
    case "record": {
      const children = Object.values(value.fields).map(valueStats);
      return children.reduce(
        (total, child) => ({
          nodes: total.nodes + child.nodes,
          fields: total.fields + child.fields,
          edges: total.edges + child.edges,
          canonicalBytes: total.canonicalBytes + child.canonicalBytes,
        }),
        { nodes: 1, fields: Object.keys(value.fields).length, edges: 0, canonicalBytes: 0 },
      );
    }
    case "list": {
      const children = value.items.map(valueStats);
      return children.reduce(
        (total, child) => ({
          nodes: total.nodes + child.nodes,
          fields: total.fields + child.fields,
          edges: total.edges + child.edges,
          canonicalBytes: total.canonicalBytes + child.canonicalBytes,
        }),
        { nodes: 1, fields: 0, edges: value.items.length, canonicalBytes: 0 },
      );
    }
    case "string":
      return { nodes: 1, fields: 0, edges: 0, canonicalBytes: value.value.length };
    case "number":
      return {
        nodes: 1,
        fields: 0,
        edges: 0,
        canonicalBytes: numberCanonical(value).length,
      };
    default:
      throw new Error(`unsupported oracle value ${value.kind}`);
  }
}

function countsWire(counts) {
  return bytes(
    u32(counts.documents),
    u32(counts.roots),
    u32(counts.nodes),
    u32(counts.fields),
    u32(counts.edges),
    u32(counts.canonicalBytes),
    u32(counts.structuralNodes),
  );
}

function documentDigests(spec, candidateOrdinal) {
  const roots = Object.entries(spec.roots)
    .sort(([left], [right]) => (left === right ? 0 : left === "package" ? -1 : 1));
  const stats = roots.map(([, root]) => valueStats(root));
  const counts = stats.reduce(
    (total, value) => ({
      documents: 1,
      roots: roots.length,
      nodes: total.nodes + value.nodes,
      fields: total.fields + value.fields,
      edges: total.edges + value.edges,
      canonicalBytes: total.canonicalBytes + value.canonicalBytes,
      structuralNodes:
        roots.length +
        total.nodes + value.nodes +
        total.fields + value.fields +
        total.edges + value.edges,
    }),
    { documents: 1, roots: roots.length, nodes: 0, fields: 0, edges: 0, canonicalBytes: 0, structuralNodes: roots.length },
  );
  counts.structuralNodes = counts.roots + counts.nodes + counts.fields + counts.edges;

  const sourceDigest = digest(
    "w.seed.man0.document.source/1",
    utf8(spec.source),
  );
  const semanticRoots = roots.map(([kind, value]) =>
    frame("root", bytes(u8(kind === "package" ? 0 : 1), semanticValue(value))),
  );
  const semanticDigest = digest(
    "w.seed.man0.document.semantic/1",
    bytes(schema, sequence("roots", semanticRoots)),
  );
  const provenanceDigest = digest(
    "w.seed.man0.document.provenance/1",
    bytes(
      schema,
      frame("candidate-ordinal", u32(candidateOrdinal)),
      bindingNone,
      frame("source-digest", sourceDigest),
    ),
  );
  const receiptDigest = digest(
    "w.seed.man0.document.receipt/1",
    bytes(
      schema,
      frame("limits", limitsWire),
      frame("counts", countsWire(counts)),
      frame("binding", bindingNone),
      frame("source-digest", sourceDigest),
      frame("semantic-digest", semanticDigest),
      frame("provenance-digest", provenanceDigest),
    ),
  );
  return { counts, sourceDigest, semanticDigest, provenanceDigest, receiptDigest };
}

function batchDigests(specs) {
  const documents = specs.map((spec, index) => documentDigests(spec, index));
  const counts = documents.reduce(
    (total, document) => {
      for (const key of Object.keys(total)) total[key] += document.counts[key];
      return total;
    },
    { documents: 0, roots: 0, nodes: 0, fields: 0, edges: 0, canonicalBytes: 0, structuralNodes: 0 },
  );
  const semanticDigest = digest(
    "w.seed.man0.batch.semantic/1",
    bytes(schema, sequence("documents", documents.map((item) => item.semanticDigest))),
  );
  const provenanceDigest = digest(
    "w.seed.man0.batch.provenance/1",
    bytes(schema, sequence("documents", documents.map((item) => item.provenanceDigest))),
  );
  const receiptDigest = digest(
    "w.seed.man0.batch.receipt/1",
    bytes(
      schema,
      frame("limits", limitsWire),
      frame("counts", countsWire(counts)),
      frame("semantic-digest", semanticDigest),
      frame("provenance-digest", provenanceDigest),
      sequence("documents", documents.map((item) => item.receiptDigest)),
    ),
  );
  return { counts, documents, semanticDigest, provenanceDigest, receiptDigest };
}

const alphaBeta = record({ alpha: decimal("1", "0"), beta: string("A") });
const scenarios = Object.freeze({
  S0: {
    source: 'package { alpha: 1 beta: "A" }\n',
    roots: { package: alphaBeta },
  },
  S1: {
    source: 'package {\n  // same semantics\n  beta: "\\u{41}",\n  alpha: 1.0e0,\n}\n',
    roots: { package: alphaBeta },
  },
  S2: {
    source: "workspace {}\npackage {}\n",
    roots: { workspace: record({}), package: record({}) },
  },
  S3: {
    source: "package { value: [1, 2] }\n",
    roots: { package: record({ value: list(decimal("1", "0"), decimal("2", "0")) }) },
  },
  S4: {
    source: "package { value: [2, 1] }\n",
    roots: { package: record({ value: list(decimal("2", "0"), decimal("1", "0")) }) },
  },
  S5: {
    source: 'package { decimal: 1_000.0e+2, text: "A\\nB", hex: 0x00_Af }\n',
    roots: {
      package: record({
        decimal: decimal("1", "5"),
        text: string("A\nB"),
        hex: integer(16, "af"),
      }),
    },
  },
});

function toJson(batch) {
  const hex = (value) => value.toString("hex");
  return {
    counts: batch.counts,
    document: batch.documents.map((document) => ({
      counts: document.counts,
      source: hex(document.sourceDigest),
      semantic: hex(document.semanticDigest),
      provenance: hex(document.provenanceDigest),
      receipt: hex(document.receiptDigest),
    })),
    batch: {
      semantic: hex(batch.semanticDigest),
      provenance: hex(batch.provenanceDigest),
      receipt: hex(batch.receiptDigest),
    },
  };
}

const output = Object.fromEntries(
  Object.entries(scenarios).map(([name, spec]) => [name, toJson(batchDigests([spec]))]),
);
output.S0_S2 = toJson(batchDigests([scenarios.S0, scenarios.S2]));

const expectedDocument = (counts, source, semantic, provenance, receipt) => ({
  counts,
  source,
  semantic,
  provenance,
  receipt,
});

const expected = Object.freeze({
  S0: {
    counts: [1, 1, 3, 2, 0, 83, 6],
    documents: [expectedDocument(
      [1, 1, 3, 2, 0, 83, 6],
      "2f44e64c86b98866777b0b559195b2662a753ff3787aa2fa460ed38b216d3c65",
      "794fb70a3a6c6b67981431ba97602c183071d0f0a3b661e9f744dd7f950dafe8",
      "1d8ea6e2b4f239b0c4a3e6d4f33378b3a2077804bfe07a436ab6d5c36fb3a5e2",
      "77d7817c026699591dc0f95085c8148a47fc817902e598ebdec5e2e2e16b267f",
    )],
    batch: {
      semantic: "0e5d2fb4aa0a26251911f7f85f7b38220086196b9dc333368cc78dbdf004a158",
      provenance: "727d3e8f832a58e93f3c9162423b9a743a501a6cf610683828c93a85a930ea69",
      receipt: "e919595832fed9265bb1711b6f160f160ec28453873008ab34ed7c2bbcdf9f8b",
    },
  },
  S1: {
    counts: [1, 1, 3, 2, 0, 83, 6],
    documents: [expectedDocument(
      [1, 1, 3, 2, 0, 83, 6],
      "19ae808cc6125fdd950b8d7c2e21718b45dfc45967f630188ade71b453d3960f",
      "794fb70a3a6c6b67981431ba97602c183071d0f0a3b661e9f744dd7f950dafe8",
      "70f368a8b423bf89373704b5089a423d0b555f9aa67621caf7bdf639feb24890",
      "ebd7200cfb8415df2a5ca100fbb32ae978160c09945f551d61a0b7195756bdf9",
    )],
    batch: {
      semantic: "0e5d2fb4aa0a26251911f7f85f7b38220086196b9dc333368cc78dbdf004a158",
      provenance: "f2d44c482d3e96ebd292124704c040efa4f824bb32c4fcb87464c5590c55a493",
      receipt: "eb3ebc846ce3e1780a35da2108ecb446e867dfd3d03c233646b52d2f59660b5d",
    },
  },
  S2: {
    counts: [1, 2, 2, 0, 0, 0, 4],
    documents: [expectedDocument(
      [1, 2, 2, 0, 0, 0, 4],
      "b9f315ca23902dd7b05ca6403cbcd59d75c247faeb8401a19bcfc30e573a41df",
      "4687b3fcc2441603c4e8c5f1f4b7eaccb28dc7471eadde3fa53b3f3e702108f3",
      "9e734d08c7dc79397dd80e37b1179fc80edf544348272d0300f51cb3fe8a9c4b",
      "42490fc9fe98a33a091806c0d33a9c7e0641d2946bd75488cb6f5b0084c8e8bb",
    )],
    batch: {
      semantic: "5f0d26525af8630fcec1e3bf5c28d2fb44c63c4ad9e374612bf5565ecbd1fc1f",
      provenance: "e7b735f4d7d70bd48906690c637450078613e567119e58323081f77ba64128a7",
      receipt: "2d4171087f491c60385844110c1e3b37603998b43df8c29acc87d775a0bba6cc",
    },
  },
  S3: {
    counts: [1, 1, 4, 1, 2, 164, 8],
    documents: [expectedDocument(
      [1, 1, 4, 1, 2, 164, 8],
      "8dd1b489e2cbd9b8f121c233893550d27568e9733da2b03bf62df5bcb49b7365",
      "76cbe9c7f37bc7651838412766b9c434d5ad47de2f3eac0591665fb2b45fc13b",
      "31887b27010a9f3bc650c90c2658a8bebf6ee8b8e4c32f848c21dce9da7fab0c",
      "fcc173c43b45dee44962fae0263c85736ae52ab78f65988426e362a28cc2bd5e",
    )],
    batch: {
      semantic: "f46953e0ecf67065c54faeb51c4828e8f61e711b0e957636147bc8fa16312e8c",
      provenance: "0ca41ef4593ce2b7282e607e60a9f537c5c138ddf2cdfafe8a735d293af6cd22",
      receipt: "5af7847274dacd86bfe955b5bb9c09f0d6f9dfcf4dc2dc3057ef710f09699011",
    },
  },
  S4: {
    counts: [1, 1, 4, 1, 2, 164, 8],
    documents: [expectedDocument(
      [1, 1, 4, 1, 2, 164, 8],
      "b1be81c1f0dee36adb4a8c811eac24039191daea0b5feee913535780e53f257b",
      "c276161e074c8e13e3b041a707277267383ac13f016e084129232631b6650369",
      "8d58ad7103e8162092c5a973ffb15f281d0db56fb42f8d4b5962c9c6897eb936",
      "90d8fe47d40e6ce7dd70b0dcfe15b5c4b01523c3db7e387de715454f80bde0b1",
    )],
    batch: {
      semantic: "62a4c84da39d46bd174e1aba5fc9fe0df944f742d157bea799ed30e91a3186a5",
      provenance: "19aba5274f2fcac04b5cb75ef2dc211c93cfc2d3d8eb242699a1927e9a300cc5",
      receipt: "d5548ccf6fa53807c875a1488cb1cf7580586fa4adf8f92d15e8a7310df85782",
    },
  },
  S5: {
    counts: [1, 1, 4, 3, 0, 167, 8],
    documents: [expectedDocument(
      [1, 1, 4, 3, 0, 167, 8],
      "b1e9677b41597272e2d2c1be41a56dfc3e564a5d8fa94fc0d9ba48a1d5ab1566",
      "1b165953f56bca327d0a5f6f869f29e21342f972b9610a0eba9374733042532b",
      "587dacd5b2cab5af18710f844e482128724f7d54470770504dc8bee8036d9863",
      "7707f85ae159071d502561532cb58b5ec81b0eb4a2c83b419fb05925cbeb49e7",
    )],
    batch: {
      semantic: "1de20a742bd30d97805f15a9f7a4b0fb336af91f7adc075828466f5ed577a972",
      provenance: "b66b5fb677a2d5f374ec05081b3fc6a6d7b1a98ccaf5accbc6151ea3535c8cae",
      receipt: "c29d9cfb250dc24d2d805e993602474c380ce0b4025aadc8ed1bd7271fe7b06c",
    },
  },
  S0_S2: {
    counts: [2, 3, 5, 2, 0, 83, 10],
    documents: [
      expectedDocument(
        [1, 1, 3, 2, 0, 83, 6],
        "2f44e64c86b98866777b0b559195b2662a753ff3787aa2fa460ed38b216d3c65",
        "794fb70a3a6c6b67981431ba97602c183071d0f0a3b661e9f744dd7f950dafe8",
        "1d8ea6e2b4f239b0c4a3e6d4f33378b3a2077804bfe07a436ab6d5c36fb3a5e2",
        "77d7817c026699591dc0f95085c8148a47fc817902e598ebdec5e2e2e16b267f",
      ),
      expectedDocument(
        [1, 2, 2, 0, 0, 0, 4],
        "b9f315ca23902dd7b05ca6403cbcd59d75c247faeb8401a19bcfc30e573a41df",
        "4687b3fcc2441603c4e8c5f1f4b7eaccb28dc7471eadde3fa53b3f3e702108f3",
        "51f93588d10a443993a74b09ac47ef54b9403a751cd1a263f114530a0cd68bae",
        "96496b00b45715baadc1a46306518266ea66f8f059f8c6de2435560f8edeba33",
      ),
    ],
    batch: {
      semantic: "377c9ec12efa15549f8bc71bd1fe3aa4968d9693f7d11577722283d6242b1fed",
      provenance: "b2c87d21c6ad20075b79b8337afb60d94bab3cb92dd022c852d5c831332a8b0b",
      receipt: "f216ce3ea77a4f4a2cc050a2c36fd6d901cf5d7c6f16e95daa9ee9c08a6518ca",
    },
  },
});

const countsArray = (counts) => [
  counts.documents,
  counts.roots,
  counts.nodes,
  counts.fields,
  counts.edges,
  counts.canonicalBytes,
  counts.structuralNodes,
];

function firstMismatch(name, actual, expectedValue) {
  if (JSON.stringify(countsArray(actual.counts)) !== JSON.stringify(expectedValue.counts))
    return `${name}.counts`;
  if (actual.document.length !== expectedValue.documents.length)
    return `${name}.document.length`;
  for (let index = 0; index < actual.document.length; index += 1) {
    const actualDocument = actual.document[index];
    const expectedDocumentValue = expectedValue.documents[index];
    if (JSON.stringify(countsArray(actualDocument.counts)) !==
        JSON.stringify(expectedDocumentValue.counts))
      return `${name}.document[${index}].counts`;
    for (const field of ["source", "semantic", "provenance", "receipt"])
      if (actualDocument[field] !== expectedDocumentValue[field])
        return `${name}.document[${index}].${field}`;
  }
  for (const field of ["semantic", "provenance", "receipt"])
    if (actual.batch[field] !== expectedValue.batch[field])
      return `${name}.batch.${field}`;
  return null;
}

if (process.argv.includes("--print")) {
  process.stdout.write(`${JSON.stringify(output, null, 2)}\n`);
} else {
  for (const [name, expectedValue] of Object.entries(expected)) {
    const mismatch = firstMismatch(name, output[name], expectedValue);
    if (mismatch !== null) {
      console.error(`manifest golden mismatch: ${mismatch}`);
      process.exitCode = 1;
      break;
    }
  }
}
