// BRX2 alternative fixture. This changes the API to an owned nominal sum.
enum SelectedLine {
  primary(String)
  fallback(String)
}

export fn observeBorrowRelation(
  primary: ref String,
  fallback: ref String,
): SelectedLine {
  return .primary(primary.materialize())
}
