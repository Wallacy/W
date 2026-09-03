// R1 Last Light Python-transform study variant.

struct ArrivalTicket {
  orderId: u64
  priority: u8
}

fn urgentOrderIds(
  _ tickets: ref Array<ArrivalTicket>,
  limit: usize,
): Array<u64> {
  return tickets.lazy
    .filter((ticket) => ticket.priority == 1)
    .map((ticket) => ticket.orderId)
    .take(limit)
    .collect()
}

test "lazy pipeline preserves bounded urgent selection" for urgentOrderIds {
  let tickets = [
    ArrivalTicket(orderId: 7, priority: 1),
    ArrivalTicket(orderId: 9, priority: 1),
    ArrivalTicket(orderId: 11, priority: 0),
  ]

  expect urgentOrderIds(tickets, limit: 1) == [7]
}
