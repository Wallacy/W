# WVUI0 — first-party web application hosts

`WVUI0` is a human-and-machine candidate study for hosting one web application
across embedded WebViews, external browsers, browser/Wasm targets, and bounded
terminal projections. It does not implement or promote a W API, syntax,
provider, runtime, packaging format, or security claim.

The intended product is closer in scope to Tauri, Electrobun, or Wails than to
a native widget toolkit: HTML/CSS/JavaScript owns the desktop UI, while W owns
the application state, typed native commands, lifecycle, packaging policy, and
platform capabilities. Native game rendering remains a separate product. Both
may share lower window, event-loop, input, and opaque native-handle primitives;
graphics providers continue to own render surfaces, swapchains, and presentation.

## Selected direction for later prototyping

Keep these layers independent:

1. a provider-neutral window and main-thread event-loop foundation, shared with
   graphics packages without imposing a renderer;
2. a host-agnostic web application containing immutable assets and a typed
   command/event protocol;
3. an embedded provider for WebView2, WKWebView, WebKitGTK, or Android WebView;
4. an external-browser provider with explicit loopback origin and session
   lifecycle;
5. a browser/Wasm host that reuses the application and protocol but does not
   pretend to be a desktop WebView;
6. optional terminal semantic and full/raster providers;
7. an application-shell packager for assets, platform metadata, signing,
   updates, and declared sidecars.

The first-party default should use the operating-system WebView. A bundled CEF
or other pinned engine is an explicit package/profile choice because it changes
artifact size, update ownership, vulnerability response, and runtime closure.
The study does not select a particular wrapper implementation.

## Provider matrix

| Strategy | Candidates | Required disclosure |
|---|---|---|
| System embedded WebView | WebView2, WKWebView, WebKitGTK, Android WebView | engine/runtime version, target, features, profile, lifecycle |
| Minimal wrapper reference | `webview/webview`, Wry | platform dependencies, event loop, native handles, bridge |
| Application-shell reference | Tauri, Electrobun, Wails | assets, permissions, IPC, sidecars, signing, distribution, update |
| External browser | WebUI or a Neutralino-like loopback host | bind address, origin, one-use token, browser identity, close |
| Bundled engine | CEF, Qt WebEngine | exact engine, package size, update/security owner |
| Browser/Wasm | browser host APIs | browser capabilities, event-loop policy, assets, bridge |
| Terminal semantic | HTML/accessibility projection | supported semantics, focus, commands, live state |
| Terminal raster | Carbonyl/Chromium-like provider | browser dependency, rendering and input contract |

## Typed bridge and lifecycle

The bridge reuses W services rather than inventing a WebView-specific channel.
Every command declares owner, direction, version, bounded request and response,
error type, and capability. Reflection must not expose an automatic native
proxy. Unknown commands, stale generations, oversized payloads, and undeclared
capabilities fail closed.

The UI/main-thread domain owns native windows, views, menus, and engine calls.
Structured children may serve assets, process bridge messages, supervise
sidecars, or drive an external browser. Closing the application cancels those
children, drains bounded events, invalidates the session generation, and then
releases native resources in provider order. Provider initialization failure is
observable and never selects another provider implicitly.

## Sidecars and packaging

A sidecar is not arbitrary shell authority. The application manifest must bind
an exact logical name to target-specific executable artifacts, hashes,
signatures, supported target/ABI, argument grammar, environment allowlist,
working-directory policy, stdin/stdout/stderr bounds, capability set, sandbox
policy, restart policy, and structured shutdown deadline. The packager rejects
missing, ambiguous, unsigned-when-required, or incompatible artifacts.

The web content can invoke only declared typed commands; it cannot construct a
path or spawn an arbitrary executable. Sidecars are children of the application
lifecycle, with explicit cancellation and process-tree cleanup. Their runtime
closure and update identity remain distinct from the W host executable.

The application shell also owns immutable asset identity, compression, custom
origin mapping, CSP generation, platform bundle metadata, code signing,
notarization, installer/update artifacts, and reproducibility receipts. These
are build/package responsibilities, not `std.webview` methods.

## Closed security defaults

- Local assets use an immutable custom origin rather than `file://` identity.
- Remote content receives no native commands by default.
- Navigation, new windows, downloads, devtools, permissions, persistent
  profiles, sidecars, and arbitrary evaluation are denied by default.
- Loopback hosts bind only to loopback and validate `Host`, `Origin`, session
  generation, and a one-use token that never appears in a URL or log.
- Message size, rate, traversal, allocation, and outstanding-request limits are
  explicit.
- Terminal providers sanitize control bytes, ANSI, OSC, and links.
- A WebView is not treated as a sandbox or as proof of origin isolation.

## Availability and platform composition

`WVUI0` composes [AVF0](../avf0-availability-feature/) to separate package
features, target facts, installed engine/runtime facts, and product policy.
When embedded UI is required, absence of its provider is a startup error.
Browser, terminal, bundled-engine, and embedded modes are explicit product
choices, never silent fallbacks.

The lower window/event-loop package may be shared with game/graphics packages,
but WebView DOM rendering and GPU scene rendering remain independent. A hybrid
application may compose both providers only through explicit surfaces and
lifecycles.

## Required executable witness

The same application must run with identical typed commands and close behavior
in at least two real native WebView providers before API promotion. Additional
witnesses cover an external browser and browser/Wasm. Mocks are useful for unit
tests but cannot establish provider support.

Each native witness must exercise asset loading, navigation rejection, typed
commands, stale messages, payload limits, window close, cancellation, and
provider unavailability. The sidecar witness additionally exercises artifact
selection, hash failure, denied arguments, bounded streams, crash, timeout,
cancel, and descendant cleanup.

Planned measurements are cold start, first contentful paint, resident memory,
idle CPU, package/cache size, bridge latency and throughput, input jank,
sidecar startup/shutdown, and update size. This study contains no measurements
or performance ranking.

## Evidence boundary

Current evidence is limited to official architecture and API documentation plus
this crosspoint inventory. Missing evidence includes real provider receipts,
W compilation and execution, security adversarial execution, packaging,
sidecars, accessibility review, and performance results. Stop the study on a
stale provider assumption, an unreceipted engine, a mock-only smoke, a security
bypass, an implicit fallback, or an attempted API/specification promotion.

## Primary references

- [Tauri architecture](https://v2.tauri.app/concept/architecture/)
- [Tauri capabilities](https://v2.tauri.app/security/capabilities/)
- [Tauri sidecars](https://v2.tauri.app/develop/sidecar/)
- [Electrobun architecture](https://framework.blackboard.sh/electrobun/guides/architecture/overview/)
- [Wails runtime](https://wails.io/docs/reference/runtime/intro/)
- [Wry](https://github.com/tauri-apps/wry)
- [`webview/webview`](https://github.com/webview/webview)
- [WebView2 distribution](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution)
- [WebView2 security](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/security)
- [WKWebView](https://developer.apple.com/documentation/webkit/wkwebview)
- [WebKitGTK WebView](https://webkitgtk.org/reference/webkit2gtk/stable/class.WebView.html)
- [Android WebView](https://developer.android.com/develop/ui/views/layout/webapps/webview)
- [WAI-ARIA](https://www.w3.org/TR/wai-aria/)

The sources were reviewed on 2026-09-25. They support only the architectural
observations above, not W conformance or implementation claims.
