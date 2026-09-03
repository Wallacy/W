module gen1NominalStateMachine

export enum DialogueState {
  waiting
  offered
  finished
}

export enum DialogueFailure: Error {
  closed
}

export struct DialogueSession {
  let state: DialogueState
  let turns: usize
}

export enum DialogueStep {
  request
  reply
  done
}

export fn observeSuspension(
  _ session: inout DialogueSession,
  _ answer: take String,
): DialogueStep throws DialogueFailure {
  if session.state == .finished {
    throw DialogueFailure.closed
  }
  session.turns = session.turns + 1
  session.state = .offered
  return .reply
}

export fn closeDialogue(_ session: take DialogueSession): DialogueState {
  return session.state
}

export enum CursorState {
  ready
  holding
  finished
}

export struct MenuCursor {
  let state: CursorState
  let retained: usize
}

export fn observeTraversal(
  _ cursor: inout MenuCursor,
  _ line: take String,
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
  _ parent: inout DialogueSession,
  _ child: take DialogueSession,
): DialogueState throws DialogueFailure {
  if parent.state == .finished {
    throw DialogueFailure.closed
  }
  parent.state = child.state
  return parent.state
}
