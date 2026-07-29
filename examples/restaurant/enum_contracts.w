/// Closed enum subsets preserve facts across public interfaces.
import { ServiceStage } from restaurant.domain

export alias WorkStage =
  ServiceStage<[.reserving, .preparing, .serving]>

export alias ActiveStage =
  ServiceStage<[.accepted, .reserving, .preparing, .serving]>

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

export fn nextWorkStage(inventoryReady: Bool): WorkStage {
  return switch inventoryReady {
    case true: .preparing
    case false: .reserving
  }
}

export fn workInstruction(stage: WorkStage): String {
  return switch stage {
    case .reserving: "Reserve ingredients"
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

export fn describeOutcome<T: Display>(
  outcome: ContinuingOutcome<T>,
): String {
  return switch outcome {
    case .ready(let value): value.display()
    case .delayed(let seconds): "Delay: ${seconds} seconds"
  }
}

export fn preparedValue<T>(outcome: ReadyOutcome<T>): T {
  return switch outcome {
    case .ready(let value): value
  }
}

export fn reserveCourse(
  ingredientsReady: Bool,
  delaySeconds: u32,
): WorkStage throws RecoverableServiceFault {
  if !ingredientsReady {
    throw .ingredientsMissing("Horizon Cake")
  }
  if delaySeconds > 0 {
    throw .delayed(delaySeconds)
  }
  return .preparing
}

export fn reservationMessage(
  ingredientsReady: Bool,
  delaySeconds: u32,
): String {
  do {
    let stage = try reserveCourse(ingredientsReady, delaySeconds: delaySeconds)
    return workInstruction(stage)
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

test "payload enum subset keeps payload access" for describeOutcome {
  let outcome: ContinuingOutcome<String> = .ready("Horizon Cake")
  let ready: ReadyOutcome<String> = .ready("Horizon Cake")

  expect describeOutcome(outcome) == "Horizon Cake"
  expect preparedValue(ready) == "Horizon Cake"
}

test "error subset limits the exhaustive catch" for reservationMessage {
  expect reservationMessage(false, delaySeconds: 0) ==
    "Missing ingredients for Horizon Cake"
  expect reservationMessage(true, delaySeconds: 3) == "Wait 3 seconds"
}
