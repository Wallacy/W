// Selected current form for BRX0. No source lifetime names are used.
export protocol MenuSelection {
  static fn observeBorrowExpressivity(
    primary: ref String,
    fallback: ref String,
  ): view String

  static fn observeUniqueBorrow(primary: ref String): view String
}

export fn observeCursor(cursor: ref String): view String {
  return cursor
}
