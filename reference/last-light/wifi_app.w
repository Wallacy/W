// Dedicated HTTP component for the captive portal.

import http from std
import {
  LoginRequest,
  SessionId,
  WifiError,
  WifiSessionApi,
} from wifi
import service { WifiSessionApi as wifiSessions } from wifi

async fn fetchWifi(
  request: take http.Request,
  ctx: http.Context,
): http.Response throws WifiError {
  return switch (request.method, request.path) {
    case (.post, "/login"):
      let input = try await request.decodeJson<LoginRequest>(maximumBytes: 4<KiB>)
      let session = try await wifiSessions.login(take input)
      try http.Response.json(session)
    case (.post, "/logout"):
      let id = try await request.decodeJson<SessionId>(maximumBytes: 1<KiB>)
      try await wifiSessions.revoke(id)
      http.Response(status: .noContent)
    case (_, _):
      http.Response(status: .notFound)
  }
}

entry LastLightWifi(fetchWifi)
