// Bounded SDK0 network contracts.
//
// This file fixes the typed surface for network carriers. The intrinsic seam
// remains missing until the gates in DESIGN.md pass.

import * from std.io
import { Duration } from std.time

export enum AddressFamily: Copy & Equatable & Hashable {
  ipv4
  ipv6
  any
}

export enum AddressError: Error & Duplicable {
  invalidText
  invalidShape
  invalidHostname
  scopeOnIpv4
  zeroScope
}

// Limit labels keep budget failures portable while preserving the bound that
// the caller requested. A smaller product or deployment envelope is valid, but
// the effective maximum must be reported.
export enum NetworkLimitKind: Copy & Equatable {
  resolveAddresses
  resolveCnameDepth
  resolveResponseBytes
  resolveAllocationBytes
  connectAttempts
  connectQueue
  listenerBacklog
  listenerQueue
  datagramBytes
  datagramQueue
  bufferBytes
}

export enum NetworkError: Error & Duplicable {
  denied
  unavailable
  overloaded
  limitExceeded(kind: NetworkLimitKind, maximum: usize)
  addressInUse
  addressNotAvailable
  nameNotFound
  nameTemporary
  networkUnreachable
  hostUnreachable
  connectionRefused
  reset
  aborted
  disconnected
  timedOut
  messageTooLarge(maximum: usize)
  unsupported
  system(IoError)
}

export struct Ipv4Address: Duplicable & Equatable & Hashable {
  octetValues: [u8; 4]

  // The fixed shape makes every value valid without a runtime parser.
  export const init(
    _ first: u8,
    _ second: u8,
    _ third: u8,
    _ fourth: u8,
  ) {
    self.octetValues = [first, second, third, fourth]
  }

  init(validatedOctets: [u8; 4]) {
    self.octetValues = validatedOctets
  }

  export static fn parse(_ text: ref String): Ipv4Address throws AddressError {
    return unsafe { try stdNetIpv4Parse(text) }
  }

  export static fn loopback(): Ipv4Address {
    return Ipv4Address(127, 0, 0, 1)
  }

  export static fn unspecified(): Ipv4Address {
    return Ipv4Address(0, 0, 0, 0)
  }

  export fn octets(): [u8; 4] {
    return octetValues
  }

  export fn text(): String {
    return unsafe { stdNetIpv4Format(ref self) }
  }

  export fn duplicate(): Ipv4Address {
    return Ipv4Address(validatedOctets: octetValues)
  }

  export fn equals(other: ref Ipv4Address): Bool {
    return octetValues == other.octetValues
  }

  fn hash(into hasher: inout Hasher) {
    for octet in octetValues { hasher.append(octet) }
  }
}

export struct Ipv6Address: Duplicable & Equatable & Hashable {
  segmentValues: [u16; 8]

  // The fixed shape makes every value valid without a runtime parser.
  export const init(
    _ first: u16,
    _ second: u16,
    _ third: u16,
    _ fourth: u16,
    _ fifth: u16,
    _ sixth: u16,
    _ seventh: u16,
    _ eighth: u16,
  ) {
    self.segmentValues = [
      first,
      second,
      third,
      fourth,
      fifth,
      sixth,
      seventh,
      eighth,
    ]
  }

  init(validatedSegments: [u16; 8]) {
    self.segmentValues = validatedSegments
  }

  export static fn parse(_ text: ref String): Ipv6Address throws AddressError {
    return unsafe { try stdNetIpv6Parse(text) }
  }

  export static fn loopback(): Ipv6Address {
    return Ipv6Address(0, 0, 0, 0, 0, 0, 0, 1)
  }

  export static fn unspecified(): Ipv6Address {
    return Ipv6Address(0, 0, 0, 0, 0, 0, 0, 0)
  }

  export fn segments(): [u16; 8] {
    return segmentValues
  }

  // Formatting follows RFC 5952. Scope IDs never enter this formatter.
  export fn text(): String {
    return unsafe { stdNetIpv6Format(ref self) }
  }

  export fn duplicate(): Ipv6Address {
    return Ipv6Address(validatedSegments: segmentValues)
  }

  export fn equals(other: ref Ipv6Address): Bool {
    return segmentValues == other.segmentValues
  }

  fn hash(into hasher: inout Hasher) {
    for segment in segmentValues { hasher.append(segment) }
  }
}

export enum IpAddress: Duplicable & Equatable & Hashable {
  v4(Ipv4Address)
  v6(Ipv6Address)

  // This parser accepts only an IP literal. It never performs DNS.
  export static fn parse(_ text: ref String): IpAddress throws AddressError {
    return unsafe { try stdNetIpParse(text) }
  }

  export fn family(): AddressFamily {
    return switch self {
      case .v4: .ipv4
      case .v6: .ipv6
    }
  }

  export fn text(): String {
    return switch self {
      case .v4(let address): address.text()
      case .v6(let address): address.text()
    }
  }

  export fn duplicate(): IpAddress {
    return switch self {
      case .v4(let address): .v4(address.duplicate())
      case .v6(let address): .v6(address.duplicate())
    }
  }

  export fn equals(other: ref IpAddress): Bool {
    return switch (self, other) {
      case (.v4(let left), .v4(let right)): left == right
      case (.v6(let left), .v6(let right)): left == right
      case _: false
    }
  }

  fn hash(into hasher: inout Hasher) {
    switch self {
      case .v4(let address):
        hasher.append(4)
        address.hash(into: inout hasher)
      case .v6(let address):
        hasher.append(6)
        address.hash(into: inout hasher)
    }
  }
}

export struct SocketAddress: Duplicable & Equatable & Hashable {
  export ip: IpAddress
  export port: u16
  export scopeId: u32?

  export init(
    ip: IpAddress,
    port: u16,
    scopeId: u32? = .none,
  ) throws AddressError {
    if let scope = scopeId {
      guard scope > 0 else { throw .zeroScope }
      if case .v4(_) = ip {
        throw .scopeOnIpv4
      }
    }

    self.ip = take ip
    self.port = port
    self.scopeId = scopeId
  }

  init(validatedIp: IpAddress, port: u16, scopeId: u32?) {
    self.ip = take validatedIp
    self.port = port
    self.scopeId = scopeId
  }

  // Grammar is numeric and port is mandatory:
  // 192.0.2.1:443, [2001:db8::1]:443, [fe80::1%3]:443.
  // IPv6 scope text stays inside brackets. Host names are rejected, and
  // parsing never performs DNS. Scope zero is rejected; none means absent.
  export static fn parse(_ text: ref String): SocketAddress throws AddressError {
    return unsafe { try stdNetSocketAddressParse(text) }
  }

  export fn family(): AddressFamily {
    return ip.family()
  }

  export fn text(): String {
    // Formatting emits the canonical form accepted by parse, so a valid value
    // round-trips. IPv6 uses brackets and keeps a nonzero scope inside.
    return unsafe { stdNetSocketAddressFormat(ref self) }
  }

  export fn duplicate(): SocketAddress {
    return SocketAddress(
      validatedIp: ip.duplicate(),
      port: port,
      scopeId: scopeId,
    )
  }

  export fn equals(other: ref SocketAddress): Bool {
    return ip == other.ip && port == other.port && scopeId == other.scopeId
  }

  fn hash(into hasher: inout Hasher) {
    ip.hash(into: inout hasher)
    hasher.append(port)
    if let scope = scopeId { hasher.append(scope) }
  }
}

// HostName is absolute and is not a free String. UTS #46 nontransitional
// processing, STD3 rules, IDNA2008 validity, lowercase A-label storage, and DNS
// bounds apply. One trailing dot is accepted and removed. An OS search suffix
// is never applied.
export struct HostName: Duplicable & Equatable & Hashable {
  canonical: String

  export init(_ value: String) throws AddressError {
    self.canonical = unsafe {
      try stdNetHostNameNormalize(ref value)
    }
  }

  init(validatedCanonical: String) {
    self.canonical = take validatedCanonical
  }

  export fn text(): view String {
    return canonical
  }

  export fn duplicate(): HostName {
    return HostName(validatedCanonical: canonical.materialize())
  }

  export fn equals(other: ref HostName): Bool {
    return canonical == other.canonical
  }

  fn hash(into hasher: inout Hasher) {
    hasher.append(canonical)
  }
}

export struct Endpoint: Duplicable & Equatable & Hashable {
  export host: EndpointHost
  export port: u16<(1...65_535)>

  export init(
    ip: IpAddress,
    port: u16<(1...65_535)>,
  ) {
    self.host = .ip(take ip)
    self.port = port
  }

  export init(
    host: HostName,
    port: u16<(1...65_535)>,
  ) {
    self.host = .name(take host)
    self.port = port
  }

  export fn duplicate(): Endpoint {
    return switch host {
      case .ip(let ip): Endpoint(ip: ip.duplicate(), port: port)
      case .name(let name): Endpoint(host: name.duplicate(), port: port)
    }
  }

  export fn equals(other: ref Endpoint): Bool {
    return host == other.host && port == other.port
  }

  fn hash(into hasher: inout Hasher) {
    host.hash(into: inout hasher)
    hasher.append(port)
  }
}

export enum EndpointHost: Duplicable & Equatable & Hashable {
  ip(IpAddress)
  name(HostName)

  export fn duplicate(): EndpointHost {
    return switch self {
      case .ip(let address): .ip(address.duplicate())
      case .name(let name): .name(name.duplicate())
    }
  }

  export fn equals(other: ref EndpointHost): Bool {
    return switch (self, other) {
      case (.ip(let left), .ip(let right)): left == right
      case (.name(let left), .name(let right)): left == right
      case _: false
    }
  }

  fn hash(into hasher: inout Hasher) {
    switch self {
      case .ip(let address):
        hasher.append(4)
        address.hash(into: inout hasher)
      case .name(let name):
        hasher.append(7)
        name.hash(into: inout hasher)
    }
  }
}

enum ListenAddressShape: Duplicable & Equatable & Hashable {
  loopback(u16, AddressFamily)
  allInterfaces(u16, AddressFamily)
  socket(SocketAddress)

  fn duplicate(): ListenAddressShape {
    return switch self {
      case .loopback(let port, let family): .loopback(port, family)
      case .allInterfaces(let port, let family): .allInterfaces(port, family)
      case .socket(let address): .socket(address.duplicate())
    }
  }

  fn equals(other: ref ListenAddressShape): Bool {
    return switch (self, other) {
      case (.loopback(let leftPort, let leftFamily), .loopback(let rightPort, let rightFamily)):
        leftPort == rightPort && leftFamily == rightFamily
      case (.allInterfaces(let leftPort, let leftFamily), .allInterfaces(let rightPort, let rightFamily)):
        leftPort == rightPort && leftFamily == rightFamily
      case (.socket(let left), .socket(let right)): left == right
      case _: false
    }
  }

  fn hash(into hasher: inout Hasher) {
    switch self {
      case .loopback(let port, let family):
        hasher.append(1)
        hasher.append(port)
        hasher.append(family)
      case .allInterfaces(let port, let family):
        hasher.append(2)
        hasher.append(port)
        hasher.append(family)
      case .socket(let address):
        hasher.append(3)
        address.hash(into: inout hasher)
    }
  }
}

// ListenAddress is nominal so a caller cannot smuggle a remote Endpoint into
// a bind operation. Port zero asks the host to select a free local port.
export struct ListenAddress: Duplicable & Equatable & Hashable {
  shape: ListenAddressShape

  init(validatedShape: ListenAddressShape) {
    self.shape = validatedShape
  }

  export static fn loopback(
    port: u16,
    family: AddressFamily = .any,
  ): ListenAddress {
    return ListenAddress(validatedShape: .loopback(port, family))
  }

  export static fn allInterfaces(
    port: u16,
    family: AddressFamily = .any,
  ): ListenAddress {
    return ListenAddress(validatedShape: .allInterfaces(port, family))
  }

  export static fn address(_ address: SocketAddress): ListenAddress {
    return ListenAddress(validatedShape: .socket(take address))
  }

  export fn duplicate(): ListenAddress {
    return switch shape {
      case .loopback(let port, let family): .loopback(port, family)
      case .allInterfaces(let port, let family): .allInterfaces(port, family)
      case .socket(let address): .socket(address.duplicate())
    }
  }

  export fn equals(other: ref ListenAddress): Bool {
    return shape == other.shape
  }

  fn hash(into hasher: inout Hasher) {
    shape.hash(into: inout hasher)
  }
}

export struct ResolveLimits: Copy & Equatable {
  export maximumAddresses: usize<(1...)>
  export maximumCnameDepth: usize
  export maximumResponseBytes: usize<(1...)>
  export maximumAllocationBytes: usize<(1...)>

  export const init(
    maximumAddresses: usize<(1...)> = 16,
    maximumCnameDepth: usize = 8,
    maximumResponseBytes: usize<(1...)> = 64<KiB>,
    maximumAllocationBytes: usize<(1...)> = 256<KiB>,
  ) {
    self.maximumAddresses = maximumAddresses
    self.maximumCnameDepth = maximumCnameDepth
    self.maximumResponseBytes = maximumResponseBytes
    self.maximumAllocationBytes = maximumAllocationBytes
  }
}

export enum AddressPreference: Copy & Equatable {
  system
  ipv4First
  ipv6First
}

export struct ConnectOptions: Duplicable & Equatable {
  export maximumAttempts: usize<(1...16)>
  export fallbackDelay: Duration<(0...30<s>)>
  export preference: AddressPreference
  export local: SocketAddress?

  export const init(
    maximumAttempts: usize<(1...16)> = 4,
    fallbackDelay: Duration<(0...30<s>)> = 250<ms>,
    preference: AddressPreference = .system,
    local: SocketAddress? = .none,
  ) {
    self.maximumAttempts = maximumAttempts
    self.fallbackDelay = fallbackDelay
    self.preference = preference
    self.local = local
  }

  export fn duplicate(): ConnectOptions {
    return ConnectOptions(
      maximumAttempts: maximumAttempts,
      fallbackDelay: fallbackDelay,
      preference: preference,
      local: local?.duplicate(),
    )
  }

  export fn equals(other: ref ConnectOptions): Bool {
    return maximumAttempts == other.maximumAttempts
      && fallbackDelay == other.fallbackDelay
      && preference == other.preference
      && local == other.local
  }
}

export struct ListenerLimits: Copy & Equatable {
  export maximumBacklog: usize<(1...)>
  export maximumQueuedAccepts: usize<(1...)>

  export const init(
    maximumBacklog: usize<(1...)> = 128,
    maximumQueuedAccepts: usize<(1...)> = 128,
  ) {
    self.maximumBacklog = maximumBacklog
    self.maximumQueuedAccepts = maximumQueuedAccepts
  }
}

export struct DatagramLimits: Copy & Equatable {
  export maximumDatagramBytes: usize<(1...)>
  export maximumQueuedDatagrams: usize<(1...)>
  export maximumQueuedBytes: usize<(1...)>

  export const init(
    maximumDatagramBytes: usize<(1...)> = 65_507,
    maximumQueuedDatagrams: usize<(1...)> = 128,
    maximumQueuedBytes: usize<(1...)> = 1<MiB>,
  ) {
    self.maximumDatagramBytes = maximumDatagramBytes
    self.maximumQueuedDatagrams = maximumQueuedDatagrams
    self.maximumQueuedBytes = maximumQueuedBytes
  }
}

export struct Datagram: Duplicable {
  export bytes: Bytes
  export peer: SocketAddress
  export truncated: Bool

  init(validatedBytes: take Bytes, peer: SocketAddress, truncated: Bool) {
    self.bytes = take validatedBytes
    self.peer = take peer
    self.truncated = truncated
  }

  export fn duplicate(): Datagram {
    return Datagram(
      validatedBytes: copy bytes,
      peer: peer.duplicate(),
      truncated: truncated,
    )
  }
}

foreign intrinsic from "std.net@1" {
  type NetworkHandle
  type TcpConnectionHandle
  type TcpReadHalfHandle
  type TcpWriteHalfHandle
  type TcpListenerHandle
  type UdpSocketHandle
  type UdpReceiveHalfHandle
  type UdpSendHalfHandle

  fn stdNetIpv4Parse(text: ref String): Ipv4Address throws AddressError
  fn stdNetIpv4Format(address: ref Ipv4Address): String
  fn stdNetIpv6Parse(text: ref String): Ipv6Address throws AddressError
  fn stdNetIpv6Format(address: ref Ipv6Address): String
  fn stdNetIpParse(text: ref String): IpAddress throws AddressError
  fn stdNetSocketAddressParse(text: ref String): SocketAddress throws AddressError
  fn stdNetSocketAddressFormat(address: ref SocketAddress): String
  fn stdNetHostNameNormalize(value: ref String): String throws AddressError

  async fn stdNetResolve(
    named network: ref NetworkHandle,
    named host: ref HostName,
    named port: u16<(1...65_535)>,
    named limits: ref ResolveLimits,
  ): Array<SocketAddress> throws NetworkError
  async fn stdNetConnectTcp(
    named network: ref NetworkHandle,
    named endpoint: ref Endpoint,
    named options: ref ConnectOptions,
  ): TcpConnectionHandle throws NetworkError
  async fn stdNetListenTcp(
    named network: ref NetworkHandle,
    named address: ref ListenAddress,
    named limits: ref ListenerLimits,
  ): TcpListenerHandle throws NetworkError
  async fn stdNetBindUdp(
    named network: ref NetworkHandle,
    named address: ref ListenAddress,
    named limits: ref DatagramLimits,
  ): UdpSocketHandle throws NetworkError

  fn stdNetTcpLocalAddress(handle: ref TcpConnectionHandle): SocketAddress
  fn stdNetTcpPeerAddress(handle: ref TcpConnectionHandle): SocketAddress
  async fn stdNetTcpRead(
    named handle: inout TcpConnectionHandle,
    appendTo destination: inout Bytes,
    named maximum: usize<(1...)>,
  ): ReadStep throws NetworkError
  async fn stdNetTcpWrite(
    handle: inout TcpConnectionHandle,
    source: view Bytes,
  ): WriteStep throws NetworkError
  fn stdNetTcpSplit(
    handle: inout TcpConnectionHandle,
  ): (TcpReadHalfHandle, TcpWriteHalfHandle)
  async fn stdNetTcpFinishWriting(
    handle: inout TcpConnectionHandle,
  ): TcpReadHalfHandle throws NetworkError
  fn stdNetTcpDrop(handle: inout TcpConnectionHandle)

  async fn stdNetTcpReadHalfRead(
    named handle: inout TcpReadHalfHandle,
    appendTo destination: inout Bytes,
    named maximum: usize<(1...)>,
  ): ReadStep throws NetworkError
  fn stdNetTcpReadHalfDrop(handle: inout TcpReadHalfHandle)

  async fn stdNetTcpWriteHalfWrite(
    handle: inout TcpWriteHalfHandle,
    source: view Bytes,
  ): WriteStep throws NetworkError
  async fn stdNetTcpWriteHalfFinish(
    handle: inout TcpWriteHalfHandle,
  ): () throws NetworkError
  fn stdNetTcpWriteHalfDrop(handle: inout TcpWriteHalfHandle)

  async fn stdNetTcpAccept(
    handle: inout TcpListenerHandle,
  ): (TcpConnectionHandle, SocketAddress) throws NetworkError
  fn stdNetTcpLocalAddresses(handle: ref TcpListenerHandle): Array<SocketAddress>
  fn stdNetTcpListenerDrop(handle: inout TcpListenerHandle)

  fn stdNetUdpLocalAddresses(handle: ref UdpSocketHandle): Array<SocketAddress>
  fn stdNetUdpSplit(
    handle: inout UdpSocketHandle,
  ): (UdpReceiveHalfHandle, UdpSendHalfHandle)
  async fn stdNetUdpReceive(
    named handle: inout UdpSocketHandle,
    named maximumBytes: usize<(1...)>,
  ): Datagram throws NetworkError
  async fn stdNetUdpSend(
    named handle: inout UdpSocketHandle,
    named source: view Bytes,
    to address: ref SocketAddress,
  ): () throws NetworkError
  fn stdNetUdpDrop(handle: inout UdpSocketHandle)
  async fn stdNetUdpReceiveHalfReceive(
    named handle: inout UdpReceiveHalfHandle,
    named maximumBytes: usize<(1...)>,
  ): Datagram throws NetworkError
  fn stdNetUdpReceiveHalfDrop(handle: inout UdpReceiveHalfHandle)
  async fn stdNetUdpSendHalfSend(
    named handle: inout UdpSendHalfHandle,
    named source: view Bytes,
    to address: ref SocketAddress,
  ): () throws NetworkError
  fn stdNetUdpSendHalfDrop(handle: inout UdpSendHalfHandle)

  fn stdNetNetworkDrop(handle: inout NetworkHandle)
}

// The host creates this owner through the entry binding. Source code cannot
// call the initializer or duplicate the capability.
export struct Network {
  handle: NetworkHandle

  init(hostHandle: NetworkHandle) {
    self.handle = hostHandle
  }

  // Calls through a Network borrow create independent operation state. They
  // may coexist when capability and policy checks have no conflict. Task
  // cancellation drains the operation before its borrow or buffer is released.
  export async fn resolve(
    _ host: ref HostName,
    named port: u16<(1...65_535)>,
    named limits: ResolveLimits = ResolveLimits(),
  ): Array<SocketAddress> throws NetworkError {
    return unsafe {
      try await stdNetResolve(
        network: ref handle,
        host: host,
        port: port,
        limits: ref limits,
      )
    }
  }

  export async fn connectTcp(
    to endpoint: ref Endpoint,
    named options: ConnectOptions = ConnectOptions(),
  ): TcpConnection throws NetworkError {
    let connection = unsafe {
      try await stdNetConnectTcp(
        network: ref handle,
        endpoint: endpoint,
        options: ref options,
      )
    }
    return TcpConnection(validatedHandle: connection)
  }

  export async fn listenTcp(
    at address: ref ListenAddress,
    named limits: ListenerLimits = ListenerLimits(),
  ): TcpListener throws NetworkError {
    let listener = unsafe {
      try await stdNetListenTcp(
        network: ref handle,
        address: address,
        limits: ref limits,
      )
    }
    return TcpListener(validatedHandle: listener)
  }

  export async fn bindUdp(
    at address: ref ListenAddress,
    named limits: DatagramLimits = DatagramLimits(),
  ): UdpSocket throws NetworkError {
    let socket = unsafe {
      try await stdNetBindUdp(
        network: ref handle,
        address: address,
        limits: ref limits,
      )
    }
    return UdpSocket(validatedHandle: socket)
  }

  deinit {
    // Safe code reaches deinit only after borrows have drained. Residual
    // physical or latched state is cleared, then the capability is released
    // once.
    unsafe { stdNetNetworkDrop(inout handle) }
  }
}

export struct TcpConnection: ByteSource<NetworkError> & ByteSink<NetworkError> {
  handle: TcpConnectionHandle

  // Mutating async reads and writes hold an exclusive borrow until completion
  // or cancellation drain. EOF is sticky; a latched error is observed before
  // a new operation, and reset, abort, or disconnect never reopens the handle.

  init(validatedHandle: TcpConnectionHandle) {
    self.handle = validatedHandle
  }

  export fn localAddress(): SocketAddress {
    return unsafe { stdNetTcpLocalAddress(ref handle) }
  }

  export fn peerAddress(): SocketAddress {
    return unsafe { stdNetTcpPeerAddress(ref handle) }
  }

  export mut async fn read(
    appendTo destination: inout Bytes,
    named maximum: usize<(1...)>,
  ): ReadStep throws NetworkError {
    return unsafe {
      try await stdNetTcpRead(
        handle: inout handle,
        appendTo: inout destination,
        maximum: maximum,
      )
    }
  }

  export mut async fn write(source: view Bytes): WriteStep throws NetworkError {
    return unsafe {
      try await stdNetTcpWrite(inout handle, source)
    }
  }

  export take fn split(): (TcpReadHalf, TcpWriteHalf) {
    let (read, write) = unsafe {
      stdNetTcpSplit(inout handle)
    }
    return (
      TcpReadHalf(validatedHandle: read),
      TcpWriteHalf(validatedHandle: write),
    )
  }

  // Consumes the unsplit connection, commits FIN, and returns its read half.
  // This take async receiver is consumed before suspension and remains
  // consumed on success, error, or cancellation.
  export take async fn finishWriting(): TcpReadHalf throws NetworkError {
    let read = unsafe {
      try await stdNetTcpFinishWriting(inout handle)
    }
    return TcpReadHalf(validatedHandle: read)
  }

  deinit {
    // Safe code reaches deinit after borrows have drained. Unsplit drop then
    // aborts the connection, clears residual state, and releases once.
    unsafe { stdNetTcpDrop(inout handle) }
  }
}

export struct TcpReadHalf: ByteSource<NetworkError> {
  handle: TcpReadHalfHandle

  // The read cursor keeps its exclusive borrow through completion or drain.
  // EOF and terminal transport errors remain latched.

  init(validatedHandle: TcpReadHalfHandle) {
    self.handle = validatedHandle
  }

  export mut async fn read(
    appendTo destination: inout Bytes,
    named maximum: usize<(1...)>,
  ): ReadStep throws NetworkError {
    return unsafe {
      try await stdNetTcpReadHalfRead(
        handle: inout handle,
        appendTo: inout destination,
        maximum: maximum,
      )
    }
  }

  // Dropping a read half ends receive authority only. It does not finish the
  // sibling write half.
  deinit {
    unsafe { stdNetTcpReadHalfDrop(inout handle) }
  }
}

export struct TcpWriteHalf: ByteSink<NetworkError> {
  handle: TcpWriteHalfHandle

  // Mutating async writes hold the exclusive borrow through completion or
  // cancellation drain. Full duplex TCP uses split to provide two cursors.

  init(validatedHandle: TcpWriteHalfHandle) {
    self.handle = validatedHandle
  }

  export mut async fn write(source: view Bytes): WriteStep throws NetworkError {
    return unsafe {
      try await stdNetTcpWriteHalfWrite(inout handle, source)
    }
  }

  // finish commits FIN. This take async receiver is consumed before
  // suspension and remains consumed on success, error, or cancellation.
  // Drop without finish aborts the whole connection, and the sibling read
  // half observes reset or aborted rather than hanging.
  export take async fn finish(): () throws NetworkError {
    return unsafe {
      try await stdNetTcpWriteHalfFinish(inout handle)
    }
  }

  deinit {
    // After all borrows drain, an unfinished write half aborts the whole
    // connection. The sibling read half receives reset or aborted rather than
    // hanging.
    unsafe { stdNetTcpWriteHalfDrop(inout handle) }
  }
}

export struct AcceptedTcp {
  export connection: TcpConnection
  export peer: SocketAddress

  init(validatedConnection: TcpConnection, peer: SocketAddress) {
    self.connection = take validatedConnection
    self.peer = take peer
  }
}

export struct TcpListener {
  handle: TcpListenerHandle

  // accept is a mutating async operation. Its exclusive borrow lasts through
  // completion or cancellation drain.

  init(validatedHandle: TcpListenerHandle) {
    self.handle = validatedHandle
  }

  export mut async fn accept(): AcceptedTcp throws NetworkError {
    let (connection, peer) = unsafe {
      try await stdNetTcpAccept(inout handle)
    }
    return AcceptedTcp(
      validatedConnection: TcpConnection(validatedHandle: connection),
      peer: take peer,
    )
  }

  export fn localAddresses(): Array<SocketAddress> {
    return unsafe { stdNetTcpLocalAddresses(ref handle) }
  }

  deinit {
    // Safe code reaches deinit after accept borrows have drained. Residual
    // listener state is cleared and the handle is released once.
    unsafe { stdNetTcpListenerDrop(inout handle) }
  }
}

export struct UdpSocket {
  handle: UdpSocketHandle

  // W-1252: the whole socket serializes mutation. A consuming split creates
  // one independent receive cursor and one independent send cursor.

  init(validatedHandle: UdpSocketHandle) {
    self.handle = validatedHandle
  }

  export fn localAddresses(): Array<SocketAddress> {
    return unsafe { stdNetUdpLocalAddresses(ref handle) }
  }

  export take fn split(): (UdpReceiveHalf, UdpSendHalf) {
    let (receive, send) = unsafe { stdNetUdpSplit(inout handle) }
    return (
      UdpReceiveHalf(validatedHandle: receive),
      UdpSendHalf(validatedHandle: send),
    )
  }

  export mut async fn receive(
    maximumBytes: usize<(1...)>,
  ): Datagram throws NetworkError {
    return unsafe {
      try await stdNetUdpReceive(
        handle: inout handle,
        maximumBytes: maximumBytes,
      )
    }
  }

  export mut async fn send(
    source: view Bytes,
    to address: ref SocketAddress,
  ): () throws NetworkError {
    return unsafe {
      try await stdNetUdpSend(
        handle: inout handle,
        source: source,
        to: address,
      )
    }
  }

  deinit {
    // Safe code reaches deinit after receive/send borrows have drained.
    // Residual socket state is cleared and the handle is released once.
    unsafe { stdNetUdpDrop(inout handle) }
  }
}

export struct UdpReceiveHalf {
  handle: UdpReceiveHalfHandle

  init(validatedHandle: UdpReceiveHalfHandle) {
    self.handle = validatedHandle
  }

  export mut async fn receive(
    maximumBytes: usize<(1...)>,
  ): Datagram throws NetworkError {
    return unsafe {
      try await stdNetUdpReceiveHalfReceive(
        handle: inout handle,
        maximumBytes: maximumBytes,
      )
    }
  }

  deinit {
    unsafe { stdNetUdpReceiveHalfDrop(inout handle) }
  }
}

export struct UdpSendHalf {
  handle: UdpSendHalfHandle

  init(validatedHandle: UdpSendHalfHandle) {
    self.handle = validatedHandle
  }

  export mut async fn send(
    source: view Bytes,
    to address: ref SocketAddress,
  ): () throws NetworkError {
    return unsafe {
      try await stdNetUdpSendHalfSend(
        handle: inout handle,
        source: source,
        to: address,
      )
    }
  }

  deinit {
    unsafe { stdNetUdpSendHalfDrop(inout handle) }
  }
}

// IP literal parsing never performs DNS. Endpoint host names are normalized
// before they enter resolve or connect, and each dynamic result is checked
// against product, deployment, and call envelopes.
test "typed network values keep IPv4 and IPv6 shapes distinct" {
  let v4 = Ipv4Address.loopback()
  let v6 = Ipv6Address.loopback()
  expect v4.octets() == [127, 0, 0, 1]
  expect v6.segments()[7] == 1
  do {
    let socket = try SocketAddress(ip: .v4(v4), port: 0)
    expect socket.family() == .ipv4
  } catch error {
    panic("loopback IPv4 address was rejected")
  }
}

test "scope IDs are IPv6-only" {
  do {
    let _ = try SocketAddress(
      ip: .v4(Ipv4Address.unspecified()),
      port: 0,
      scopeId: .some(2),
    )
    panic("IPv4 accepted an IPv6 scope ID")
  } catch .scopeOnIpv4 {
    expect true
  }
}

test "scope zero is not a second spelling of no scope" {
  do {
    let _ = try SocketAddress(
      ip: .v6(Ipv6Address.loopback()),
      port: 443,
      scopeId: .some(0),
    )
    panic("IPv6 accepted a zero scope ID")
  } catch .zeroScope {
    expect true
  }
}
