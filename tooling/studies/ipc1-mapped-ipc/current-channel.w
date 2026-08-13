// IPC1 current baseline: a bounded typed service channel.

module ipc1.current.channel

import * from std.io

export struct Ipc1MappedIpcChannel {
  requests: Channel<Bytes><.send>
  replies: Channel<Bytes><.receive>
}

export async fn Ipc1MappedIpc(
  channel: take Ipc1MappedIpcChannel,
  payload: take Bytes,
): Bytes throws ChannelSendError<Bytes><[.closed]> {
  try await channel.requests.send(take payload)
  return await channel.replies.receive()
}
