// atlas:begin deployment-root
deployment {
  schema: "w.deployment/1"
  name: "atlas/local"
  artifacts: [
    .product("atlas-syntax", target: "host", profile: "parse")
  ]
}
// atlas:end deployment-root
