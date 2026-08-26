# SVC0 — directional service streams

SVC0 tests whether W needs separate RPC stream types or syntax. The selected
contract uses the existing `Stream` carrier directly in service parameter and
result positions. Position determines direction. A direct input is transferred
with `take`; a direct output remains opaque with `some`.

The four accepted shapes are unary, server-streaming, client-streaming, and
bidirectional. This study rejects implicit `Channel`, `stream fn`, published
`any Stream`, nested stream edges, borrowed or non-wire items, missing boundary
failure, a service open without `await`, and settlement without drain.

`Channel<T><.receive>` already conforms to `Stream<T, Never>`. A caller that
needs a push producer can therefore open a bounded channel explicitly, keep the
send endpoint, and transfer the receive endpoint as the service input stream.
No service declaration invents capacity or a hidden queue.

The host oracle derives outcomes from facts. It does not execute W or provide a
compiler, runtime pump, provider, cross-route fault test, benchmark, or human or
model usability result.
