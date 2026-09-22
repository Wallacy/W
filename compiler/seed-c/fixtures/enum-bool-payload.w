enum ServiceState {
  offline
  flags(open: Bool, staffed: Bool, stocked: Bool, licensed: Bool)
  charge(amount: i64)
  checked(open: Bool, amount: i64)
}

fn flags(open: Bool): ServiceState {
  return .flags(licensed: true, open: open, staffed: false, stocked: true)
}

fn isOpen(state: ServiceState): Bool {
  return switch state {
    case .checked(open: let allowed, amount: _): allowed
    case .charge(amount: _): false
    case .offline: false
    case .flags(open: let allowed, ...): allowed
  }
}

fn amount(state: ServiceState): i64 {
  return switch state {
    case .charge(amount: let value): value
    case .checked(amount: let value, open: _): value
    case .offline: 0
    case .flags(open: _, staffed: _, stocked: _, licensed: _): 0
  }
}

fn isLicensed(state: ServiceState): Bool {
  return switch state {
    case .flags(licensed: let valid, ...): valid
    case .offline: false
    case .charge(amount: _): false
    case .checked(open: _, amount: _): false
  }
}

entry {
  let open = flags(open: true)
  let closed = flags(open: false)
  let charge: ServiceState = .charge(amount: 17)
  let checked: ServiceState = .checked(amount: 31, open: true)
  let openResult = isOpen(state: open)
  let closedResult = isOpen(state: closed)
  let chargeResult = isOpen(state: charge)
  let checkedResult = isOpen(state: checked)
  let chargeAmount = amount(state: charge)
  let checkedAmount = amount(state: checked)
  let licensed = isLicensed(state: closed)
  print("States ${openResult}/${closedResult}/${chargeResult}/${checkedResult}; charges ${chargeAmount}/${checkedAmount}; licensed ${licensed}")
}
