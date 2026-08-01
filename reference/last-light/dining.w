// Serial-turn dining room with bounded admission and observable applause.

import { PaymentProof } from restaurant.billing
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
  service(ServiceFailure)
}

export protocol AudienceApi {
  async fn measure(dish: take Dish): ApplauseLevel throws DiningRoomError
}

export protocol DiningRoomApi {
  async fn serve(dish: take Dish, payment: PaymentProof): Receipt throws DiningRoomError
  async fn serve(
    at tableId: TableId,
    dish: take Dish,
    payment: PaymentProof,
  ): Receipt throws DiningRoomError
}

package import service diningRoom: DiningRoomApi

fn defaultTables(): Map<TableId, Table> {
  return [
    1: Table(id: 1, seats: 2, state: .available),
    2: Table(id: 2, seats: 4, state: .available),
    3: Table(id: 3, seats: 8, state: .available),
    42: Table(id: 42, seats: 42, state: .available),
  ]
}

export service PrismDiningRoom as DiningRoomApi {
  audience: ServiceRef<AudienceApi>
  var tables: Map<TableId, Table> = defaultTables()

  init(audience: ServiceRef<AudienceApi>) {
    self.audience = audience
  }

  mut fn setTableState(tableId: TableId, state: TableState) {
    guard let inout table = tables[tableId] else panic("reserved table disappeared")
    table.state = state
  }

  mut async fn serve(dish: take Dish, payment: PaymentProof): Receipt throws DiningRoomError {
    guard payment.canServe else throw .paymentIncomplete
    guard let tableId = tables.first(where: (entry) => entry.value.state == .available)?.key else throw .full

    return try await serve(at: tableId, dish: take dish, payment: payment)
  }

  mut async fn serve(
    at tableId: TableId,
    dish: take Dish,
    payment: PaymentProof,
  ): Receipt throws DiningRoomError {
    guard payment.canServe else throw .paymentIncomplete
    guard tables[tableId]?.state == .some(.available) else throw .full

    setTableState(tableId, state: .serving)
    defer {
      setTableState(tableId, state: .available)
    }

    let orderId = dish.orderId
    async let measuredApplause = audience.measure(take dish)
    let applause = try await measuredApplause
    guard applause >= applauseThreshold else {
      throw .insufficientApplause(found: applause, required: applauseThreshold)
    }

    return Receipt(orderId: orderId, total: payment.amount, traceId: Trace.current.id)
  }
}

test "an authorization cannot enter the dining room" {
  let payment = PaymentProof(
    paymentId: 10,
    amount: Money(minorUnits: 4_242, currency: .cr),
    state: .authorized,
  )

  expect !payment.canServe
}
