// Focused SDK0 HTTP oracle.
//
// These declarations fix the public shape. They do not claim execution while
// std.http@1 and the required carrier providers are missing.

import iec from std
import http from std.http
import json from std.json
import net from std.net

struct OraclePayload: json.Codable {
  message: String
}

fn requestConstructorOracle(): http.Request throws http.RequestError {
  let init = http.RequestInit(
    method: .post,
    body: .some(.string("hello")),
  )
  return try http.Request("https://example.test/commands", init: take init)
}

async fn consumingBodyLimitOracle(
  request: take http.Request,
): Bytes throws http.HttpBodyError {
  return try await (take request).bytes(maximumBytes: 64<iec.KiB>)
}

fn boundedCloneOracle(
  request: take http.Request,
): (http.Request, http.Request) throws http.BodyCloneError {
  return try (take request).clone(maximumBufferedBytes: 64<iec.KiB>)
}

fn responseConstructorOracle(): http.Response throws http.ResponseError {
  return try http.Response(
    "ok",
    status: http.StatusCode.ok,
  )
}

fn responseJsonOracle(): http.Response throws http.ResponseError {
  let payload = OraclePayload(message: "ok")
  return try http.Response.json(
    value: ref payload,
    maximumBytes: 4<iec.KiB>,
  )
}

// The host supplies an incoming Request with immutable Headers. W does not
// expose a positive mutating borrow. Copying the list and using RequestOverride
// is the explicit positive path for a modified request.
fn copiedHeadersRequestOverrideOracle(
  request: take http.Request,
): http.Request throws http.RequestError {
  let copied = http.Headers(copying: request.headers)
  let override = http.RequestOverride(headers: .some(take copied))
  return try http.Request(take request, override: take override)
}

async fn serveSignatureOracle<Failure: Error>(
  at address: net.ListenAddress,
  using network: ref net.Network,
  limits: http.ServerLimits,
  handler: some async fn(
    take http.Request,
    http.Context,
  ): http.Response throws Failure,
): () throws http.ServerError {
  return try await http.serve(
    at: address,
    using: network,
    limits: limits,
    handler: handler,
  )
}

// Provider-gated runtime cases:
// - a body read over 64 KiB returns limitExceeded and consumes the owner;
// - a second read is rejected as alreadyUsed;
// - bounded clone applies tee backpressure at 64 KiB;
// - Response.json encodes OraclePayload and adds application/json only when
//   the header is absent;
// - direct mutation of incoming Headers is rejected by the W borrow checker;
//   the immutable adapter guard is a defense-in-depth check covered internally
//   by std/http, while this oracle proves the copying + override path;
// - serve admits before task creation and drains accepted roots on cancellation.
