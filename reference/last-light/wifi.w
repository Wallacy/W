// Captive portal and session control for restaurant Wi-Fi.

import * from std.crypto
import * from std.http
import * from std.storage
import * from std.time

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
  remaining: Duration<(0...)>
}

export enum WifiError: Error {
  decode(BodyDecodeError<DecodeError>)
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

test "roles remain a closed authority set" {
  let role: WifiRole = .guest
  expect role in (.guest, .crew, .observatory)
}
