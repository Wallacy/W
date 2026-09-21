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

// Public C uses the installed LLVM driver and the MSVC ABI. Keep this recipe
// separate from the private GCC/MinGW composite because their linkers and
// runtime libraries are not interchangeable.
export const CLANG_C_TARGET = "x86_64-pc-windows-msvc";

export const CLANG_RELEASE_FLAGS = Object.freeze([
  "-O3",
  "-flto=full",
  "-ffunction-sections",
  "-fdata-sections",
  "-fuse-ld=lld",
  "-fms-runtime-lib=dll",
  "-Wl,/Brepro",
  "-Wl,/OPT:REF",
  "-Wl,/OPT:ICF",
  "-Wl,/INCREMENTAL:NO",
  "-Wl,/DEBUG:NONE",
]);

export function cReleaseFlags({ wholeProgram = false } = {}) {
  return wholeProgram ? [...C_RELEASE_FLAGS, C_WHOLE_PROGRAM_FLAG] : [...C_RELEASE_FLAGS];
}

export function clangReleaseFlags() {
  return [...CLANG_RELEASE_FLAGS];
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

export const PLATFORM_MINIMAL_C_RECIPE = "clang-c23-freestanding";
export const PLATFORM_MINIMAL_RUST_RECIPE = "rustc-edition-2024-no-std";

const PLATFORM_MINIMAL_C_WINDOWS_FLAGS = Object.freeze([
  "--target=x86_64-pc-windows-msvc",
  "-O3",
  "-flto=full",
  "-ffunction-sections",
  "-fdata-sections",
  "-ffreestanding",
  "-fno-builtin",
  "-fno-stack-protector",
  "-fno-unwind-tables",
  "-fno-asynchronous-unwind-tables",
  "-fno-ident",
  "-fuse-ld=lld",
  "-nostdlib",
  "-Wl,/Brepro",
  "-Wl,/ENTRY:w_entry",
  "-Wl,/SUBSYSTEM:CONSOLE",
  "-Wl,/MACHINE:X64",
  "-Wl,/OPT:REF",
  "-Wl,/OPT:ICF",
  "-Wl,/INCREMENTAL:NO",
  "-Wl,/DEBUG:NONE",
  "-lkernel32",
]);

const PLATFORM_MINIMAL_C_LINUX_FLAGS = Object.freeze([
  "--target=x86_64-unknown-linux-gnu",
  "-O3",
  "-flto=full",
  "-ffunction-sections",
  "-fdata-sections",
  "-ffreestanding",
  "-fno-builtin",
  "-fno-stack-protector",
  "-fno-unwind-tables",
  "-fno-asynchronous-unwind-tables",
  "-fno-ident",
  "-fuse-ld=lld",
  "-nostdlib",
  "-static",
  "-Wl,-e,_start",
  "-Wl,--gc-sections",
  "-Wl,--strip-all",
  "-Wl,--build-id=none",
]);

const PLATFORM_MINIMAL_RUST_COMMON_FLAGS = Object.freeze([
  "--edition=2024",
  "-C", "opt-level=3",
  "-C", "lto=fat",
  "-C", "codegen-units=1",
  "-C", "panic=abort",
  "-C", "debuginfo=0",
  "-C", "strip=symbols",
  "-C", "link-dead-code=no",
]);

const PLATFORM_MINIMAL_RUST_WINDOWS_FLAGS = Object.freeze([
  "-C", "default-linker-libraries=no",
  "-C", "link-arg=/ENTRY:w_entry",
  "-C", "link-arg=/SUBSYSTEM:CONSOLE",
  "-C", "link-arg=/MACHINE:X64",
  "-C", "link-arg=/OPT:REF",
  "-C", "link-arg=/OPT:ICF",
  "-C", "link-arg=/INCREMENTAL:NO",
  "-C", "link-arg=/DEBUG:NONE",
  "-C", "link-arg=/DEFAULTLIB:kernel32.lib",
]);

function platformMinimalLinuxRustFlags(linker) {
  if (typeof linker !== "string" || linker.length === 0) {
    throw new TypeError("platform-minimal Linux Rust recipe requires an LLD linker");
  }
  return [
    "-C", "force-unwind-tables=no",
    "-C", "linker-flavor=ld.lld",
    "-C", `linker=${linker}`,
    "-C", "default-linker-libraries=no",
    "-C", "link-arg=-static",
    "-C", "link-arg=--no-pie",
    "-C", "link-arg=-e",
    "-C", "link-arg=_start",
    "-C", "link-arg=--gc-sections",
    "-C", "link-arg=--strip-all",
    "-C", "link-arg=--build-id=none",
  ];
}

export function platformMinimalCFlags(platformTarget, dialectFlag = "-std=c23") {
  if (!["windows-x64", "linux-wsl-x64"].includes(platformTarget)) {
    throw new TypeError(`unsupported platform-minimal C target: ${platformTarget}`);
  }
  if (dialectFlag !== "-std=c23") {
    throw new TypeError(`platform-minimal C requires final C23 mode, got ${dialectFlag}`);
  }
  return [
    ...(platformTarget === "windows-x64" ? PLATFORM_MINIMAL_C_WINDOWS_FLAGS : PLATFORM_MINIMAL_C_LINUX_FLAGS),
    dialectFlag,
  ];
}

export function platformMinimalRustFlags(platformTarget, linker = undefined) {
  if (!["windows-x64", "linux-wsl-x64"].includes(platformTarget)) {
    throw new TypeError(`unsupported platform-minimal Rust target: ${platformTarget}`);
  }
  const target = platformTarget === "windows-x64"
    ? "x86_64-pc-windows-msvc"
    : "x86_64-unknown-linux-gnu";
  return [
    ...PLATFORM_MINIMAL_RUST_COMMON_FLAGS,
    `--target=${target}`,
    ...(platformTarget === "windows-x64"
      ? PLATFORM_MINIMAL_RUST_WINDOWS_FLAGS
      : platformMinimalLinuxRustFlags(linker)),
  ];
}

export function platformMinimalRecipeExamples(linker = "<host-ld.lld>") {
  return [
    {
      language: "C23",
      platform: "Windows x64 / MSVC",
      command: `clang ${platformMinimalCFlags("windows-x64").join(" ")} <source> -o <artifact>`,
    },
    {
      language: "Rust 2024",
      platform: "Windows x64 / MSVC",
      command: `rustc <source> ${platformMinimalRustFlags("windows-x64").join(" ")} -o <artifact>`,
    },
    {
      language: "C23",
      platform: "Linux x64 / WSL2",
      command: `clang ${platformMinimalCFlags("linux-wsl-x64").join(" ")} <source> -o <artifact>`,
    },
    {
      language: "Rust 2024",
      platform: "Linux x64 / WSL2",
      command: `rustc <source> ${platformMinimalRustFlags("linux-wsl-x64", linker).join(" ")} -o <artifact>`,
    },
  ];
}

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
  "/Brepro",
  "/entry:mainCRTStartup",
  "/subsystem:console",
  "/nodefaultlib",
  "/machine:x64",
  "/opt:ref",
  "/opt:icf",
  "/incremental:no",
  "/merge:.pdata=.rdata",
]);
