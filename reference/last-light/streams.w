// Bounded streams and channels at the Last Light restaurant.

import * from std.io
import streaming from std.stream
import {
  BrigadeError,
  MixingJob,
  MixingResult,
  inferredSuspension,
} from execution

export enum QueueError: Error {
  admission(ChannelClosed)
  send(ChannelSendError<Order><[.closed]>)
}

export async fn submitOrder(
  output: Channel<Order><.send>,
  order: take Order,
): () throws ChannelSendError<Order><[.closed]> {
  try await output.send(take order)
}

export fn trySubmitOrder(
  output: ref Channel<Order><.send>,
  order: take Order,
): Order? {
  do {
    try output.trySend(take order)
    return .none
  } catch .full(let returnedOrder) {
    return .some(returnedOrder)
  } catch .closed(let returnedOrder) {
    return .some(returnedOrder)
  }
}

export async fn submitAfterAdmission(
  output: ref Channel<Order><.send>,
  order: take Order,
): () throws QueueError {
  let permit = try await output.reserve()
  try (take permit).send(take order)
}

export async fn acceptOrders(
  input: take Channel<Order><.receive>,
): Array<Order> {
  var inbox = take input
  var accepted: Array<Order> = []

  for await order in inbox {
    accepted.append(take order)
  }

  return accepted
}

export async fn inspectMenuLines<E: Error>(
  source: take some Stream<view String, E>,
): usize throws E {
  var lines = take source
  var nonempty = 0_usize

  for try await line in lines {
    if !line.isEmpty { nonempty += 1 }
  }

  return nonempty
}

export enum ReadableBytePumpOutcome<
  ReadFailure: Error,
  WriteFailure: Error,
> {
  complete(committed: usize)
  readFailed(cause: ReadFailure, committed: usize)
  writeFailed(
    cause: WriteAllError<WriteFailure>,
    sourceAdvanced: usize,
    committed: usize,
    payload: Bytes,
  )
}

export async fn pumpReadableBytes<
  ReadFailure: Error,
  WriteFailure: Error,
  Destination: ByteSink<WriteFailure>,
>(
  source: take streaming.ReadableStream<Bytes, ReadFailure>,
  destination: take Destination,
  maximumChunkBytes: usize<(1...)>,
): ReadableBytePumpOutcome<ReadFailure, WriteFailure> {
  var input = take source
  var output = take destination
  var scratch = Bytes()
  scratch.reserve(minimumCapacity: maximumChunkBytes)
  var committed: usize = 0

  while true {
    let step: ReadStep

    do {
      step = try await input.read(
        appendTo: inout scratch,
        maximum: maximumChunkBytes,
      )
    } catch error {
      return .readFailed(cause: error, committed: committed)
    }

    switch step {
      case .data(let count):
        let chunk: view Bytes = scratch[0..<count]

        do {
          try await output.writeAll(chunk)
          committed += count
          scratch.clear()
        } catch error {
          return .writeFailed(
            cause: error,
            sourceAdvanced: count,
            committed: committed,
            payload: take scratch,
          )
        }
      case .end:
        return .complete(committed: committed)
    }
  }
}

export async fn mirrorReadableBytes<
  ReadFailure: Error & Duplicable,
  LeftFailure: Error,
  RightFailure: Error,
  LeftDestination: ByteSink<LeftFailure>,
  RightDestination: ByteSink<RightFailure>,
>(
  source: take streaming.ReadableStream<Bytes, ReadFailure>,
  leftDestination: take LeftDestination,
  rightDestination: take RightDestination,
  maximumBufferedBytes: usize<(1...)>,
  maximumChunkBytes: usize<(1...)>,
): (
  ReadableBytePumpOutcome<ReadFailure, LeftFailure>,
  ReadableBytePumpOutcome<ReadFailure, RightFailure>,
) throws streaming.ReadableStreamUseError {
  let (left, right) = try (take source).tee(
    maximumBufferedBytes: maximumBufferedBytes,
  )

  async let leftPump = pumpReadableBytes(
    take left,
    destination: take leftDestination,
    maximumChunkBytes: maximumChunkBytes,
  )
  async let rightPump = pumpReadableBytes(
    take right,
    destination: take rightDestination,
    maximumChunkBytes: maximumChunkBytes,
  )

  return await (leftPump, rightPump)
}

export async fn serveOneByOne<S: Stream<Order, Never>>(
  source: take S,
): usize {
  var orders = take source
  var served = 0_usize

  while let order = await orders.next() {
    serve(take order)
    served += 1
  }

  return served
}

// Work distribution remains a structured Stream adapter. It does not make the
// receive endpoint MPMC and does not create a second queue abstraction.
export fn mixOrderStream(
  source: take some Stream<MixingJob, BrigadeError>,
  limit: usize<(1...256)>,
): some Stream<MixingResult, BrigadeError> {
  return (take source).parallelMap<.compute>(
    limit: limit,
    ordering: .completion,
    using: inferredSuspension,
  )
}

export async fn runBoundedOrderWindow(
  first: take Order,
  second: take Order,
): Array<Order> throws ChannelSendError<Order><[.closed]> {
  let (output, input) = Channel<Order>.open(capacity: 1)

  async let firstSend = submitOrder(copy output, take first)
  async let secondSend = submitOrder(copy output, take second)
  let _ = take output

  let accepted = await acceptOrders(take input)
  let _ = try await firstSend
  let _ = try await secondSend
  return accepted
}

export async fn handOffAtRendezvous(
  order: take Order,
): Order throws ChannelSendError<Order><[.closed]> {
  let (output, input) = Channel<Order>.open(capacity: 0)

  async let received = (take input).receive()
  try await output.send(take order)

  guard let receivedOrder = await received else {
    panic("rendezvous ended before its accepted order")
  }
  return receivedOrder
}

export async fn closeAfterReservedOrder(
  order: take Order,
): Order throws QueueError {
  let (output, input) = Channel<Order>.open(capacity: 1)
  let permit = try await output.reserve()
  input.close()
  try (take permit).send(take order)

  guard let receivedOrder = await input.receive() else {
    panic("graceful close revoked an accepted permit")
  }
  return receivedOrder
}

export async fn recoverAfterReceiverAbort(
  order: take Order,
): Order {
  let (output, input) = Channel<Order>.open(capacity: 1)
  let _ = take input

  do {
    try await output.send(take order)
  } catch .closed(let returnedOrder) {
    return returnedOrder
  }

  panic("an aborted receiver accepted an order")
}

// Compile-fail assays:
// let _ = Channel<view String>.open(capacity: 1) // A view is not transferable.
// async let left = input.receive()          // The receiver is not shareable.
// async let right = input.receive()
// output.close()                             // Senders cannot close globally.
// borrowedLines.append(line)                // The view would cross the next iteration.
// let next = try await lines.next()         // Rejected when `line` is used again later.
// let copy = copy readable                   // ReadableStream is move-only.
// let _ = readable.tee(maximumBufferedItems: 8)
// // Rejected when Item or Failure is not Duplicable.
// do {
//   try await (take readable).cancel()
// } catch error {
//   inspect(readable)                    // Rejected: Failure does not restore owner.
// }

// Provider-gated runtime assays; these do not execute while
// std.readable-stream@1 is missing:
// - injected cancel failure commits inert state and cleanup remains exactly once.
// - maximumBufferedItems limits item lag only, not bytes in duplicated graphs.
// - maximumBufferedBytes bounds byte lag, and left drop does not cancel right.
