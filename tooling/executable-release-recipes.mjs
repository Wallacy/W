// Shared performance-first release flags for executable comparisons.
// Size-only opt levels and host-specific CPU tuning are intentionally excluded.

export const C_RELEASE_FLAGS = Object.freeze([
  "-O3",
  "-flto",
  "-ffunction-sections",
  "-fdata-sections",
  "-Wl,--gc-sections",
  "-s",
]);

export const RUST_RELEASE_FLAGS = Object.freeze([
  "-C", "opt-level=3",
  "-C", "lto=fat",
  "-C", "codegen-units=1",
  "-C", "panic=abort",
  "-C", "debuginfo=0",
  "-C", "strip=symbols",
  "-C", "link-arg=/DEBUG:NONE",
]);

export const W_MLIR_OPT_FLAGS = Object.freeze([
  "--verify-each",
  "--canonicalize",
  "--cse",
]);

export const W_LLC_FLAGS = Object.freeze([
  "-O3",
  "-filetype=obj",
  "-mtriple=x86_64-pc-windows-msvc",
]);

export const W_LLD_LINK_FLAGS = Object.freeze([
  "/entry:mainCRTStartup",
  "/subsystem:console",
  "/nodefaultlib",
  "/machine:x64",
  "/opt:ref",
  "/opt:icf",
  "/incremental:no",
]);
