// Dedicated HTTP component for the captive portal.

import iec from std
import http from std
import json from std.json
import {
  LoginRequest,
  SessionId,
  WifiError,
  WifiSession,
  WifiSessionApi,
} from wifi
import {
  LoginDocument,
  RevokeDocument,
  SessionDocument,
  WifiDocumentError,
} from wifi_documents
import { ProblemCode, problemResponse } from http_documents
import service { WifiSessionApi as wifiSessions } from wifi

enum WifiAppError: Error {
  decode(http.BodyDecodeError<json.DecodeError>)
  document(WifiDocumentError)
  service(WifiError)
  response(http.ResponseError)
}

fn wifiProblemCode(error: ref WifiAppError): ProblemCode? {
  return switch error {
    case .decode(_): .some(.malformedJson)
    case .document(_):
      .some(.invalidWifiDocument)
    case _: .none
  }
}

fn wifiProblemResponse(code: ProblemCode): http.Response throws WifiAppError {
  do {
    return try problemResponse(code: code, maximumBytes: 4<iec.KiB>)
  } catch error {
    throw .response(error)
  }
}

async fn handleWifi(
  request: take http.Request,
  ctx: http.Context,
): http.Response throws WifiAppError {
  return switch (request.method, request.url.pathname) {
    case (.post, "/login"):
      let document: LoginDocument
      do {
        document = try await (take request).json<LoginDocument>(maximumBytes: 4<iec.KiB>)
      } catch error {
        throw .decode(error)
      }
      let input: LoginRequest
      do {
        input = try (take document).loginRequest()
      } catch error {
        throw .document(error)
      }
      let session: WifiSession
      do {
        session = try await wifiSessions.login(take input)
      } catch error {
        throw .service(error)
      }
      let output = SessionDocument(session: ref session)
      let response: http.Response
      do {
        response = try http.Response.json(value: ref output, maximumBytes: 4<iec.KiB>)
      } catch error {
        throw .response(error)
      }
      response
    case (.post, "/logout"):
      let document: RevokeDocument
      do {
        document = try await (take request).json<RevokeDocument>(maximumBytes: 1<iec.KiB>)
      } catch error {
        throw .decode(error)
      }
      let id: SessionId
      do {
        id = try (take document).sessionId()
      } catch error {
        throw .document(error)
      }
      do {
        try await wifiSessions.revoke(id)
      } catch error {
        throw .service(error)
      }
      let response: http.Response
      do {
        response = try http.Response(status: http.StatusCode.noContent)
      } catch error {
        throw .response(error)
      }
      response
    case (_, _):
      let response: http.Response
      do {
        response = try http.Response(status: http.StatusCode.notFound)
      } catch error {
        throw .response(error)
      }
      response
  }
}

async fn fetchWifi(
  request: take http.Request,
  ctx: http.Context,
): http.Response throws WifiAppError {
  do {
    return try await handleWifi(take request, ctx)
  } catch error {
    switch error {
      case .decode(_):
        return try wifiProblemResponse(.malformedJson)
      case .document(_):
        return try wifiProblemResponse(.invalidWifiDocument)
      case _:
        throw error
    }
  }
}

entry LastLightWifi(fetchWifi)

test "wifi maps only documented boundary failures" {
  let malformed = WifiAppError.decode(.codec(.invalidNumber(location: json.Location(byteOffset: 0))))
  let invalid = WifiAppError.document(.invalidDecimal(.id))
  let service = WifiAppError.service(.expired)

  expect wifiProblemCode(ref malformed) == .some(.malformedJson)
  expect wifiProblemCode(ref invalid) == .some(.invalidWifiDocument)
  expect wifiProblemCode(ref service) == .none
}
