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

  products: [
    {
      name: "last-light-native"
      kind: .executable
      module: "restaurant.app"
      entry: ".default"
      host: "w.host/native-process@1"
      targets: ["desktop"]
      capabilities: [.stdio, .network, .signals, .clock]
    },
    {
      name: "last-light-worker"
      kind: .component
      module: "restaurant.worker_app"
      entry: "LastLightWorker"
      host: "w.host/http-worker@1"
      targets: ["wasi"]
      capabilities: [.network, .clock]
    },
    {
      name: "last-light-wifi"
      kind: .component
      module: "restaurant.wifi_app"
      entry: "LastLightWifi"
      host: "w.host/http-worker@1"
      targets: ["server", "wasi"]
      capabilities: [.network, .clock, .storage, .secrets]
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
      capabilities: [.stdio, .network, .clock]
    },
    {
      name: "last-light-mobile"
      kind: .executable
      module: "restaurant.mobile_app"
      entry: "LastLightMobile"
      host: "w.host/mobile-app@1"
      targets: ["mobile"]
      capabilities: [.network, .clock, .notifications]
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
      targets: ["server"]
      capabilities: [.network, .clock, .database]
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
      targets: ["wasm32-wasi-preview2"]
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
