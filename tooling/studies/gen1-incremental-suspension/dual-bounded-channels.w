module gen1DualBoundedChannels

import * from std.io
import streaming from std.stream

export struct DialogueChannels {
  requests: Channel<send: String>
  replies: Channel<receive: String>
}

export async fn observeSuspension(
  _ channels: take DialogueChannels,
  _ answer: take String,
): String? throws ChannelSendError<String><[.closed]> {
  try await channels.requests.send(take answer)
  let reply = await channels.replies.receive()
  return reply
}

export async fn observeDialogue(
  _ channels: take DialogueChannels,
  _ answer: take String,
): String? throws ChannelSendError<String><[.closed]> {
  try await channels.requests.send(take answer)
  return await channels.replies.receive()
}
