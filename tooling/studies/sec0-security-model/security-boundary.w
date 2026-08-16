import std
// security boundary witness

export struct TenantRequest {
  tenant: String
  body: Bytes
}

export fn route(request: take TenantRequest): Bool {
  return request.tenant.count > 0 && request.body.count <= 65_536
}
