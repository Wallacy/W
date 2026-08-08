// R1 Last Light contract-envelope study source.

enum ServiceStage {
  accepted
  reserving
  preparing
  serving
  completed
  cancelled
}

const fn canMove(from current: ServiceStage, to next: ServiceStage): Bool {
  return switch current {
    case .accepted: next in (.reserving, .cancelled)
    case .reserving: next in (.preparing, .cancelled)
    case .preparing: next in (.serving, .cancelled)
    case .serving: next in (.completed, .cancelled)
    case .completed: false
    case .cancelled: false
  }
}

const fn isValidStagePath(stages: StaticList<ServiceStage>): Bool {
  guard stages.count > 0 else return false

  for index in 1..<stages.count {
    if !canMove(from: stages[index - 1], to: stages[index]) {
      return false
    }
  }

  return true
}

struct StagePath<
  const _ stages: StaticList<[ServiceStage, (isValidStagePath(.member))]>,
> {
  orderId: u64
}

fn standardStagePath(
  orderId: u64,
): StagePath<[.accepted, .reserving, .preparing, .serving, .completed]> {
  return StagePath(orderId: orderId)
}

test "stage contracts validate the complete path" for isValidStagePath {
  expect isValidStagePath([.accepted, .reserving, .preparing, .serving, .completed])
  expect !isValidStagePath([.accepted, .completed])
}
