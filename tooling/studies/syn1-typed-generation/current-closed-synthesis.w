import * from std

export struct ReservationKey: Hashable & Reflectable {
  table: u32
  sequence: u64
}

fn showReservation(key: ReservationKey): String {
  return "reservation"
}

entry(showReservation)
