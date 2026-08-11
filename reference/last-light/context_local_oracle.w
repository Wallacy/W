// Context propagation for the last restaurant before the end of the Universe.

module context_local_oracle

export enum ContextBoundary {
  directCall
  asyncChild
  taskGroupChild
  spawnedChild
  serviceCall
  wireCall
  deviceLaunch
  foreignCallback
  hostEntry
}

export enum TaskLocalVisibility {
  currentBinding
  defaultValue
}

export const fn taskLocalVisibility(
  boundary: ContextBoundary,
  hasBinding: Bool,
): TaskLocalVisibility {
  guard hasBinding else return .defaultValue

  return switch boundary {
    case .directCall: .currentBinding
    case .asyncChild: .currentBinding
    case .taskGroupChild: .currentBinding
    case .spawnedChild: .currentBinding
    case .serviceCall: .defaultValue
    case .wireCall: .defaultValue
    case .deviceLaunch: .defaultValue
    case .foreignCallback: .defaultValue
    case .hostEntry: .defaultValue
  }
}

export struct TaskLocalPopFacts {
  operationSettled: Bool
  childrenDrained: Bool
  dependenciesClosed: Bool
}

export const fn canPopTaskLocal(facts: TaskLocalPopFacts): Bool {
  return facts.operationSettled
    && facts.childrenDrained
    && facts.dependenciesClosed
}

export enum ThreadLocalDecision {
  accepted
  typeMustBeCopy
  dropNotAllowed
  nativeTlsUnavailable
  operationMaySuspend
  dependencyEscapes
  fiberEmulationRejected
}

export struct ThreadLocalFacts {
  copyable: Bool
  hasDrop: Bool
  nativeTls: Bool
  operationMaySuspend: Bool
  dependencyEscapes: Bool
  fiberEmulation: Bool
}

export const fn threadLocalDecision(
  facts: ThreadLocalFacts,
): ThreadLocalDecision {
  if !facts.copyable { return .typeMustBeCopy }
  if facts.hasDrop { return .dropNotAllowed }
  if !facts.nativeTls { return .nativeTlsUnavailable }
  if facts.operationMaySuspend { return .operationMaySuspend }
  if facts.dependencyEscapes { return .dependencyEscapes }
  if facts.fiberEmulation { return .fiberEmulationRejected }
  return .accepted
}

test "task-local context follows only the structured task tree" {
  expect taskLocalVisibility(
    boundary: .spawnedChild,
    hasBinding: true,
  ) == .currentBinding

  expect taskLocalVisibility(
    boundary: .serviceCall,
    hasBinding: true,
  ) == .defaultValue
}

test "task-local pop waits for child and dependency drain" {
  expect canPopTaskLocal(
    facts: TaskLocalPopFacts(
      operationSettled: true,
      childrenDrained: true,
      dependenciesClosed: true,
    ),
  )
}

test "safe TLS stays synchronous and physical" {
  expect threadLocalDecision(
    facts: ThreadLocalFacts(
      copyable: true,
      hasDrop: false,
      nativeTls: true,
      operationMaySuspend: false,
      dependencyEscapes: false,
      fiberEmulation: false,
    ),
  ) == .accepted
}
