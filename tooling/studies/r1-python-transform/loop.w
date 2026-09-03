// R1 Last Light Python-transform study variant.

struct ArrivalTicket {
  orderId: u64
  priority: u8
}

fn urgentOrderIds(
  _ tickets: ref Array<ArrivalTicket>,
  limit: usize,
): Array<u64> {
  var urgent: Array<u64> = []
  if limit == 0 { return urgent }

  for ref ticket in tickets {
    if ticket.priority != 1 { continue }
    urgent.append(ticket.orderId)
    if urgent.count == limit { break }
  }

  return urgent
}

test "explicit loop preserves bounded urgent selection" for urgentOrderIds {
  let tickets = [
    ArrivalTicket(orderId: 7, priority: 1),
    ArrivalTicket(orderId: 9, priority: 1),
    ArrivalTicket(orderId: 11, priority: 0),
  ]

  expect urgentOrderIds(tickets, limit: 1) == [7]
}
