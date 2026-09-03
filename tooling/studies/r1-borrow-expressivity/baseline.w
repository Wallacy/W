// Selected current form for BRX0. No source lifetime names are used.
export protocol MenuSelection {
  static fn observeBorrowExpressivity(
    _ primary: ref String,
    _ fallback: ref String,
  ): view String

  static fn observeUniqueBorrow(_ primary: ref String): view String
}

export fn observeCursor(_ cursor: ref String): view String {
  return cursor
}
