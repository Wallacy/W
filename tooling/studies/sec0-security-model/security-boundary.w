import std
// security boundary witness

export struct TenantRequest {
  let tenant: String
  let body: Bytes
}

export fn route(_ request: take TenantRequest): Bool {
  return request.tenant.count > 0 && request.body.count <= 65_536
}
