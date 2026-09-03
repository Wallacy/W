// Alternative B2 shape. The aggregate carries a nominal choice, so it is an
// API change and does not preserve the direct borrowed result contract.
export enum SelectedLine {
  primary(String)
  fallback(String)
}

export protocol MenuSelection {
  static fn observeBorrowExpressivity(
    _ primary: ref String,
    _ fallback: ref String,
  ): SelectedLine
}
