// Data-only workspace for the Last Light reference product.

workspace {
  schema: "w.workspace/1"
  members: [
    ".",
    "packages/menu-compiler",
  ]
  defaultMembers: ["."]
  patches: []
  resolution: {
    schema: "w.resolution/1"
    resolver: "w.resolver/1"
    workspaceDigest: "sha256:a94ca5dee307bfdcec6bcc89a9443762c954968d87bc5dc3f68a5a9c1547530b"
    contexts: [
      {
        name: "last-light-native"
        root: .product("last-light-native")
        use: .product
        targetRole: .target
        target: "x86_64-unknown-linux-gnu"
        features: []
        targetVariants: ["native-terminal::posix"]
        activeSourceSet: "sha256:b0a8c64198258d92fc0f55a012c99a92a67f8a85e770f5c53933b0ce1b50f05b"
        resolutionDigest: "sha256:88a9aa826e4d9896648eeb9b7c07e0afefe695bd8dc2c24bfd9976923bca99e4"
        nodes: ["sha256:22d0414c0b18f89bb91f3e2ea5b5368b557664f8f910a41102a5e7f4f28f3c67", "sha256:3b51417f058a4a66d6166525d9fd588c97e79649db57d599fb7e559be24f8a44", "sha256:3e896724d6f6f896039de431e424309492cd2b2b30b47c0cf33e1f0f1b064de0"]
        rootEdges: [
          { alias: "chart", id: "sha256:3b51417f058a4a66d6166525d9fd588c97e79649db57d599fb7e559be24f8a44" }
          { alias: "menuCompiler", id: "sha256:3e896724d6f6f896039de431e424309492cd2b2b30b47c0cf33e1f0f1b064de0" }
        ]
      },
      {
        name: "menu-compiler"
        root: .tool("menu-compiler")
        use: .build
        targetRole: .execution
        target: "x86_64-pc-windows-msvc"
        features: []
        targetVariants: []
        activeSourceSet: "sha256:69b732ebb3f0f4bbeff9992102fdb41bdb5edac8045ce3a552ea342c95316e16"
        resolutionDigest: "sha256:646b5a07b21f4553d870257f4f02d9b8f6000fea81b4aafac45ef73a87d96edc"
        nodes: ["sha256:3e896724d6f6f896039de431e424309492cd2b2b30b47c0cf33e1f0f1b064de0"]
        rootEdges: []
      },
    ]
    packages: [
      {
        id: "sha256:22d0414c0b18f89bb91f3e2ea5b5368b557664f8f910a41102a5e7f4f28f3c67"
        authority: "w"
        name: "last-light/restaurant"
        version: "0.1.0"
        source: .member(path: ".")
        dependencies: [
          { alias: "chart", id: "sha256:3b51417f058a4a66d6166525d9fd588c97e79649db57d599fb7e559be24f8a44", use: .product }
          { alias: "menuCompiler", id: "sha256:3e896724d6f6f896039de431e424309492cd2b2b30b47c0cf33e1f0f1b064de0", use: .build }
        ]
      },
      {
        id: "sha256:3b51417f058a4a66d6166525d9fd588c97e79649db57d599fb7e559be24f8a44"
        authority: "w"
        name: "fiction/chart"
        version: "1.2.0"
        source: .registry("w")
        dependencies: []
      },
      {
        id: "sha256:3e896724d6f6f896039de431e424309492cd2b2b30b47c0cf33e1f0f1b064de0"
        authority: "w"
        name: "last-light/menu-compiler"
        version: "0.1.0"
        source: .member(path: "packages/menu-compiler")
        dependencies: []
      },
    ]
  }
  deployments: [
    {
      schema: "w.deployment/1"
      name: "local"

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
          import: "restaurant/paymentGateway"
          provider: .adapter("last-light.dev/payments@1")
        },
        {
          import: "restaurant/audience"
          provider: .adapter("last-light.dev/audience@1")
        },
        {
          import: "restaurant/aromaDevice"
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
    },
    {
      schema: "w.deployment/1"
      name: "distributed"

      security: {
        wrpc: {
          profile: "service-default"
          channels: {
            network: [.quicTls13Mutual, .tls13Mutual]
            ipc: [.ipcPeer]
          }
          identity: .unit(trustDomain: "last-light.production")
          credentials: .capability("last-light/workload-identity")
          trustRoots: .capability("last-light/workload-trust")
          earlyData: .deny
          handshake: {
            pendingPerAuthority: 32
            credentialBytes: 64KiB
            helloBytes: 64KiB
            interfaces: 128
            compatibilityMaps: 64
            timeout: 5<s>
          }
          lifecycle: {
            maximumAge: 1<h>
            drainTimeout: 5<s>
            revocation: .terminate
          }
          capabilities: {
            importSlots: 2_048
            exportSlots: 2_048
            ordinalsPerFrame: 64
            entriesPerPipeline: 128
          }
        }
      }

      verification: {
        require: [.maintainerAuthorized, .reproducible]
        reject: [.revoked, .yanked]
        transparency: .required
      }

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
    },
    {
      schema: "w.deployment/1"
      name: "benchmark"

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
        execution: [
          {
            unit: "benchmark/main"
            tasks: {
              live: 65_536
              frameBytes: 1GiB
              timers: 65_536
            }
            pools: [{ name: "cpu", capacity: 64 }]
          },
        ]
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
  ]
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
