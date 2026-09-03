import * from std

export struct ReservationKey: Hashable & Reflectable {
  let table: u32
  let sequence: u64
}

fn showReservation(key: ReservationKey): String {
  return "reservation"
}

entry(showReservation)
