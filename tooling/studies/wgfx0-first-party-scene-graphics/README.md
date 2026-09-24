# WGFX0: first-party scene and graphics library

Status: **design-oracle input**. This study records a candidate standard-library
and first-party-package boundary. It does not implement a renderer, graphics
provider, shader compiler, scene package, asset loader, or public W API.

## Question

Can W provide the approachable, broadly composable 2D/3D surface that makes
Three.js productive for humans and code-generating agents, while producing a
smaller and more predictable native implementation across browser, desktop,
mobile, headless, and accelerator targets?

The answer is a layered candidate rather than one monolithic standard-library
module. Three.js is valuable here as an architectural and usability reference,
not as an API to clone and not as source to port.

## Lessons retained from Three.js

The useful high-level vocabulary is compact:

- a scene graph of local transforms;
- cameras, geometries, materials, textures, lights, and render targets;
- animation clips, actions, blending, bones, and morph targets;
- explicit loaders, with glTF as the first interoperable scene format;
- ordered post-processing passes and render-to-texture composition;
- a modern WebGPU route with a WebGL2 fallback;
- separately imported addons instead of forcing every loader, control, and
  effect into the core product.

The study deliberately does not inherit JavaScript's object graph, garbage
collection, runtime property lookup, stringly typed shader bindings, ambient
DOM ownership, or manual best-effort resource disposal. Three.js documents that
GPU buffers, textures, materials, render targets, and related resources need
explicit disposal; W should make that lifecycle structural rather than
conventional.

## Candidate W boundary

| Layer | Candidate responsibility | Must not own |
|---|---|---|
| Language/compiler | Fixed arrays and views, numeric and matrix semantics, SIMD, typed kernels, ownership, tasks, specialization, reachability elimination, and CPU/GPU lowering | Window systems, asset formats, or a specific graphics API |
| Bundled standard library | Portable color/image/extent/transform values, bounded asset bytes, stable resource and presentation protocols, and the minimum math shared by renderers | A complete scene engine, every file format, or a platform SDK |
| First-party scene package | Scene/world construction, cameras, geometry generation, materials, lighting, animation, culling, instancing, render-graph composition, and ergonomic defaults | Raw Vulkan, Metal, D3D12, WebGPU, WebGL, or OS window calls |
| First-party asset packages | glTF first; image, font, mesh compression, and other codecs only when their ownership and bounds are closed | Ambient filesystem/network access or silent codec fallback |
| Target providers | Devices, queues, surfaces, swapchains, command submission, synchronization, shader artifacts, presentation, and target SDK integration | W scene semantics or portable asset identity |
| Registry packages | Optional controls, effects, physics, importers, editors, and specialized render techniques | Authority over the bundled core contract |

This partition allows a small headless product to retain only math, transforms,
and culling, while a game or editor reaches the scene and provider packages it
actually uses. Whole-package and whole-program reachability remain responsible
for removing unused materials, loaders, passes, kernels, and providers.

## Data and ownership direction

The pleasant source surface may expose scene nodes and resources, but physical
representation is selected from verified use:

- transforms and visibility use dense value storage suitable for vectorization;
- stable public references use typed generational handles rather than raw
  pointers;
- immutable geometry, texture, sampler, and pipeline descriptions are values;
- GPU allocations and mapped ranges are explicit owners tied to one device and
  provider generation;
- objects remain virtual until identity, sharing, escape, or provider ownership
  requires materialization;
- scene traversal may lower to structure-of-arrays, breadth-first levels, or a
  compiler-selected static form instead of preserving an allocation per node;
- commands and render passes form a typed graph with declared read/write
  resources, so hazards, transitions, pass fusion, and dead-pass elimination can
  be checked before submission;
- close cancels and drains dependent work before releasing target resources.

Shared resources must not be guessed from scene reachability. A loader returns
an owned asset bundle whose resources and dependencies can be moved into a
scene, retained explicitly, or closed as one unit.

## Ergonomic direction

The primary package should support a concise path from declarations to a first
frame while preserving explicit lower layers. A generated program should be
able to construct a camera, a light, one material, and one mesh without writing
provider boilerplate. Advanced users must still be able to define kernels,
material graphs, render passes, resource budgets, residency, and synchronization
without escaping to a foreign language.

Materials should prefer a typed W expression or node graph that can lower to
the selected target shader representation. Raw target shader source remains an
explicit provider escape hatch, not the portable default. The same semantic
material may specialize for CPU reference rendering, WebGPU/SPIR-V, Vulkan,
Metal, D3D12, or another viable provider where capabilities permit it.

## MLIR and provider route

W-owned verified IR keeps scene, ownership, aliasing, resource, color, layout,
and synchronization semantics until they are explicit. MLIR is then an internal
lowering toolkit:

- `math`, `tensor`, `linalg`, and `vector` can support transforms, culling,
  skinning, image operations, and CPU reference paths;
- `gpu` can represent kernel structure and launches without defining the W
  scheduler or provider API;
- `spirv` can carry compatible shader/kernel semantics toward SPIR-V targets;
- target adapters still own surface creation, queues, shader compilation or
  translation, synchronization, and presentation.

No generic dialect removes the need for Metal, D3D12, Vulkan, browser WebGPU,
or window-system integration. Those remain explicit, versioned providers.

## First executable witnesses

Promote the candidate incrementally, in dependency order:

1. **Headless transform/culling.** One static hierarchy produces a deterministic
   visible-set and world-transform digest on scalar and SIMD CPU paths.
2. **Reference triangle/cube.** One typed scene renders to a bounded image with a
   W-owned CPU reference rasterizer and one real GPU provider; compare pixels or
   a tolerance-qualified image oracle.
3. **Textured glTF scene.** Load one bounded glTF asset bundle, render it, close
   every resource exactly once, and reject malformed offsets, sizes, cycles, and
   unsupported extensions.
4. **Animated instanced scene.** Exercise clips, blending, skinning or morph
   targets, culling, instancing, uploads, and deterministic frame capture.
5. **Render pipeline.** Compose multiple typed render and post-processing passes;
   verify resource hazards, pass order, fusion opportunities, and final output.

Each witness records compile time, frame time and distribution, CPU/GPU memory,
uploads, draw/dispatch counts, pipeline compilations, package size, binary size,
and dependency closure. Correctness, applicability, cold-start, steady-state,
and maximum-throughput lanes remain separate.

## AI-generation readiness

One-shot generation is a real usability target, but it is not evidence by
itself. The package should provide:

- a small orthogonal vocabulary and strong typed defaults;
- examples whose declarations and uses are complete;
- diagnostics that name the missing capability, resource, pass, or lifecycle
  edge;
- deterministic assets and captures for automated comparison;
- a bounded model-evaluation corpus that asks agents to produce a scene, repair
  a malformed scene, optimize a measured scene, and target another provider.

Generated programs must pass the same ownership, bounds, shader, resource,
security, and performance gates as human-authored programs.

## Dependencies and current gaps

The candidate depends on fixed arrays/views, matrices, SIMD, general modules and
generics, explicit ownership, resource-safe async work, stable provider ABIs,
accelerator/shader lowering, image buffers and codecs, asset packaging, and real
surface/device providers. None is made implemented by this study.

The first graphics work follows the ranked compiler roadmap. It may prepare
independent CPU reference algorithms and provider research, but it must not
displace scalar control flow, aggregates, ownership, errors, or the public
native product path.

## Evidence boundary

The current evidence is limited to official architecture and API documentation,
the existing W design contracts, and this layer/witness analysis. There is no W
graphics API, compiled scene, provider smoke, GPU output, image oracle, security
result, model study, or performance result.

## Official primary references

References were accessed on 2026-09-24.

| Source | Bounded claim used by this study |
|---|---|
| [Three.js fundamentals](https://threejs.org/manual/pages/fundamentals.html) | The approachable core composes renderer, scene graph, camera, geometry, material, texture, and lights over lower-level WebGL. |
| [Three.js scene graph](https://threejs.org/manual/pages/scenegraph.html) | Local-space hierarchy is the central composition model. |
| [Three.js animation system](https://threejs.org/manual/pages/animation-system.html) | Clips, actions, mixers, blending, bones, morph targets, and grouped animation form one system. |
| [Three.js installation and addons](https://threejs.org/manual/pages/installation.html) | Controls, loaders, and post-processing are imported separately from the engine fundamentals. |
| [Three.js glTF loader](https://threejs.org/docs/pages/GLTFLoader.html) | glTF loading includes scenes, cameras, animations, extensions, and separately configured compression/texture decoders. |
| [Three.js cleanup](https://threejs.org/manual/pages/cleanup.html) | GPU-related resources require explicit application-directed disposal. |
| [Three.js WebGPU renderer](https://threejs.org/manual/pages/webgpurenderer) | The modern renderer targets WebGPU, provides WebGL2 fallback, and uses typed shader nodes. |
| [Three.js WebGPU post-processing](https://threejs.org/manual/pages/webgpu-postprocessing.html) | Node composition and MRT support can combine effects and reduce render passes. |
| [Three.js package metadata](https://github.com/mrdoob/three.js/blob/dev/package.json) | The package separates core, addons, WebGPU, and shader-node entrypoints and maintains tree-shaking tests. |
| [MLIR GPU dialect](https://mlir.llvm.org/docs/Dialects/GPU/) | The dialect is a middle-level abstraction for GPU kernels, launches, and binaries rather than a complete host graphics API. |
| [MLIR SPIR-V dialect](https://mlir.llvm.org/docs/Dialects/SPIR-V/) | MLIR provides SPIR-V representation and conversion paths while host resource management remains outside the dialect. |
| [MLIR Vector dialect](https://mlir.llvm.org/docs/Dialects/Vector/) | Virtual vectors preserve retargetable vector semantics before target-specific lowering. |

## Stop condition

Stop and reopen the candidate if a proposed surface requires one physical
object per scene node, hides GPU/resource lifetime, grants ambient I/O, couples
portable semantics to one graphics API, silently falls back between providers,
or makes an implementation/performance claim without a real product witness.
