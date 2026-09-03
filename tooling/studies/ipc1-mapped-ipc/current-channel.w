// IPC1 current baseline: a bounded typed service channel.

module ipc1CurrentChannel

import * from std.io

export struct Ipc1MappedIpcChannel {
  let requests: Channel<send: Bytes>
  let replies: Channel<receive: Bytes>
}

export async fn Ipc1MappedIpc(
  _ channel: take Ipc1MappedIpcChannel,
  _ payload: take Bytes,
): Bytes throws ChannelSendError<Bytes><[.closed]> {
  try await channel.requests.send(take payload)
  return await channel.replies.receive()
}
