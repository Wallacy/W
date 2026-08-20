export class FormatterRangeEvidenceError extends Error {
  constructor(message) {
    super(message)
    this.name = "FormatterRangeEvidenceError"
  }
}

function fail(message) {
  throw new FormatterRangeEvidenceError(message)
}

export function splitTrees(output) {
  const text = output.replaceAll("\r\n", "\n").replace(/\u001b\[[0-9;]*m/g, "")
  const starts = [...text.matchAll(/^\(source_file\b/gm)].map((match) => match.index)
  return starts.map((start, index) => text.slice(start, starts[index + 1] ?? text.length).trim())
}

export function hasRecovery(tree) {
  return tree.includes("(ERROR") || tree.includes("(MISSING")
}

const cstRangePattern = /^(\d+):(\d+) +- +(\d+):(\d+)( +)(\S.*)$/

function comparePoints(left, right) {
  if (left.row !== right.row) return left.row - right.row
  return left.column - right.column
}

function parseCstPayload(payload, owner) {
  let marked = false
  if (payload.startsWith("•")) {
    marked = true
    payload = payload.slice(1)
    if (payload.length === 0) {
      fail(`${owner} has an empty marked CST node`)
    }
  }

  let field = null
  const fieldMatch = /^(\w+): (.+)$/.exec(payload)
  if (fieldMatch && fieldMatch[1] !== "MISSING") {
    field = fieldMatch[1]
    payload = fieldMatch[2]
  }

  if (/^"(?:[^"\\]|\\.)*"$/.test(payload)) {
    return { field, marked, name: null, recovery: false, display: payload }
  }
  if (/^`[\s\S]*`$/.test(payload)) {
    return { field, marked, name: null, recovery: false, display: payload }
  }
  if (/^MISSING: ".*"$/.test(payload) || payload === "ERROR") {
    return { field, marked, name: null, recovery: true, display: payload }
  }

  const named = /^(\w+)(?: `([\s\S]*)`)?$/.exec(payload)
  if (!named) {
    fail(`${owner} has a malformed CST node payload ${JSON.stringify(payload)}`)
  }
  return {
    field,
    marked,
    name: named[1],
    recovery: false,
    display: payload,
  }
}

function parseCstRecords(output, expectedCount, owners = []) {
  const text = output.replaceAll("\r\n", "\n").replace(/\u001b\[[0-9;]*m/g, "")
  const lines = text.split("\n")
  const records = []
  let current = null

  for (const [lineIndex, line] of lines.entries()) {
    if (line.length === 0) {
      if (lineIndex === lines.length - 1) continue
      fail(`CST line ${lineIndex + 1} is empty`)
    }
    const match = cstRangePattern.exec(line)
    if (!match) {
      fail(`CST line ${lineIndex + 1} is malformed: ${JSON.stringify(line)}`)
    }

    const node = parseCstPayload(match[6], `CST line ${lineIndex + 1}`)
    const parsed = {
      start: { row: Number(match[1]), column: Number(match[2]) },
      end: { row: Number(match[3]), column: Number(match[4]) },
      labelStart: line.length - match[6].length,
      ...node,
      line: lineIndex + 1,
    }

    if (node.name === "source_file") {
      if (node.field !== null || node.recovery) {
        fail(`CST line ${lineIndex + 1} has an invalid source_file root`)
      }
      if (current !== null) records.push(current)
      current = { root: parsed, nodes: [parsed] }
      continue
    }

    if (current === null) {
      if (node.recovery) {
        const owner = owners[records.length] ?? `CST record ${records.length + 1}`
        fail(`${owner} CST node on line ${lineIndex + 1} uses parser recovery (${node.display})`)
      }
      fail(`CST line ${lineIndex + 1} appears before a source_file root`)
    }
    current.nodes.push(parsed)
  }

  if (current !== null) records.push(current)
  if (records.length !== expectedCount) {
    fail(`CST record count ${records.length} does not match ${expectedCount} sources`)
  }
  if (records.some((record) => record.root === undefined)) {
    fail("CST record is missing its source_file root")
  }
  return records
}

function sourcePointData(source) {
  const bytes = Buffer.from(source, "utf8")
  const lineStarts = [0]
  const lineLengths = []
  let rowStart = 0
  for (let index = 0; index < bytes.length; index += 1) {
    if (bytes[index] !== 0x0a) continue
    const contentEnd = index > rowStart && bytes[index - 1] === 0x0d ? index - 1 : index
    lineLengths.push(contentEnd - rowStart)
    rowStart = index + 1
    lineStarts.push(rowStart)
  }
  lineLengths.push(bytes.length - rowStart)
  return {
    lineLengths,
    lineStarts,
    end: { row: lineLengths.length - 1, column: lineLengths.at(-1) },
  }
}

function opaqueRangesForSource(source, bodies, owner) {
  return bodies.map((body, index) => {
    if (typeof body !== "string" || body.length === 0) {
      fail(`${owner} opaque foreign body ${index} is empty or invalid`)
    }
    const wrapped = `{${body}}`
    const start = source.indexOf(wrapped)
    if (start < 0 || source.indexOf(wrapped, start + 1) >= 0) {
      fail(`${owner} does not contain opaque foreign body ${index} exactly once`)
    }
    const startOffset = Buffer.byteLength(source.slice(0, start), "utf8") + 1
    return {
      startOffset,
      endOffset: startOffset + Buffer.byteLength(body, "utf8"),
    }
  })
}

function validateCstRecord(record, source, owner, opaqueBodies, expectedBom) {
  const points = sourcePointData(source)
  const bytes = Buffer.from(source, "utf8")
  const hasBomPrefix = bytes.subarray(0, 3).equals(Buffer.from([0xef, 0xbb, 0xbf]))
  const encodingPrefixBytes = hasBomPrefix ? 3 : 0
  if (hasBomPrefix !== expectedBom) {
    fail(`${owner} BOM prefix does not match its declared input encoding`)
  }
  const opaqueRanges = opaqueRangesForSource(source, opaqueBodies, owner)
  const pointToOffset = (point, label) => {
    if (!Number.isSafeInteger(point.row) || !Number.isSafeInteger(point.column)) {
      fail(`${owner} ${label} point is not an integer`)
    }
    if (point.row < 0 || point.row >= points.lineLengths.length) {
      fail(`${owner} ${label} row ${point.row} is outside the source`)
    }
    if (point.column < 0 || point.column > points.lineLengths[point.row]) {
      fail(`${owner} ${label} byte column ${point.column} is outside row ${point.row}`)
    }
    return points.lineStarts[point.row] + point.column
  }
  const range = (node) => {
    const startOffset = pointToOffset(node.start, "start")
    const endOffset = pointToOffset(node.end, "end")
    if (comparePoints(node.start, node.end) > 0) {
      fail(`${owner} CST node on line ${node.line} has start after end`)
    }
    return { startOffset, endOffset }
  }
  const validatePointOrder = (node) => {
    if (comparePoints(node.start, node.end) > 0) {
      fail(`${owner} CST node on line ${node.line} has start after end`)
    }
  }

  const rootRange = range(record.root)
  const expectedRootStart = hasBomPrefix ? { row: 0, column: 3 } : { row: 0, column: 0 }
  if (comparePoints(record.root.start, expectedRootStart) !== 0) {
    fail(
      `${owner} CST root starts at ${record.root.start.row}:${record.root.start.column}, expected ${expectedRootStart.row}:${expectedRootStart.column}`,
    )
  }
  if (rootRange.startOffset !== encodingPrefixBytes) {
    fail(`${owner} CST root has an unexpected gap before its encoding prefix`)
  }
  if (comparePoints(record.root.end, points.end) !== 0) {
    fail(
      `${owner} CST root ends at ${record.root.end.row}:${record.root.end.column}, expected ${points.end.row}:${points.end.column}`,
    )
  }

  const conceptualRootRange = {
    startOffset: encodingPrefixBytes === 0 ? rootRange.startOffset : 0,
    endOffset: rootRange.endOffset,
  }
  const rootLabelStart = record.root.labelStart
  const stack = []
  const ranges = new Map()
  const comments = []
  let opaqueRangeExemptions = 0
  for (const node of record.nodes) {
    if (node.recovery) {
      fail(`${owner} CST node on line ${node.line} uses parser recovery (${node.display})`)
    }
    if (node.labelStart < rootLabelStart || (node.labelStart - rootLabelStart) % 2 !== 0) {
      fail(`${owner} CST node on line ${node.line} has unstable structural indentation`)
    }
    const depth = (node.labelStart - rootLabelStart) / 2
    if (depth > stack.length) {
      if (depth !== stack.length + 1) {
        fail(`${owner} CST node on line ${node.line} skips an indentation level`)
      }
    } else {
      stack.length = depth
    }
    const parent = depth > 0 ? stack[depth - 1] : null
    const insideOpaqueBody = parent?.opaque === true
    const isOpaqueBody = node.name === "foreign_body"
    const exemptFromRangeValidation = insideOpaqueBody || isOpaqueBody
    if (!exemptFromRangeValidation) validatePointOrder(node)
    const nodeRange =
      node === record.root
        ? conceptualRootRange
        : exemptFromRangeValidation
          ? null
          : range(node)
    if (nodeRange !== null && node !== record.root && nodeRange.startOffset < encodingPrefixBytes) {
      fail(`${owner} CST node on line ${node.line} overlaps its encoding prefix`)
    }
    if (isOpaqueBody) {
      if (opaqueRangeExemptions >= opaqueRanges.length) {
        fail(`${owner} has an unclassified foreign_body range exemption`)
      }
      opaqueRangeExemptions += 1
    }
    if (depth > 0) {
      if (!parent) {
        fail(`${owner} CST node on line ${node.line} has no indentation parent`)
      }
      if (!exemptFromRangeValidation) {
        const parentRange = parent.range
        if (!parentRange) {
          fail(`${owner} CST node on line ${node.line} has an opaque range parent`)
        }
        if (
          nodeRange.startOffset < parentRange.startOffset ||
          nodeRange.endOffset > parentRange.endOffset
        ) {
          fail(`${owner} CST node on line ${node.line} escapes its indentation parent`)
        }
      }
    }
    if (nodeRange !== null) ranges.set(node, nodeRange)
    stack.push({ node, range: nodeRange, opaque: insideOpaqueBody || isOpaqueBody })

    if (node.name === "comment" && !insideOpaqueBody) {
      const commentBytes = bytes.subarray(nodeRange.startOffset, nodeRange.endOffset)
      const text = commentBytes.toString("utf8")
      if (Buffer.byteLength(text, "utf8") !== commentBytes.length) {
        fail(`${owner} comment on line ${node.line} is not valid UTF-8`)
      }
      if (
        !/^\/\/[^\r\n]*$/.test(text) &&
        !/^\/\*[^*]*\*+([^/*][^*]*\*+)*\/$/.test(text)
      ) {
        fail(`${owner} comment on line ${node.line} is not a valid comment token`)
      }
      comments.push({
        node,
        range: nodeRange,
        text,
        nearestNamedAncestorPath: stack
          .slice(0, depth)
          .map(({ node: ancestor }) => ancestor.name)
          .filter((name) => name !== null),
      })
    }
  }
  if (opaqueRangeExemptions !== opaqueRanges.length) {
    fail(
      `${owner} has ${opaqueRanges.length} opaque foreign bodies but ${opaqueRangeExemptions} foreign_body range exemptions`,
    )
  }
  const recordedRootRange = ranges.get(record.root)
  if (
    recordedRootRange.startOffset !== conceptualRootRange.startOffset ||
    recordedRootRange.endOffset !== conceptualRootRange.endOffset
  ) {
    fail(`${owner} CST root range was not recorded`)
  }
  return { comments, ranges, encodingPrefixBytes, opaqueRangeExemptions, opaqueRanges }
}

function commentSignatures(validated, digest, owner) {
  const signatures = []
  for (const comment of validated.comments) {
    const textDigest = digest(comment.text)
    if (typeof textDigest !== "string") {
      fail(`${owner} digest callback must return a string`)
    }
    signatures.push({ textDigest, nearestNamedAncestorPath: comment.nearestNamedAncestorPath })
  }
  return signatures
}

function sourceMetadata(source, index) {
  if (!source || typeof source !== "object") {
    fail(`source metadata ${index} is not an object`)
  }
  if (typeof source.id !== "string" || source.id.length === 0) {
    fail(`source metadata ${index} has no id`)
  }
  if (typeof source.text !== "string") {
    fail(`${source.id} has no source text`)
  }
  const opaqueForeignBodies = source.opaqueForeignBodies ?? []
  if (!Array.isArray(opaqueForeignBodies)) {
    fail(`${source.id} opaqueForeignBodies is not an array`)
  }
  return {
    id: source.id,
    text: source.text,
    opaqueForeignBodies,
    expectedBom: source.expectedBom === true,
  }
}

function pairMetadata(pair, index, sourceCount) {
  if (!pair || typeof pair !== "object") {
    fail(`pair metadata ${index} is not an object`)
  }
  if (typeof pair.id !== "string" || pair.id.length === 0) {
    fail(`pair metadata ${index} has no id`)
  }
  for (const key of ["inputIndex", "outputIndex"]) {
    if (!Number.isSafeInteger(pair[key]) || pair[key] < 0 || pair[key] >= sourceCount) {
      fail(`${pair.id} has an invalid ${key}`)
    }
  }
  return pair
}

function validatePairing(metadata, pairData) {
  if (metadata.length !== pairData.length * 2) {
    fail(
      `source/pair metadata cardinality is ${metadata.length} sources for ${pairData.length} pairs; expected exactly two sources per pair`,
    )
  }

  const sourceIds = new Set()
  for (const source of metadata) {
    if (sourceIds.has(source.id)) fail(`${source.id} source id is duplicated`)
    sourceIds.add(source.id)
  }

  const pairIds = new Set()
  for (const pair of pairData) {
    if (pairIds.has(pair.id)) fail(`${pair.id} pair id is duplicated`)
    pairIds.add(pair.id)
  }

  const assigned = new Map()
  for (const pair of pairData) {
    if (pair.inputIndex === pair.outputIndex) {
      fail(`${pair.id} inputIndex and outputIndex must differ`)
    }
    for (const index of [pair.inputIndex, pair.outputIndex]) {
      const previous = assigned.get(index)
      if (previous !== undefined) {
        fail(`${pair.id} reuses source index ${index} already assigned to ${previous}`)
      }
      assigned.set(index, pair.id)
    }
  }

  const missing = []
  for (let index = 0; index < metadata.length; index += 1) {
    if (!assigned.has(index)) missing.push(index)
  }
  if (missing.length > 0) {
    fail(`source index coverage is incomplete; unpaired indexes: ${missing.join(", ")}`)
  }
}

export function validateFormatterRangeEvidence({ cstOutput, sources, pairs, digest }) {
  if (typeof cstOutput !== "string") fail("CST output is not text")
  if (!Array.isArray(sources) || sources.length === 0) fail("source metadata is empty")
  if (!Array.isArray(pairs) || pairs.length === 0) fail("pair metadata is empty")
  if (typeof digest !== "function") fail("digest callback is missing")

  const metadata = sources.map(sourceMetadata)
  const pairData = pairs.map((pair, index) => pairMetadata(pair, index, metadata.length))
  validatePairing(metadata, pairData)
  const records = parseCstRecords(
    cstOutput,
    metadata.length,
    metadata.map((source) => source.id),
  )
  const validated = []
  let bomPrefixesValidated = 0
  let opaqueRangeExemptions = 0
  for (const [index, source] of metadata.entries()) {
    const result = validateCstRecord(
      records[index],
      source.text,
      source.id,
      source.opaqueForeignBodies,
      source.expectedBom,
    )
    validated.push(result)
    if (result.encodingPrefixBytes > 0) bomPrefixesValidated += 1
    opaqueRangeExemptions += result.opaqueRangeExemptions
  }

  let commentOccurrences = 0
  let stableAttachmentPairs = 0
  for (const pair of pairData) {
    const input = metadata[pair.inputIndex]
    const output = metadata[pair.outputIndex]
    const inputComments = commentSignatures(
      validated[pair.inputIndex],
      digest,
      `${pair.id} input`,
    )
    const outputComments = commentSignatures(
      validated[pair.outputIndex],
      digest,
      `${pair.id} output`,
    )
    commentOccurrences += inputComments.length + outputComments.length
    if (JSON.stringify(inputComments) !== JSON.stringify(outputComments)) {
      fail(`${pair.id} changes comment text, order, or named attachment`)
    }
    if (inputComments.length > 0 || outputComments.length > 0) stableAttachmentPairs += 1
    if (input.id.length === 0 || output.id.length === 0) {
      fail(`${pair.id} has an empty source id`)
    }
  }

  return {
    rangeValidCsts: validated.length,
    bomPrefixesValidated,
    opaqueRangeExemptions,
    commentOccurrences,
    stableAttachmentPairs,
  }
}
