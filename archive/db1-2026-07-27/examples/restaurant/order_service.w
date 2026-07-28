// W Working Draft — pseudocódigo pedagógico, não executável.
// `service` baixa para object + descriptor; ServiceHost escolhe scope/placement.

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
  async fn move(to stage: OrderStage): Void throws OrderError
  async fn complete(with summary: DishSummary): Receipt throws OrderError
  async fn requestCancellation(): Void throws OrderError
}

fn canMove(from current: OrderStage, to next: OrderStage): Bool {
  switch current {
    case .accepted:
      return next in (.preparing, .cancelled)
    case .preparing:
      return next in (.baking, .finishing, .cancelled)
    case .baking:
      return next in (.finishing, .cancelled)
    case .finishing:
      return next in (.completed, .cancelled)
    case .completed:
      return false
    case .cancelled:
      return false
  }
}

// Um handler externo por vez é a policy candidata do primeiro protótipo.
service OrderState as OrderApi {
  id: OrderId
  var stage: OrderStage

  mut async fn move(to next: OrderStage): Void throws OrderError {
    guard canMove(from: stage, to: next) else {
      throw .invalidTransition(from: stage, to: next)
    }
    stage = next
  }

  mut async fn complete(with summary: DishSummary): Receipt throws OrderError {
    try await move(to: .completed)
    return Receipt(orderId: id, dish: summary)
  }

  mut async fn requestCancellation(): Void throws OrderError {
    if stage == .cancelled {
      return
    }
    try await move(to: .cancelled)
  }
}

export async fn openOrder(id: OrderId, on host: inout ServiceHost): ServiceRef<OrderApi> throws OrderError {
  // .key, .serial e .bounded são configuração candidata, não gramática.
  do {
    return try await host.startService(
      OrderState(id: id, stage: .accepted),
      as: OrderApi,
      scope: .key(id),
      policy: .serial,
      mailbox: .bounded(items: 32, bytes: 64KiB),
    )
  } catch let error {
    throw .start(error)
  }
}
