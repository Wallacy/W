struct Ticket {
  number: i32
}

fn chooseTicket(enabled: Bool): Ticket? {
  if enabled {
    return .some(Ticket(number: 42))
  }
  return .none
}
