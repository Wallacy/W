// Shared performance-first portable release flags for executable comparisons.
// Size-only opt levels and host-specific CPU tuning are intentionally excluded.
// A future/local native recipe must be identified separately as release-native
// and never compare against these portable cells.

export const NATIVE_RECIPE_PROFILE = "release-native";

export const C_RELEASE_FLAGS = Object.freeze([
  "-O3",
  "-flto",
  "-ffunction-sections",
  "-fdata-sections",
  "-Wl,--gc-sections",
  "-s",
]);

export const C_WHOLE_PROGRAM_FLAG = "-fwhole-program";

export function cReleaseFlags({ wholeProgram = false } = {}) {
  return wholeProgram ? [...C_RELEASE_FLAGS, C_WHOLE_PROGRAM_FLAG] : [...C_RELEASE_FLAGS];
}

export const RUST_RELEASE_FLAGS = Object.freeze([
  "-C", "opt-level=3",
  "-C", "lto=fat",
  "-C", "codegen-units=1",
  "-C", "panic=abort",
  "-C", "debuginfo=0",
  "-C", "strip=symbols",
  "-C", "link-dead-code=no",
  "-C", "link-arg=/OPT:REF",
  "-C", "link-arg=/OPT:ICF",
  "-C", "link-arg=/INCREMENTAL:NO",
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
