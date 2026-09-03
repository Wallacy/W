// Pure oracle for the logical Lazy behavior state.

export enum LazyLogicalState {
  uninitialized
  initializing
  initialized
  faulted
  closed
}

export enum LazyEvent {
  beginAccess
  waitForWinner
  publish
  assignExclusive
  panicInitializer
  close
}

export enum LazyLowering {
  local
  isolated
  concurrent
}

export const fn nextLazyState(
  state: LazyLogicalState,
  on event: LazyEvent,
): LazyLogicalState? {
  return switch (state, event) {
    case (.uninitialized, .beginAccess): .some(.initializing)
    case (.initializing, .waitForWinner): .some(.initializing)
    case (.initializing, .publish): .some(.initialized)
    case (.uninitialized, .assignExclusive): .some(.initialized)
    case (.initialized, .assignExclusive): .some(.initialized)
    case (.initializing, .panicInitializer): .some(.faulted)
    case (.uninitialized, .close): .some(.closed)
    case (.initialized, .close): .some(.closed)
    case (.faulted, .close): .some(.closed)
    case (_, _): .none
  }
}

export const fn canBlockWhenContended(_ lowering: LazyLowering): Bool {
  return switch lowering {
    case .local: false
    case .isolated: false
    case .concurrent: true
  }
}

test "one winner publishes one lazy value" for nextLazyState {
  let started = nextLazyState(state: .uninitialized, on: .beginAccess)
  let waiting = nextLazyState(state: .initializing, on: .waitForWinner)
  let published = nextLazyState(state: .initializing, on: .publish)

  expect started == .some(.initializing)
  expect waiting == .some(.initializing)
  expect published == .some(.initialized)
  expect nextLazyState(state: .initialized, on: .publish) == none
}

test "exclusive assignment supersedes the initializer" for nextLazyState {
  expect nextLazyState(state: .uninitialized, on: .assignExclusive)
    == .some(.initialized)
  expect nextLazyState(state: .initializing, on: .assignExclusive) == none
}

test "panic faults instead of publishing a partial value" for nextLazyState {
  expect nextLazyState(state: .initializing, on: .panicInitializer)
    == .some(.faulted)
  expect nextLazyState(state: .faulted, on: .publish) == none
}

test "only concurrent lowering can wait for another winner" for canBlockWhenContended {
  expect canBlockWhenContended(.local) == false
  expect canBlockWhenContended(.isolated) == false
  expect canBlockWhenContended(.concurrent) == true
}
