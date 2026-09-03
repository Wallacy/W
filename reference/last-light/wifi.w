// Captive portal and session control for restaurant Wi-Fi.

import * from std.crypto
import * from std.http
import json from std.json
import * from std.storage
import * from std.time

export type DeviceId = String<(.graphemes.count <= 128)>
export type WifiVoucher = String<(.bytes.count <= 256)>
export type SessionId = u128

export enum WifiRole {
  guest
  crew
  observatory
}

export struct LoginRequest {
  let device: DeviceId
  let voucher: WifiVoucher
}

export struct WifiSession {
  let id: SessionId
  let device: DeviceId
  let role: WifiRole
  let remaining: Duration<(0...)>
}

export enum WifiError: Error {
  decode(BodyDecodeError<json.DecodeError>)
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
