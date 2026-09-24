// atlas:begin package-root
package {
  schema: "w.package/1"
  authority: .registry("w")
  name: "atlas/syntax"
  edition: "2026"
  products: [{
    name: "atlas-syntax"
    kind: .executable
    module: "app"
  }]
  build: {
    profiles: [
      {
        name: "debug"
        optimize: .none
        checks: .full
      },
      {
        name: "release"
        optimize: .speed
        checks: .safe
      },
    ]
    recipes: [
      {
        name: "benchmark"
        baseProfile: "release"
        kind: .benchmark
        cpuPolicy: .portable
      },
    ]
  }
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
      artifacts: [.product(
        "atlas-syntax",
        target: "x86_64-unknown-linux-gnu",
        profile: "release",
        recipe: "benchmark",
        size: .compact,
        debug: .sidecar,
      )]
    },
  ]
}
// atlas:end workspace-root
