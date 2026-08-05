// Web abort signals for bounded Last Light telemetry.

import webAbort from std.abort
import http from std.http
import { URL } from std.url

export enum TelemetryFetchError: Error {
  unavailable
  aborted(webAbort.AbortReason)
  body(http.HttpBodyError)
}

// This protocol is the Last Light host seam for the logical std.http.fetch
// contract. The HTTP source declaration remains gated with Request/Response.
export protocol TelemetryClient {
  async fn fetch(
    endpoint: ref URL,
    signal: ref webAbort.AbortSignal,
  ): http.Response throws TelemetryFetchError
}

export fn completedEvacuation(): webAbort.AbortSignal {
  return webAbort.AbortSignal.abort(reason: .requested(.shutdown))
}

export fn retainEvacuationSignal(
  signal: ref webAbort.AbortSignal,
): webAbort.AbortSignal {
  return copy signal
}

export fn requireTelemetryPending(
  signal: ref webAbort.AbortSignal,
): () throws webAbort.AbortReason {
  try signal.throwIfAborted()
}

// The operational timeout charges the origin execution-domain timer budget.
// It is independent of creator/root cancellation, does not read wall time, and
// does not grant clock or network authority.
export async fn fetchBlackHoleTelemetry<Client: TelemetryClient>(
  client: ref Client,
  endpoint: ref URL,
  timeout: TaskTimeout,
): http.Response throws TelemetryFetchError {
  let deadline = webAbort.AbortSignal.timeout(for: timeout)
  return try await client.fetch(endpoint: endpoint, signal: deadline)
}

// A controller creates an independent Web lifetime. The host seam maps its
// reason to TelemetryFetchError.aborted in the TaskOutcome. It does not cancel
// this caller task.
export async fn supersedeBlackHoleTelemetry<Client: TelemetryClient>(
  client: ref Client,
  endpoint: ref URL,
): TaskOutcome<http.Response, TelemetryFetchError> {
  let controller = webAbort.AbortController()
  async let pending = client.fetch(
    endpoint: endpoint,
    signal: controller.signal,
  )

  controller.abort(reason: .requested(.superseded))
  return await pending.outcome()
}

// The incoming handler owns only observation. Its Request signal mirrors the
// request root and can be passed to another Web operation.
export async fn forwardIncomingTelemetry<Client: TelemetryClient>(
  client: ref Client,
  request: take http.Request,
  endpoint: ref URL,
): http.Response throws TelemetryFetchError {
  return try await client.fetch(
    endpoint: endpoint,
    signal: request.signal,
  )
}

// The rest list is lexical and bounded. An already-aborted request signal wins
// before the later timeout signal. Future concurrent aborts use one atomic
// transition without a synthetic tie order.
export fn requestOrTelemetryDeadline(
  request: ref http.Request,
  timeout: TaskTimeout,
): webAbort.AbortSignal throws webAbort.AbortSignalCombineError {
  let deadline = webAbort.AbortSignal.timeout(for: timeout)
  // Two direct arguments and two unique pending leaves both fit the fan-in.
  let requestDeadline = try webAbort.AbortSignal.any(
    maximumSources: 2,
    request.signal,
    deadline,
  )

  // The outer any flattens requestDeadline and deduplicates request.signal.
  // Its two unique pending leaves still fit maximumSources.
  return try webAbort.AbortSignal.any(
    maximumSources: 2,
    requestDeadline,
    request.signal,
  )
}

export async fn awaitEvacuation(
  signal: ref webAbort.AbortSignal,
): webAbort.AbortReason {
  let reason = await signal.wait()
  assert signal.aborted
  guard let observed = signal.reason else panic("abort reason missing")
  let _ = observed
  return reason
}

// Compile-fail assays:
// request.signal.abort()                    // A signal has no abort authority.
// let second = copy controller              // AbortController is move-only.
// webAbort.AbortSignal.abort(reason: .custom("free-form"))
// // AbortReason has no custom or dynamic case.
// export service fn send(signal: webAbort.AbortSignal)
// // AbortSignal is not WireValue and cannot cross a service payload directly.
// signal.addEventListener("abort", handler) // EventTarget is not in SDK0.

// Provider-gated runtime assays; these do not execute while
// std.abort-state@1 is missing:
// - abort racing wait never loses a wake and publishes reason before aborted.
// - wait versus task cancellation follows settlement and removes a loser once.
// - concurrent repeated abort keeps the first Copy reason.
// - controller drop alone does not abort a surviving signal.
// - timeout escapes its creator and ignores creator/root cancellation; fire or
//   last drop removes its timer exactly once.
// - any(maximumSources: 1, abortedA, abortedB) fails the direct-argument bound
//   without registration; an aborted input does not bypass that validation.
// - one pending nested any with two unique leaves fails maximumSources: 1
//   without registration.
// - passing the same pending leaf twice registers that leaf once.
// - after both bounds pass, any uses lexical order for already-aborted inputs.
// - any removes every non-owning registration without a refcount cycle or leak.
// - a slow observer receives one wake and no unbounded event buffer.
