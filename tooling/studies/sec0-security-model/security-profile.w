import std
// security profile witness

export struct SecurityBudget {
  memory: usize
  requests: usize
}

export protocol MediatedBoundary {
  fn admit(_ budget: ref SecurityBudget): Bool
}

export fn admitLocal(_ budget: ref SecurityBudget): Bool {
  return budget.memory > 0 && budget.requests > 0
}
