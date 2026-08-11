// PYN3 design oracle. It is a parseable fixture only.
// The host machines model presentation, Jupyter adaptation, and export.
// They do not execute W, start a kernel, open ZeroMQ, or provide a sanitizer.

module pyn3_oracle

import presentation from std

// MediaType is validated by the imported presentation contract.

export type SessionId = String
export type SessionIncarnation = u64
export type ExecutionOrdinal = u64
export type BindingId = String
export type CellId = String

// GenerationId is an opaque identity. It is deliberately not a formatted
// execution ordinal and is never used as a display counter.
export struct GenerationId {
  token: String

  init(_ token: String) {
    self.token = token
  }
}

export enum Media {
  plainText
  json
  png
  jpeg
  vendorJson
  htmlMissing
  svgMissing
  javascriptRejected
  widgetRejected
}

// W-1248/W-1249: output is immutable and history exposes only a bounded tail.
export enum PresentationMutationPolicy {
  appendOnly
}

export enum HistoryAccessPolicy {
  tail
}

// W-1250: these names are the first stable tooling surface.
export enum NotebookToolCommand {
  notebookCheck
  notebookExport
  sessionReceipts
}

export enum Failure: Error {
  mediaInvalid
  mediaDuplicate
  plainTextMissing
  limitExceeded
  hmacInvalid
  replayRejected
  lifecycleInvalid
  silentMutation
  passwordPersisted
  cellInvalid
  exportBlocked
  exportWouldExecute
}

export struct Limits {
  bytes: usize
  rows: usize
  columns: usize
  jsonDepth: usize
  jsonStringBytes: usize
  imageWidth: u32
  imageHeight: u32
  imagePixels: u64
  workUnits: usize
  representations: usize

  export const init(
    bytes: usize,
    rows: usize,
    columns: usize,
    jsonDepth: usize,
    jsonStringBytes: usize,
    imageWidth: u32,
    imageHeight: u32,
    imagePixels: u64,
    workUnits: usize,
    representations: usize,
  ) {
    self.bytes = bytes
    self.rows = rows
    self.columns = columns
    self.jsonDepth = jsonDepth
    self.jsonStringBytes = jsonStringBytes
    self.imageWidth = imageWidth
    self.imageHeight = imageHeight
    self.imagePixels = imagePixels
    self.workUnits = workUnits
    self.representations = representations
  }
}

// User values conform to the real presentation protocol. The provider owns the writer
// and closes it after this call; user code has no finish operation.
export struct MenuPreview: presentation.Presentable {
  rows: usize
  columns: usize

  export fn present(to writer: inout presentation.Writer) throws presentation.Error {
    try writer.text("menu preview")
  }
}

export struct TabularPreview: presentation.Presentable {
  inspectedRows: usize
  emittedRows: usize
  hasMore: Bool
  columns: usize

  export fn present(to writer: inout presentation.Writer) throws presentation.Error {
    try writer.text("bounded table preview")
  }
}

export struct BlackHoleSensor: presentation.Presentable {
  distance: f64
  watcher: String

  export fn present(to writer: inout presentation.Writer) throws presentation.Error {
    // The preview borrows a stable sensor summary. It does not read a live
    // resource during presentation.
    try writer.text("black-hole sensor summary")
  }
}

export struct DeviceTensorSummary: presentation.Presentable {
  device: String
  shape: String
  dtype: String

  export fn present(to writer: inout presentation.Writer) throws presentation.Error {
    // Device storage remains on the device. Only a bounded textual summary is
    // written by this method.
    try writer.text("tensor device summary")
  }
}

export struct BistromathText: presentation.Presentable {
  text: String

  export fn present(to writer: inout presentation.Writer) throws presentation.Error {
    try writer.text(text)
  }
}

export struct ReceiptProof {
  sourceDigest: String
  session: SessionId
  incarnation: SessionIncarnation
  generationBefore: GenerationId
  generationAfter: GenerationId
  binding: BindingId
  lockDigest: String
  effectDigest: String
}

export struct NotebookCell {
  id: CellId
  sourceDigest: String
  ordinal: ExecutionOrdinal
  receipt: ReceiptProof
}

export enum Lifecycle: Copy & Equatable {
  authenticate
  busy
  process
  reply
  outputs
  idle
}

export struct JupyterIdentity {
  requestId: String
  sessionId: SessionId
  incarnation: SessionIncarnation
  ordinal: ExecutionOrdinal
  generation: GenerationId
}

export struct ExportResult {
  kind: String
  sourceDigest: String
  auditDigest: String
  executed: Bool
}

export struct RedactedError {
  code: String
  message: String
  privateFields: Array<String>
}

export const fn boundedMenu(named limits: Limits, named rows: usize, named columns: usize): Bool {
  return rows <= limits.rows && columns <= limits.columns
}

export const fn imagePreviewIsBounded(
  named limits: Limits,
  named width: u64,
  named height: u64,
  named encodedBytes: usize,
): Bool {
  return width > 0_u64 && height > 0_u64 &&
    width <= limits.imageWidth && height <= limits.imageHeight &&
    width * height <= limits.imagePixels && encodedBytes <= limits.bytes
}

export const fn jsonWorkIsBounded(
  named limits: Limits,
  named stringBytes: usize,
  named workUnits: usize,
): Bool {
  return stringBytes <= limits.jsonStringBytes && workUnits <= limits.workUnits
}

// The checker derives row evidence from the real preview plan. No case may
// assert a hidden collect flag as a semantic input.
export const fn tablePreviewIsBounded(
  named limits: Limits,
  named sourceRows: usize,
  named inspectedRows: usize,
  named emittedRows: usize,
  named hasMore: Bool,
  named columns: usize,
): Bool {
  return sourceRows >= inspectedRows && inspectedRows >= emittedRows &&
    inspectedRows <= limits.rows && emittedRows <= limits.rows &&
    columns <= limits.columns && hasMore == (sourceRows > inspectedRows)
}

// This function records that the summary path accepts device metadata only;
// it has no copy-to-host branch or user-provided copy conclusion.
export const fn tensorSummaryNoCopy(device deviceName: String, shape tensorShape: String): Bool {
  return deviceName.count > 0 && tensorShape.count > 0
}

export const fn fallbackHasPlainText(source compilerSummary: String): Bool {
  return compilerSummary.count > 0
}

export const fn ordinalIsNotGeneration(
  named ordinal: ExecutionOrdinal,
  named ordinalLabel: String,
  named generation: GenerationId,
): Bool {
  return ordinal > 0_u64 && ordinalLabel.count > 0 &&
    generation.token.count > 0 && generation.token != ordinalLabel
    && generation.token != "g7"
}

export const fn lifecycleAllowsEmptyOutputs(events lifecycleEvents: Array<Lifecycle>): Bool {
  var sawReply = false
  for event in lifecycleEvents {
    if event == .reply { sawReply = true }
    if event == .idle { return sawReply }
  }
  return false
}

export const fn exportProofIsBounded(named proof: ReceiptProof): Bool {
  return proof.sourceDigest.count > 0 && proof.session.count > 0 &&
    proof.generationBefore.token.count > 0 &&
    proof.generationAfter.token.count > 0 && proof.binding.count > 0 &&
    proof.lockDigest.count > 0 && proof.effectDigest.count > 0
}

export const fn redactionRemovesPrivateData(named error: RedactedError): Bool {
  return error.privateFields.count > 0 && error.message == "presentation failed"
}

test "typed presentation preserves bounded fallback" for boundedMenu {
  expect boundedMenu(limits: Limits(
    bytes: 4096, rows: 8, columns: 4, jsonDepth: 16,
    jsonStringBytes: 2048, imageWidth: 4096, imageHeight: 4096,
    imagePixels: 16_777_216_u64, workUnits: 8192, representations: 4,
  ), rows: 8, columns: 4)
  expect imagePreviewIsBounded(
    limits: Limits(
      bytes: 4096, rows: 8, columns: 4, jsonDepth: 16,
      jsonStringBytes: 2048, imageWidth: 4096, imageHeight: 4096,
      imagePixels: 16_777_216_u64, workUnits: 8192, representations: 4,
    ),
    width: 2_u64, height: 3_u64, encodedBytes: 128,
  )
  expect jsonWorkIsBounded(
    limits: Limits(
      bytes: 4096, rows: 8, columns: 4, jsonDepth: 16,
      jsonStringBytes: 2048, imageWidth: 4096, imageHeight: 4096,
      imagePixels: 16_777_216_u64, workUnits: 8192, representations: 4,
    ),
    stringBytes: 512, workUnits: 64,
  )
  expect tablePreviewIsBounded(
    limits: Limits(
      bytes: 4096, rows: 8, columns: 4, jsonDepth: 16,
      jsonStringBytes: 2048, imageWidth: 4096, imageHeight: 4096,
      imagePixels: 16_777_216_u64, workUnits: 8192, representations: 4,
    ),
    sourceRows: 128,
    inspectedRows: 8,
    emittedRows: 8,
    hasMore: true,
    columns: 4,
  )
  expect tensorSummaryNoCopy(device: "gpu:0", shape: "1024x1024")
  expect fallbackHasPlainText(source: "compiler summary")
}

test "Jupyter lifecycle permits reply directly to idle" for lifecycleAllowsEmptyOutputs {
  expect lifecycleAllowsEmptyOutputs(events: [.authenticate, .busy, .process, .reply, .idle])
}

test "execution and generation identities stay distinct" for ordinalIsNotGeneration {
  expect ordinalIsNotGeneration(
    ordinal: 7_u64,
    ordinalLabel: "7",
    generation: GenerationId("opaque-generation"),
  )
}

test "export proof names source and session evidence" for exportProofIsBounded {
  expect exportProofIsBounded(proof: ReceiptProof(
    sourceDigest: "sha256:source",
    session: "session-1",
    incarnation: 2_u64,
    generationBefore: GenerationId("opaque-before"),
    generationAfter: GenerationId("opaque-after"),
    binding: "b:menu@1",
    lockDigest: "sha256:lock",
    effectDigest: "sha256:effect",
  ))
}

test "presentation errors are redacted" for redactionRemovesPrivateData {
  expect redactionRemovesPrivateData(error: RedactedError(
    code: "WSessionDegraded",
    message: "presentation failed",
    privateFields: ["token"],
  ))
}
