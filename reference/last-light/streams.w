// Bounded streams and channels at the Last Light restaurant.

import * from std.io
import streaming from std.stream
import {
  Course,
  Guest,
  GuestCount,
  GuestName,
  Order,
} from domain
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
  output: Channel<send: Order>,
  order: take Order,
): () throws ChannelSendError<Order><[.closed]> {
  try await output.send(value: take order)
}

export fn trySubmitOrder(
  output: ref Channel<send: Order>,
  order: take Order,
): Order? {
  do {
    try output.trySend(value: take order)
    return .none
  } catch .full(let returnedOrder) {
    return .some(returnedOrder)
  } catch .closed(let returnedOrder) {
    return .some(returnedOrder)
  }
}

export async fn submitAfterAdmission(
  output: ref Channel<send: Order>,
  order: take Order,
): () throws QueueError {
  let permit = try await output.reserve()
  try (take permit).send(value: take order)
}

export async fn acceptOrders(
  input: take Channel<receive: Order>,
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
          scratch.reset()
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

  let leftPump = async pumpReadableBytes(
    source: take left,
    destination: take leftDestination,
    maximumChunkBytes: maximumChunkBytes,
  )
  let rightPump = async pumpReadableBytes(
    source: take right,
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

  let firstSend = async submitOrder(output: copy output, order: take first)
  let secondSend = async submitOrder(output: copy output, order: take second)
  let _ = take output

  let accepted = await acceptOrders(input: take input)
  let _ = try await firstSend
  let _ = try await secondSend
  return accepted
}

export async fn handOffAtRendezvous(
  order: take Order,
): Order throws ChannelSendError<Order><[.closed]> {
  let (output, input) = Channel<Order>.open(capacity: 0)

  let received = async (take input).receive()
  try await output.send(value: take order)

  guard let receivedOrder = await received else {
    panic("rendezvous ended before its accepted order")
  }
  return receivedOrder
}

export async fn closeAfterReservedOrder(
  order: take Order,
): Order throws QueueError {
  let (output, initialInput) = Channel<Order>.open(capacity: 1)
  var input = take initialInput
  let permit = try await output.reserve()
  input.close()
  try (take permit).send(value: take order)

  guard let receivedOrder = await input.receive() else {
    panic("graceful close revoked an accepted permit")
  }
  return receivedOrder
}

export async fn recoverCanceledRendezvousPermit(
  order: take Order,
): Order throws QueueError {
  let (output, initialInput) = Channel<Order>.open(capacity: 0)
  var input = take initialInput
  let pending = async input.receive()
  let permit = try await output.reserve()

  pending#cancel(reason: .userRequest)
  switch await (take pending)#outcome() {
    case .canceled(_): ()
    case .success(_): panic("canceled receive produced an item")
    case .error(_): panic("nonthrowing receive produced an error")
  }

  do {
    try (take permit).send(value: take order)
    panic("a revoked rendezvous permit accepted an item")
  } catch .closed(let returnedOrder) {
    input.close()
    guard let _ = await input.receive() else {
      return returnedOrder
    }
    panic("closing an open channel exposed a stale rendezvous item")
  }
}

test "canceled rendezvous returns the original owner" for
recoverCanceledRendezvousPermit {
  let original = Order(
    id: 1545,
    guest: Guest(id: 1, name: try GuestName("Rendezvous")),
    guests: try GuestCount(1),
    course: .quietSalad,
    notes: .none,
  )
  let returned = try await recoverCanceledRendezvousPermit(order: take original)
  expect returned.id == 1545
  expect returned.guest.id == 1
}

export async fn recoverAfterReceiverAbort(
  order: take Order,
): Order {
  let (output, input) = Channel<Order>.open(capacity: 1)
  let _ = take input

  do {
    try await output.send(value: take order)
  } catch .closed(let returnedOrder) {
    return returnedOrder
  }

  panic("an aborted receiver accepted an order")
}

// Compile-fail assays:
// let _ = Channel<view String>.open(capacity: 1) // A view is not transferable.
// var input = take input0
// let left = async input.receive()          // The exclusive cursor is pending.
// let right = async input.receive()         // Overlapping receiver loan.
// input.close()                             // Requires the loan to be joined.
// output.close()                             // Senders cannot close globally.
// borrowedLines.append(line)                // The view would cross the next iteration.
// let next = try await lines.next()         // Rejected when `line` is used again later.
// let copy = copy readable                   // ReadableStream is move-only.
// let _ = readable.tee(items: 8)
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
