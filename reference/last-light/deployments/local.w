// Local deployment plan for development and deterministic fault tests.

deployment {
  schema: "w.deployment/1"
  name: "last-light/local"

  artifacts: [
    {
      name: "restaurant"
      source: .product(
        "last-light-native",
        target: "x86_64-unknown-linux-gnu",
        profile: "debug",
        packing: "single-process",
      )
    },
  ]

  placement: [
    {
      unit: "restaurant/main"
      host: .local
    },
  ]

  bindings: [
    {
      import: "restaurant/pantry"
      provider: .adapter("last-light.dev/pantry@1")
    },
    {
      import: "restaurant/ovens"
      provider: .adapter("last-light.dev/ovens@1")
    },
    {
      import: "restaurant/payment-gateway"
      provider: .adapter("last-light.dev/payments@1")
    },
    {
      import: "restaurant/audience"
      provider: .adapter("last-light.dev/audience@1")
    },
    {
      import: "restaurant/aroma-device"
      provider: .adapter("last-light.dev/aroma-device@1")
    },
  ]

  adapters: [
    {
      artifact: "restaurant"
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
        unit: "restaurant/main"
        tasks: {
          live: 1_024
          frameBytes: 32MiB
          timers: 1_024
        }
        pools: [
          { name: "cpu", capacity: 1 }
          { name: "blocking", capacity: 2 }
        ]
      },
    ]
    supervisors: [
      {
        artifact: "restaurant"
        binding: "fulfillment"
        roots: 256
        running: 8
        admissionQueued: 32
      },
    ]
  }
}
