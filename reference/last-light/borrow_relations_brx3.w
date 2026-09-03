// BRX3 source-shaped fixtures for explicit borrowed-result relations.
//
// The `borrows` clause is the selected pre-1.0 source form. It names source
// parameter slots and result ordinals. It does not name lifetimes.

export protocol MenuSelectionRelation {
  static fn select(
    primary: ref String,
    fallback: ref String,
  ): view String borrows(0: [primary, fallback])
}

export fn selectPrimaryRelation(
  primary: ref String,
  fallback: ref String,
): view String borrows(0: [primary]) {
  return primary
}

export fn selectPairRelation(
  primary: ref String,
  fallback: ref String,
): (view String, view String) borrows(0: [primary], 1: [fallback]) {
  return (primary, fallback)
}

// `borrows` remains an identifier outside a function tail.
export fn borrowsIdentifierContext() {}

export type SelectorRelation = fn(
  ref String,
  ref String,
): view String borrows(0: [0])

foreign W from "last-light.menu" {
  fn selectPrimaryImported(
    _ primary: ref String,
    _ fallback: ref String,
  ): view String borrows(0: [primary])
}
