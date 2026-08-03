// Bootstrap projection of the typed package contract.
// The final declaration DSL remains research in W/DESIGN.md section 6.3.

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
      root: "."
      include: ["*.w"]
      exclude: ["package.w", "workspace.w"]
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

  runtimeGraphs: [
    {
      name: "restaurant-core"
      servicePolicy: {
        resolution: .startup
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
        links: [
          .component,
          .wrpc(transports: [.ipc, .network]),
        ]
        dynamicRebinding: .deny
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
      targets: ["embedded"]
      capabilities: [.monotonicClock, .interrupts, .mmio]
    },
    {
      name: "last-light-audio"
      kind: .firmware
      module: "audio_app"
      entry: "LastLightAudio"
      host: "w.host/audio-device@1"
      targets: ["embedded"]
      capabilities: [.monotonicClock, .interrupts, .mmio]
    },
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
