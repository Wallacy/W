import { existsSync } from "node:fs";
import { mkdtemp, readFile, rm, stat, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

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
  const { defaultCacheDirectory, MATERIALIZED_MANIFEST } =
    await import("./acquire-mlir0-windows.mjs");
  const cache = defaultCacheDirectory();
  const document = JSON.parse(await readFile(join(cache, MATERIALIZED_MANIFEST), "utf8"));
  if (document.destination !== cache || document.asset?.llvmTag !== "llvmorg-23.1.1")
    fail("materialized Windows MLIR manifest is not the pinned 23.1.1 cache");
  const tool = (name) => {
    const relative = document.tools?.[name]?.relativePath;
    const value = relative ? resolve(cache, relative) : null;
    if (!value || !existsSync(value) || document.tools[name].version !== "23.1.1")
      fail(`materialized ${name} is unavailable or not 23.1.1`);
    return value;
  };
  return {
    mlirOpt: tool("mlir-opt.exe"),
    mlirTranslate: tool("mlir-translate.exe"),
    llc: tool("llc.exe"),
    lldLink: tool("lld-link.exe"),
  };
}

function hostTools() {
  const tool = (name) => Bun.which(name) ?? fail(`${name} is unavailable`);
  const tools = {
    mlirOpt: tool("mlir-opt"),
    mlirTranslate: tool("mlir-translate"),
    llc: tool("llc"),
    lldLink: null,
  };
  for (const value of [tools.mlirOpt, tools.mlirTranslate, tools.llc]) {
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
    "w_seed_cooperative0_tests", "--parallel", "2"]);
  const suffix = process.platform === "win32" ? ".exe" : "";
  const witness = resolve(directory, `w_seed_cooperative0_tests${suffix}`);
  run(witness, []);

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
    run(tools.mlirTranslate, ["--mlir-to-llvmir", files.linux.lowered,
      "-o", llvm]);
    run(tools.llc, ["-filetype=obj", "-mtriple=x86_64-unknown-linux-gnu",
      "-relocation-model=pic", "-O3", llvm, "-o", object]);
    const wslPath = run("wsl.exe", ["-d", "Ubuntu", "--", "wslpath", "-a",
      directory.replaceAll("\\", "/")]).stdout.toString().trim();
    const gcc = Bun.spawnSync({ cmd: ["wsl.exe", "-d", "Ubuntu", "--",
      "bash", "-lc", "command -v gcc >/dev/null"], stdout: "pipe", stderr: "pipe" });
    if (gcc.exitCode === 0) {
      run("wsl.exe", ["-d", "Ubuntu", "--", "gcc", "-O3", "-s",
        `${wslPath}/linux.o`, "-o", `${wslPath}/cooperative`]);
      exactExecution(run("wsl.exe", ["-d", "Ubuntu", "--",
        `${wslPath}/cooperative`]), "Linux/WSL ELF");
      linuxEvidence = "cross-target-object+target-link+execution";
    }
  } else if (process.platform === "linux") {
    const linker = Bun.which("cc") ?? Bun.which("gcc");
    if (linker) {
      const llvm = join(directory, "linux.ll");
      const object = join(directory, "linux.o");
      const executable = join(directory, "cooperative");
      run(tools.mlirTranslate, ["--mlir-to-llvmir", files.linux.lowered,
        "-o", llvm]);
      run(tools.llc, ["-filetype=obj", "-mtriple=x86_64-unknown-linux-gnu",
        "-relocation-model=pic", "-O3", llvm, "-o", object]);
      run(linker, ["-O3", "-s", object, "-o", executable]);
      exactExecution(run(executable, []), "native Linux ELF");
      linuxEvidence = "native-target-link+execution";
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
