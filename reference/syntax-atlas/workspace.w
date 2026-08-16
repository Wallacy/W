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
