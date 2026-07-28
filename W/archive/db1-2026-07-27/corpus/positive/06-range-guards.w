fn shouldAccumulate(rawDuty: f64, error: f64): Bool {
  switch rawDuty {
    case 0.0...1.0:
      return true
    case ..<0.0 where error > 0.0:
      return true
    case 1.0>.. where error < 0.0:
      return true
    case _:
      return false
  }
}
