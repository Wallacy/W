module gen1BuilderHelper

export struct DialogueSession {
  requestsOut: Channel<String><.send>
  requestsIn: Channel<String><.receive>
  repliesOut: Channel<String><.send>
  repliesIn: Channel<String><.receive>
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
