// Reproducible host plan for the seven TechEmpower workload families.

deployment {
  schema: "w.deployment/1"
  name: "last-light/benchmark"

  artifacts: [
    {
      name: "benchmark"
      source: .product(
        "last-light-benchmark",
        target: "x86_64-unknown-linux-gnu",
        profile: "benchmark",
        packing: "entry-only",
      )
    },
  ]

  placement: [
    {
      unit: "benchmark/main"
      host: .pool("benchmark-host")
    },
  ]

  bindings: [
    {
      import: "benchmark/benchmark-database"
      provider: .adapter(
        "w.database/postgresql@1",
        configuration: {
          credentials: .secret("benchmark-postgresql")
          connections: 256
          queuedOperations: 4_096
          queuedBytes: 64MiB
          pipelineDepth: 20
          pipelineSynchronization: .perStatement
          diskLogging: .deny
        },
      )
    },
    {
      import: "benchmark/cached-worlds"
      provider: .adapter(
        "w.cache/local@1",
        configuration: {
          maximumEntries: 10_000
          maximumActiveLoads: 256
          maximumQueuedLoads: 4_096
          expiration: .none
        },
      )
    },
  ]

  limits: {
    http: [
      {
        artifact: "benchmark"
        activeRequests: 16_384
        queuedRequests: 16_384
        queuedBytes: 64MiB
        connections: 32_768
      },
    ]
  }
}
