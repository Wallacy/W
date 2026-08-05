// Portable URL value contracts.
//
// The intrinsic signatures are the implementation seam for the WHATWG URL
// parser, IDNA, percent encoding, and UTF-16 ordering. They have no public ABI
// and do not grant network or filesystem authority.

export enum UrlParseError: Error {
  invalidURL
}

export enum URLComponent {
  href
  protocol
  username
  password
  host
  hostname
  port
  pathname
  search
  hash
}

export enum UrlMutationError: Error {
  invalidValue(component: URLComponent)
  incompatibleState(component: URLComponent)
}

export struct URLSearchParam: Duplicable {
  storedName: String
  storedValue: String

  export init(name: String, value: String) {
    self.storedName = name
    self.storedValue = value
  }

  export fn name(): view String {
    return storedName
  }

  export fn value(): view String {
    return storedValue
  }

  export fn duplicate(): URLSearchParam {
    return URLSearchParam(name: copy storedName, value: copy storedValue)
  }
}

enum URLViewComponent {
  href
  origin
  protocol
  username
  password
  host
  hostname
  port
  pathname
  search
  hash
}

// These signatures require the versioned std URL algorithm provider. A source
// implementation must pass the URL Standard and WPT gates named in DESIGN.md.
// The draft deliberately does not substitute a partial parser.
foreign intrinsic from "std.url-record@1" {
  type URLRecord
  fn stdURLParseAbsolute(input: ref String): URLRecord throws UrlParseError
  fn stdURLParseRelative(
    input: ref String,
    base: ref URLRecord,
  ): URLRecord throws UrlParseError
  fn stdURLCanParseAbsolute(input: ref String): Bool
  fn stdURLCanParseRelative(input: ref String, base: ref URLRecord): Bool
  fn stdURLReplaceComponent(
    record: ref URLRecord,
    component: URLComponent,
    value: ref String,
  ): URLRecord throws UrlMutationError
  fn stdURLReplaceSearch(record: ref URLRecord, value: ref String): URLRecord
  fn stdURLReplaceHash(record: ref URLRecord, value: ref String): URLRecord
  fn stdURLReplaceQuery(record: ref URLRecord, query: ref String?): URLRecord
  fn stdURLView(
    record: ref URLRecord,
    component: URLViewComponent,
  ): view String
  fn stdURLDuplicate(record: ref URLRecord): URLRecord
  fn stdURLMaterializeSearchParams(record: ref URLRecord): URLSearchParams
  fn stdURLParseForm(input: ref String): Array<URLSearchParam>
  fn stdURLSerializeForm(input: ref Array<URLSearchParam>): String
  fn stdURLSortForm(input: inout Array<URLSearchParam>): ()
}

export struct URLSearchParams: Duplicable {
  pairs: Array<URLSearchParam>
  edited: Bool

  export init(_ initialPairs: take URLSearchParam...) {
    self.pairs = take initialPairs
    self.edited = false
  }

  export init(_ encoded: String) {
    self.pairs = unsafe { stdURLParseForm(encoded) }
    self.edited = false
  }

  export init(copying source: ref URLSearchParams) {
    self.pairs = source.pairs.map((pair) => pair.duplicate())
    self.edited = false
  }

  export size: usize {
    get => pairs.count
  }

  export fn duplicate(): URLSearchParams {
    return URLSearchParams(copying: self)
  }

  export mut fn append(name: String, value: String) {
    edited = true
    pairs.append(URLSearchParam(name: take name, value: take value))
  }

  export mut fn delete(name: ref String) {
    edited = true
    var kept: Array<URLSearchParam> = []

    for pair in take pairs {
      if pair.name() != name { kept.append(take pair) }
    }

    pairs = take kept
  }

  export mut fn delete(name: ref String, value: ref String) {
    edited = true
    var kept: Array<URLSearchParam> = []

    for pair in take pairs {
      if pair.name() != name || pair.value() != value {
        kept.append(take pair)
      }
    }

    pairs = take kept
  }

  export fn get(name: ref String): Option<view String> {
    for pair in pairs {
      if pair.name() == name { return .some(pair.value()) }
    }

    return .none
  }

  export fn getAll(name: ref String): Array<String> {
    var values: Array<String> = []

    for pair in pairs {
      if pair.name() == name { values.append(copy pair.value()) }
    }

    return values
  }

  export fn has(name: ref String): Bool {
    for pair in pairs {
      if pair.name() == name { return true }
    }

    return false
  }

  export fn has(name: ref String, value: ref String): Bool {
    for pair in pairs {
      if pair.name() == name && pair.value() == value { return true }
    }

    return false
  }

  export mut fn set(name: String, value: String) {
    edited = true

    if !has(name) {
      pairs.append(URLSearchParam(name: take name, value: take value))
      return
    }

    var replaced = false
    var updated: Array<URLSearchParam> = []

    for pair in take pairs {
      if pair.name() != name {
        updated.append(take pair)
      } else if !replaced {
        updated.append(URLSearchParam(name: copy pair.name(), value: take value))
        replaced = true
      }
    }

    pairs = take updated
  }

  export mut fn sort() {
    edited = true
    unsafe { stdURLSortForm(inout pairs) }
  }

  export fn entries(): Array<URLSearchParam> {
    return pairs.map((pair) => pair.duplicate())
  }

  export fn keys(): Array<String> {
    return pairs.map((pair) => copy pair.name())
  }

  export fn values(): Array<String> {
    return pairs.map((pair) => copy pair.value())
  }

  export fn toString(): String {
    return unsafe { stdURLSerializeForm(pairs) }
  }

  mut fn beginEdit() {
    edited = false
  }

  mut fn finishEdit(): Bool {
    let wasEdited = edited
    edited = false
    return wasEdited
  }
}

export struct URL: Duplicable {
  record: URLRecord

  export init(_ input: String) throws UrlParseError {
    self.record = unsafe { try stdURLParseAbsolute(input) }
  }

  export init(_ input: String, base: ref URL) throws UrlParseError {
    self.record = unsafe {
      try stdURLParseRelative(input, base: base.record)
    }
  }

  init(validatedRecord: URLRecord) {
    self.record = validatedRecord
  }

  export static fn parse(_ input: ref String): URL? {
    return try? URL(copy input)
  }

  export static fn parse(_ input: ref String, base: ref URL): URL? {
    return try? URL(copy input, base: base)
  }

  export static fn canParse(_ input: ref String): Bool {
    return unsafe { stdURLCanParseAbsolute(input) }
  }

  export static fn canParse(_ input: ref String, base: ref URL): Bool {
    return unsafe { stdURLCanParseRelative(input, base: base.record) }
  }

  export href: view String {
    get => unsafe { stdURLView(record, component: .href) }
  }

  export origin: view String {
    get => unsafe { stdURLView(record, component: .origin) }
  }

  export protocol: view String {
    get => unsafe { stdURLView(record, component: .protocol) }
  }

  export username: view String {
    get => unsafe { stdURLView(record, component: .username) }
  }

  export password: view String {
    get => unsafe { stdURLView(record, component: .password) }
  }

  export host: view String {
    get => unsafe { stdURLView(record, component: .host) }
  }

  export hostname: view String {
    get => unsafe { stdURLView(record, component: .hostname) }
  }

  export port: view String {
    get => unsafe { stdURLView(record, component: .port) }
  }

  export pathname: view String {
    get => unsafe { stdURLView(record, component: .pathname) }
  }

  export search: view String {
    get => unsafe { stdURLView(record, component: .search) }
  }

  export hash: view String {
    get => unsafe { stdURLView(record, component: .hash) }
  }

  export fn searchParams(): URLSearchParams {
    return unsafe { stdURLMaterializeSearchParams(record) }
  }

  export fn duplicate(): URL {
    return URL(validatedRecord: unsafe { stdURLDuplicate(record) })
  }

  export fn toString(): String {
    return copy href
  }

  export fn toJSON(): String {
    return copy href
  }

  export mut fn setHref(_ value: String): () throws UrlMutationError {
    record = unsafe {
      try stdURLReplaceComponent(record, component: .href, value: value)
    }
  }

  export mut fn setProtocol(_ value: String): () throws UrlMutationError {
    record = unsafe {
      try stdURLReplaceComponent(record, component: .protocol, value: value)
    }
  }

  export mut fn setUsername(_ value: String): () throws UrlMutationError {
    record = unsafe {
      try stdURLReplaceComponent(record, component: .username, value: value)
    }
  }

  export mut fn setPassword(_ value: String): () throws UrlMutationError {
    record = unsafe {
      try stdURLReplaceComponent(record, component: .password, value: value)
    }
  }

  export mut fn setHost(_ value: String): () throws UrlMutationError {
    record = unsafe {
      try stdURLReplaceComponent(record, component: .host, value: value)
    }
  }

  export mut fn setHostname(_ value: String): () throws UrlMutationError {
    record = unsafe {
      try stdURLReplaceComponent(record, component: .hostname, value: value)
    }
  }

  export mut fn setPort(_ value: String): () throws UrlMutationError {
    record = unsafe {
      try stdURLReplaceComponent(record, component: .port, value: value)
    }
  }

  export mut fn setPathname(_ value: String): () throws UrlMutationError {
    record = unsafe {
      try stdURLReplaceComponent(record, component: .pathname, value: value)
    }
  }

  export mut fn setSearch(_ value: String) {
    record = unsafe { stdURLReplaceSearch(record, value: value) }
  }

  export mut fn setHash(_ value: String) {
    record = unsafe { stdURLReplaceHash(record, value: value) }
  }

  export mut fn editSearchParams<Failure: Error>(
    _ edit: fn(inout URLSearchParams): () throws Failure,
  ): () throws Failure {
    var params = unsafe { stdURLMaterializeSearchParams(record) }
    params.beginEdit()
    try edit(inout params)
    if !params.finishEdit() { return }

    let encoded = params.toString()
    let query: String? = if encoded.isEmpty {
      .none
    } else {
      .some(take encoded)
    }
    record = unsafe { stdURLReplaceQuery(record, query: query) }
  }
}
