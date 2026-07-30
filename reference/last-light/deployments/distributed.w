// Distributed plan for the restaurant, edge, observatory, and control systems.

deployment {
  schema: "w.deployment/1"
  name: "last-light/distributed"

  artifacts: [
    {
      name: "core"
      source: .product(
        "last-light-native",
        target: "aarch64-unknown-linux-gnu",
        profile: "release",
        packing: "split-services",
      )
    },
    {
      name: "http"
      source: .product(
        "last-light-worker",
        target: "wasm32-wasip3",
        profile: "release",
        packing: "entry-only",
      )
    },
    {
      name: "wifi"
      source: .product(
        "last-light-wifi",
        target: "wasm32-wasip3",
        profile: "release",
        packing: "entry-only",
      )
    },
    {
      name: "observatory"
      source: .product(
        "last-light-observatory",
        target: "aarch64-unknown-linux-gnu",
        profile: "release",
        packing: "entry-only",
      )
    },
    {
      name: "kitchen"
      source: .release(
        "last-light/kitchen-control@0.1.0",
        product: "controller",
        target: "thumbv7em-none-eabihf",
      )
    },
    {
      name: "payments"
      source: .release(
        "last-light/payment-gateway@0.1.0",
        product: "server",
        target: "aarch64-unknown-linux-gnu",
      )
    },
    {
      name: "audience"
      source: .release(
        "last-light/audience-meter@0.1.0",
        product: "controller",
        target: "riscv32-unknown-none-elf",
      )
    },
    {
      name: "sessions"
      source: .release(
        "last-light/wifi-sessions@0.1.0",
        product: "component",
        target: "wasm32-wasip3",
      )
    },
    {
      name: "satellites"
      source: .release(
        "last-light/satellite-control@0.1.0",
        product: "controller",
        target: "riscv32-unknown-none-elf",
      )
    },
  ]

  placement: [
    { unit: "core/gateway", host: .pool("restaurant-api") },
    { unit: "core/planning", host: .pool("observatory-compute") },
    { unit: "core/finance", host: .pool("finance") },
    { unit: "core/dining", host: .pool("restaurant-floor") },
    { unit: "http/main", host: .pool("edge-wasm") },
    { unit: "wifi/main", host: .pool("edge-wasm") },
    { unit: "observatory/main", host: .pool("observatory-control") },
    { unit: "kitchen/controller", host: .device("kitchen-control") },
    { unit: "payments/main", host: .pool("finance") },
    { unit: "audience/controller", host: .device("audience-meter") },
    { unit: "sessions/main", host: .pool("edge-state") },
    { unit: "satellites/controller", host: .fleet("satellite-swarm") },
  ]

  bindings: [
    {
      import: "http/last-light"
      provider: .service(unit: "core/gateway", binding: "last-light")
    },
    {
      import: "core/pantry"
      provider: .service(unit: "kitchen/controller", binding: "pantry")
    },
    {
      import: "core/ovens"
      provider: .service(unit: "kitchen/controller", binding: "ovens")
    },
    {
      import: "core/payment-gateway"
      provider: .service(unit: "payments/main", binding: "payment-gateway")
    },
    {
      import: "core/audience"
      provider: .service(unit: "audience/controller", binding: "audience")
    },
    {
      import: "core/aroma-device"
      provider: .device("aroma-probe")
    },
    {
      import: "wifi/wifi-sessions"
      provider: .service(unit: "sessions/main", binding: "wifi-sessions")
    },
    {
      import: "observatory/satellites"
      provider: .service(unit: "satellites/controller", binding: "satellites")
    },
  ]

  adapters: [
    {
      artifact: "core"
      supervisor: "fulfillment"
      role: .workflowJournal
      provider: .adapter(
        "w.std/sqlite-workflow@1",
        storage: .capability("last-light/workflow-store"),
      )
    },
  ]

  limits: {
    supervisors: [
      {
        artifact: "core"
        binding: "fulfillment"
        roots: 4_096
        running: 32
        admissionQueued: 64
      },
    ]
  }
}
