// atlas:begin package-root
package {
  schema: "w.package/1"
  name: "atlas/syntax"
  edition: "2026"
  dependencies: [
    {
      alias: "city"
      package: "fiction/city"
      source: .registry("w")
    }
  ]
}
// atlas:end package-root

// atlas:begin workspace-root
workspace {
  schema: "w.workspace/1"
  members: ["."]
  defaultMembers: ["."]
  patches: []
  resolution: {
    schema: "w.resolution/1"
    resolver: "w.resolver/1"
    contexts: []
    packages: []
  }
  deployments: [
    {
      schema: "w.deployment/1"
      name: "local"
      artifacts: [.product("atlas-syntax", target: "host", profile: "parse")]
    },
  ]
}
// atlas:end workspace-root
