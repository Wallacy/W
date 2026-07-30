// Captive portal and session control for restaurant Wi-Fi.

import std.crypto
import std.http
import std.storage
import std.time

export type DeviceId = String<(.graphemes.count <= 128)>
export type SessionId = u128

export enum WifiRole {
  guest
  crew
  observatory
}

export struct LoginRequest {
  device: DeviceId
  voucher: String<(.bytes.count <= 256)>
}

export struct WifiSession {
  id: SessionId
  device: DeviceId
  role: WifiRole
  expiresAt: Instant
}

export enum WifiError: Error {
  decode(DecodeError)
  response(ResponseError)
  service(ServiceFailure)
  invalidVoucher
  rateLimited(retryAfter: Duration)
  expired
  storage(StorageError)
  crypto(CryptoError)
}

export protocol WifiSessionApi {
  async fn login(request: take LoginRequest): WifiSession throws WifiError
  async fn inspect(id: SessionId): WifiSession throws WifiError
  async fn revoke(id: SessionId): () throws WifiError
}

export const wifiSessions = ServiceBinding<WifiSessionApi>(name: "wifi-sessions")

package async fn fetchWifi(
  request: http.Request,
  ctx: http.Context,
): http.Response throws WifiError {
  let sessions = try await ctx.services.get(wifiSessions)

  return switch (request.method, request.path) {
    case (.post, "/login"):
      let input = try request.json.decode<LoginRequest>(maximumBytes: 4<KiB>)
      let session = try await sessions.login(take input)
      try http.Response.json(session)
    case (.post, "/logout"):
      let id = try request.json.decode<SessionId>(maximumBytes: 1<KiB>)
      try await sessions.revoke(id)
      http.Response(status: .noContent)
    case (_, _):
      http.Response(status: .notFound)
  }
}

test "roles remain a closed authority set" {
  let role: WifiRole = .guest
  expect role in (.guest, .crew, .observatory)
}
