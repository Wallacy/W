// Serial-turn dining room with bounded admission and observable applause.

import { IdempotencyKey, Payment } from restaurant.billing
import { Dish, Money, Receipt } from restaurant.domain
import { ApplauseLevel, applauseThreshold } from restaurant.units

export type TableId = u32

export enum TableState {
  available
  reserved
  serving
}

export struct Table {
  id: TableId
  seats: u16
  state: TableState
}

export enum DiningRoomError: Error {
  full
  paymentIncomplete
  audienceUnavailable
  insufficientApplause(found: ApplauseLevel, required: ApplauseLevel)
}

export protocol AudienceApi {
  async fn measure(dish: ref Dish): ApplauseLevel throws DiningRoomError
}

export protocol DiningRoomApi {
  async fn serve(dish: take Dish, payment: ref Payment): Receipt throws DiningRoomError
}

fn paymentCanServe(payment: ref Payment): Bool {
  return payment.state == .captured
}

export service PrismDiningRoom as DiningRoomApi {
  audience: ServiceRef<AudienceApi>
  var tables: Map<TableId, Table> = Map()

  mut async fn serve(dish: take Dish, payment: ref Payment): Receipt throws DiningRoomError {
    guard paymentCanServe(payment) else throw .paymentIncomplete
    guard let tableId = tables.first(where: (entry) => entry.value.state == .available)?.key else throw .full

    tables[tableId].state = .serving
    defer {
      tables[tableId].state = .available
    }

    async on .device let measuredApplause = audience.measure(dish)
    let applause = try await measuredApplause
    guard applause >= applauseThreshold else {
      throw .insufficientApplause(found: applause, required: applauseThreshold)
    }

    return Receipt(orderId: dish.orderId, total: payment.amount, traceId: Trace.current.id)
  }
}

test "an authorization cannot enter the dining room" for paymentCanServe {
  let payment = Payment(
    id: 10,
    key: try IdempotencyKey("order:10:capture"),
    amount: Money(minorUnits: 4_242, currency: .cr),
    state: .authorized,
  )

  expect !paymentCanServe(payment)
}
