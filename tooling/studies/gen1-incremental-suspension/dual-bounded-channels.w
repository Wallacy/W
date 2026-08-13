module gen1.dualBoundedChannels

import * from std.io
import streaming from std.stream

export struct DialogueChannels {
  requests: Channel<String><.send>
  replies: Channel<String><.receive>
}

export async fn observeSuspension(
  channels: take DialogueChannels,
  answer: take String,
): String? throws ChannelSendError<String><[.closed]> {
  try await channels.requests.send(take answer)
  let reply = await channels.replies.receive()
  return reply
}

export async fn observeDialogue(
  channels: take DialogueChannels,
  answer: take String,
): String? throws ChannelSendError<String><[.closed]> {
  try await channels.requests.send(take answer)
  return await channels.replies.receive()
}
