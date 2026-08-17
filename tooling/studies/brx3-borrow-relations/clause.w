export protocol MenuSelectionRelation {
  static fn select(primary: ref String, fallback: ref String): view String
    borrows(0: [primary, fallback])
}

export type SelectorRelation = fn(ref String, ref String): view String
  borrows(0: [0])
