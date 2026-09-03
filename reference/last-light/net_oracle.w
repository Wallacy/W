// Focused SDK0 network oracle.
//
// These calls fix the typed compile surface. They do not claim execution while
// std.net@1 is missing and no host capability is bound.

import iec from std
import * from std.io
import net from std.net
import si from std

fn addressOracle(): (net.Ipv4Address, net.Ipv6Address, net.SocketAddress) {
  let v4 = net.Ipv4Address(127, 0, 0, 1)
  let v6 = net.Ipv6Address(0, 0, 0, 0, 0, 0, 0, 1)
  let socket = try! net.SocketAddress(
    ip: .v6(v6),
    port: 8080,
    scopeId: .none,
  )
  let socketText = socket.text()
  let parsedSocket = try! net.SocketAddress.parse(ref socketText)
  expect parsedSocket == socket

  let ipv4Text = "192.0.2.1:443"
  let parsedIpv4 = try! net.SocketAddress.parse(ref ipv4Text)
  expect parsedIpv4.text() == ipv4Text

  let ipv6Text = "[2001:db8::1]:443"
  let parsedIpv6 = try! net.SocketAddress.parse(ref ipv6Text)
  expect parsedIpv6.text() == ipv6Text

  let scopedIpv6Text = "[fe80::1%3]:443"
  let parsedScopedIpv6 = try! net.SocketAddress.parse(ref scopedIpv6Text)
  expect parsedScopedIpv6.text() == scopedIpv6Text
  return (v4, v6, socket)
}

fn listenAddressOracle(): (net.ListenAddress, net.ListenAddress) {
  let loopback = .loopback(port: 8080, family: .any)
  let allInterfaces = .allInterfaces(port: 0, family: .any)
  return (loopback, allInterfaces)
}

async fn resolveAndConnectOracle(
  network: ref net.Network,
): net.TcpConnection throws net.NetworkError {
  let host = try net.HostName("example.test")
  let limits = net.ResolveLimits(
    maximumAddresses: 8,
    maximumCnameDepth: 4,
    maximumResponseBytes: 16<iec.KiB>,
    maximumAllocationBytes: 64<iec.KiB>,
  )
  let addresses = try await network.resolve(ref host, port: 443, limits: limits)
  expect addresses.count <= limits.maximumAddresses

  let endpoint = net.Endpoint(host: host.duplicate(), port: 443)
  let options = net.ConnectOptions(
    maximumAttempts: 4,
    fallbackDelay: 250<si.ms>,
    preference: .system,
  )
  return try await network.connectTcp(to: ref endpoint, options: options)
}

async fn tcpSplitOracle(
  network: ref net.Network,
): () throws net.NetworkError {
  var connection = try await resolveAndConnectOracle(network: network)
  let (input, output) = (take connection).split()

  let requestText = "ping"
  let request = Bytes(copying: requestText.bytes)
  do {
    try await output.writeAll(request)
  } catch error {
    throw error.cause
  }
  try await output.finish()

  var response = Bytes()
  var reachedEnd = false
  while !reachedEnd && response.count < 4<iec.KiB> {
    let step = try await input.read(
      appendTo: inout response,
      maximum: 4<iec.KiB>,
    )
    switch step {
      case .data(let count):
        expect count > 0
      case .end:
        reachedEnd = true
    }
  }
  // A response at the bound is intentionally truncated by policy. The oracle
  // records that bound instead of treating a short read as complete.
  expect reachedEnd || response.count == 4<iec.KiB>
}

async fn tcpFinishWritingOracle(
  network: ref net.Network,
): net.TcpReadHalf throws net.NetworkError {
  var connection = try await resolveAndConnectOracle(network: network)
  return try await (take connection).finishWriting()
}

async fn listenerOracle(
  network: ref net.Network,
): () throws net.NetworkError {
  let address = .loopback(port: 0, family: .any)
  var listener = try await network.listenTcp(
    at: ref address,
    limits: net.ListenerLimits(
      maximumBacklog: 64,
      maximumQueuedAccepts: 32,
    ),
  )
  let accepted = try await listener.accept()
  let _peer = accepted.peer
  let _local = listener.localAddresses()
  let _connection = accepted.connection
}

async fn udpOracle(
  network: ref net.Network,
  probe: view Bytes,
  peer: ref net.SocketAddress,
): () throws net.NetworkError {
  let address = .allInterfaces(port: 0, family: .any)
  var socket = try await network.bindUdp(
    at: ref address,
    limits: net.DatagramLimits(
      maximumDatagramBytes: 1<iec.KiB>,
      maximumQueuedDatagrams: 32,
      maximumQueuedBytes: 32<iec.KiB>,
    ),
  )
  // W-1252: the two unique halves progress independently without a shared
  // socket or lock in application source.
  let (receive, send) = (take socket).split()
  let inbound = async receive.receive(maximumBytes: 1<iec.KiB>)
  let outbound = async send.send(source: probe, to: peer)
  let (received, _) = try await (inbound, outbound)
  let datagram: net.Datagram = take received
  if datagram.truncated {
    expect datagram.bytes.count == 1<iec.KiB>
  }
}

// Provider-gated runtime cases:
// - IPv4 and IPv6 format follow their selected RFC forms.
// - SocketAddress parse requires a numeric host and a port, and text round-trips.
// - resolve and connect apply bounded DNS, RFC 6724 ordering, and RFC 8305.
// - split permits one read cursor and one write cursor only.
// - finish and finishWriting send FIN. Drop without finish is abortive.
// - Unsplit UDP serializes mutation; split permits one receive and one send.
// - UDP reports truncation explicitly and preserves each datagram boundary.
