// Public value contracts for HTTP messages and server admission.

import iec from std
import { AbortReason, AbortSignal } from std.abort
import { Blob } from std.blob
import cache from std.cache
import database from std.database
import json from std.json
import net from std.net
import { ReadableStream } from std.stream
import { URL, URLSearchParams } from std.url

const fn isHttpTokenByte(byte: u8): Bool {
  if (byte >= b'0' && byte <= b'9')
    || (byte >= b'A' && byte <= b'Z')
    || (byte >= b'a' && byte <= b'z') {
    return true
  }

  return switch byte {
    case 33: true  // !
    case 35: true  // #
    case 36: true  // $
    case 37: true  // %
    case 38: true  // &
    case 39: true  // '
    case 42: true  // *
    case 43: true  // +
    case 45: true  // -
    case 46: true  // .
    case 94: true  // ^
    case 95: true  // _
    case 96: true  // `
    case 124: true // |
    case 126: true // ~
    case _: false
  }
}

const fn isHttpToken(value: ref String): Bool {
  if value.bytes.isEmpty { return false }

  for byte in value.bytes {
    if !isHttpTokenByte(byte: byte) { return false }
  }

  return true
}

const fn isHttpFieldValue(value: ref String): Bool {
  for byte in value.bytes {
    if byte != 9 && (byte < 32 || byte == 127) { return false }
  }

  return true
}

const fn asciiLowercaseScalar(value: UnicodeScalar): UnicodeScalar {
  if value == 'A' { return 'a' }
  if value == 'B' { return 'b' }
  if value == 'C' { return 'c' }
  if value == 'D' { return 'd' }
  if value == 'E' { return 'e' }
  if value == 'F' { return 'f' }
  if value == 'G' { return 'g' }
  if value == 'H' { return 'h' }
  if value == 'I' { return 'i' }
  if value == 'J' { return 'j' }
  if value == 'K' { return 'k' }
  if value == 'L' { return 'l' }
  if value == 'M' { return 'm' }
  if value == 'N' { return 'n' }
  if value == 'O' { return 'o' }
  if value == 'P' { return 'p' }
  if value == 'Q' { return 'q' }
  if value == 'R' { return 'r' }
  if value == 'S' { return 's' }
  if value == 'T' { return 't' }
  if value == 'U' { return 'u' }
  if value == 'V' { return 'v' }
  if value == 'W' { return 'w' }
  if value == 'X' { return 'x' }
  if value == 'Y' { return 'y' }
  if value == 'Z' { return 'z' }
  return value
}

const fn asciiLowercaseToken(value: take String): String {
  var result = String(reservingBytes: value.bytes.count)

  for scalar in value.scalars {
    result.append(asciiLowercaseScalar(value: scalar))
  }

  return result
}

const fn normalizeHttpFieldValue(value: take String): String throws HttpSyntaxError {
  guard isHttpFieldValue(value: value) else throw .invalidHeaderValue

  var normalized = String(reservingBytes: value.bytes.count)
  var pendingWhitespace = String()
  var started = false

  for scalar in value.scalars {
    if scalar == ' ' || scalar == '\t' {
      if started { pendingWhitespace.append(scalar) }
      continue
    }

    normalized.append(pendingWhitespace)
    pendingWhitespace.clear()
    normalized.append(scalar)
    started = true
  }

  return normalized
}

const fn asciiLowercaseByte(value: u8): u8 {
  if value >= b'A' && value <= b'Z' { return value + (b'a' - b'A') }
  return value
}

fn asciiCaseInsensitiveRangeEquals(
  value: ref String,
  start: usize,
  end: usize,
  expected: ref String,
): Bool {
  if end - start != expected.bytes.count { return false }

  var offset: usize = 0
  while offset < expected.bytes.count {
    if asciiLowercaseByte(value: value.bytes[start + offset]) != expected.bytes[offset] {
      return false
    }
    offset += 1
  }

  return true
}

fn startsWithAsciiCaseInsensitive(value: ref String, expected: ref String): Bool {
  if value.bytes.count < expected.bytes.count { return false }
  return asciiCaseInsensitiveRangeEquals(value: value, start: 0, end: expected.bytes.count, expected: expected)
}

fn containsForbiddenMethod(value: ref String): Bool {
  var start: usize = 0

  while start <= value.bytes.count {
    var separator = start
    while separator < value.bytes.count && value.bytes[separator] != b',' {
      separator += 1
    }
    var end = separator

    while start < end && (value.bytes[start] == b' ' || value.bytes[start] == b'\t') {
      start += 1
    }
    while end > start && (value.bytes[end - 1] == b' ' || value.bytes[end - 1] == b'\t') {
      end -= 1
    }

    if asciiCaseInsensitiveRangeEquals(value: value, start: start, end: end, expected: "connect")
      || asciiCaseInsensitiveRangeEquals(value: value, start: start, end: end, expected: "trace")
      || asciiCaseInsensitiveRangeEquals(value: value, start: start, end: end, expected: "track") {
      return true
    }

    if separator == value.bytes.count { return false }
    start = separator + 1
  }

  return false
}

const fn isCorsUnsafeRequestHeaderByte(value: u8): Bool {
  if value < 32 && value != b'\t' { return true }
  if value == 127 { return true }

  return switch value {
    case 34: true  // "
    case 40: true  // (
    case 41: true  // )
    case 58: true  // :
    case 60: true  // <
    case 62: true  // >
    case 63: true  // ?
    case 64: true  // @
    case 91: true  // [
    case 92: true  // backslash
    case 93: true  // ]
    case 123: true // {
    case 125: true // }
    case _: false
  }
}

fn containsCorsUnsafeRequestHeaderByte(value: ref String): Bool {
  for byte in value.bytes {
    if isCorsUnsafeRequestHeaderByte(value: byte) { return true }
  }
  return false
}

const fn isCorsLanguageByte(value: u8): Bool {
  if (value >= b'0' && value <= b'9')
    || (value >= b'A' && value <= b'Z')
    || (value >= b'a' && value <= b'z') {
    return true
  }

  return value == b' ' || value == b'*' || value == b',' || value == b'-'
    || value == b'.' || value == b';' || value == b'='
}

fn isCorsLanguageValue(value: ref String): Bool {
  for byte in value.bytes {
    if !isCorsLanguageByte(value: byte) { return false }
  }
  return true
}

fn isNoCorsSafelistedMediaType(value: ref String): Bool {
  if containsCorsUnsafeRequestHeaderByte(value: value) { return false }

  var start: usize = 0
  var end = value.bytes.count
  while start < end && (value.bytes[start] == b' ' || value.bytes[start] == b'\t') {
    start += 1
  }

  var essenceEnd = start
  while essenceEnd < end && value.bytes[essenceEnd] != b';' { essenceEnd += 1 }
  while essenceEnd > start
    && (value.bytes[essenceEnd - 1] == b' ' || value.bytes[essenceEnd - 1] == b'\t') {
    essenceEnd -= 1
  }

  return asciiCaseInsensitiveRangeEquals(
    value: value,
    start: start,
    end: essenceEnd,
    expected: "application/x-www-form-urlencoded",
  ) || asciiCaseInsensitiveRangeEquals(value: value, start: start, end: essenceEnd, expected: "multipart/form-data")
    || asciiCaseInsensitiveRangeEquals(value: value, start: start, end: essenceEnd, expected: "text/plain")
}

object NoCorsSafelist {
  fn contains(name: ref HeaderName, value: ref String): Bool {
    if value.bytes.count > 128 { return false }

    return switch name.text() {
      case "accept": !containsCorsUnsafeRequestHeaderByte(value: value)
      case "accept-language": isCorsLanguageValue(value: value)
      case "content-language": isCorsLanguageValue(value: value)
      case "content-type": isNoCorsSafelistedMediaType(value: value)
      case _: false
    }
  }
}

export struct MethodToken {
  let value: String

  export const init(value: String) throws HttpSyntaxError {
    guard isHttpToken(value: value) else throw .invalidMethod
    self.value = value
  }

  export fn text(): view String {
    return value
  }
}

export enum Method {
  get
  head
  query
  post
  put
  delete
  connect
  options
  trace
  patch
  other(MethodToken)
}

export type StatusCode = u16<(100..<600)>

extension StatusCode {
  export const ok = StatusCode(200)
  export const noContent = StatusCode(204)
  export const badRequest = StatusCode(400)
  export const forbidden = StatusCode(403)
  export const notFound = StatusCode(404)
  export const methodNotAllowed = StatusCode(405)
  export const unprocessableContent = StatusCode(422)
  export const internalServerError = StatusCode(500)
  export const serviceUnavailable = StatusCode(503)
}

export struct HeaderName {
  let value: String

  export const init(value: String) throws HttpSyntaxError {
    guard isHttpToken(value: value) else throw .invalidHeaderName
    self.value = asciiLowercaseToken(value: take value)
  }

  const init(canonicalValue: String) {
    self.value = canonicalValue
  }

  export fn text(): view String {
    return value
  }

  fn equals(other: ref HeaderName): Bool {
    return value == other.value
  }

  fn isForbiddenRequest(value: ref String): Bool {
    if startsWithAsciiCaseInsensitive(value: self.value, expected: "proxy-")
      || startsWithAsciiCaseInsensitive(value: self.value, expected: "sec-") {
      return true
    }

    return switch self.value {
      case "accept-charset": true
      case "accept-encoding": true
      case "access-control-request-headers": true
      case "access-control-request-method": true
      case "connection": true
      case "content-length": true
      case "cookie": true
      case "cookie2": true
      case "date": true
      case "dnt": true
      case "expect": true
      case "host": true
      case "keep-alive": true
      case "origin": true
      case "referer": true
      case "set-cookie": true
      case "te": true
      case "trailer": true
      case "transfer-encoding": true
      case "upgrade": true
      case "via": true
      case "x-http-method": containsForbiddenMethod(value: value)
      case "x-http-method-override": containsForbiddenMethod(value: value)
      case "x-method-override": containsForbiddenMethod(value: value)
      case _: false
    }
  }

  fn isTransportControlledResponse(): Bool {
    return switch value {
      case "connection": true
      case "content-length": true
      case "keep-alive": true
      case "te": true
      case "trailer": true
      case "transfer-encoding": true
      case "upgrade": true
      case _: false
    }
  }

  fn isNoCorsSafelisted(): Bool {
    return value == "accept" || value == "accept-language"
      || value == "content-language" || value == "content-type"
  }

  fn isPrivilegedNoCors(): Bool {
    return value == "range"
  }
}

export struct HeaderField {
  let storedName: HeaderName
  let storedValue: String

  export const init(
    name: HeaderName,
    value: String,
  ) throws HttpSyntaxError {
    self.storedName = name
    self.storedValue = try normalizeHttpFieldValue(value: take value)
  }

  const init(
    validatedName: HeaderName,
    normalizedValue: String,
  ) {
    self.storedName = validatedName
    self.storedValue = normalizedValue
  }

  export fn name(): ref HeaderName {
    return storedName
  }

  export fn value(): view String {
    return storedValue
  }

  fn isNoCorsSafelisted(): Bool {
    return NoCorsSafelist.contains(storedName, value: storedValue)
  }
}

object HeaderListProjection {
  fn sortAndCombine(source: ref Array<HeaderField>): Array<HeaderField> {
    var sorted = source.map((field) => copy field)
    sorted.sort(by: (left, right) => left.name().text().compare(right.name().text()))
    var projected: Array<HeaderField> = []
    var index: usize = 0

    while index < sorted.count {
      let name = copy sorted[index].name()
      if name.text() == "set-cookie" {
        while index < sorted.count && sorted[index].name().equals(name) {
          projected.append(HeaderField(
            validatedName: copy name,
            normalizedValue: sorted[index].value().materialize(),
          ))
          index += 1
        }
        continue
      }

      var combined = String()
      var found = false
      while index < sorted.count && sorted[index].name().equals(name) {
        if found { combined.append(", ") }
        combined.append(sorted[index].value())
        found = true
        index += 1
      }
      projected.append(HeaderField(
        validatedName: take name,
        normalizedValue: take combined,
      ))
    }

    return projected
  }
}

export enum HeaderMutationDenied {
  immutable
  forbiddenRequestHeader
  noCorsUnsafeRequestHeader
  transportControlledResponseHeader
}

export enum HeadersError: Error {
  syntax(HttpSyntaxError)
  mutationDenied(name: HeaderName, reason: HeaderMutationDenied)
}

enum HeadersGuard {
  none
  request
  requestNoCors
  response
  immutable
}

export struct Headers {
  let fields: Array<HeaderField>
  let guard: HeadersGuard

  export init(initialFields: take HeaderField...) {
    self.fields = take initialFields
    self.guard = .none
  }

  export init(copying source: ref Headers) {
    self.fields = source.fields.map((field) => HeaderField(
      validatedName: HeaderName(canonicalValue: field.name().text().materialize()),
      normalizedValue: field.value().materialize(),
    ))
    self.guard = .none
  }

  init(
    guardedFields: take Array<HeaderField>,
    guard: HeadersGuard,
  ) {
    self.fields = take guardedFields
    self.guard = guard
  }

  export mut fn append(
    name: String,
    value: String,
  ): () throws HeadersError {
    let field = try HeaderField(
      name: try HeaderName(take name),
      value: take value,
    )
    try self.validateAppend(field)
    fields.append(take field)
    self.removePrivilegedNoCorsHeaders()
  }

  export mut fn delete(name: ref String): () throws HeadersError {
    let checked = try HeaderName(copy name)
    try self.validateDeletion(checked)
    if !contains(checked) { return }

    var kept: Array<HeaderField> = []
    for field in take fields {
      if !field.name().equals(checked) {
        kept.append(take field)
      }
    }
    fields = take kept
    self.removePrivilegedNoCorsHeaders()
  }

  export fn get(name: ref String): String? throws HeadersError {
    let checked = try HeaderName(copy name)
    var combined = String()
    var found = false

    for field in fields {
      if !field.name().equals(checked) { continue }
      if found { combined.append(", ") }
      combined.append(field.value())
      found = true
    }

    if !found { return .none }
    return .some(take combined)
  }

  export fn getSetCookie(): Array<String> {
    var cookies: Array<String> = []
    for field in fields {
      if field.name().text() == "set-cookie" {
        cookies.append(field.value().materialize())
      }
    }
    return cookies
  }

  export fn has(name: ref String): Bool throws HeadersError {
    let checked = try HeaderName(copy name)
    for field in fields {
      if field.name().equals(checked) { return true }
    }
    return false
  }

  export mut fn set(
    name: String,
    value: String,
  ): () throws HeadersError {
    let replacement = try HeaderField(
      name: try HeaderName(take name),
      value: take value,
    )
    try self.validateSet(replacement)

    let replacementName = copy replacement.name()
    var first: usize? = .none
    var search: usize = 0
    while search < fields.count {
      if fields[search].name().equals(replacementName) {
        first = .some(search)
        break
      }
      search += 1
    }

    if let firstIndex = first {
      fields[firstIndex] = take replacement

      var updated: Array<HeaderField> = []
      var index: usize = 0
      for field in take fields {
        if index == firstIndex || !field.name().equals(replacementName) {
          updated.append(take field)
        }
        index += 1
      }
      fields = take updated
    } else {
      fields.append(take replacement)
    }

    self.removePrivilegedNoCorsHeaders()
  }

  export fn entries(): Array<HeaderField> {
    return HeaderListProjection.sortAndCombine(fields)
  }

  fn contains(name: ref HeaderName): Bool {
    for field in fields {
      if field.name().equals(name) { return true }
    }
    return false
  }

  fn validateAppend(field: ref HeaderField): () throws HeadersError {
    try validateMutation(field)
    if guard != .requestNoCors { return }

    var temporaryValue = String()
    var found = false
    for current in fields {
      if !current.name().equals(field.name()) { continue }
      if found { temporaryValue.append(", ") }
      temporaryValue.append(current.value())
      found = true
    }
    if found { temporaryValue.append(", ") }
    temporaryValue.append(field.value())

    guard NoCorsSafelist.contains(field.name(), value: temporaryValue) else {
      throw .mutationDenied(
        name: copy field.name(),
        reason: .noCorsUnsafeRequestHeader,
      )
    }
  }

  fn validateSet(field: ref HeaderField): () throws HeadersError {
    try validateMutation(field)
    if guard == .requestNoCors && !field.isNoCorsSafelisted() {
      throw .mutationDenied(
        name: copy field.name(),
        reason: .noCorsUnsafeRequestHeader,
      )
    }
  }

  fn validateMutation(field: ref HeaderField): () throws HeadersError {
    switch guard {
      case .none:
        return
      case .immutable:
        throw .mutationDenied(
          name: copy field.name(),
          reason: .immutable,
        )
      case .request:
        guard !field.name().isForbiddenRequest(field.value()) else {
          throw .mutationDenied(
            name: copy field.name(),
            reason: .forbiddenRequestHeader,
          )
        }
      case .requestNoCors:
        return
      case .response:
        guard !field.name().isTransportControlledResponse() else {
          throw .mutationDenied(
            name: copy field.name(),
            reason: .transportControlledResponseHeader,
          )
        }
    }
  }

  fn validateDeletion(name: ref HeaderName): () throws HeadersError {
    let empty = try HeaderField(name: copy name, value: "")
    try validateMutation(empty)

    if guard == .requestNoCors
      && !name.isNoCorsSafelisted()
      && !name.isPrivilegedNoCors() {
      throw .mutationDenied(
        name: copy name,
        reason: .noCorsUnsafeRequestHeader,
      )
    }
  }

  mut fn removePrivilegedNoCorsHeaders() {
    if guard != .requestNoCors { return }

    var kept: Array<HeaderField> = []
    for field in take fields {
      if !field.name().isPrivilegedNoCors() {
        kept.append(take field)
      }
    }
    fields = take kept
  }
}

export enum HttpSyntaxError: Error {
  invalidMethod
  invalidTarget
  invalidHeaderName
  invalidHeaderValue
  invalidStatus(found: u16)
}

export enum HttpBodyError: Error & Duplicable {
  alreadyUsed
  locked
  aborted(AbortReason)
  malformed
  limitExceeded(maximumBytes: usize)
  incomplete
  transport(IoError)
}

export enum BodyDecodeError<CodecFailure: Error>: Error {
  body(HttpBodyError)
  codec(CodecFailure)
}

export enum BodyCloneError: Error {
  alreadyUsed
  locked
  unsupported
}

export enum RequestError: Error {
  syntax(HttpSyntaxError)
  headers(HeadersError)
  headerLimitExceeded(maximumFields: usize, maximumBytes: usize)
  bodyNotAllowed(method: Method)
  body(HttpBodyError)
  formData(FormDataError)
  bodySourceInvalid
  unsupportedPolicy
}

export enum ResponseError: Error {
  syntax(HttpSyntaxError)
  headers(HeadersError)
  headerLimitExceeded(maximumFields: usize, maximumBytes: usize)
  encoding(json.EncodeError)
  formData(FormDataError)
  bodyNotAllowed(status: StatusCode)
  invalidServerResponse
}

export enum HttpError: Error {
  syntax(HttpSyntaxError)
  body(HttpBodyError)
  overloaded
  timedOut
  unavailable
  protocol
  unsupported
}

export enum ServerError: Error {
  unavailable
  unsupported
  invalidConfiguration
  listen(net.NetworkError)
  accept(net.NetworkError)
  protocol(HttpError)
}

export struct MessageLimits {
  let targetBytes: usize<(1...)>
  let headerBytes: usize<(1...)>
  let headerFields: usize<(1...)>
  let bodyBytes: usize<(1...)>
}

export struct ServerLimits {
  let activeRequests: usize<(1...)>
  let queuedRequests: usize
  let queuedBytes: usize
  let connections: usize<(1...)>
  let message: MessageLimits
}

export enum FormDataLimitKind: Copy {
  entries
  nameBytes
  filenameBytes
  textBytes
  blobBytes
  payloadBytes
  encodedBytes
}

export enum FormDataError: Error & Copy {
  unsupportedMediaType
  malformed
  contentTypeControlled
  limitExceeded(kind: FormDataLimitKind, maximum: usize)
}

export struct FormDataLimits: Copy & Equatable {
  let maximumEntries: usize<(1...)>
  let maximumNameBytes: usize<(1...)>
  let maximumFilenameBytes: usize<(1...)>
  let maximumTextBytes: usize<(1...)>
  let maximumBlobBytes: usize<(1...)>
  let maximumPayloadBytes: usize<(1...)>
  let maximumEncodedBytes: usize<(1...)>

  export const init(
    maximumEntries: usize<(1...)>,
    maximumNameBytes: usize<(1...)>,
    maximumFilenameBytes: usize<(1...)>,
    maximumTextBytes: usize<(1...)>,
    maximumBlobBytes: usize<(1...)>,
    maximumPayloadBytes: usize<(1...)>,
    maximumEncodedBytes: usize<(1...)>,
  ) {
    self.maximumEntries = maximumEntries
    self.maximumNameBytes = maximumNameBytes
    self.maximumFilenameBytes = maximumFilenameBytes
    self.maximumTextBytes = maximumTextBytes
    self.maximumBlobBytes = maximumBlobBytes
    self.maximumPayloadBytes = maximumPayloadBytes
    self.maximumEncodedBytes = maximumEncodedBytes
  }

  export static fn standard(): FormDataLimits {
    return FormDataLimits(
      maximumEntries: 128,
      maximumNameBytes: 8<iec.KiB>,
      maximumFilenameBytes: 8<iec.KiB>,
      maximumTextBytes: 1<iec.MiB>,
      maximumBlobBytes: 64<iec.MiB>,
      maximumPayloadBytes: 64<iec.MiB>,
      maximumEncodedBytes: 65<iec.MiB>,
    )
  }
}

export enum FormDataValue: Duplicable {
  text(String)
  blob(Blob, filename: String)

  export fn duplicate(): FormDataValue {
    return switch self {
      case .text(let value): .text(copy value)
      case .blob(let value, let filename):
        .blob(copy value, filename: copy filename)
    }
  }

}

export struct FormDataEntry: Duplicable {
  let storedName: String
  let storedValue: FormDataValue
  let storedRetainedBytes: usize

  init(
    name: String,
    value: FormDataValue,
    retainedBytes: usize,
  ) {
    self.storedName = take name
    self.storedValue = take value
    self.storedRetainedBytes = retainedBytes
  }

  export fn name(): view String {
    return storedName
  }

  export fn value(): ref FormDataValue {
    return storedValue
  }

  export fn duplicate(): FormDataEntry {
    return FormDataEntry(
      name: copy storedName,
      value: storedValue.duplicate(),
      retainedBytes: storedRetainedBytes,
    )
  }
}

struct FormDataAdmission {
  let totalBytes: usize
  let entryBytes: usize
}

export struct FormData: Duplicable {
  let storedLimits: FormDataLimits
  let storedPayloadBytes: usize
  let storedEntries: Array<FormDataEntry>

  export init(limits: FormDataLimits = FormDataLimits.standard()) {
    self.storedLimits = limits
    self.storedPayloadBytes = 0
    self.storedEntries = []
  }

  init(
    limits: FormDataLimits,
    payloadBytes: usize,
    entries: take Array<FormDataEntry>,
  ) {
    self.storedLimits = limits
    self.storedPayloadBytes = payloadBytes
    self.storedEntries = take entries
  }

  export let size: usize {
    get => storedEntries.count
  }

  export let limits: FormDataLimits {
    get => storedLimits
  }

  export fn duplicate(): FormData {
    var copied: Array<FormDataEntry> = []
    for ref entry in storedEntries { copied.append(entry.duplicate()) }
    return FormData(
      limits: storedLimits,
      payloadBytes: storedPayloadBytes,
      entries: take copied,
    )
  }

  fn admittedPayload(
    name: ref String,
    value: ref FormDataValue,
    replacingBytes: usize = 0,
  ): FormDataAdmission throws FormDataError {
    guard name.bytes.count <= storedLimits.maximumNameBytes else {
      throw .limitExceeded(
        kind: .nameBytes,
        maximum: storedLimits.maximumNameBytes,
      )
    }

    switch value {
      case .text(let text):
        guard text.bytes.count <= storedLimits.maximumTextBytes else {
          throw .limitExceeded(
            kind: .textBytes,
            maximum: storedLimits.maximumTextBytes,
          )
        }
      case .blob(let blob, let filename):
        guard filename.bytes.count <= storedLimits.maximumFilenameBytes else {
          throw .limitExceeded(
            kind: .filenameBytes,
            maximum: storedLimits.maximumFilenameBytes,
          )
        }
        guard blob.size <= storedLimits.maximumBlobBytes else {
          throw .limitExceeded(
            kind: .blobBytes,
            maximum: storedLimits.maximumBlobBytes,
          )
        }
    }

    var entryBytes = name.bytes.count
    do {
      switch value {
        case .text(let text):
          entryBytes = try entryBytes.checkedAdd(text.bytes.count)
        case .blob(let blob, let filename):
          entryBytes = try entryBytes.checkedAdd(filename.bytes.count)
          entryBytes = try entryBytes.checkedAdd(blob.size)
      }
    } catch {
      throw .limitExceeded(
        kind: .payloadBytes,
        maximum: storedLimits.maximumPayloadBytes,
      )
    }

    let retained = storedPayloadBytes - replacingBytes
    var next: usize
    do {
      next = try retained.checkedAdd(entryBytes)
    } catch {
      throw .limitExceeded(
        kind: .payloadBytes,
        maximum: storedLimits.maximumPayloadBytes,
      )
    }
    guard next <= storedLimits.maximumPayloadBytes else {
      throw .limitExceeded(
        kind: .payloadBytes,
        maximum: storedLimits.maximumPayloadBytes,
      )
    }
    return FormDataAdmission(totalBytes: next, entryBytes: entryBytes)
  }

  mut fn appendEntry(
    name: String,
    value: FormDataValue,
  ): () throws FormDataError {
    guard storedEntries.count < storedLimits.maximumEntries else {
      throw .limitExceeded(
        kind: .entries,
        maximum: storedLimits.maximumEntries,
      )
    }
    let admission = try admittedPayload(name: ref name, value: ref value)
    storedEntries.append(FormDataEntry(
      name: take name,
      value: take value,
      retainedBytes: admission.entryBytes,
    ))
    storedPayloadBytes = admission.totalBytes
  }

  export mut fn append(
    name: String,
    value: String,
  ): () throws FormDataError {
    try appendEntry(name: take name, value: .text(take value))
  }

  export mut fn append(
    name: String,
    blob file: take Blob,
    filename fileName: String = "blob",
  ): () throws FormDataError {
    try appendEntry(
      name: take name,
      value: .blob(take file, filename: take fileName),
    )
  }

  export mut fn set(
    name: String,
    value: String,
  ): () throws FormDataError {
    try setEntry(name: take name, value: .text(take value))
  }

  export mut fn set(
    name: String,
    blob file: take Blob,
    filename fileName: String = "blob",
  ): () throws FormDataError {
    try setEntry(
      name: take name,
      value: .blob(take file, filename: take fileName),
    )
  }

  mut fn setEntry(
    name: String,
    value: FormDataValue,
  ): () throws FormDataError {
    var matches: usize = 0
    var replacedBytes: usize = 0
    for ref entry in storedEntries {
      if entry.storedName == name {
        matches += 1
        replacedBytes += entry.storedRetainedBytes
      }
    }

    let finalCount = storedEntries.count - matches + 1
    guard finalCount <= storedLimits.maximumEntries else {
      throw .limitExceeded(
        kind: .entries,
        maximum: storedLimits.maximumEntries,
      )
    }
    let admission = try admittedPayload(
      name: ref name,
      value: ref value,
      replacingBytes: replacedBytes,
    )

    var replacement = FormDataEntry(
      name: take name,
      value: take value,
      retainedBytes: admission.entryBytes,
    )
    var inserted = false
    var updated: Array<FormDataEntry> = []
    for entry in take storedEntries {
      if entry.storedName == replacement.storedName {
        if !inserted {
          updated.append(take replacement)
          inserted = true
        }
      } else {
        updated.append(take entry)
      }
    }
    if !inserted { updated.append(take replacement) }

    storedEntries = take updated
    storedPayloadBytes = admission.totalBytes
  }

  export mut fn delete(name: ref String) {
    var kept: Array<FormDataEntry> = []
    var keptBytes: usize = 0
    for entry in take storedEntries {
      if entry.storedName != name {
        keptBytes += entry.storedRetainedBytes
        kept.append(take entry)
      }
    }
    storedEntries = take kept
    storedPayloadBytes = keptBytes
  }

  export fn get(name: ref String): ref FormDataValue? {
    for ref entry in storedEntries {
      if entry.storedName == name { return entry.storedValue }
    }
    return .none
  }

  export fn getAll(name: ref String): Array<FormDataValue> {
    var values: Array<FormDataValue> = []
    for ref entry in storedEntries {
      if entry.storedName == name {
        values.append(entry.storedValue.duplicate())
      }
    }
    return values
  }

  export fn has(name: ref String): Bool {
    return get(key: name) != .none
  }

  export fn entries(): Array<FormDataEntry> {
    var result: Array<FormDataEntry> = []
    for ref entry in storedEntries { result.append(entry.duplicate()) }
    return result
  }
}

// Request and Response use the same six body sources. FormData stores the
// logical list; only std.http serializes it and chooses the multipart boundary.
export enum BodySource {
  string(String)
  bytes(Bytes)
  urlSearchParams(URLSearchParams)
  blob(Blob)
  formData(FormData)
  stream(ReadableStream<Bytes, HttpBodyError>)
}

export enum BodyOverride {
  inherit
  none
  replace(BodySource)
}

export enum RequestMode {
  cors
  sameOrigin
  noCors
  navigate
}

export enum CredentialsMode {
  omit
  sameOrigin
  include
}

export enum CacheMode {
  default
  noStore
  reload
  noCache
  forceCache
  onlyIfCached
}

export enum RedirectMode {
  follow
  error
  manual
}

export enum ReferrerPolicy {
  noReferrer
  noReferrerWhenDowngrade
  origin
  originWhenCrossOrigin
  sameOrigin
  strictOrigin
  strictOriginWhenCrossOrigin
  unsafeUrl
}

export enum Duplex {
  half
}

export enum Priority {
  high
  low
  auto
}

export enum RequestDestination {
  // Provider mapping preserves the exact Fetch destination strings, including
  // `audioWorklet`, `paintWorklet`, `sharedWorker`, and `json`.
  none
  audio
  audioWorklet
  document
  embed
  font
  frame
  iframe
  image
  json
  manifest
  object
  paintWorklet
  report
  script
  sharedWorker
  style
  track
  video
  worker
  xslt
}

export enum RequestReferrer {
  client
  none
  url(URL)
}

// An outer absence means defaults/inherit. `.some(.none)` explicitly clears
// the integrity metadata without introducing a nested Optional/String shape.
export enum RequestIntegrity {
  none
  value(String)
}

export enum ResponseType {
  basic
  cors
  default
  error
  opaque
  opaqueRedirect
}

export struct RequestInit {
  let method: Method?
  let headers: Headers?
  let body: BodySource?
  let signal: AbortSignal?
  let mode: RequestMode?
  let credentials: CredentialsMode?
  let cache: CacheMode?
  let redirect: RedirectMode?
  let referrer: RequestReferrer?
  let referrerPolicy: ReferrerPolicy?
  // none means no override in RequestInit; `.some(.none)` is only meaningful
  // for RequestOverride, where it clears inherited metadata.
  let integrity: RequestIntegrity?
  let duplex: Duplex?
  let priority: Priority?

  export init(
    method: Method? = .none,
    headers: Headers? = .none,
    body: BodySource? = .none,
    signal: AbortSignal? = .none,
    mode: RequestMode? = .none,
    credentials: CredentialsMode? = .none,
    cache: CacheMode? = .none,
    redirect: RedirectMode? = .none,
    referrer: RequestReferrer? = .none,
    referrerPolicy: ReferrerPolicy? = .none,
    integrity: RequestIntegrity? = .none,
    duplex: Duplex? = .none,
    priority: Priority? = .none,
  ) {
    self.method = method
    self.headers = take headers
    self.body = take body
    self.signal = take signal
    self.mode = mode
    self.credentials = credentials
    self.cache = cache
    self.redirect = redirect
    self.referrer = take referrer
    self.referrerPolicy = referrerPolicy
    self.integrity = take integrity
    self.duplex = duplex
    self.priority = priority
  }
}

export struct RequestOverride {
  let method: Method?
  let headers: Headers?
  let body: BodyOverride
  let signal: AbortSignal?
  let mode: RequestMode?
  let credentials: CredentialsMode?
  let cache: CacheMode?
  let redirect: RedirectMode?
  let referrer: RequestReferrer?
  let referrerPolicy: ReferrerPolicy?
  // Outer absence inherits. `.some(.none)` clears the inherited value.
  let integrity: RequestIntegrity?
  let duplex: Duplex?
  let priority: Priority?

  export init(
    method: Method? = .none,
    headers: Headers? = .none,
    body: BodyOverride = .inherit,
    signal: AbortSignal? = .none,
    mode: RequestMode? = .none,
    credentials: CredentialsMode? = .none,
    cache: CacheMode? = .none,
    redirect: RedirectMode? = .none,
    referrer: RequestReferrer? = .none,
    referrerPolicy: ReferrerPolicy? = .none,
    integrity: RequestIntegrity? = .none,
    duplex: Duplex? = .none,
    priority: Priority? = .none,
  ) {
    self.method = method
    self.headers = take headers
    self.body = take body
    self.signal = take signal
    self.mode = mode
    self.credentials = credentials
    self.cache = cache
    self.redirect = redirect
    self.referrer = take referrer
    self.referrerPolicy = referrerPolicy
    self.integrity = take integrity
    self.duplex = duplex
    self.priority = priority
  }
}

export enum TemplateLimitKind {
  outputBytes
  values
}

export enum TemplateError: Error {
  unavailable
  missing
  invalid
  limitExceeded(kind: TemplateLimitKind, maximum: usize)
}

export struct TemplateLimits {
  let maximumOutputBytes: usize<(1...)>
  let maximumValues: usize

  export const init(
    maximumOutputBytes: usize<(1...)>,
    maximumValues: usize,
  ) {
    self.maximumOutputBytes = maximumOutputBytes
    self.maximumValues = maximumValues
  }
}

// The single provider owns message, body, context, and host-server handles.
// Public owners never expose these handles. Provider operations commit an
// inert state before suspension or outcome propagation.
foreign intrinsic from "std.http@1" {
  type RequestHandle
  type ResponseHandle
  type ContextHandle
  type RandomHandle
  type DatabaseRegistryHandle
  type CacheRegistryHandle
  type TemplateRegistryHandle
  type TemplateHandle

  fn stdHttpRequestFromString(
    _ input: ref String,
    _ init: take RequestInit,
  ): RequestHandle throws RequestError
  // Owned URL is consumed by the provider; the `Copy`-suffixed entry below
  // materializes from a borrowed URL and is intentionally distinct.
  fn stdHttpRequestFromOwnedURL(
    _ input: take URL,
    _ init: take RequestInit,
  ): RequestHandle throws RequestError
  fn stdHttpRequestFromURLCopy(
    _ input: ref URL,
    _ init: take RequestInit,
  ): RequestHandle throws RequestError
  fn stdHttpRequestOverride(
    _ handle: inout RequestHandle,
    _ override: take RequestOverride,
  ): () throws RequestError
  fn stdHttpRequestMethod(_ handle: ref RequestHandle): Method
  fn stdHttpRequestURL(_ handle: ref RequestHandle): ref URL
  fn stdHttpRequestHeaders(_ handle: ref RequestHandle): ref Headers
  fn stdHttpRequestSignal(_ handle: ref RequestHandle): ref AbortSignal
  fn stdHttpRequestBodyUsed(_ handle: ref RequestHandle): Bool
  fn stdHttpRequestDestination(_ handle: ref RequestHandle): RequestDestination
  fn stdHttpRequestMode(_ handle: ref RequestHandle): RequestMode
  fn stdHttpRequestCredentials(_ handle: ref RequestHandle): CredentialsMode
  fn stdHttpRequestCache(_ handle: ref RequestHandle): CacheMode
  fn stdHttpRequestRedirect(_ handle: ref RequestHandle): RedirectMode
  fn stdHttpRequestReferrer(_ handle: ref RequestHandle): RequestReferrer
  fn stdHttpRequestReferrerPolicy(_ handle: ref RequestHandle): ReferrerPolicy
  fn stdHttpRequestIntegrity(_ handle: ref RequestHandle): view String
  fn stdHttpRequestDuplex(_ handle: ref RequestHandle): Duplex
  fn stdHttpRequestPriority(_ handle: ref RequestHandle): Priority
  fn stdHttpRequestBody(_ handle: inout RequestHandle): ReadableStream<Bytes, HttpBodyError>?
  async fn stdHttpRequestBytes(
    _ handle: inout RequestHandle,
    _ maximumBytes: usize<(1...)>,
  ): Bytes throws HttpBodyError
  async fn stdHttpRequestText(
    _ handle: inout RequestHandle,
    _ maximumBytes: usize<(1...)>,
  ): String throws HttpBodyError
  async fn stdHttpRequestBlob(
    _ handle: inout RequestHandle,
    _ maximumBytes: usize<(1...)>,
  ): Blob throws HttpBodyError
  async fn stdHttpRequestFormData(
    _ handle: inout RequestHandle,
    _ limits: FormDataLimits,
  ): FormData throws BodyDecodeError<FormDataError>
  fn stdHttpRequestClone(
    _ handle: inout RequestHandle,
    _ maximumBufferedBytes: usize<(1...)>,
  ): (RequestHandle, RequestHandle) throws BodyCloneError
  fn stdHttpRequestDrop(_ handle: inout RequestHandle)

  fn stdHttpResponseCreate(
    _ body: take BodySource?,
    _ status: StatusCode,
    _ statusText: take String,
    _ headers: take Headers,
  ): ResponseHandle throws ResponseError
  fn stdHttpResponseError(): ResponseHandle
  fn stdHttpResponseStatus(_ handle: ref ResponseHandle): u16<(0..<600)>
  fn stdHttpResponseOk(_ handle: ref ResponseHandle): Bool
  fn stdHttpResponseStatusText(_ handle: ref ResponseHandle): view String
  fn stdHttpResponseHeaders(_ handle: ref ResponseHandle): ref Headers
  fn stdHttpResponseURL(_ handle: ref ResponseHandle): URL?
  fn stdHttpResponseRedirected(_ handle: ref ResponseHandle): Bool
  fn stdHttpResponseType(_ handle: ref ResponseHandle): ResponseType
  fn stdHttpResponseBodyUsed(_ handle: ref ResponseHandle): Bool
  fn stdHttpResponseBody(_ handle: inout ResponseHandle): ReadableStream<Bytes, HttpBodyError>?
  async fn stdHttpResponseBytes(
    _ handle: inout ResponseHandle,
    _ maximumBytes: usize<(1...)>,
  ): Bytes throws HttpBodyError
  async fn stdHttpResponseText(
    _ handle: inout ResponseHandle,
    _ maximumBytes: usize<(1...)>,
  ): String throws HttpBodyError
  async fn stdHttpResponseBlob(
    _ handle: inout ResponseHandle,
    _ maximumBytes: usize<(1...)>,
  ): Blob throws HttpBodyError
  async fn stdHttpResponseFormData(
    _ handle: inout ResponseHandle,
    _ limits: FormDataLimits,
  ): FormData throws BodyDecodeError<FormDataError>
  fn stdHttpResponseClone(
    _ handle: inout ResponseHandle,
    _ maximumBufferedBytes: usize<(1...)>,
  ): (ResponseHandle, ResponseHandle) throws BodyCloneError
  fn stdHttpResponseDrop(_ handle: inout ResponseHandle)

  fn stdHttpContextRandom(_ handle: ref ContextHandle): RandomHandle
  fn stdHttpContextDatabases(_ handle: ref ContextHandle): DatabaseRegistryHandle
  fn stdHttpContextCaches(_ handle: ref ContextHandle): CacheRegistryHandle
  fn stdHttpContextTemplates(_ handle: ref ContextHandle): TemplateRegistryHandle
  fn stdHttpContextSignal(_ handle: ref ContextHandle): AbortSignal
  fn stdHttpRandomInteger(_ handle: ref RandomHandle, _ range: Range<i32>): i32
  fn stdHttpDatabaseGet(
    _ handle: ref DatabaseRegistryHandle,
    _ binding: const database.Binding,
  ): some database.Database
  fn stdHttpCacheGet<Key: Equatable & Hashable & Duplicable, Value: Duplicable>(
    _ handle: ref CacheRegistryHandle,
    _ binding: const cache.LocalBinding<Key, Value>,
  ): some cache.LocalCache<Key, Value>
  fn stdHttpTemplateGet(
    _ handle: ref TemplateRegistryHandle,
    _ binding: const TemplateBinding,
  ): TemplateHandle
  fn stdHttpTemplateRender<Value: json.Encodable>(
    _ handle: ref TemplateHandle,
    _ values: ref Array<Value>,
  ): String throws TemplateError
  fn stdHttpContextDrop(_ handle: inout ContextHandle)
  fn stdHttpRandomDrop(_ handle: inout RandomHandle)
  fn stdHttpDatabaseRegistryDrop(_ handle: inout DatabaseRegistryHandle)
  fn stdHttpCacheRegistryDrop(_ handle: inout CacheRegistryHandle)
  fn stdHttpTemplateRegistryDrop(_ handle: inout TemplateRegistryHandle)
  fn stdHttpTemplateDrop(_ handle: inout TemplateHandle)

  async fn stdHttpServe<Failure: Error>(
    _ address: net.ListenAddress,
    _ network: ref net.Network,
    _ limits: ServerLimits,
    _ handler: some async fn(take Request, Context): Response throws Failure,
  ): () throws ServerError
}

export struct Request {
  let handle: RequestHandle

  init(validatedHandle: RequestHandle) {
    self.handle = validatedHandle
  }

  export init(input: take String, init: take RequestInit = RequestInit()) throws RequestError {
    self.handle = unsafe {
      try stdHttpRequestFromString(ref input, take init)
    }
  }

  export init(input: take URL, init: take RequestInit = RequestInit()) throws RequestError {
    // The owned URL entry consumes the URL in the foreign call. Borrowed URL
    // callers use the explicitly copying overload below.
    self.handle = unsafe {
      try stdHttpRequestFromOwnedURL(take input, take init)
    }
  }

  export init(copying input: ref URL, init: take RequestInit = RequestInit()) throws RequestError {
    self.handle = unsafe {
      try stdHttpRequestFromURLCopy(input, take init)
    }
  }

  export init(
    input: take Request,
    override: take RequestOverride = RequestOverride(),
  ) throws RequestError {
    self.handle = take input.handle
    unsafe { try stdHttpRequestOverride(inout self.handle, take override) }
  }

  export let method: Method {
    get => unsafe { stdHttpRequestMethod(ref handle) }
  }

  export let url: ref URL {
    get => unsafe { stdHttpRequestURL(ref handle) }
  }

  export let headers: ref Headers {
    get => unsafe { stdHttpRequestHeaders(ref handle) }
  }

  export let signal: ref AbortSignal {
    get => unsafe { stdHttpRequestSignal(ref handle) }
  }

  export let bodyUsed: Bool {
    get => unsafe { stdHttpRequestBodyUsed(ref handle) }
  }

  export let destination: RequestDestination {
    get => unsafe { stdHttpRequestDestination(ref handle) }
  }

  export let mode: RequestMode {
    get => unsafe { stdHttpRequestMode(ref handle) }
  }

  export let credentials: CredentialsMode {
    get => unsafe { stdHttpRequestCredentials(ref handle) }
  }

  export let cache: CacheMode {
    get => unsafe { stdHttpRequestCache(ref handle) }
  }

  export let redirect: RedirectMode {
    get => unsafe { stdHttpRequestRedirect(ref handle) }
  }

  export let referrer: RequestReferrer {
    get => unsafe { stdHttpRequestReferrer(ref handle) }
  }

  export let referrerPolicy: ReferrerPolicy {
    get => unsafe { stdHttpRequestReferrerPolicy(ref handle) }
  }

  export let integrity: view String {
    get => unsafe { stdHttpRequestIntegrity(ref handle) }
  }

  export let duplex: Duplex {
    get => unsafe { stdHttpRequestDuplex(ref handle) }
  }

  export let priority: Priority {
    get => unsafe { stdHttpRequestPriority(ref handle) }
  }

  // This consuming extraction is the W spelling of the Body.body stream.
  export take fn body(): ReadableStream<Bytes, HttpBodyError>? {
    return unsafe { stdHttpRequestBody(inout handle) }
  }

  export take async fn bytes(maximumBytes limit: usize<(1...)>): Bytes throws HttpBodyError {
    return unsafe { try await stdHttpRequestBytes(inout handle, limit) }
  }

  export take async fn text(maximumBytes limit: usize<(1...)>): String throws HttpBodyError {
    return unsafe { try await stdHttpRequestText(inout handle, limit) }
  }

  export take async fn blob(maximumBytes limit: usize<(1...)>): Blob throws HttpBodyError {
    return unsafe { try await stdHttpRequestBlob(inout handle, limit) }
  }

  export take async fn formData(
    limits formLimits: FormDataLimits = FormDataLimits.standard(),
  ): FormData throws BodyDecodeError<FormDataError> {
    return unsafe { try await stdHttpRequestFormData(inout handle, formLimits) }
  }

  export take async fn json<Value: json.Decodable>(
    maximumBytes byteLimit: usize<(1...)>,
    profile decodeProfile: json.Profile = .interoperable,
    unknownMembers memberPolicy: json.UnknownMemberPolicy = .reject,
  ): Value throws BodyDecodeError<json.DecodeError> {
    return try await json(
      limits: json.Limits(maximumBytes: byteLimit),
      profile: decodeProfile,
      unknownMembers: memberPolicy,
    )
  }

  export take async fn json<Value: json.Decodable>(
    limits decodeLimits: json.Limits,
    profile decodeProfile: json.Profile = .interoperable,
    unknownMembers memberPolicy: json.UnknownMemberPolicy = .reject,
  ): Value throws BodyDecodeError<json.DecodeError> {
    // JSON is ordinary W composition: consume bounded bytes, then decode with
    // std.json. The HTTP provider does not own a generic JSON intrinsic.
    var payload: Bytes
    do {
      payload = try await bytes(maximumBytes: decodeLimits.maximumBytes)
    } catch error {
      throw .body(error)
    }
    do {
      return try json.decode<Value>(
        ref payload,
        limits: decodeLimits,
        profile: decodeProfile,
        unknownMembers: memberPolicy,
      )
    } catch error {
      throw .codec(error)
    }
  }

  export take fn clone(
    maximumBufferedBytes bufferLimit: usize<(1...)>,
  ): (Request, Request) throws BodyCloneError {
    let (left, right) = unsafe {
      try stdHttpRequestClone(
        inout handle,
        bufferLimit,
      )
    }
    return (Request(validatedHandle: left), Request(validatedHandle: right))
  }

  deinit {
    unsafe { stdHttpRequestDrop(inout handle) }
  }
}

export struct Response {
  let handle: ResponseHandle

  init(validatedHandle: ResponseHandle) {
    self.handle = validatedHandle
  }

  export init(
    status: StatusCode = StatusCode.ok,
    statusText: String = "",
    headers: take Headers = Headers(),
  ) throws ResponseError {
    self.handle = unsafe {
      try stdHttpResponseCreate(
        .none,
        status,
        take statusText,
        take headers,
      )
    }
  }

  export init(
    body: take String,
    status: StatusCode = StatusCode.ok,
    statusText: String = "",
    headers: take Headers = Headers(),
  ) throws ResponseError {
    self.handle = unsafe {
      try stdHttpResponseCreate(
        .some(.string(take body)),
        status,
        take statusText,
        take headers,
      )
    }
  }

  export init(
    body: take Bytes,
    status: StatusCode = StatusCode.ok,
    statusText: String = "",
    headers: take Headers = Headers(),
  ) throws ResponseError {
    self.handle = unsafe {
      try stdHttpResponseCreate(
        .some(.bytes(take body)),
        status,
        take statusText,
        take headers,
      )
    }
  }

  export init(
    body: take URLSearchParams,
    status: StatusCode = StatusCode.ok,
    statusText: String = "",
    headers: take Headers = Headers(),
  ) throws ResponseError {
    self.handle = unsafe {
      try stdHttpResponseCreate(
        .some(.urlSearchParams(take body)),
        status,
        take statusText,
        take headers,
      )
    }
  }

  export init(
    body: take Blob,
    status: StatusCode = StatusCode.ok,
    statusText: String = "",
    headers: take Headers = Headers(),
  ) throws ResponseError {
    self.handle = unsafe {
      try stdHttpResponseCreate(
        .some(.blob(take body)),
        status,
        take statusText,
        take headers,
      )
    }
  }

  export init(
    body: take FormData,
    status: StatusCode = StatusCode.ok,
    statusText: String = "",
    headers: take Headers = Headers(),
  ) throws ResponseError {
    self.handle = unsafe {
      try stdHttpResponseCreate(
        .some(.formData(take body)),
        status,
        take statusText,
        take headers,
      )
    }
  }

  export init(
    body: take ReadableStream<Bytes, HttpBodyError>,
    status: StatusCode = StatusCode.ok,
    statusText: String = "",
    headers: take Headers = Headers(),
  ) throws ResponseError {
    self.handle = unsafe {
      try stdHttpResponseCreate(
        .some(.stream(take body)),
        status,
        take statusText,
        take headers,
      )
    }
  }

  export init(
    body: take BodySource,
    status: StatusCode = StatusCode.ok,
    statusText: String = "",
    headers: take Headers = Headers(),
  ) throws ResponseError {
    self.handle = unsafe {
      try stdHttpResponseCreate(
        .some(take body),
        status,
        take statusText,
        take headers,
      )
    }
  }

  export static fn error(): Response {
    return Response(validatedHandle: unsafe { stdHttpResponseError() })
  }

  export static fn json<Value: json.Encodable>(
    value input: ref Value,
    maximumBytes byteLimit: usize<(1...)>,
    status responseStatus: StatusCode = StatusCode.ok,
    statusText responseStatusText: String = "",
    profile encodeProfile: json.Profile = .interoperable,
    headers responseHeaders: take Headers = Headers(),
  ) throws ResponseError {
    return try json(
      value: ref input,
      limits: json.Limits(maximumBytes: byteLimit),
      status: responseStatus,
      statusText: take responseStatusText,
      profile: encodeProfile,
      headers: take responseHeaders,
    )
  }

  export static fn json<Value: json.Encodable>(
    value input: ref Value,
    limits encodeLimits: json.Limits,
    status responseStatus: StatusCode = StatusCode.ok,
    statusText responseStatusText: String = "",
    profile encodeProfile: json.Profile = .interoperable,
    headers responseHeaders: take Headers = Headers(),
  ) throws ResponseError {
    // Encoding is borrowed and composed with the ordinary Bytes Response;
    // no generic JSON operation crosses the std.http provider seam.
    var encoded: Bytes
    do {
      encoded = try json.encode(
        ref input,
        limits: encodeLimits,
        profile: encodeProfile,
      )
    } catch error {
      throw .encoding(error)
    }
    var preparedHeaders = take responseHeaders
    do {
      if !(try preparedHeaders.has("content-type")) {
        try preparedHeaders.set("content-type", "application/json")
      }
    } catch error {
      throw .headers(error)
    }
    return try Response(
      take encoded,
      status: responseStatus,
      statusText: take responseStatusText,
      headers: take preparedHeaders,
    )
  }

  export let status: u16<(0..<600)> {
    get => unsafe { stdHttpResponseStatus(ref handle) }
  }

  export let ok: Bool {
    get => unsafe { stdHttpResponseOk(ref handle) }
  }

  export let statusText: view String {
    get => unsafe { stdHttpResponseStatusText(ref handle) }
  }

  export let headers: ref Headers {
    get => unsafe { stdHttpResponseHeaders(ref handle) }
  }

  export let url: URL? {
    get => unsafe { stdHttpResponseURL(ref handle) }
  }

  export let redirected: Bool {
    get => unsafe { stdHttpResponseRedirected(ref handle) }
  }

  export let type: ResponseType {
    get => unsafe { stdHttpResponseType(ref handle) }
  }

  export let bodyUsed: Bool {
    get => unsafe { stdHttpResponseBodyUsed(ref handle) }
  }

  export take fn body(): ReadableStream<Bytes, HttpBodyError>? {
    return unsafe { stdHttpResponseBody(inout handle) }
  }

  export take async fn bytes(maximumBytes limit: usize<(1...)>): Bytes throws HttpBodyError {
    return unsafe { try await stdHttpResponseBytes(inout handle, limit) }
  }

  export take async fn text(maximumBytes limit: usize<(1...)>): String throws HttpBodyError {
    return unsafe { try await stdHttpResponseText(inout handle, limit) }
  }

  export take async fn blob(maximumBytes limit: usize<(1...)>): Blob throws HttpBodyError {
    return unsafe { try await stdHttpResponseBlob(inout handle, limit) }
  }

  export take async fn formData(
    limits formLimits: FormDataLimits = FormDataLimits.standard(),
  ): FormData throws BodyDecodeError<FormDataError> {
    return unsafe { try await stdHttpResponseFormData(inout handle, formLimits) }
  }

  export take async fn json<Value: json.Decodable>(
    maximumBytes byteLimit: usize<(1...)>,
    profile decodeProfile: json.Profile = .interoperable,
    unknownMembers memberPolicy: json.UnknownMemberPolicy = .reject,
  ): Value throws BodyDecodeError<json.DecodeError> {
    return try await json(
      limits: json.Limits(maximumBytes: byteLimit),
      profile: decodeProfile,
      unknownMembers: memberPolicy,
    )
  }

  export take async fn json<Value: json.Decodable>(
    limits decodeLimits: json.Limits,
    profile decodeProfile: json.Profile = .interoperable,
    unknownMembers memberPolicy: json.UnknownMemberPolicy = .reject,
  ): Value throws BodyDecodeError<json.DecodeError> {
    // Keep Response JSON on the same W composition path as Request JSON.
    var payload: Bytes
    do {
      payload = try await bytes(maximumBytes: decodeLimits.maximumBytes)
    } catch error {
      throw .body(error)
    }
    do {
      return try json.decode<Value>(
        ref payload,
        limits: decodeLimits,
        profile: decodeProfile,
        unknownMembers: memberPolicy,
      )
    } catch error {
      throw .codec(error)
    }
  }

  export take fn clone(
    maximumBufferedBytes bufferLimit: usize<(1...)>,
  ): (Response, Response) throws BodyCloneError {
    let (left, right) = unsafe {
      try stdHttpResponseClone(
        inout handle,
        bufferLimit,
      )
    }
    return (Response(validatedHandle: left), Response(validatedHandle: right))
  }

  deinit {
    unsafe { stdHttpResponseDrop(inout handle) }
  }
}

export struct RandomSource {
  let handle: RandomHandle

  init(validatedHandle: RandomHandle) {
    self.handle = validatedHandle
  }

  export fn integer(in range: Range<i32>): i32 {
    return unsafe { stdHttpRandomInteger(ref handle, range) }
  }

  deinit {
    unsafe { stdHttpRandomDrop(inout handle) }
  }
}

export struct DatabaseRegistry {
  let handle: DatabaseRegistryHandle

  init(validatedHandle: DatabaseRegistryHandle) {
    self.handle = validatedHandle
  }

  export fn get(
    binding: const database.Binding,
  ): some database.Database {
    return unsafe { stdHttpDatabaseGet(ref handle, binding) }
  }

  deinit {
    unsafe { stdHttpDatabaseRegistryDrop(inout handle) }
  }
}

export struct CacheRegistry {
  let handle: CacheRegistryHandle

  init(validatedHandle: CacheRegistryHandle) {
    self.handle = validatedHandle
  }

  export fn get<
    Key: Equatable & Hashable & Duplicable,
    Value: Duplicable,
  >(
    binding: const cache.LocalBinding<Key, Value>,
  ): some cache.LocalCache<Key, Value> {
    return unsafe { stdHttpCacheGet(ref handle, binding) }
  }

  deinit {
    unsafe { stdHttpCacheRegistryDrop(inout handle) }
  }
}

export struct TemplateBinding {
  let name: String
  let limits: TemplateLimits
  let version: u32<(1...)>

  export const init(
    name: String,
    limits: TemplateLimits,
    version: u32<(1...)> = 1,
  ) {
    self.name = take name
    self.limits = limits
    self.version = version
  }
}

// This host-template wrapper is provisional server extension behavior. It is
// not HTTP/Web semantics; the binding fixes provider limits and version.
export struct Template {
  let handle: TemplateHandle

  init(validatedHandle: TemplateHandle) {
    self.handle = validatedHandle
  }

  export fn render<Value: json.Encodable>(
    values: ref Array<Value>,
  ): String throws TemplateError {
    return unsafe {
      try stdHttpTemplateRender(ref handle, values)
    }
  }

  deinit {
    unsafe { stdHttpTemplateDrop(inout handle) }
  }
}

export struct TemplateRegistry {
  let handle: TemplateRegistryHandle

  init(validatedHandle: TemplateRegistryHandle) {
    self.handle = validatedHandle
  }

  export fn get(binding: const TemplateBinding): Template {
    return Template(validatedHandle: unsafe {
      stdHttpTemplateGet(ref handle, binding)
    })
  }

  deinit {
    unsafe { stdHttpTemplateRegistryDrop(inout handle) }
  }
}

export struct Context {
  let handle: ContextHandle

  init(validatedHandle: ContextHandle) {
    self.handle = validatedHandle
  }

  // Each projection asks the provider for an independent retained owner. The
  // temporary wrapper lives through the full expression and drops afterward;
  // an explicitly bound wrapper may outlive the Context value, but never the
  // request root.
  export let random: RandomSource {
    get => RandomSource(validatedHandle: unsafe {
      stdHttpContextRandom(ref handle)
    })
  }

  export let databases: DatabaseRegistry {
    get => DatabaseRegistry(validatedHandle: unsafe {
      stdHttpContextDatabases(ref handle)
    })
  }

  export let caches: CacheRegistry {
    get => CacheRegistry(validatedHandle: unsafe {
      stdHttpContextCaches(ref handle)
    })
  }

  export let templates: TemplateRegistry {
    get => TemplateRegistry(validatedHandle: unsafe {
      stdHttpContextTemplates(ref handle)
    })
  }

  // The provider returns an owned/duplicated signal. Its lifetime is
  // independent from the Context value, but never outlives the request root.
  export let signal: AbortSignal {
    get => unsafe { stdHttpContextSignal(ref handle) }
  }

  deinit {
    unsafe { stdHttpContextDrop(inout handle) }
  }
}

export async fn serve<Failure: Error>(
  at address: net.ListenAddress,
  using network: ref net.Network,
  limits: ServerLimits,
  handler: some async fn(take Request, Context): Response throws Failure,
): () throws ServerError {
  return unsafe {
    try await stdHttpServe(
      address,
      network,
      limits,
      handler,
    )
  }
}

export protocol HttpHandler<Failure: Error> {
  async fn fetch(
    request: take Request,
    context: Context,
  ): Response throws Failure
}

test "HTTP tokens accept only the RFC token alphabet" {
  expect isHttpToken(value: "GET")
  expect isHttpToken(value: "x-last-light")
  expect !isHttpToken(value: "")
  expect !isHttpToken(value: "bad method")
  expect !isHttpToken(value: "método")
}

test "Headers normalize names and outer HTTP whitespace" {
  var headers = Headers()
  try headers.append("X-Last-Light", " \talpha\t beta\t ")

  expect try headers.get("x-last-light") == "alpha\t beta"
  expect headers.entries()[0].name().text() == "x-last-light"
}

test "Headers reject CRLF before mutation" {
  var headers = Headers()

  do {
    try headers.append("x-safe", "text\r\ninjected")
    panic("Headers accepted a value containing CRLF")
  } catch .syntax(.invalidHeaderValue) {
    expect !(try headers.has("x-safe"))
  } catch error {
    panic("Headers returned the wrong error for CRLF")
  }
}

test "request and immutable guards return typed mutation errors" {
  var requestHeaders = Headers(guardedFields: [], guard: .request)
  do {
    try requestHeaders.append("Cookie", "session=secret")
    panic("request guard accepted a forbidden request header")
  } catch .mutationDenied(let name, let reason) {
    expect name.text() == "cookie"
    expect reason == .forbiddenRequestHeader
  } catch error {
    panic("request guard returned the wrong error")
  }

  var immutableHeaders = Headers(guardedFields: [], guard: .immutable)
  do {
    try immutableHeaders.set("x-safe", "value")
    panic("immutable guard accepted mutation")
  } catch .mutationDenied(let name, let reason) {
    expect name.text() == "x-safe"
    expect reason == .immutable
  } catch error {
    panic("immutable guard returned the wrong error")
  }
}

test "request-no-cors validates the combined value and removes Range" {
  let fragment = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  var combined = Headers(guardedFields: [], guard: .requestNoCors)
  try combined.append("Accept", fragment.materialize())

  do {
    try combined.append("Accept", fragment.materialize())
    panic("request-no-cors accepted a combined value larger than 128 bytes")
  } catch .mutationDenied(let name, let reason) {
    expect name.text() == "accept"
    expect reason == .noCorsUnsafeRequestHeader
    expect try combined.get("accept") == fragment
  } catch error {
    panic("request-no-cors returned the wrong combined-value error")
  }

  var ranged = Headers(
    guardedFields: [
      try HeaderField(name: try HeaderName("Range"), value: "bytes=0-99"),
    ],
    guard: .requestNoCors,
  )
  try ranged.append("Accept-Language", "en")
  expect !(try ranged.has("range"))
}

test "response guard rejects transport framing but permits Set-Cookie" {
  var headers = Headers(guardedFields: [], guard: .response)
  try headers.append("Set-Cookie", "shift=violet; Path=/; HttpOnly")

  do {
    try headers.set("Content-Length", "42")
    panic("response guard accepted transport-controlled framing")
  } catch .mutationDenied(let name, let reason) {
    expect name.text() == "content-length"
    expect reason == .transportControlledResponseHeader
    expect headers.getSetCookie() == ["shift=violet; Path=/; HttpOnly"]
  } catch error {
    panic("response guard returned the wrong framing error")
  }
}

test "Headers preserve repeated fields and separate Set-Cookie" {
  var headers = Headers()
  try headers.append("x-course", "soup")
  try headers.append("X-Course", "cake")
  try headers.append("set-cookie", "shift=violet; Path=/; HttpOnly")
  try headers.append("Set-Cookie", "table=7; Path=/; SameSite=Lax")

  expect try headers.get("x-course") == "soup, cake"
  expect headers.getSetCookie() == [
    "shift=violet; Path=/; HttpOnly",
    "table=7; Path=/; SameSite=Lax",
  ]
}
