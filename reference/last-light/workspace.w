// Data-only workspace for the Last Light reference product.

workspace {
  schema: "w.workspace/1"
  members: [
    ".",
    "packages/menu-compiler",
  ]
  defaultMembers: ["."]
  patches: []
  toolchainPolicy: {
    catalogs: [.distribution]
    systemImports: .explicit
    providerOrder: [.distribution, .system]
    foreignLanguages: [.c]
    executionPlatforms: [
      {
        name: "linux-x64"
        target: "x86_64-unknown-linux-gnu"
        sandbox: "w.build-sandbox/1"
      },
      {
        name: "windows-x64"
        target: "x86_64-pc-windows-msvc"
        sandbox: "w.build-sandbox/1"
      },
      {
        name: "macos-arm64"
        target: "aarch64-apple-darwin"
        sandbox: "w.build-sandbox/1"
      },
    ]
  }
}
