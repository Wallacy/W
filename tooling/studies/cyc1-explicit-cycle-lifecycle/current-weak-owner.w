// CYC1 current composition: one strong owner and an explicit weak back edge.

module cyc1_current_weak_owner

export object CycleFixture {
  parent: weak CycleFixture?
  children: Array<shared CycleFixture>
}

export fn makeCycleFixture(): CycleFixture {
  return CycleFixture(
    parent: .none,
    children: Array<shared CycleFixture>(),
  )
}
