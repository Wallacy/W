module gen1BuilderHelper

export struct DialogueSession {
  let requestsOut: Channel<send: String>
  let requestsIn: Channel<receive: String>
  let repliesOut: Channel<send: String>
  let repliesIn: Channel<receive: String>
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
