/// Closed enum subsets preserve facts across public interfaces.
import { ServiceStage } from domain

export alias WorkStage =
  ServiceStage<[.reserving, .preparing, .serving]>

export alias ActiveStage =
  ServiceStage<[.accepted, .reserving, .preparing, .serving]>

export alias StageAfterAccepted =
  ServiceStage<[.reserving, .cancelled]>

export alias CancellableStage =
  ServiceStage<[.accepted, .reserving, .preparing]>

export alias TerminalStage =
  ServiceStage<[.completed, .cancelled]>

export enum KitchenOutcome<T> {
  ready(T)
  delayed(u32)
  cancelled
}

export alias ContinuingOutcome<T> =
  KitchenOutcome<T><[.ready, .delayed]>

export alias ReadyOutcome<T> =
  KitchenOutcome<T><[.ready]>

export enum ServiceFault: Error {
  ingredientsMissing(String)
  delayed(u32)
  universeEnded
}

export alias RecoverableServiceFault =
  ServiceFault<[.ingredientsMissing, .delayed]>

export enum OvenReading {
  stable(i32)
  warming(current: i32, target: i32)
  failed(String)
}

export alias UsableOvenReading =
  OvenReading<[.stable, .warming]>

export struct CancelRequest {
  let stage: CancellableStage
}

export fn nextWorkStage(inventoryReady: Bool): WorkStage {
  return switch inventoryReady {
    case true: .preparing
    case false: .reserving
  }
}

export fn routeAcceptedOrder(canReserve: Bool): StageAfterAccepted {
  return switch canReserve {
    case true: .reserving
    case false: .cancelled
  }
}

export fn workInstruction(stage: WorkStage): String {
  return switch stage {
    case ServiceStage.reserving: "Reserve ingredients"
    case .preparing: "Prepare the course"
    case .serving: "Serve the guest"
  }
}

export fn asWorkStage(stage: ServiceStage): WorkStage? {
  return try? WorkStage(stage)
}

export fn widenStage(stage: WorkStage): ActiveStage {
  return stage
}

export fn requestCancellation(stage: CancellableStage): CancelRequest {
  return CancelRequest(stage: stage)
}

export fn terminalMessage(stage: TerminalStage): String {
  return switch stage {
    case .completed: "Archive the successful dinner"
    case .cancelled: "Return the ingredients to this universe"
  }
}

export fn asTerminalStage(stage: ServiceStage): TerminalStage? {
  return try? TerminalStage(stage)
}

export fn describeOutcome<T: Display>(
  outcome: ContinuingOutcome<T>,
): String {
  return switch outcome {
    case .ready(let value): value.display()
    case .delayed(let seconds): "Delay: ${seconds} seconds"
  }
}

export fn requestedTemperature(reading: UsableOvenReading): i32 {
  return switch reading {
    case .stable(let temperature): temperature
    case .warming(target: let target, ...): target
  }
}

export fn cancellationStage(request: take CancelRequest): CancellableStage {
  return switch take request {
    case CancelRequest(stage: let stage): stage
  }
}

export fn preparedValue<T>(outcome: ReadyOutcome<T>): T {
  return switch outcome {
    case .ready(let value): value
  }
}

export fn reserveCourse(
  ingredientsReady: Bool,
  delaySeconds delay: u32,
): WorkStage throws RecoverableServiceFault {
  if !ingredientsReady {
    throw .ingredientsMissing("Horizon Cake")
  }
  if delay > 0 {
    throw .delayed(delay)
  }
  return .preparing
}

export fn reservationMessage(
  ingredientsReady: Bool,
  delaySeconds delay: u32,
): String {
  do {
    let stage = try reserveCourse(ingredientsReady: ingredientsReady, delaySeconds: delay)
    return workInstruction(stage: stage)
  } catch .ingredientsMissing(let dish) {
    return "Missing ingredients for ${dish}"
  } catch .delayed(let seconds) {
    return "Wait ${seconds} seconds"
  }
}

test "enum subset excludes cancellation" for nextWorkStage {
  let stage = nextWorkStage(true)

  expect stage == .preparing
  expect workInstruction(stage) == "Prepare the course"
  expect asWorkStage(.cancelled) == .none
}

test "transition return publishes only reachable stages" for routeAcceptedOrder {
  let reserved = routeAcceptedOrder(canReserve: true)
  let cancelled = routeAcceptedOrder(canReserve: false)

  expect reserved == .reserving
  expect cancelled == .cancelled
  expect terminalMessage(stage: try TerminalStage(cancelled)) ==
    "Return the ingredients to this universe"
}

test "parameter subset rejects completed orders by contract" for requestCancellation {
  let request = requestCancellation(stage: .preparing)

  expect request.stage == .preparing
  expect asTerminalStage(stage: .serving) == .none
  expect asTerminalStage(stage: .completed) == .some(.completed)
}

test "payload enum subset keeps payload access" for describeOutcome {
  let outcome: ContinuingOutcome<String> = .ready("Horizon Cake")
  let ready: ReadyOutcome<String> = .ready("Horizon Cake")

  expect describeOutcome(outcome) == "Horizon Cake"
  expect preparedValue(ready) == "Horizon Cake"
}

test "payload shape follows the selected enum case" for requestedTemperature {
  let warming: UsableOvenReading = .warming(current: 120, target: 180)
  let stable: UsableOvenReading = .stable(180)

  expect requestedTemperature(warming) == 180
  expect requestedTemperature(stable) == 180
}

test "error subset limits the exhaustive catch" for reservationMessage {
  expect reservationMessage(ingredientsReady: false, delaySeconds: 0) ==
    "Missing ingredients for Horizon Cake"
  expect reservationMessage(ingredientsReady: true, delaySeconds: 3) == "Wait 3 seconds"
}
