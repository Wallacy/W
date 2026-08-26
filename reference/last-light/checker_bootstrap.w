// closed-single-source Restaurant checker witness.

module checker_bootstrap

export enum DiningRoomState {
  open
  closed
}

export struct OrderAdmission {
  state: DiningRoomState
  canServe: Bool
}

export fn canAcceptOrder(open: Bool): Bool {
  return open
}
