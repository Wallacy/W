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

const fn isHttpFieldValue(value: ref Bytes): Bool {
  for byte in value {
    if byte != 9 && (byte < 32 || byte == 127) { return false }
  }

  return true
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

export struct HeaderName {
  value: String

  export const init(value: String) throws HttpSyntaxError {
    guard isHttpToken(value) else throw .invalidHeaderName
    self.value = value
  }

  export fn text(): view String {
    return value
  }
}

export struct HeaderField {
  storedName: HeaderName
  storedValue: Bytes

  export const init(
    name: HeaderName,
    value: Bytes,
  ) throws HttpSyntaxError {
    guard isHttpFieldValue(value) else throw .invalidHeaderValue
    self.storedName = name
    self.storedValue = value
  }

  export fn name(): ref HeaderName {
    return storedName
  }

  export fn value(): view Bytes {
    return storedValue
  }
}

export enum HttpSyntaxError: Error {
  invalidMethod
  invalidTarget
  invalidHeaderName
  invalidHeaderValue
  invalidStatus(found: u16)
}

export enum HttpBodyError: Error {
  malformed
  limitExceeded(maximumBytes: usize)
  incomplete
  transport(IoError)
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

export enum ResponseBody {
  none
  bytes(Bytes)
  stream(any ByteSource<HttpBodyError>)
}

export protocol IncomingBody: ByteSource<HttpBodyError> {
  take async fn discard(): () throws HttpBodyError
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

test "safe header values reject line injection" {
  expect isHttpFieldValue(b"text\tvalue")
  expect !isHttpFieldValue(b"text\r\ninjected")
  expect !isHttpFieldValue(b"\x00")
}
