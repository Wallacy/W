// CYC1 current composition: a lifecycle owner removes a strong callback edge.

module cyc1_explicit_owner_close

export object CycleFixture {
  let callback: shared CycleFixture?

  fn close() {
    callback = .none
  }
}

export fn closeCycleFixture(_ value: shared CycleFixture): CycleFixture {
  return CycleFixture(callback: .none)
}
