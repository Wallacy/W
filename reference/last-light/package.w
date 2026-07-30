// Data-only package manifest for the Last Light reference product.

package {
  schema: "w.package/1"
  name: "last-light/restaurant"
  version: "0.1.0"
  edition: "2026"

  moduleSets: [
    {
      namespace: "restaurant"
      root: "."
      include: ["*.w"]
      exclude: ["package.w"]
      layout: .fileStem
    },
  ]

  runtimeGraphs: [
    {
      name: "restaurant-core"
      providers: [
        {
          binding: "last-light"
          protocol: "restaurant.restaurant::RestaurantApi"
          implementation: "restaurant.restaurant::LastLightRestaurant"
          scope: .process
          mailbox: { items: 64, bytes: 8MiB, inFlight: 1 }
          inject: {
            pantry: .service("pantry")
            ovens: .service("ovens")
            oracle: .service("oracle")
            probe: .service("aroma-probe")
            billing: .service("billing")
            diningRoom: .service("dining-room")
          }
        },
        {
          binding: "orders"
          protocol: "restaurant.supervision::OrderCoordinatorApi"
          implementation: "restaurant.supervision::OrderCoordinator"
          scope: .keyed(keyType: "restaurant.domain::OrderId")
          mailbox: { items: 8, bytes: 1MiB, inFlight: 1 }
          inject: {
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
          binding: "aroma-probe"
          protocol: "restaurant.hardware::AromaProbeApi"
          implementation: "restaurant.hardware::AromaProbeService"
          scope: .process
          mailbox: { items: 16, bytes: 1MiB, inFlight: 1 }
          inject: {
            device: .capability("aroma-device")
          }
        },
        {
          binding: "billing"
          protocol: "restaurant.billing::BillingApi"
          implementation: "restaurant.billing::BillingLedger"
          scope: .process
          mailbox: { items: 64, bytes: 4MiB, inFlight: 1 }
          inject: {
            gateway: .service("payment-gateway")
          }
        },
        {
          binding: "dining-room"
          protocol: "restaurant.dining::DiningRoomApi"
          implementation: "restaurant.dining::PrismDiningRoom"
          scope: .process
          mailbox: { items: 32, bytes: 4MiB, inFlight: 1 }
          inject: {
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
          binding: "payment-gateway"
          protocol: "restaurant.billing::PaymentGatewayApi"
          source: .deployment
        },
        {
          binding: "audience"
          protocol: "restaurant.dining::AudienceApi"
          source: .deployment
        },
        {
          binding: "aroma-device"
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
              "aroma-probe",
              "billing",
              "dining-room",
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

      exports: ["last-light", "orders"]

      packings: [
        {
          name: "single-process"
          units: [
            {
              name: "main"
              entry: true
              providers: [
                "last-light",
                "orders",
                "oracle",
                "aroma-probe",
                "billing",
                "dining-room",
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
              providers: ["last-light", "orders"]
              supervisors: ["fulfillment"]
            },
            {
              name: "planning"
              providers: ["oracle", "aroma-probe"]
            },
            {
              name: "finance"
              providers: ["billing"]
            },
            {
              name: "dining"
              providers: ["dining-room"]
            },
          ]
        },
      ]
    },
    {
      name: "restaurant-client"
      providers: []
      imports: [
        {
          binding: "last-light"
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
      providers: []
      imports: [
        {
          binding: "wifi-sessions"
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
      providers: []
      imports: [
        {
          binding: "satellites"
          protocol: "restaurant.orbit::SatelliteApi"
          keyType: "restaurant.orbit::SatelliteId"
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

  products: [
    {
      name: "last-light-native"
      kind: .executable
      module: "restaurant.app"
      entry: ".default"
      host: "w.host/native-process@1"
      targets: ["desktop"]
      runtime: "restaurant-core"
      packing: "single-process"
      capabilities: [.stdio, .network, .signals, .clock, .services, .devices]
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
      capabilities: [.stdio, .network, .clock, .services]
    },
    {
      name: "last-light-mobile"
      kind: .executable
      module: "restaurant.mobile_app"
      entry: "LastLightMobile"
      host: "w.host/mobile-app@1"
      targets: ["mobile"]
      runtime: "restaurant-client"
      packing: "entry-only"
      capabilities: [.network, .clock, .notifications, .services]
    },
    {
      name: "last-light-controller"
      kind: .firmware
      module: "restaurant.controller_app"
      entry: "LastLightController"
      host: "w.host/firmware@1"
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
      entry: "LastLightKernels"
      host: "w.host/accelerator-module@1"
      targets: ["accelerators"]
      capabilities: [.deviceMemory, .workgroups]
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

  dependencies: []

  build: {
    network: .deny
    environment: []
    profiles: [
      {
        name: "debug"
        optimize: .none
        checks: .full
        debug: .sidecar
      },
      {
        name: "release"
        optimize: .speed
        checks: .safe
        debug: .sidecar
        targetCpu: "portable"
      },
      {
        name: "benchmark"
        optimize: .speed
        checks: .safe
        debug: .none
        targetCpu: "native-declared"
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
