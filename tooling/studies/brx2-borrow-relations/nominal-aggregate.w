// BRX2 alternative fixture. This changes the API to an owned nominal sum.
enum SelectedLine {
  primary(String)
  fallback(String)
}

export fn observeBorrowRelation(
  _ primary: ref String,
  _ fallback: ref String,
): SelectedLine {
  return .primary(primary.materialize())
}
