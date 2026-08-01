// Data-only package manifest for the Last Light reference product.

package {
  schema: "w.package/1"
  authority: .registry("w")
  name: "last-light/restaurant"
  version: "0.1.0"
  edition: "2026"
  namespace: "restaurant"
  license: {
    expression: "MIT"
    files: ["LICENSE"]
  }
  publish: {
    source: .required
    files: [
      .modules,
      .path("deployments/local.w"),
      .path("deployments/distributed.w"),
      .path("deployments/benchmark.w"),
      .path("menus/final.menu"),
      .path("README.md"),
      .path("BUILD.md"),
      .path("LICENSE"),
    ]
  }

  moduleSets: [
    {
      name: "restaurant-modules"
      activation: .always
      namespace: "restaurant"
      root: "."
      include: ["*.w"]
      exclude: ["package.w", "workspace.w"]
      layout: .fileStem
    },
    {
      name: "native-terminal-posix"
      activation: .selected
      namespace: "restaurant.platform"
      root: "platform/posix"
      include: ["*.w"]
      layout: .fileStem
    },
    {
      name: "native-terminal-windows"
      activation: .selected
      namespace: "restaurant.platform"
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

  runtimeGraphs: [
    {
      name: "restaurant-core"
      links: [
        { requirement: "restaurant.restaurant::lastLight", target: .service("lastLight") },
        { requirement: "restaurant.supervision::orderCoordinators", target: .service("orders") },
        { requirement: "restaurant.kitchen::pantry", target: .service("pantry") },
        { requirement: "restaurant.kitchen::ovens", target: .service("ovens") },
        { requirement: "restaurant.oracle::oracle", target: .service("oracle") },
        { requirement: "restaurant.hardware::aromaProbe", target: .service("aromaProbe") },
        { requirement: "restaurant.billing::billing", target: .service("billing") },
        { requirement: "restaurant.dining::diningRoom", target: .service("diningRoom") },
      ]
      providers: [
        {
          binding: "lastLight"
          protocol: "restaurant.restaurant::RestaurantApi"
          implementation: "restaurant.restaurant::LastLightRestaurant"
          scope: .process
          mailbox: { items: 64, bytes: 8MiB, inFlight: 1 }
          arguments: {
            pantry: .service("pantry")
            ovens: .service("ovens")
            oracle: .service("oracle")
            probe: .service("aromaProbe")
            billing: .service("billing")
            diningRoom: .service("diningRoom")
          }
        },
        {
          binding: "orders"
          protocol: "restaurant.supervision::OrderCoordinatorApi"
          implementation: "restaurant.supervision::OrderCoordinator"
          scope: .keyed(keyType: "restaurant.domain::OrderId")
          mailbox: { items: 8, bytes: 1MiB, inFlight: 1 }
          arguments: {
            identity: .serviceIdentity
            fulfillment: .supervisor("fulfillment", key: .serviceIdentity)
          }
        },
        {
          binding: "oracle"
          protocol: "restaurant.oracle::OracleApi"
          implementation: "restaurant.oracle::TableOracle"
          scope: .process
          mailbox: { items: 64, bytes: 4MiB, inFlight: 1 }
        },
        {
          binding: "aromaProbe"
          protocol: "restaurant.hardware::AromaProbeApi"
          implementation: "restaurant.hardware::AromaProbeService"
          scope: .process
          mailbox: { items: 16, bytes: 1MiB, inFlight: 1 }
          arguments: {
            device: .capability("aromaDevice")
          }
        },
        {
          binding: "billing"
          protocol: "restaurant.billing::BillingApi"
          implementation: "restaurant.billing::BillingLedger"
          scope: .process
          mailbox: { items: 64, bytes: 4MiB, inFlight: 1 }
          arguments: {
            gateway: .service("paymentGateway")
          }
        },
        {
          binding: "diningRoom"
          protocol: "restaurant.dining::DiningRoomApi"
          implementation: "restaurant.dining::PrismDiningRoom"
          scope: .process
          mailbox: { items: 32, bytes: 4MiB, inFlight: 1 }
          arguments: {
            audience: .service("audience")
          }
        },
      ]

      imports: [
        {
          binding: "pantry"
          protocol: "restaurant.kitchen::PantryApi"
          source: .deployment
        },
        {
          binding: "ovens"
          protocol: "restaurant.kitchen::OvenApi"
          source: .deployment
        },
        {
          binding: "paymentGateway"
          protocol: "restaurant.billing::PaymentGatewayApi"
          source: .deployment
        },
        {
          binding: "audience"
          protocol: "restaurant.dining::AudienceApi"
          source: .deployment
        },
        {
          binding: "aromaDevice"
          capability: "restaurant.hardware::AromaProbeDevice"
          source: .host
        },
      ]

      supervisors: [
        {
          binding: "fulfillment"
          keyType: "restaurant.domain::OrderId"
          inputType: "restaurant.workflow::FulfillmentInput"
          progressType: "restaurant.domain::ServiceStage"
          outputType: "restaurant.domain::Receipt"
          failureType: "restaurant.restaurant::RestaurantError"
          operation: "restaurant.workflow::fulfillOrderDurably"
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
            points: "restaurant.workflow::FulfillmentPoint"
            events: ["restaurant.workflow::fulfillmentSignals"]
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
      links: [
        { requirement: "restaurant.restaurant::lastLight", target: .service("lastLight") },
      ]
      providers: []
      imports: [
        {
          binding: "lastLight"
          protocol: "restaurant.restaurant::RestaurantApi"
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
      links: [
        { requirement: "restaurant.wifi::wifiSessions", target: .service("wifiSessions") },
      ]
      providers: []
      imports: [
        {
          binding: "wifiSessions"
          protocol: "restaurant.wifi::WifiSessionApi"
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
      links: [
        { requirement: "restaurant.orbit::satelliteSwarm", target: .service("satellites") },
        {
          requirement: "restaurant.horizon::horizonMonitor"
          target: .service("horizonMonitor")
        },
      ]
      providers: []
      imports: [
        {
          binding: "satellites"
          protocol: "restaurant.orbit::SatelliteApi"
          keyType: "restaurant.orbit::SatelliteId"
          source: .deployment
        },
        {
          binding: "horizonMonitor"
          protocol: "restaurant.horizon::HorizonMonitorApi"
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
      providers: []
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
          valueType: "restaurant.benchmark_app::CachedWorld"
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
      parallelDefault: .compute
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
        default: {
          pool: "cpu"
          ready: { jobs: 16_384, frameBytes: 256MiB }
          fallback: .reject
        }
        io: {
          pool: "cpu"
          ready: { jobs: 8_192, frameBytes: 128MiB }
          fallback: .default
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
            id: "restaurant.execution::LastLightDomain.thermal"
            capabilities: [.concurrent, .parallel]
            pool: "cpu"
            ready: { jobs: 1_024, frameBytes: 64MiB }
            fallback: .compute
          },
        ]
      }
      cleanup: {
        asyncGrace: 5<s>
        blockingDrainGrace: 30<s>
      }
    },
    {
      name: "edge-bounded"
      parallelDefault: .compute
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
        default: {
          pool: "cpu"
          ready: { jobs: 8_192, frameBytes: 128MiB }
          fallback: .reject
        }
        io: {
          pool: "cpu"
          ready: { jobs: 8_192, frameBytes: 128MiB }
          fallback: .default
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
      parallelDefault: .compute
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
        default: {
          pool: "cpu"
          ready: { jobs: 65_536, frameBytes: 1GiB }
          fallback: .reject
        }
        io: {
          pool: "cpu"
          ready: { jobs: 65_536, frameBytes: 1GiB }
          fallback: .default
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
      module: "restaurant.app"
      entry: ".default"
      host: "w.host/native-process@1"
      hostBindings: [
        { slot: "process.signal", handler: "restaurant.app::shutdown" },
      ]
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
      module: "restaurant.app"
      entry: "LastLightTui"
      host: "w.host/native-process@1"
      hostBindings: [
        { slot: "process.signal", handler: "restaurant.app::shutdown" },
      ]
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
      module: "restaurant.worker_app"
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
      module: "restaurant.wifi_app"
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
      module: "restaurant.simulation_app"
      entry: "LastLightSimulation"
      host: "w.host/native-process@1"
      targets: ["desktop"]
      executionProfile: "native-bounded"
      capabilities: [.stdio]
    },
    {
      name: "last-light-observatory"
      kind: .executable
      module: "restaurant.observatory_app"
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
      module: "restaurant.horizon"
      exports: ["restaurant.horizon::classifyHorizon"]
      abi: .wExact
      targets: ["desktop"]
    },
    {
      name: "last-light-horizon-c"
      kind: .dynamicLibrary
      module: "restaurant.abi"
      exports: ["restaurant.abi::ll_horizon_classify_v1"]
      abi: .c
      runtime: .none
      panic: .forbid
      targets: ["desktop"]
    },
    {
      name: "last-light-mobile"
      kind: .executable
      module: "restaurant.mobile_app"
      entry: "LastLightMobile"
      host: "w.host/mobile-app@1"
      hostBindings: [
        { slot: "app.resume", handler: "restaurant.mobile_app::resume" },
        { slot: "app.suspend", handler: "restaurant.mobile_app::suspend" },
        { slot: "app.notification", handler: "restaurant.mobile_app::notification" },
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
      module: "restaurant.controller_app"
      entry: "LastLightController"
      host: "w.host/firmware@1"
      hostBindings: [
        { slot: "device.tick", handler: "restaurant.controller_app::sampleTick" },
        { slot: "device.interrupt", handler: "restaurant.controller_app::interrupt" },
      ]
      targets: ["embedded"]
      capabilities: [.monotonicClock, .interrupts, .mmio]
    },
    {
      name: "last-light-audio"
      kind: .firmware
      module: "restaurant.audio_app"
      entry: "LastLightAudio"
      host: "w.host/audio-device@1"
      targets: ["embedded"]
      capabilities: [.monotonicClock, .interrupts, .mmio]
    },
    {
      name: "last-light-accelerators"
      kind: .deviceBundle
      module: "restaurant.ai_harness"
      exports: ["restaurant.ai_harness::lastLightKernels"]
      host: "w.host/accelerator-module@1"
      targets: ["accelerators"]
      capabilities: [.deviceMemory, .workgroups]
    },
    {
      name: "last-light-ai-lab"
      kind: .executable
      module: "restaurant.ai_lab_app"
      entry: "LastLightAiLab"
      host: "w.host/native-process@1"
      targets: ["server"]
      capabilities: [.stdio]
    },
    {
      name: "last-light-benchmark"
      kind: .benchmark
      module: "restaurant.benchmark_app"
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
