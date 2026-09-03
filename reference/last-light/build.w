// Unified data-only build manifest: direct package and workspace records.
// DESIGN.md sections 3.5.3 and 21.1 define its document grammar and schemas.

package {
  schema: "w.package/1"
  authority: .registry("w")
  name: "last-light/restaurant"
  version: "0.1.0"
  edition: "2026"
  license: {
    expression: "MIT"
    files: ["LICENSE"]
  }
  publish: {
    source: .required
    files: [
      .modules,
      .path("menus/final.menu"),
      .path("README.md"),
      .path("BUILD.md"),
      .path("LICENSE"),
    ]
  }

  releasePolicy: {
    maintainer: { threshold: 2, keys: 3 }
    reproduction: {
      required: 2
      distinctAuthorities: 2
      compare: .allRecipeOutputs
    }
    source: .public
    transparency: .sigstoreCompatible
    metadata: {
      expiry: 7<d>
      consistentSnapshots: true
    }
  }

  moduleSets: [
    {
      name: "restaurant-modules"
      activation: .always
      root: "."
      include: ["*.w"]
      exclude: ["build.w"]
      layout: .fileStem
    },
    {
      name: "native-terminal-posix"
      activation: .selected
      namespace: "platform"
      root: "platform/posix"
      include: ["*.w"]
      layout: .fileStem
    },
    {
      name: "native-terminal-windows"
      activation: .selected
      namespace: "platform"
      root: "platform/windows"
      include: ["*.w"]
      layout: .fileStem
    },
  ]

  targetVariants: [
    {
      name: "native-terminal"
      interface: .uniform
      cases: [
        {
          name: "posix"
          target: { systems: [.linux, .darwin] }
          enables: [.moduleSet("native-terminal-posix")]
        },
        {
          name: "windows"
          target: { systems: [.windows] }
          enables: [.moduleSet("native-terminal-windows")]
        },
      ]
      fallback: { name: "not-native", enables: [] }
    },
  ]

  wrpcProfiles: [
    {
      name: "service-default"
      channels: [.tls13Mutual, .quicTls13Mutual, .ipcPeer]
      earlyData: .deny
      handshake: {
        pendingPerAuthority: 64
        credentialBytes: 128KiB
        helloBytes: 128KiB
        interfaces: 256
        compatibilityMaps: 128
        timeout: 10<s>
      }
      lifecycle: {
        maximumAge: 24<h>
        drainTimeout: 30<s>
        revocation: .terminate
      }
      capabilities: {
        importSlots: 4_096
        exportSlots: 4_096
        ordinalsPerFrame: 128
        entriesPerPipeline: 256
      }
    },
  ]

  runtimeGraphs: [
    {
      name: "restaurant-core"
      servicePolicy: {
        resolution: .startup
        wrpcProfile: "service-default"
        links: [
          .local,
          .component,
          .wrpc(transports: [.ipc, .network]),
        ]
        dynamicRebinding: .deny
      }
      services: [
        {
          binding: "lastLight"
          declaration: "restaurant::lastLight"
          scope: .process
          mailbox: { items: 64, bytes: 8MiB, inFlight: 1 }
        },
        {
          binding: "orders"
          declaration: "supervision::orderCoordinators"
          scope: .keyed(keyType: "domain::OrderId")
          mailbox: { items: 8, bytes: 1MiB, inFlight: 1 }
          arguments: {
            identity: .serviceIdentity
            fulfillment: .supervisor("fulfillment", key: .serviceIdentity)
          }
        },
        {
          binding: "oracle"
          declaration: "oracle::oracle"
          scope: .process
          mailbox: { items: 64, bytes: 4MiB, inFlight: 1 }
        },
        {
          binding: "aromaProbe"
          declaration: "hardware::aromaProbe"
          scope: .process
          mailbox: { items: 16, bytes: 1MiB, inFlight: 1 }
          arguments: {
            device: .capability("aromaDevice")
          }
        },
        {
          binding: "billing"
          declaration: "billing::billing"
          scope: .process
          mailbox: { items: 64, bytes: 4MiB, inFlight: 1 }
          arguments: {
            gateway: .service("paymentGateway")
          }
        },
        {
          binding: "diningRoom"
          declaration: "dining::diningRoom"
          scope: .process
          mailbox: { items: 32, bytes: 4MiB, inFlight: 1 }
          arguments: {
            audience: .service("audience")
          }
        },
      ]

      requirements: [
        { service: "restaurant::pantry", default: .import("pantry") },
        { service: "restaurant::ovens", default: .import("ovens") },
        { service: "workflow::pantry", default: .import("pantry") },
        { service: "workflow::ovens", default: .import("ovens") },
        { service: "supervision::pantry", default: .import("pantry") },
        { service: "supervision::ovens", default: .import("ovens") },
      ]

      imports: [
        {
          binding: "pantry"
          protocol: "kitchen::PantryApi"
          source: .deployment
        },
        {
          binding: "ovens"
          protocol: "kitchen::OvenApi"
          source: .deployment
        },
        {
          binding: "paymentGateway"
          protocol: "billing::PaymentGatewayApi"
          source: .deployment
        },
        {
          binding: "audience"
          protocol: "dining::AudienceApi"
          source: .deployment
        },
        {
          binding: "aromaDevice"
          capability: "hardware::AromaProbeDevice"
          source: .host
        },
      ]

      supervisors: [
        {
          binding: "fulfillment"
          keyType: "domain::OrderId"
          inputType: "workflow::FulfillmentInput"
          progressType: "domain::ServiceStage"
          outputType: "domain::Receipt"
          failureType: "restaurant::RestaurantError"
          operation: "workflow::fulfillOrderDurably"
          domain: .io
          context: {
            services: [
              "pantry",
              "ovens",
              "oracle",
              "aromaProbe",
              "billing",
              "diningRoom",
            ]
          }
          capacity: {
            roots: 4_096
            running: 64
            admissionQueued: 128
            queuedBytes: 16MiB
          }
          retention: { terminalItems: 4_096, terminalBytes: 64MiB }
          deduplication: {
            tombstones: 16_384
            tombstoneBytes: 8MiB
          }
          restart: .never
          durability: {
            recovery: .required
            confidentiality: .hostEncrypted
            points: "workflow::FulfillmentPoint"
            events: ["workflow::fulfillmentSignals"]
            adapters: ["w.std/sqlite-workflow@1"]
            history: {
              recordsPerRoot: 8_192
              bytesPerRoot: 64MiB
              retainedBytes: 512MiB
            }
            step: { inputBytes: 4MiB, outputBytes: 4MiB, attempts: 8 }
            inbox: {
              itemsPerRoot: 1_024
              bytesPerRoot: 8MiB
              retainedBytes: 64MiB
              tombstonesPerRoot: 4_096
            }
            retention: { terminal: 604_800<si.s> }
          }
        },
      ]

      exports: ["lastLight", "orders"]

      packings: [
        {
          name: "single-process"
          units: [
            {
              name: "main"
              entry: true
              providers: [
                "lastLight",
                "orders",
                "oracle",
                "aromaProbe",
                "billing",
                "diningRoom",
              ]
              supervisors: ["fulfillment"]
            },
          ]
        },
        {
          name: "split-services"
          units: [
            {
              name: "gateway"
              entry: true
              providers: ["lastLight", "orders"]
              supervisors: ["fulfillment"]
            },
            {
              name: "planning"
              providers: ["oracle", "aromaProbe"]
            },
            {
              name: "finance"
              providers: ["billing"]
            },
            {
              name: "dining"
              providers: ["diningRoom"]
            },
          ]
        },
      ]
    },
    {
      name: "restaurant-client"
      servicePolicy: {
        resolution: .startup
        wrpcProfile: "service-default"
        links: [
          .component,
          .wrpc(transports: [.ipc, .network]),
        ]
        dynamicRebinding: .deny
      }
      services: []
      requirements: [
        { service: "restaurant::lastLight", default: .import("lastLight") },
      ]
      imports: [
        {
          binding: "lastLight"
          protocol: "restaurant::RestaurantApi"
          source: .deployment
        },
      ]
      supervisors: []
      exports: []
      packings: [
        {
          name: "entry-only"
          units: [{ name: "main", entry: true, providers: [] }]
        },
      ]
    },
    {
      name: "wifi-edge"
      servicePolicy: {
        resolution: .startup
        wrpcProfile: "service-default"
        links: [
          .component,
          .wrpc(transports: [.ipc, .network]),
        ]
        dynamicRebinding: .deny
      }
      services: []
      requirements: [
        { service: "wifi_app::wifiSessions", default: .import("wifiSessions") },
      ]
      imports: [
        {
          binding: "wifiSessions"
          protocol: "wifi::WifiSessionApi"
          source: .deployment
        },
      ]
      supervisors: []
      exports: []
      packings: [
        {
          name: "entry-only"
          units: [{ name: "main", entry: true, providers: [] }]
        },
      ]
    },
    {
      name: "observatory-client"
      servicePolicy: {
        resolution: .startup
        wrpcProfile: "service-default"
        links: [
          .component,
          .wrpc(transports: [.ipc, .network]),
        ]
        dynamicRebinding: .deny
        streamLimits: {
          open: 64
          perStream: {
            itemBytes: 256KiB
            inFlight: { items: 8, bytes: 1MiB }
            queued: { items: 8, bytes: 1MiB }
            traversalPerItem: 1MiB
            capabilitySlots: 64
            rate: {
              itemsPerSecond: 4_096
              bytesPerSecond: 16MiB
              burstItems: 8
              burstBytes: 1MiB
            }
          }
          total: {
            inFlight: { items: 256, bytes: 16MiB }
            queued: { items: 256, bytes: 16MiB }
            capabilitySlots: 1_024
            rate: {
              itemsPerSecond: 32_768
              bytesPerSecond: 128MiB
              burstItems: 256
              burstBytes: 16MiB
            }
          }
        }
      }
      services: []
      requirements: [
        { service: "observatory_app::satelliteSwarm", default: .import("satellites") },
        { service: "observatory_app::horizonMonitor", default: .import("horizonMonitor") },
      ]
      imports: [
        {
          binding: "satellites"
          protocol: "orbit::SatelliteApi"
          keyType: "orbit::SatelliteId"
          source: .deployment
        },
        {
          binding: "horizonMonitor"
          protocol: "horizon::HorizonMonitorApi"
          source: .deployment
        },
      ]
      supervisors: []
      exports: []
      packings: [
        {
          name: "entry-only"
          units: [{ name: "main", entry: true, providers: [] }]
        },
      ]
    },
    {
      name: "benchmark-host"
      services: []
      imports: [
        {
          binding: "benchmark-database"
          capability: "std.database::Database"
          dialect: .postgresql
          source: .host
        },
        {
          binding: "cached-worlds"
          capability: "std.cache::LocalCache"
          keyType: "i32"
          valueType: "benchmark_app::CachedWorld"
          source: .host
        },
      ]
      supervisors: []
      exports: []
      packings: [
        {
          name: "entry-only"
          units: [{ name: "main", entry: true, providers: [] }]
        },
      ]
    },
  ]

  executionProfiles: [
    {
      name: "native-bounded"
      tasks: {
        live: 16_384
        frameBytes: 256MiB
        timers: 16_384
      }
      pools: [
        {
          name: "cpu"
          capacity: { minimum: 1, maximum: .hostCpuQuota }
        },
        {
          name: "blocking"
          capacity: { minimum: 1, maximum: 32 }
        },
      ]
      domains: {
        main: {
          pool: "cpu"
          ready: { jobs: 1, frameBytes: 16MiB }
          fallback: .reject
        }
        io: {
          pool: "cpu"
          ready: { jobs: 8_192, frameBytes: 128MiB }
          fallback: .reject
        }
        network: {
          pool: "cpu"
          ready: { jobs: 8_192, frameBytes: 128MiB }
          fallback: .io
        }
        compute: {
          pool: "cpu"
          ready: { jobs: 8_192, frameBytes: 256MiB }
          fallback: .reject
        }
        blocking: {
          pool: "blocking"
          ready: { jobs: 256, frameBytes: 32MiB }
          fallback: .reject
        }
        custom: [
          {
            id: "execution::thermal"
            capabilities: [.serial]
            pool: "cpu"
            ready: { jobs: 1_024, frameBytes: 64MiB }
            fallback: .reject
          },
          {
            id: "domain_oracle::catalog"
            capabilities: [.concurrent, .barrierDispatch]
            pool: "cpu"
            ready: { jobs: 1_024, frameBytes: 64MiB }
            fallback: .reject
          },
          {
            id: "synchronization::apology"
            capabilities: [.serial]
            pool: "cpu"
            ready: { jobs: 1_024, frameBytes: 64MiB }
            fallback: .reject
          },
        ]
        dynamicSerial: {
          pool: "cpu"
          live: 128
          aggregateReady: { jobs: 4_096, frameBytes: 64MiB }
          laneMaximum: { jobs: 256, frameBytes: 4MiB }
        }
      }
      cleanup: {
        asyncGrace: 5<s>
        blockingDrainGrace: 30<s>
      }
    },
    {
      name: "edge-bounded"
      tasks: {
        live: 8_192
        frameBytes: 128MiB
        timers: 16_384
      }
      pools: [
        {
          name: "cpu"
          capacity: { minimum: 1, maximum: .hostCpuQuota }
        },
      ]
      domains: {
        main: {
          pool: "cpu"
          ready: { jobs: 1, frameBytes: 8MiB }
          fallback: .reject
        }
        io: {
          pool: "cpu"
          ready: { jobs: 8_192, frameBytes: 128MiB }
          fallback: .reject
        }
        network: {
          pool: "cpu"
          ready: { jobs: 8_192, frameBytes: 128MiB }
          fallback: .io
        }
        compute: {
          pool: "cpu"
          ready: { jobs: 2_048, frameBytes: 64MiB }
          fallback: .reject
        }
      }
      cleanup: {
        asyncGrace: 2<s>
        blockingDrainGrace: 0<s>
      }
    },
    {
      name: "benchmark-bounded"
      tasks: {
        live: 65_536
        frameBytes: 1GiB
        timers: 65_536
      }
      pools: [
        {
          name: "cpu"
          capacity: { minimum: 1, maximum: .hostCpuQuota }
        },
      ]
      domains: {
        main: {
          pool: "cpu"
          ready: { jobs: 1, frameBytes: 16MiB }
          fallback: .reject
        }
        io: {
          pool: "cpu"
          ready: { jobs: 65_536, frameBytes: 1GiB }
          fallback: .reject
        }
        network: {
          pool: "cpu"
          ready: { jobs: 65_536, frameBytes: 1GiB }
          fallback: .io
        }
        compute: {
          pool: "cpu"
          ready: { jobs: 8_192, frameBytes: 256MiB }
          fallback: .reject
        }
      }
      cleanup: {
        asyncGrace: 2<s>
        blockingDrainGrace: 0<s>
      }
    },
  ]

  products: [
    {
      name: "last-light-native"
      kind: .executable
      module: "app"
      host: "w.host/native-process@1"
      targets: ["desktop"]
      runtime: "restaurant-core"
      packing: "single-process"
      executionProfile: "native-bounded"
      capabilities: [.stdio, .network, .signals, .clock, .services, .devices]
      resources: [
        .action("compile-final-menu", output: "bytecode"),
      ]
    },
    {
      name: "last-light-tui"
      kind: .executable
      module: "app"
      entry: "LastLightTui"
      host: "w.host/native-process@1"
      targets: ["desktop"]
      runtime: "restaurant-core"
      packing: "single-process"
      executionProfile: "native-bounded"
      capabilities: [.stdio, .signals, .clock, .services, .devices]
      resources: [
        .action("compile-final-menu", output: "bytecode"),
      ]
    },
    {
      name: "last-light-worker"
      kind: .component
      module: "worker_app"
      entry: "LastLightWorker"
      host: "w.host/http-worker@1"
      targets: ["wasi"]
      runtime: "restaurant-client"
      packing: "entry-only"
      executionProfile: "edge-bounded"
      capabilities: [.network, .clock, .services]
      limits: {
        http: {
          activeRequests: 1_024
          queuedRequests: 2_048
          queuedBytes: 64MiB
          connections: 8_192
          targetBytes: 16KiB
          headerBytes: 64KiB
          headerFields: 128
          bodyBytes: 1MiB
        }
      }
    },
    {
      name: "last-light-wifi"
      kind: .component
      module: "wifi_app"
      entry: "LastLightWifi"
      host: "w.host/http-worker@1"
      targets: ["server", "wasi"]
      runtime: "wifi-edge"
      packing: "entry-only"
      executionProfile: "edge-bounded"
      capabilities: [.network, .clock, .storage, .secrets, .services]
      limits: {
        http: {
          activeRequests: 512
          queuedRequests: 1_024
          queuedBytes: 8MiB
          connections: 4_096
          targetBytes: 8KiB
          headerBytes: 32KiB
          headerFields: 64
          bodyBytes: 8KiB
        }
      }
    },
    {
      name: "last-light-simulation"
      kind: .executable
      module: "simulation_app"
      entry: "LastLightSimulation"
      host: "w.host/native-process@1"
      targets: ["desktop"]
      executionProfile: "native-bounded"
      capabilities: [.stdio]
    },
    {
      name: "last-light-observatory"
      kind: .executable
      module: "observatory_app"
      entry: "LastLightObservatory"
      host: "w.host/native-process@1"
      targets: ["server"]
      runtime: "observatory-client"
      packing: "entry-only"
      executionProfile: "native-bounded"
      capabilities: [.stdio, .network, .clock, .services]
    },
    {
      name: "last-light-horizon-w"
      kind: .staticLibrary
      module: "horizon"
      exports: ["horizon::classifyHorizon"]
      abi: .wExact
      targets: ["desktop"]
    },
    {
      name: "last-light-horizon-c"
      kind: .dynamicLibrary
      module: "abi"
      exports: ["abi::ll_horizon_classify_v1"]
      abi: .c
      runtime: .none
      panic: .forbid
      targets: ["desktop"]
    },
    {
      name: "last-light-mobile"
      kind: .executable
      module: "mobile_app"
      entry: "LastLightMobile"
      host: "w.host/mobile-app@1"
      hostBindings: [
        { slot: "app.resume", handler: "mobile_app::resume" },
        { slot: "app.suspend", handler: "mobile_app::suspend" },
        { slot: "app.notification", handler: "mobile_app::notification" },
      ]
      targets: ["mobile"]
      runtime: "restaurant-client"
      packing: "entry-only"
      executionProfile: "edge-bounded"
      capabilities: [.network, .clock, .notifications, .services]
    },
    {
      name: "last-light-controller"
      kind: .firmware
      module: "controller_app"
      entry: "LastLightController"
      host: "w.host/firmware@1"
      hostBindings: [
        { slot: "device.tick", handler: "controller_app::sampleTick" },
        { slot: "device.interrupt", handler: "controller_app::interrupt" },
      ]
      // W-1238: retention is product data verified in the final payload.
      linkPlacements: [
        .section(
          symbol: "controller_app::reset",
          name: ".vectors.reset",
          retain: true,
        ),
      ]
      targets: ["embedded"]
      capabilities: [.clock, .interrupts, .mmio]
    },
    {
      name: "last-light-audio"
      kind: .firmware
      module: "audio_app"
      entry: "LastLightAudio"
      host: "w.host/audio-device@1"
      targets: ["embedded"]
      capabilities: [.clock, .interrupts, .mmio]
    },
    // Descriptor-only roots derive a source-backed module manifest. A host
    // product with concrete launch sites derives the closed device artifact.
    {
      name: "last-light-accelerators"
      kind: .deviceBundle
      module: "ai_harness"
      exports: ["ai_harness::lastLightKernels"]
      host: "w.host/accelerator-module@1"
      targets: ["accelerators"]
      capabilities: [.deviceMemory, .workgroups]
    },
    {
      name: "last-light-ai-lab"
      kind: .executable
      module: "ai_lab_app"
      entry: "LastLightAiLab"
      host: "w.host/native-process@1"
      targets: ["server"]
      capabilities: [.stdio]
    },
    {
      name: "last-light-benchmark"
      kind: .benchmark
      module: "benchmark_app"
      entry: "LastLightBenchmark"
      host: "w.host/http-worker@1"
      hostConfiguration: {
        responseHeaders: {
          server: .literal("W")
          date: .cached(maximumAge: 1<s>)
        }
        compression: .deny
        logging: {
          requests: .deny
          disk: .deny
          console: .deny
        }
      }
      targets: ["server"]
      runtime: "benchmark-host"
      packing: "entry-only"
      executionProfile: "benchmark-bounded"
      capabilities: [.network, .clock, .random, .database, .cache, .templates]
      limits: {
        http: {
          activeRequests: 16_384
          queuedRequests: 16_384
          queuedBytes: 64MiB
          connections: 32_768
          targetBytes: 16KiB
          headerBytes: 64KiB
          headerFields: 128
          bodyBytes: 1MiB
        }
        database: {
          connections: 256
          queuedOperations: 4_096
          queuedBytes: 64MiB
          pipelineDepth: 20
        }
        cache: {
          entries: 10_000
          activeLoads: 256
          queuedLoads: 4_096
        }
      }
    },
  ]

  targetSets: [
    {
      name: "desktop"
      targets: [
        "x86_64-unknown-linux-gnu",
        "aarch64-unknown-linux-gnu",
        "x86_64-pc-windows-msvc",
        "aarch64-apple-darwin",
      ]
    },
    {
      name: "server"
      targets: [
        "x86_64-unknown-linux-gnu",
        "aarch64-unknown-linux-gnu",
      ]
    },
    {
      name: "mobile"
      targets: [
        "aarch64-unknown-linux-android",
        "x86_64-unknown-linux-android",
        "aarch64-apple-ios",
        "aarch64-apple-ios-simulator",
      ]
    },
    {
      name: "wasi"
      targets: ["wasm32-wasip3"]
    },
    {
      name: "wasi-compat"
      targets: ["wasm32-wasip2"]
    },
    {
      name: "embedded"
      targets: [
        "thumbv7em-none-eabihf",
        "riscv32-unknown-none-elf",
      ]
    },
    {
      name: "accelerators"
      targets: [
        "nvptx64-nvidia-cuda",
        "amdgcn-amd-amdhsa",
        "spirv64-unknown-vulkan",
      ]
    },
  ]

  dependencies: [
    {
      alias: "menuCompiler"
      package: "last-light/menu-compiler"
      version: "^0.1.0"
      use: .build
      source: .registry("w")
    },
    {
      alias: "chart"
      package: "fiction/chart"
      version: "^1.2.0"
      use: .product
      source: .registry("w")
    },
  ]

  build: {
    network: .deny
    environment: []
    actions: [
      {
        name: "compile-final-menu"
        tool: .dependency("menuCompiler", product: "menu-compiler")
        inputs: [
          {
            binding: "menu"
            source: .file("menus/final.menu")
            maximumBytes: 64KiB
          },
        ]
        outputs: [
          {
            binding: "bytecode"
            kind: .resource
            maximumBytes: 1MiB
          },
        ]
      },
    ]
    profiles: [
      {
        name: "debug"
        optimize: .none
        checks: .full
        debug: .sidecar
        cpuPolicy: .portable
        memory: {
          generalAllocator: .system
          representation: .portable
          dynamicAllocation: .allow
          automaticStorage: .infer
        }
      },
      {
        name: "release"
        optimize: .speed
        checks: .safe
        debug: .sidecar
        cpuPolicy: .portable
        memory: {
          generalAllocator: .system
          representation: .optimized
          dynamicAllocation: .allow
          automaticStorage: .infer
        }
      },
      {
        name: "benchmark"
        optimize: .speed
        checks: .safe
        debug: .none
        cpuPolicy: .explicit
        memory: {
          generalAllocator: .system
          representation: .optimized
          dynamicAllocation: .allow
          automaticStorage: .infer
        }
      },
      {
        name: "benchmark-mimalloc"
        optimize: .speed
        checks: .safe
        debug: .none
        cpuPolicy: .explicit
        memory: {
          generalAllocator: .runtime(
            contract: "w.runtime/allocator.mimalloc@3",
            mode: .default,
          )
          representation: .optimized
          dynamicAllocation: .allow
          automaticStorage: .infer
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
    ownerDigest: "sha256:891b79dc85b8bdc4f320a5a066dae1bae2692e23234313c5d9d608332e229827"
    authorities: [
      {
        kind: .registry
        locator: "w"
        origin: { object: "sha256:1031dc23028d28379d8d168895ce528b833d3015fcfe87f9797119a9f7fff538", length: 463 }
        evidence: {
          object: "sha256:b8ad0a1726c8b88bd1eaaf4f8259c1b4cf828f71bfe0309f5bdac4a0ddad6ae4"
          length: 3014
          observedRootVersion: 2
        }
        record: { object: "sha256:b03f34017556739702b2cf6546d3484b7f11cbadb5487d35b7519e6c7ccd7bd2", length: 3539 }
      },
    ]
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
        authority: { object: "sha256:1031dc23028d28379d8d168895ce528b833d3015fcfe87f9797119a9f7fff538", length: 463 }
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
        authority: { object: "sha256:1031dc23028d28379d8d168895ce528b833d3015fcfe87f9797119a9f7fff538", length: 463 }
        name: "fiction/chart"
        version: "1.2.0"
        source: .registry("w")
        dependencies: []
      },
      {
        id: "sha256:3e896724d6f6f896039de431e424309492cd2b2b30b47c0cf33e1f0f1b064de0"
        authority: { object: "sha256:1031dc23028d28379d8d168895ce528b833d3015fcfe87f9797119a9f7fff538", length: 463 }
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
