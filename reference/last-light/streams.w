// Bounded streams and channels at the Last Light restaurant.

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

// Compile-fail assays:
// let _ = Channel<view String>.open(capacity: 1) // A view is not transferable.
// async let left = input.receive()          // The receiver is not shareable.
// async let right = input.receive()
// borrowedLines.append(line)                // The view would cross the next iteration.
// let next = try await lines.next()         // Rejected when `line` is used again later.
