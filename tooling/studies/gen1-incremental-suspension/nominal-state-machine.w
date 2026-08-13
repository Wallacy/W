module gen1.nominalStateMachine

export enum DialogueState {
  waiting
  offered
  finished
}

export enum DialogueFailure: Error {
  closed
}

export struct DialogueSession {
  state: DialogueState
  turns: usize
}

export enum DialogueStep {
  request
  reply
  done
}

export fn observeSuspension(
  session: inout DialogueSession,
  answer: take String,
): DialogueStep throws DialogueFailure {
  if session.state == .finished {
    throw DialogueFailure.closed
  }
  session.turns = session.turns + 1
  session.state = .offered
  return .reply
}

export fn closeDialogue(session: take DialogueSession): DialogueState {
  return session.state
}

export enum CursorState {
  ready
  holding
  finished
}

export struct MenuCursor {
  state: CursorState
  retained: usize
}

export fn observeTraversal(
  cursor: inout MenuCursor,
  line: take String,
): DialogueStep throws DialogueFailure {
  if cursor.state == .finished {
    throw DialogueFailure.closed
  }
  cursor.state = .holding
  cursor.retained = cursor.retained + line.count
  cursor.state = .ready
  return .reply
}

export fn observeDelegation(
  parent: inout DialogueSession,
  child: take DialogueSession,
): DialogueState throws DialogueFailure {
  if parent.state == .finished {
    throw DialogueFailure.closed
  }
  parent.state = child.state
  return parent.state
}
