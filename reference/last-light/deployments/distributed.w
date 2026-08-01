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
    {
      name: "horizon"
      source: .release(
        "last-light/horizon-monitor@0.1.0",
        product: "server",
        target: "aarch64-unknown-linux-gnu",
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
    { unit: "horizon/main", host: .pool("horizon-sensors") },
  ]

  bindings: [
    {
      import: "http/lastLight"
      provider: .service(unit: "core/gateway", binding: "lastLight")
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
      import: "core/paymentGateway"
      provider: .service(unit: "payments/main", binding: "paymentGateway")
    },
    {
      import: "core/audience"
      provider: .service(unit: "audience/controller", binding: "audience")
    },
    {
      import: "core/aromaDevice"
      provider: .device("aroma-probe")
    },
    {
      import: "wifi/wifiSessions"
      provider: .service(unit: "sessions/main", binding: "wifiSessions")
    },
    {
      import: "observatory/satellites"
      provider: .service(unit: "satellites/controller", binding: "satellites")
    },
    {
      import: "observatory/horizonMonitor"
      provider: .service(unit: "horizon/main", binding: "horizonMonitor")
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
    execution: [
      {
        unit: "core/gateway"
        tasks: {
          live: 8_192
          frameBytes: 128MiB
          timers: 8_192
        }
        pools: [
          { name: "cpu", capacity: 16 }
          { name: "blocking", capacity: 8 }
        ]
      },
      {
        unit: "core/planning"
        tasks: {
          live: 4_096
          frameBytes: 128MiB
          timers: 4_096
        }
        pools: [
          { name: "cpu", capacity: 16 }
          { name: "blocking", capacity: 1 }
        ]
      },
      {
        unit: "core/finance"
        tasks: {
          live: 4_096
          frameBytes: 64MiB
          timers: 4_096
        }
        pools: [
          { name: "cpu", capacity: 8 }
          { name: "blocking", capacity: 8 }
        ]
      },
      {
        unit: "core/dining"
        tasks: {
          live: 2_048
          frameBytes: 64MiB
          timers: 2_048
        }
        pools: [
          { name: "cpu", capacity: 4 }
          { name: "blocking", capacity: 2 }
        ]
      },
      {
        unit: "http/main"
        tasks: {
          live: 8_192
          frameBytes: 128MiB
          timers: 16_384
        }
        pools: [{ name: "cpu", capacity: 16 }]
      },
      {
        unit: "wifi/main"
        tasks: {
          live: 4_096
          frameBytes: 64MiB
          timers: 8_192
        }
        pools: [{ name: "cpu", capacity: 8 }]
      },
      {
        unit: "observatory/main"
        tasks: {
          live: 4_096
          frameBytes: 64MiB
          timers: 8_192
        }
        pools: [
          { name: "cpu", capacity: 8 }
          { name: "blocking", capacity: 1 }
        ]
      },
    ]
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
