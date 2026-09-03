module gen1BuilderHelper

export struct DialogueSession {
  requestsOut: Channel<send: String>
  requestsIn: Channel<receive: String>
  repliesOut: Channel<send: String>
  repliesIn: Channel<receive: String>
}

export fn makeDialogue(): DialogueSession {
  let (requestsOut, requestsIn) = Channel<String>.open(capacity: 1)
  let (repliesOut, repliesIn) = Channel<String>.open(capacity: 1)
  return DialogueSession(
    requestsOut: take requestsOut,
    requestsIn: take requestsIn,
    repliesOut: take repliesOut,
    repliesIn: take repliesIn,
  )
}
