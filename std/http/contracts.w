// Public value contracts for HTTP messages and server admission.

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
    if !isHttpTokenByte(byte) { return false }
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

const fn asciiLowercaseToken(value: String): String {
  var result = String(reservingBytes: value.bytes.count)

  for scalar in value.scalars {
    result.append(asciiLowercaseScalar(scalar))
  }

  return result
}

const fn normalizeHttpFieldValue(value: String): String throws HttpSyntaxError {
  guard isHttpFieldValue(value) else throw .invalidHeaderValue

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
    if asciiLowercaseByte(value.bytes[start + offset]) != expected.bytes[offset] {
      return false
    }
    offset += 1
  }

  return true
}

fn startsWithAsciiCaseInsensitive(value: ref String, expected: ref String): Bool {
  if value.bytes.count < expected.bytes.count { return false }
  return asciiCaseInsensitiveRangeEquals(value, 0, expected.bytes.count, expected)
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

    if asciiCaseInsensitiveRangeEquals(value, start, end, "connect")
      || asciiCaseInsensitiveRangeEquals(value, start, end, "trace")
      || asciiCaseInsensitiveRangeEquals(value, start, end, "track") {
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
    if isCorsUnsafeRequestHeaderByte(byte) { return true }
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
    if !isCorsLanguageByte(byte) { return false }
  }
  return true
}

fn isNoCorsSafelistedMediaType(value: ref String): Bool {
  if containsCorsUnsafeRequestHeaderByte(value) { return false }

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
    value,
    start,
    essenceEnd,
    "application/x-www-form-urlencoded",
  ) || asciiCaseInsensitiveRangeEquals(value, start, essenceEnd, "multipart/form-data")
    || asciiCaseInsensitiveRangeEquals(value, start, essenceEnd, "text/plain")
}

object NoCorsSafelist {
  fn contains(name: ref HeaderName, value: ref String): Bool {
    if value.bytes.count > 128 { return false }

    return switch name.text() {
      case "accept": !containsCorsUnsafeRequestHeaderByte(value)
      case "accept-language": isCorsLanguageValue(value)
      case "content-language": isCorsLanguageValue(value)
      case "content-type": isNoCorsSafelistedMediaType(value)
      case _: false
    }
  }
}

export struct MethodToken {
  value: String

  export const init(value: String) throws HttpSyntaxError {
    guard isHttpToken(value) else throw .invalidMethod
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
  export const notFound = StatusCode(404)
  export const internalServerError = StatusCode(500)
  export const serviceUnavailable = StatusCode(503)
}

export struct HeaderName {
  value: String

  export const init(value: String) throws HttpSyntaxError {
    guard isHttpToken(value) else throw .invalidHeaderName
    self.value = asciiLowercaseToken(take value)
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
    if startsWithAsciiCaseInsensitive(self.value, "proxy-")
      || startsWithAsciiCaseInsensitive(self.value, "sec-") {
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
      case "x-http-method": containsForbiddenMethod(value)
      case "x-http-method-override": containsForbiddenMethod(value)
      case "x-method-override": containsForbiddenMethod(value)
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
  storedName: HeaderName
  storedValue: String

  export const init(
    name: HeaderName,
    value: String,
  ) throws HttpSyntaxError {
    self.storedName = name
    self.storedValue = try normalizeHttpFieldValue(take value)
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
  fields: Array<HeaderField>
  guard: HeadersGuard

  export init(_ initialFields: take HeaderField...) {
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
  aborted
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
  unsupportedPolicy
}

export enum ResponseError: Error {
  syntax(HttpSyntaxError)
  headers(HeadersError)
  headerLimitExceeded(maximumFields: usize, maximumBytes: usize)
  encoding(JsonEncodeError)
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
  listen(NetworkError)
  accept(NetworkError)
  protocol(HttpError)
}

export struct MessageLimits {
  targetBytes: usize<(1...)>
  headerBytes: usize<(1...)>
  headerFields: usize<(1...)>
  bodyBytes: usize<(1...)>
}

export struct ServerLimits {
  activeRequests: usize<(1...)>
  queuedRequests: usize
  queuedBytes: usize
  connections: usize<(1...)>
  message: MessageLimits
}

export protocol HttpHandler {
  async fn fetch(
    request: take Request,
    context: Context,
  ): Response throws HttpError
}

test "HTTP tokens accept only the RFC token alphabet" {
  expect isHttpToken("GET")
  expect isHttpToken("x-last-light")
  expect !isHttpToken("")
  expect !isHttpToken("bad method")
  expect !isHttpToken("método")
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
