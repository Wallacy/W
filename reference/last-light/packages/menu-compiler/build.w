// Package manifest for the hermetic menu compiler.

package {
  schema: "w.package/1"
  authority: .registry("w")
  name: "last-light/menu-compiler"
  version: "0.1.0"
  edition: "2026"
  namespace: "last_light.menu"
  license: {
    expression: "MIT"
    files: ["LICENSE"]
  }
  publish: {
    source: .required
    files: [
      .modules,
      .path("README.md"),
      .path("LICENSE"),
    ]
  }

  moduleSets: [
    {
      name: "menu-compiler-modules"
      activation: .always
      namespace: "last_light.menu"
      root: "."
      include: ["*.w"]
      exclude: ["build.w"]
      layout: .fileStem
    },
  ]

  products: [
    {
      name: "menu-compiler"
      kind: .tool
      module: "last_light.menu.transform"
      host: "w.host/build-transform@1"
      capabilities: []
    },
  ]

  dependencies: []

  build: {
    network: .deny
    environment: []
    profiles: [
      {
        name: "release"
        optimize: .speed
        checks: .safe
        debug: .sidecar
        cpuPolicy: .portable
        memory: {
          generalAllocator: .system
          representation: .portable
        }
      },
    ]
    constEval: {
      steps: 1_000_000
      heap: 64MiB
      callDepth: 256
      result: 8MiB
    }
  }
}
