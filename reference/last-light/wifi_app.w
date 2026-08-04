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
  return switch (request.method, request.url.pathname) {
    case (.post, "/login"):
      let input = try await (take request).json<LoginRequest>(maximumBytes: 4<KiB>)
      let session = try await wifiSessions.login(take input)
      try http.Response.json(session)
    case (.post, "/logout"):
      let id = try await (take request).json<SessionId>(maximumBytes: 1<KiB>)
      try await wifiSessions.revoke(id)
      try http.Response(status: http.StatusCode.noContent)
    case (_, _):
      try http.Response(status: http.StatusCode.notFound)
  }
}

entry LastLightWifi(fetchWifi)
