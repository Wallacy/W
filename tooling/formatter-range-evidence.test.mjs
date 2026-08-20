import assert from "node:assert/strict"
import test from "node:test"
import {
  FormatterRangeEvidenceError,
  validateFormatterRangeEvidence,
} from "./formatter-range-evidence.mjs"

function pointForByte(source, offset) {
  const bytes = Buffer.from(source, "utf8")
  let row = 0
  let rowStart = 0
  for (let index = 0; index < offset; index += 1) {
    if (bytes[index] === 0x0a) {
      row += 1
      rowStart = index + 1
    }
  }
  return { row, column: offset - rowStart }
}

function byteOffset(source, characterIndex) {
  return Buffer.byteLength(source.slice(0, characterIndex), "utf8")
}

function node(start, end, depth, payload) {
  return { start, end, depth, payload }
}

function nodesFor(source, { bodyName = "block" } = {}) {
  const commentStartCharacter = source.indexOf("//")
  const commentEndCharacter = source.indexOf("\n", commentStartCharacter)
  const openCharacter = source.indexOf("{")
  const closeCharacter = source.lastIndexOf("}")
  const sourceEnd = byteOffset(source, closeCharacter + 1)
  const openOffset = byteOffset(source, openCharacter)
  const closeOffset = byteOffset(source, closeCharacter)
  const commentStart = byteOffset(source, commentStartCharacter)
  const commentEnd = byteOffset(source, commentEndCharacter)
  const rootStart = source.startsWith("\uFEFF")
    ? { row: 0, column: 3 }
    : { row: 0, column: 0 }
  const rootEnd = pointForByte(source, Buffer.byteLength(source, "utf8"))
  return [
    node(rootStart, rootEnd, 0, "source_file"),
    node(rootStart, pointForByte(source, sourceEnd), 1, "function"),
    node(pointForByte(source, openOffset), pointForByte(source, sourceEnd), 2, `body: ${bodyName}`),
    node(pointForByte(source, openOffset), pointForByte(source, openOffset + 1), 3, '"{"'),
    node(
      pointForByte(source, commentStart),
      pointForByte(source, commentEnd),
      3,
      `comment \`${source.slice(commentStartCharacter, commentEndCharacter)}\``,
    ),
    node(pointForByte(source, closeOffset), pointForByte(source, closeOffset + 1), 3, '"}"'),
  ]
}

function renderRecord(nodes) {
  const ranges = nodes.map(
    ({ start, end }) => `${start.row}:${start.column} - ${end.row}:${end.column}`,
  )
  const width = Math.max(...ranges.map((range) => range.length), 1) + 1
  return nodes
    .map(({ depth, payload }, index) => {
      const range = ranges[index]
      const labelStart = width + depth * 2
      return range + " ".repeat(labelStart - range.length) + payload
    })
    .join("\n")
}

function makeCase({
  inputText,
  outputText = inputText,
  inputNodes = nodesFor(inputText),
  outputNodes = nodesFor(outputText),
  inputMeta = {},
  outputMeta = {},
}) {
  return {
    cstOutput: [renderRecord(inputNodes), renderRecord(outputNodes)].join("\n"),
    sources: [
      {
        id: "fixture input",
        text: inputText,
        expectedBom: inputMeta.expectedBom,
        opaqueForeignBodies: inputMeta.opaqueForeignBodies ?? [],
      },
      {
        id: "fixture output",
        text: outputText,
        expectedBom: outputMeta.expectedBom,
        opaqueForeignBodies: outputMeta.opaqueForeignBodies ?? [],
      },
    ],
    pairs: [{ id: "fixture", inputIndex: 0, outputIndex: 1 }],
    digest: (text) => `digest:${text}`,
  }
}

function validateCase(options) {
  return validateFormatterRangeEvidence(makeCase(options))
}

function makeFourSourceCase() {
  const records = [
    nodesFor(validInput),
    nodesFor(validOutput),
    nodesFor(validInput),
    nodesFor(validOutput),
  ]
  return {
    cstOutput: records.map(renderRecord).join("\n"),
    sources: [
      { id: "fixture input", text: validInput },
      { id: "fixture output", text: validOutput },
      { id: "second input", text: validInput },
      { id: "second output", text: validOutput },
    ],
    pairs: [
      { id: "fixture", inputIndex: 0, outputIndex: 1 },
      { id: "second", inputIndex: 2, outputIndex: 3 },
    ],
    digest: (text) => `digest:${text}`,
  }
}

function expectEvidenceFailure(action, message) {
  assert.throws(action, (error) => {
    assert.ok(error instanceof FormatterRangeEvidenceError)
    assert.match(error.message, new RegExp(message.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")))
    return true
  })
}

const validInput = "fn café(){\n  // olá\n}\n"
const validOutput = "fn café() {\n  // olá\n}\n"

test("valid LF and Unicode CST evidence preserves comment attachment", () => {
  const evidence = validateCase({ inputText: validInput, outputText: validOutput })
  assert.deepEqual(evidence, {
    rangeValidCsts: 2,
    bomPrefixesValidated: 0,
    opaqueRangeExemptions: 0,
    commentOccurrences: 2,
    stableAttachmentPairs: 1,
  })
})

test("rejects an adulterated root endpoint", () => {
  const badOutput = nodesFor(validOutput)
  badOutput[0].end = { row: 0, column: 0 }
  expectEvidenceFailure(
    () => validateCase({ inputText: validInput, outputText: validOutput, outputNodes: badOutput }),
    "CST root ends",
  )
})

test("rejects ERROR and MISSING parser recovery nodes with their source owner", () => {
  for (const recovery of ["ERROR", 'MISSING: "comment"']) {
    const badOutput = nodesFor(validOutput)
    badOutput[4].payload = recovery
    expectEvidenceFailure(
      () => validateCase({ inputText: validInput, outputText: validOutput, outputNodes: badOutput }),
      `fixture output CST node on line`,
    )
  }
})

test("rejects a child range outside its indentation parent", () => {
  const badInput = nodesFor(validInput)
  badInput[2].end = { row: 1, column: 2 }
  expectEvidenceFailure(
    () => validateCase({ inputText: validInput, inputNodes: badInput, outputText: validOutput }),
    "escapes its indentation parent",
  )
})

test("accepts declared BOM prefixes and rejects missing or forged prefixes", () => {
  const bomInput = "\uFEFF" + validInput
  const bomOutput = "\uFEFF" + validOutput
  const valid = validateCase({
    inputText: bomInput,
    outputText: bomOutput,
    inputMeta: { expectedBom: true },
    outputMeta: { expectedBom: true },
  })
  assert.equal(valid.bomPrefixesValidated, 2)

  expectEvidenceFailure(
    () =>
      validateCase({
        inputText: validInput,
        outputText: validOutput,
        inputMeta: { expectedBom: true },
        outputMeta: { expectedBom: true },
      }),
    "BOM prefix does not match",
  )
  expectEvidenceFailure(
    () =>
      validateCase({
        inputText: bomInput,
        outputText: bomOutput,
        inputMeta: { expectedBom: false },
        outputMeta: { expectedBom: false },
      }),
    "BOM prefix does not match",
  )
})

const foreignBody = "\n  raw;\n"
const foreignInput = `fn<C>x(){${foreignBody}}\n`
const foreignNodes = [
  node({ row: 0, column: 0 }, { row: 3, column: 0 }, 0, "source_file"),
  node({ row: 0, column: 0 }, { row: 2, column: 1 }, 1, "function"),
  node({ row: 0, column: 999 }, { row: 99, column: 1 }, 2, "body: foreign_body"),
  node({ row: 99, column: 1 }, { row: 99, column: 4 }, 3, "`opaque raw`"),
]

test("pairs each foreign_body exemption and leaves its descendants opaque", () => {
  const evidence = validateCase({
    inputText: foreignInput,
    outputText: foreignInput,
    inputNodes: foreignNodes,
    outputNodes: foreignNodes,
    inputMeta: { opaqueForeignBodies: [foreignBody] },
    outputMeta: { opaqueForeignBodies: [foreignBody] },
  })
  assert.equal(evidence.opaqueRangeExemptions, 2)

  expectEvidenceFailure(
    () =>
      validateCase({
        inputText: foreignInput,
        outputText: foreignInput,
        inputNodes: foreignNodes,
        outputNodes: foreignNodes,
      }),
    "unclassified foreign_body range exemption",
  )
  expectEvidenceFailure(
    () =>
      validateCase({
        inputText: foreignInput,
        outputText: foreignInput,
        inputNodes: foreignNodes.slice(0, 2),
        outputNodes: foreignNodes.slice(0, 2),
        inputMeta: { opaqueForeignBodies: [foreignBody] },
        outputMeta: { opaqueForeignBodies: [foreignBody] },
      }),
    "opaque foreign bodies but 0 foreign_body range exemptions",
  )
})

test("rejects changed comment text and changed named ancestor attachment", () => {
  const changedText = "fn café() {\n  // adeus\n}\n"
  expectEvidenceFailure(
    () => validateCase({ inputText: validInput, outputText: changedText }),
    "changes comment text, order, or named attachment",
  )

  const changedAncestor = nodesFor(validOutput, { bodyName: "return_statement" })
  expectEvidenceFailure(
    () =>
      validateCase({
        inputText: validInput,
        outputText: validOutput,
        outputNodes: changedAncestor,
      }),
    "changes comment text, order, or named attachment",
  )
})

test("rejects malformed CST lines and divergent record counts", () => {
  const fixture = makeCase({ inputText: validInput, outputText: validOutput })
  expectEvidenceFailure(
    () => validateFormatterRangeEvidence({ ...fixture, cstOutput: fixture.cstOutput + "\nnot a CST line" }),
    "CST line",
  )
  expectEvidenceFailure(
    () => validateFormatterRangeEvidence({ ...fixture, cstOutput: renderRecord(nodesFor(validInput)) }),
    "CST record count",
  )
})

test("rejects pairing metadata drift before comparing CST comments", () => {
  const duplicatePair = makeFourSourceCase()
  duplicatePair.pairs[1].id = duplicatePair.pairs[0].id
  expectEvidenceFailure(
    () => validateFormatterRangeEvidence(duplicatePair),
    "pair id is duplicated",
  )

  const reusedSource = makeFourSourceCase()
  reusedSource.pairs[1].inputIndex = 0
  expectEvidenceFailure(
    () => validateFormatterRangeEvidence(reusedSource),
    "reuses source index 0",
  )

  const duplicateSource = makeFourSourceCase()
  duplicateSource.sources[2].id = duplicateSource.sources[0].id
  expectEvidenceFailure(
    () => validateFormatterRangeEvidence(duplicateSource),
    "source id is duplicated",
  )

  const sameSource = makeFourSourceCase()
  sameSource.pairs[0].outputIndex = sameSource.pairs[0].inputIndex
  expectEvidenceFailure(
    () => validateFormatterRangeEvidence(sameSource),
    "inputIndex and outputIndex must differ",
  )

  const unpairedSource = makeCase({ inputText: validInput, outputText: validOutput })
  unpairedSource.sources.push({ id: "unpaired", text: validInput })
  expectEvidenceFailure(
    () => validateFormatterRangeEvidence(unpairedSource),
    "source/pair metadata cardinality",
  )
})
