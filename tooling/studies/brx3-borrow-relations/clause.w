export protocol MenuSelectionRelation {
  static fn select(_ primary: ref String, _ fallback: ref String): view String
    borrows(0: [primary, fallback])
}

export type SelectorRelation = fn(ref String, ref String): view String
  borrows(0: [0])
