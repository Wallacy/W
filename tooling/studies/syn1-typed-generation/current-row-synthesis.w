import data from std

export struct TabularTelemetryRow: data.Row {
  warning: String?
  count: u64
}

fn emitRow(row: TabularTelemetryRow): u64 {
  return row.count
}

entry(emitRow)
