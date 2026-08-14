// CYC1 current composition: publish the owner before installing a weak self edge.

module cyc1_post_init_self_weak

export object CycleFixture {
  selfLink: weak CycleFixture?

  export init() {
    self.selfLink = .none
  }

  fn installAfterPublication(_ owner: shared CycleFixture) {
    self.selfLink = owner
  }
}

export fn makeCycleFixture(): shared CycleFixture {
  let owner: shared CycleFixture = CycleFixture()
  return owner
}
