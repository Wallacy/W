# Use-case readiness

Current assessment: 2026-09-10. This is a non-normative review and proposed work
queue, not a new language contract or a support claim.
[DESIGN.md](DESIGN.md) defines behavior. [PLATFORM-SUPPORT.md](PLATFORM-SUPPORT.md)
records compiler hosts, emitted targets, cross-compilation edges, and support
evidence. A game is a use case, not a target triple.

The Last Light source/entry/product reuse plan is maintained in
[Shared modules and surface witnesses](reference/last-light/BUILD.md#35-shared-modules-and-surface-witnesses).

## Assessment rules

Three questions remain separate: can the language express the program, is its
platform contract defined, and does a real implementation execute it?
General FFI can enable an application without a first-party library. It does
not establish safe wrappers, portability, usability, or performance.

The current compiler executes bounded native programs such as Hello and
Restaurant. Linux/WSL and native Windows have bounded witnesses, with different
feature scopes. No row below claims a complete application platform. Contracts,
parseable examples, and host-side design oracles are not W execution evidence.
The platform catalog currently contains no fully supported target.

## Coverage and next evidence

Sections below refer to DESIGN unless another source is named. Each witness
uses the real compiler and provider. A mock or metadata check is insufficient.

| Use case and hosts | Existing design basis | Remaining design or integration work | Smallest useful execution witness |
|---|---|---|---|
| CLI and native tools: Linux, Windows, macOS | Process entry, ownership, errors, I/O, files; §§14.5, 20.8 | General compiler/runtime, process input and OS-string ABI, SDK packaging | One unchanged executable branches on runtime arguments, reads a file, reports errors, and closes owners |
| Terminal applications | Native process and I/O; WVUI0 terminal candidates | Terminal events, rendering, resize, terminal-state restoration, accessibility policy | Interactive screen with resize, input, cancellation, and restored terminal settings |
| Servers, networking, and distributed services | Structured execution, service boundaries, HTTP, streams, wRPC; §§12–14, 23.1 | Executable transport/TLS providers, overload and shutdown behavior, operational evidence | Two real processes exchange a request and bounded stream under disconnect and overload |
| Hosted HTTP/edge applications | `http-worker@1`, typed request/response and capability roots; §§14, 20.8.2 | Specific host ABI, runtime restrictions, packaging, and host integration | Deployed test handler streams a response and cleans up after client disconnect |
| Libraries, plugins, and trusted modules | C ABI, compiled interfaces, binary-first packages, registry and runner contracts; §§19–21 | Load/unload ABI, retained callbacks, signing/revocation, sandbox and registry implementations | A host loads a module, rejects tampering, calls it, and unloads after callback drain |
| Desktop HTML/CSS UI | [WVUI0](tooling/studies/wvui0-web-ui-providers/README.md) embedded/browser candidates | Provider selection and public API remain research; bridge authority, UI-thread affinity, engine lifecycle | Same typed command and close lifecycle in two real native WebView providers |
| Browser applications and browser games | Web data contracts, availability model, WVUI0 browser candidate | Browser execution/JS bridge, DOM or canvas integration, scheduling and GPU capability policy | W application handles browser input and presents frames without blocking the browser event loop |
| WASI components outside a browser | Explicit component and host boundaries, capability-based I/O; §§13, 20.8 | Wasm lowering, component ABI, selected WASI runtime/provider and packaging | One component executes with granted capabilities and rejects an unavailable import |
| Mobile applications and games: Android/iOS | `mobile-app@1`, host slots, capabilities; §20.8.2 and `mobile_app.w` | Concrete SDK/UI/input/audio adapters, suspend/resume and resource loss, signing and packaging | Install, interact, background/resume, rotate, and close on real target devices |
| Native 2D/3D games and visualization | Values/enums, views, explicit allocation, concurrency, math/tensors and C FFI; §§8–9, 12, 17–19 | No closed interactive graphics/input API found; define surface/frame lifetime, presentation, device loss, and provider boundaries | Small game with W simulation/rendering, real input, resize, presentation, and safe close |
| Audio and low-latency processing | `audio-device@1`, fixed buffers, effects, [audio example](reference/last-light/audio.w) | Concrete audio SDK/provider and callback contract; allocation/blocking/drop bounds and underrun policy | Real audio callback with measured deadline misses, bounded storage, and device shutdown |
| Scientific computing and CPU data processing | Matrix/tensor shapes, SIMD, numeric modes, mapped memory, data formats; §§15–18 | General lowering, vectorization/fusion, implementations of numerical/data providers | Runtime-sized workload with correctness oracle and matched C/Rust comparison |
| GPU compute and GPU rendering work | Explicit kernels, transfers, queues and completion; §12.7.2 | Kernel/backend/provider execution; rendering needs a separate presentation bridge; general recursive GPU programming is not the baseline | CPU/device result comparison plus delayed completion, cancellation, and device-loss handling |
| ML inference and training | Tensors, quantization, typed autodiff direction, DLPack; §17 | Implemented kernels/autodiff, model/operators, memory planning and distributed integration | Small model performs inference and one training step against an independent numerical reference |
| Notebooks and Python interoperability | Python adapters, sessions, bounded rich output; §§17.1, 24.11–24.12 | Executable bridges, kernel/session provider, cancellation and retained-object lifecycle | Real notebook cell computes, presents output, is canceled, and releases imported owners |
| Firmware, RTOS and systems components | `firmware@1`, MMIO, interrupts, linker placement and assembly; §§19.3, 20.8 | Board/OS profiles, boot/interrupt ABI, memory and timing evidence; no blanket hard-real-time promise | Boot on a board, service an interrupt, handle budget exhaustion, and verify placement |
| BPF, FPGA, HDL and ASIC extensions | Separate target/provider and timing boundaries; §23.6 and system-target contracts | Explicit restricted subsets, verifier or synthesis rules, timing model, and toolchain/provider evidence | Target-specific verified load or synthesized design with simulator/hardware equivalence |
| Build tools and test infrastructure | `build-transform@1`, `test-harness@1`, hermetic build and inline-test contracts | Executable harness/provider, real compiler diagnostics, reproducible package transforms | A source change fails a real test, then a corrected build reproduces its artifact |
| Cross-platform distribution | Target/profile/toolchain identities and [nine native baseline edges](PLATFORM-SUPPORT.md#cross-compilation-baseline) | Native tool bundles, SDK/sysroot/linker provenance, packaging and execution for each edge | Build on one native host and execute on another target; compilation alone does not pass |

WASI does not imply browser support. GPU compute does not imply graphics
presentation. A WebView does not imply a sandbox for trusted native commands.
General bounded allocation and cancellation do not imply hard-real-time bounds.
The DLPack device-kind catalog includes Metal and WebGPU. Those names describe
interoperability identities, not implemented W code generation or graphics
providers. Apple CPU support would not, by itself, establish Apple GPU support.

## What the game case exposes

The current design can express a game through general computation and platform
FFI, once implemented. A game engine, ECS, physics package, font library, or
scene graph need not become language features. A small engine can remain W
application code. Platform presentation and input still need an implementation.

The current GPU baseline is deliberately narrower than general W.
Section 12.7.2 rejects recursion, tasks, services, host I/O, dynamic dispatch,
and host FFI inside kernels. Therefore, a recursive CPU algorithm is not
automatically a supported GPU kernel. Changing this boundary requires a
separate design decision, not removal of a checker guard.

The presentation draft in [std/presentation](std/presentation/contracts.w)
provides bounded rich-output contracts. It is not a window, frame buffer,
swapchain, or interactive rendering API. The audio host profile and reference
example likewise do not establish an executable audio provider.

## Prioritized follow-up

| Priority | Finite question or deliverable | Acceptance boundary |
|---|---|---|
| P1 for interactive readiness | Specify the smallest presentation/input provider contract using existing owners, views, host slots and domains | Frame acquisition, valid access, submit, completion, resize/loss and close have explicit lifetimes; input authority and thread affinity are specified |
| P1 for low-latency claims | Reconcile frame/audio deadlines with structured cleanup and non-preemptible device work | No hidden unbounded wait on the deadline path; in-flight work remains bounded and live until safe completion; missed deadlines are observable |
| P2 | Build a small Last Light game/visualization witness without a pre-existing engine | Fixed-input simulation oracle plus actual window/input/frame evidence; headless success alone does not pass presentation |
| P2 | Evaluate richer GPU expressiveness on irregular workloads | Compare explicit kernels with bounded recursion or flattened work queues; record memory, launch and transfer costs before extending the baseline |
| P2 | Close each platform claim through its smallest real application witness | Bind source, host, target, ABI/provider and result evidence; retain candidate status when any boundary is missing |

The initial game witness should use CPU rendering and a minimal platform
bridge. GPU rendering and audio are separate extensions, not prerequisites for
the first interactive result. SDL or a direct native adapter are candidates,
not selected dependencies. No new keyword or mandatory game framework is
proposed by this review.

For future measurements, reuse the executable catalog and WBench. Record
build latency, binary size, resident memory, allocations, CPU time, frame-time
distribution, input latency, and relevant GPU transfer/launch costs. Compare
C/Rust with the same scene, numerical contract, provider, and optimization tier.
No benchmark samples or performance claims are created by this document.

## External boundary references

- [SDL window-surface update](https://wiki.libsdl.org/SDL3/SDL_UpdateWindowSurface)
  illustrates the minimal pixel-presentation boundary and its main-thread rule.
- [SDL GPU presentation acquisition](https://wiki.libsdl.org/SDL3/SDL_AcquireGPUSwapchainTexture)
  illustrates frame ownership, in-flight limits, possible waits, and thread affinity.
- [GLFW getting started](https://www.glfw.org/docs/latest/quick.html)
  separates window/context creation, rendering and event processing.

These references describe external APIs, not W implementation evidence. The
reported Bend2 demo and its current standard library were not independently
audited in this review. This assessment does not depend on their claimed scope.

Benchmark disposition: `not-applicable` (documentation-only readiness review).
