// atlas:begin package-root
package {
  schema: "w.package/1"
  root: "."
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
      { name: "debug", optimize: .none, checks: .full },
      { name: "release", optimize: .speed, checks: .safe },
    ]
    recipes: [
      { name: "benchmark", baseProfile: "release", kind: .benchmark, cpuPolicy: .portable },
    ]
  }
  dependencies: [
    { alias: "city", package: "fiction/city", source: .registry("w") },
  ]
}

package {
  schema: "w.package/1"
  root: "tools/indexer"
  authority: .registry("w")
  name: "atlas/indexer"
  edition: "2026"
  products: [{ name: "atlas-indexer", kind: .tool, module: "indexer" }]
  build: {
    network: .deny
    profiles: [{ name: "release", optimize: .speed, checks: .safe }]
  }
}
// atlas:end package-root

// atlas:begin build-root
build {
  schema: "w.build/1"
  default: { package: "atlas/syntax", product: "atlas-syntax" }
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
// atlas:end build-root
