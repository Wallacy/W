import { existsSync } from "node:fs";
import { createHash } from "node:crypto";
import { mkdtemp, readFile, rm, stat, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { assertCrtFreeElf } from "./check-w-run.mjs";

const root = resolve(import.meta.dir, "..");
const seed = resolve(root, "compiler", "seed-c");
const cmake = Bun.which("cmake");
const ninja = Bun.which("ninja");
const compiler = ["cc", "gcc", "clang", "cl"]
  .map((name) => Bun.which(name)).find(Boolean);
const expected = Buffer.from("Cooperative 88\n");

function fail(message) {
  throw new Error(`COOPERATIVE MLIR0: ${message}`);
}

function run(command, args, { cwd = root, stdin = null } = {}) {
  const result = Bun.spawnSync({
    cmd: [command, ...args], cwd, stdin: stdin ?? undefined,
    stdout: "pipe", stderr: "pipe",
  });
  if (result.exitCode !== 0) {
    const details = result.stderr.toString().trim() ||
      result.stdout.toString().trim();
    fail(`${command} ${args.join(" ")} failed${details ? `: ${details}` : ""}`);
  }
  return result;
}

async function windowsTools() {
  const { defaultCacheDirectory, validateManifest, validateMaterialized } =
    await import("./acquire-mlir0-windows.mjs");
  const cache = defaultCacheDirectory();
  const manifestBytes = await readFile(join(root, "tooling",
    "mlir0-windows-toolchain.json"));
  const manifest = JSON.parse(manifestBytes.toString("utf8"));
  const manifestErrors = validateManifest(manifest);
  if (manifestErrors.length !== 0)
    fail(`Windows MLIR manifest is invalid: ${manifestErrors.join("; ")}`);
  const pinSha256 = createHash("sha256").update(manifestBytes).digest("hex");
  const materialized = await validateMaterialized(cache, manifest, pinSha256);
  const tool = (name) => {
    const relative = materialized.tools?.[name]?.relativePath;
    const value = relative ? resolve(cache, relative) : null;
    if (!value || !existsSync(value) || materialized.tools[name].version !== "23.1.1")
      fail(`materialized ${name} is unavailable or not 23.1.1`);
    return value;
  };
  return {
    mlirOpt: tool("mlir-opt.exe"),
    mlirTranslate: tool("mlir-translate.exe"),
    llc: tool("llc.exe"),
    lldLink: tool("lld-link.exe"),
    ldLld: tool("ld.lld.exe"),
  };
}

function hostTools() {
  const tool = (name) => Bun.which(name) ?? fail(`${name} is unavailable`);
  const tools = {
    mlirOpt: tool("mlir-opt"),
    mlirTranslate: tool("mlir-translate"),
    llc: tool("llc"),
    lldLink: null,
    ldLld: tool("ld.lld"),
  };
  for (const value of [tools.mlirOpt, tools.mlirTranslate, tools.llc,
    tools.ldLld]) {
    const version = run(value, ["--version"]).stdout.toString();
    if (!version.includes("23.1.1")) fail(`${value} is not pinned 23.1.1`);
  }
  return tools;
}

function lower(tools, input, output) {
  run(tools.mlirOpt, [
    input, "-o", output,
    "--convert-scf-to-cf", "--convert-arith-to-llvm",
    "--convert-func-to-llvm", "--convert-cf-to-llvm",
    "--reconcile-unrealized-casts", "--canonicalize", "--cse",
    "--verify-each",
  ]);
}

function exactExecution(result, label) {
  if (!result.stdout.equals(expected) || result.stderr.length !== 0)
    fail(`${label} output differs from Cooperative 88\\n`);
}

if (!cmake || !ninja || !compiler) {
  console.log("COOPERATIVE MLIR0: BLOCKED host CMake/Ninja/compiler unavailable");
  process.exit(0);
}

const directory = await mkdtemp(join(tmpdir(), "w-cooperative-mlir0-"));
try {
  run(cmake, ["-S", seed, "-B", directory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release"], { cwd: root });
  run(cmake, ["--build", directory, "--target",
    "w_seed_cooperative0_tests", "w_seed_wrt0_tests", "--parallel", "2"]);
  const suffix = process.platform === "win32" ? ".exe" : "";
  const witness = resolve(directory, `w_seed_cooperative0_tests${suffix}`);
  const wrt0Witness = resolve(directory, `w_seed_wrt0_tests${suffix}`);
  run(witness, []);
  run(wrt0Witness, []);
  const wrt0Ir = run(wrt0Witness, ["--emit-linux-x86-64-llvm"]).stdout;
  const wrt0Markers = ["target triple = \"x86_64-unknown-linux-gnu\"",
    "@w_seed_linux_initial_argc", "@w_seed_linux_initial_argv",
    "@w_seed_process_argc", "@w_seed_process_argv", "@w_seed_linux_start",
    "@write", ".globl _start", "syscall"];
  if (wrt0Markers.some((marker) => !wrt0Ir.includes(Buffer.from(marker))) ||
      wrt0Ir.includes(Buffer.from([0])) || wrt0Ir.at(-1) !== 0x0a)
    fail("shared Linux x86-64 WRT0 evidence is incomplete");

  const tools = process.platform === "win32" ? await windowsTools() : hostTools();
  const modes = [
    ["neutral", "--emit-target-neutral-mlir"],
    ["linux", "--emit-cooperative-linux-mlir"],
    ["windows", "--emit-cooperative-windows-mlir"],
  ];
  const files = {};
  for (const [name, mode] of modes) {
    const emitted = run(witness, [mode]).stdout;
    if (!emitted.includes(Buffer.from("func.call @w_seed_cooperative_core")) &&
        name !== "neutral") fail(`${name} projection omits the shared core call`);
    if (name === "neutral" && (emitted.includes(Buffer.from("llvm.target_triple")) ||
        emitted.includes(Buffer.from("llvm."))))
      fail("target-neutral core contains target-specific LLVM surface");
    const input = join(directory, `${name}.mlir`);
    const lowered = join(directory, `${name}-lowered.mlir`);
    await writeFile(input, emitted);
    lower(tools, input, lowered);
    files[name] = { input, lowered };
  }

  let windowsBytes = null;
  if (process.platform === "win32") {
    const { findWindowsSdkKernel32 } = await import("./windows-build-support.mjs");
    const sdk = await findWindowsSdkKernel32();
    const llvm = join(directory, "windows.ll");
    const object = join(directory, "windows.obj");
    const executable = join(directory, "cooperative.exe");
    run(tools.mlirTranslate, ["--mlir-to-llvmir", files.windows.lowered,
      "-o", llvm]);
    run(tools.llc, ["-filetype=obj", "-mtriple=x86_64-pc-windows-msvc",
      "-O3", llvm, "-o", object]);
    run(tools.lldLink, ["/entry:mainCRTStartup", "/subsystem:console",
      "/nodefaultlib", "/machine:x64", `/out:${executable}`, object,
      sdk.path, "/Brepro", "/opt:ref", "/opt:icf", "/incremental:no"]);
    exactExecution(run(executable, []), "Windows PE");
    windowsBytes = (await stat(executable)).size;
  }

  let linuxEvidence = "blocked";
  if (process.platform === "win32" && Bun.which("wsl.exe")) {
    const llvm = join(directory, "linux.ll");
    const object = join(directory, "linux.o");
    const wrt0Llvm = join(directory, "wrt0.ll");
    const wrt0Object = join(directory, "wrt0.o");
    const executable = join(directory, "cooperative-linux");
    await writeFile(wrt0Llvm, wrt0Ir);
    run(tools.mlirTranslate, ["--mlir-to-llvmir", files.linux.lowered,
      "-o", llvm]);
    run(tools.llc, ["-filetype=obj", "-mtriple=x86_64-unknown-linux-gnu",
      "-relocation-model=pic", "-O3", llvm, "-o", object]);
    run(tools.llc, ["-filetype=obj", "-mtriple=x86_64-unknown-linux-gnu",
      "-relocation-model=pic", "-O3", wrt0Llvm, "-o", wrt0Object]);
    run(tools.ldLld, ["-pie", "--no-dynamic-linker", "-e", "_start",
      "--gc-sections", "-z", "noexecstack", "-s", object, wrt0Object,
      "-o", executable]);
    assertCrtFreeElf(await readFile(executable));
    const wslExecutable = run("wsl.exe", ["-d", "Ubuntu", "--", "wslpath",
      "-a", executable.replaceAll("\\", "/")]).stdout.toString().trim();
    exactExecution(run("wsl.exe", ["-d", "Ubuntu", "--", wslExecutable]),
      "Windows-cross-linked Linux/WSL ELF");
    linuxEvidence = "windows-host-cross-link+target-execution";
  } else if (process.platform === "linux") {
    const linker = tools.ldLld;
    if (linker) {
      const llvm = join(directory, "linux.ll");
      const object = join(directory, "linux.o");
      const wrt0Llvm = join(directory, "wrt0.ll");
      const wrt0Object = join(directory, "wrt0.o");
      const executable = join(directory, "cooperative");
      await writeFile(wrt0Llvm, wrt0Ir);
      run(tools.mlirTranslate, ["--mlir-to-llvmir", files.linux.lowered,
        "-o", llvm]);
      run(tools.llc, ["-filetype=obj", "-mtriple=x86_64-unknown-linux-gnu",
        "-relocation-model=pic", "-O3", llvm, "-o", object]);
      run(tools.llc, ["-filetype=obj", "-mtriple=x86_64-unknown-linux-gnu",
        "-relocation-model=pic", "-O3", wrt0Llvm, "-o", wrt0Object]);
      run(linker, ["-pie", "--no-dynamic-linker", "-e", "_start",
        "--gc-sections", "-z", "noexecstack", "-s", object, wrt0Object,
        "-o", executable]);
      assertCrtFreeElf(await readFile(executable));
      exactExecution(run(executable, []), "native Linux ELF");
      linuxEvidence = "native-crt-free-link+execution";
    }
  }

  console.log(
    `COOPERATIVE MLIR0: core + Windows/Linux projections passed ` +
    `MLIR=23.1.1 windowsPeBytes=${windowsBytes ?? "not-run"} ` +
    `linux=${linuxEvidence}`,
  );
} finally {
  await rm(directory, { recursive: true, force: true });
}
