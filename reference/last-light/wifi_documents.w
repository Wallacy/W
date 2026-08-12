// Directional JSON documents for the captive portal.
// These vectors are compile/provider-gated until std.json@1 and std.http@1
// exist.  They do not claim provider execution.

import iec from std
import json from std.json
import * from std.time
import {
  DeviceId,
  LoginRequest,
  SessionId,
  WifiRole,
  WifiSession,
  WifiVoucher,
} from wifi

export enum WifiDocumentField: Copy & Equatable {
  device
  voucher
  id
}

export enum WifiDocumentError: Error {
  invalidDecimal(WifiDocumentField)
  nonCanonicalDecimal(WifiDocumentField)
  invalidRefinement(WifiDocumentField)
}

export struct LoginDocument: json.Decodable {
  device: String
  voucher: String

  export take fn loginRequest(): LoginRequest throws WifiDocumentError {
    let device = try DeviceId(device.materialize())
      .mapError((_) => WifiDocumentError.invalidRefinement(.device))
    let voucher = try WifiVoucher(voucher.materialize())
      .mapError((_) => WifiDocumentError.invalidRefinement(.voucher))
    return LoginRequest(device: device, voucher: voucher)
  }
}

export struct RevokeDocument: json.Decodable {
  id: String

  export take fn sessionId(): SessionId throws WifiDocumentError {
    let carrier = try u128.parse(id)
      .mapError((_) => WifiDocumentError.invalidDecimal(.id))
    guard id == carrier.display() else throw .nonCanonicalDecimal(.id)
    return SessionId(carrier)
  }
}

fn roleToken(role: WifiRole): String {
  return switch role {
    case .guest: "guest"
    case .crew: "crew"
    case .observatory: "observatory"
  }
}

export struct SessionDocument: json.Encodable {
  session: ref WifiSession

  export init(session: ref WifiSession) {
    self.session = session
  }

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let id = u128(session.id).display()
    let role = roleToken(session.role)
    let nanoseconds: i128 = session.remaining.nanoseconds
    let remainingNanoseconds = nanoseconds.display()
    try writer.withObject((object) => {
      try object.field("id", value: ref id)
      try object.field("device", value: ref session.device)
      try object.field("role", value: ref role)
      try object.field("remainingNanoseconds", value: ref remainingNanoseconds)
    })
  }
}

test "wifi login and revoke documents keep raw fields out of semantic errors" {
  var loginBytes: Bytes = b"{\"device\":\"tablet-7\",\"voucher\":\"violet\"}"
  let login = try json.decode<LoginDocument>(ref loginBytes, limits: json.Limits(maximumBytes: 4<iec.KiB>))
  let request = try login.loginRequest()
  expect request.device == "tablet-7"

  var revokeBytes: Bytes = b"{\"id\":\"340282366920938463463374607431768211455\"}"
  let revoke = try json.decode<RevokeDocument>(ref revokeBytes, limits: json.Limits(maximumBytes: 1<iec.KiB>))
  expect try revoke.sessionId() == u128.max
}

test "wifi session uses canonical decimal values and explicit role token" {
  let session = WifiSession(
    id: u128.max,
    device: "tablet-7",
    role: .guest,
    remaining: Duration(nanoseconds: 30_000_000_000),
  )
  let document = SessionDocument(session: ref session)
  let bytes = try json.encode(ref document, limits: json.Limits(maximumBytes: 1<iec.KiB>))
  expect bytes == b"{\"id\":\"340282366920938463463374607431768211455\",\"device\":\"tablet-7\",\"role\":\"guest\",\"remainingNanoseconds\":\"30000000000\"}"
}

test "wifi document failures do not echo voucher text" {
  var loginBytes: Bytes = b"{\"device\":\"tablet-7\",\"voucher\":\"secret-voucher\"}"
  let login = try json.decode<LoginDocument>(ref loginBytes, limits: json.Limits(maximumBytes: 1<iec.KiB>))
  var longVoucher = String()
  for _ in 0..<257 { longVoucher.append("x") }
  do {
    let _ = try LoginDocument(
      device: login.device,
      voucher: longVoucher,
    ).loginRequest()
    panic("oversized voucher was accepted")
  } catch .invalidRefinement(.voucher) {}
}
