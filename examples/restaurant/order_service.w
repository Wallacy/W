// W Working Draft — pseudocódigo pedagógico, não executável.
// ServiceHost/ServiceRef são APIs candidatas; `service` não é keyword adotada.

import { ServiceHost, ServiceRef, StartError } from std.service
import { DishSummary, OrderId, Receipt } from restaurant.domain

export enum OrderStage {
  accepted
  preparing
  baking
  finishing
  completed
  cancelled
}

export enum OrderError: Error {
  overloaded
  invalidTransition(from: OrderStage, to: OrderStage)
  cancelled
  unavailable
  start(StartError)
}

export protocol OrderApi {
  fn move(to stage: OrderStage): Void async throws OrderError
  fn complete(with summary: DishSummary): Receipt async throws OrderError
  fn requestCancellation(): Void async throws OrderError
}

fn canMove(from current: OrderStage, to next: OrderStage): Bool {
  switch current {
    case .accepted:
      return next == .preparing || next == .cancelled
    case .preparing:
      return next == .baking || next == .finishing || next == .cancelled
    case .baking:
      return next == .finishing || next == .cancelled
    case .finishing:
      return next == .completed || next == .cancelled
    case .completed:
      return false
    case .cancelled:
      return false
  }
}

// Um handler externo por vez é a policy candidata do primeiro protótipo.
object OrderState {
  id: OrderId
  var stage: OrderStage

  mut fn move(to next: OrderStage): Void async throws OrderError {
    guard canMove(from: stage, to: next) else {
      throw .invalidTransition(from: stage, to: next)
    }
    stage = next
  }

  mut fn complete(with summary: DishSummary): Receipt async throws OrderError {
    try await move(to: .completed)
    return Receipt(orderId: id, dish: summary)
  }

  mut fn requestCancellation(): Void async throws OrderError {
    if stage == .cancelled {
      return
    }
    try await move(to: .cancelled)
  }
}

export fn openOrder(
  id: OrderId,
  on host: inout ServiceHost,
): ServiceRef<OrderApi> async throws OrderError {
  // .key, .serial e .bounded são configuração candidata, não gramática.
  do {
    return try await host.startService(
      OrderState(id: id, stage: .accepted),
      as: OrderApi,
      scope: .key(id),
      policy: .serial,
      mailbox: .bounded(items: 32, bytes: 64 KiB),
    )
  } catch let error {
    throw .start(error)
  }
}
