// Pure oracle for the HTTP mapping of RestPC operations.

import http from std

enum RestPcOperation {
  getResource
  queryOrders
  submitCommand
  unsupported
}

const fn expectedOperation(
  for method: http.Method,
  path resourcePath: ref String,
): RestPcOperation {
  return switch (method, resourcePath) {
    case (.get, "/menu"): .getResource
    case (.query, "/orders"): .queryOrders
    case (.post, "/commands"): .submitCommand
    case (_, _): .unsupported
  }
}

test "RestPC does not hide a query in GET or POST" for expectedOperation {
  expect expectedOperation(for: .get, path: "/menu") == .getResource
  expect expectedOperation(for: .query, path: "/orders") == .queryOrders
  expect expectedOperation(for: .get, path: "/orders") == .unsupported
  expect expectedOperation(for: .post, path: "/orders") == .unsupported
  expect expectedOperation(for: .post, path: "/commands") == .submitCommand
}
