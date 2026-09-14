import { existsSync } from "node:fs";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

const root = resolve(import.meta.dir, "..");
const seed = resolve(root, "compiler", "seed-c");

function fail(message) {
  throw new Error(`PARALLEL MLIR0: ${message}`);
}

function run(command, args, { cwd = root } = {}) {
  const result = Bun.spawnSync({
    cmd: [command, ...args], cwd, stdout: "pipe", stderr: "pipe",
  });
  if (result.exitCode !== 0) {
    const details = result.stderr.toString().trim() ||
      result.stdout.toString().trim();
    fail(`${command} ${args.join(" ")} failed${details ? `: ${details}` : ""}`);
  }
  return result;
}

function exactExit(command, args, expectedExit) {
  const result = Bun.spawnSync({
    cmd: [command, ...args], cwd: root, stdout: "pipe", stderr: "pipe",
  });
  if (result.exitCode !== expectedExit || result.stdout.length !== 0 ||
      result.stderr.length !== 0)
    fail(`${command} ${args.join(" ")} expected silent exit ${expectedExit}, got ${result.exitCode}`);
}

async function tools() {
  if (process.platform === "win32") {
    const { defaultCacheDirectory, validateManifest, validateMaterialized } =
      await import("./acquire-mlir0-windows.mjs");
    const { createHash } = await import("node:crypto");
    const cache = defaultCacheDirectory();
    const manifestBytes = await readFile(join(root, "tooling",
      "mlir0-windows-toolchain.json"));
    const manifest = JSON.parse(manifestBytes.toString("utf8"));
    const errors = validateManifest(manifest);
    if (errors.length !== 0) fail(`invalid toolchain manifest: ${errors.join("; ")}`);
    const pin = createHash("sha256").update(manifestBytes).digest("hex");
    const materialized = await validateMaterialized(cache, manifest, pin);
    const get = (name) => {
      const relative = materialized.tools?.[name]?.relativePath;
      const value = relative ? resolve(cache, relative) : null;
      if (!value || !existsSync(value) ||
          materialized.tools[name].version !== "23.1.1")
        fail(`materialized ${name} is unavailable or not 23.1.1`);
      return value;
    };
    return {
      mlirOpt: get("mlir-opt.exe"),
      mlirTranslate: get("mlir-translate.exe"),
      llc: get("llc.exe"),
      lldLink: get("lld-link.exe"),
    };
  }
  const get = (name) => Bun.which(name) ?? fail(`${name} is unavailable`);
  const result = {
    mlirOpt: get("mlir-opt"), mlirTranslate: get("mlir-translate"),
    llc: get("llc"),
  };
  for (const tool of Object.values(result)) {
    const version = run(tool, ["--version"]).stdout.toString();
    if (!version.includes("23.1.1")) fail(`${tool} is not pinned 23.1.1`);
  }
  return result;
}

const cmake = Bun.which("cmake") ?? fail("cmake is unavailable");
const ninja = Bun.which("ninja") ?? fail("ninja is unavailable");
void ninja;
const directory = await mkdtemp(join(tmpdir(), "w-parallel-mlir0-"));
try {
  run(cmake, ["-S", seed, "-B", directory, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release", "-DW_SEED_C_STANDARD=23"]);
  run(cmake, ["--build", directory, "--target", "w_seed_hir0_tests",
    "--parallel", "2"]);
  const suffix = process.platform === "win32" ? ".exe" : "";
  const witness = resolve(directory, `w_seed_hir0_tests${suffix}`);
  const emitted = run(witness, ["--emit-parallel-entry-mlir"]);
  if (emitted.stderr.length !== 0 || emitted.stdout.includes(Buffer.from([0])))
    fail("emitter produced stderr or embedded NUL");
  const text = emitted.stdout.toString("utf8");
  for (const marker of [
    "w-seed-mlir0-parallel-entry-2",
    "func.func private @w_coop_fn_0",
    "func.func private @w_coop_fn_1",
    "func.func private @w_coop_fn_2",
    "func.func @w_seed_parallel_task_0(%arg0: i64) -> i64",
    "func.func @w_seed_parallel_task_1(%arg0: i64, %arg1: i64) -> i64",
    "func.call @w_coop_fn_2(%arg0, %arg1) : (i64, i64) -> i64",
  ]) if (!text.includes(marker)) fail(`artifact omits ${marker}`);
  for (const forbidden of ["@w_coop_fn_3", "@w_seed_cooperative_core",
    "%cv0 = arith.constant 20 : i64", "%cv2 = arith.constant 2 : i64",
    "llvm.target_triple", "@main", "@mainCRTStartup"])
    if (text.includes(forbidden)) fail(`artifact unexpectedly contains ${forbidden}`);

  const available = await tools();
  const input = join(directory, "parallel-entry.mlir");
  const lowered = join(directory, "parallel-entry-lowered.mlir");
  const llvm = join(directory, "parallel-entry.ll");
  const windowsObject = join(directory, "parallel-entry.obj");
  const linuxObject = join(directory, "parallel-entry.o");
  await writeFile(input, emitted.stdout);
  run(available.mlirOpt, [input, "-o", lowered, "--convert-arith-to-llvm",
    "--convert-func-to-llvm", "--reconcile-unrealized-casts",
    "--canonicalize", "--cse", "--verify-each"]);
  run(available.mlirTranslate, ["--mlir-to-llvmir", lowered, "-o", llvm]);
  const llvmText = await readFile(llvm, "utf8");
  if (!/define i64 @w_seed_parallel_task_0\(i64 %[^)]+\)/u.test(llvmText) ||
      !/define i64 @w_seed_parallel_task_1\(i64 %[^,]+, i64 %[^)]+\)/u.test(llvmText) ||
      !llvmText.includes("define internal i64 @w_coop_fn_0") ||
      !llvmText.includes("define internal i64 @w_coop_fn_1") ||
      !llvmText.includes("define internal i64 @w_coop_fn_2") ||
      llvmText.includes("@w_coop_fn_3") ||
      /call i64 @w_coop_fn_[12]\(i64 (?:20|2)(?:,|\))/u.test(llvmText)) {
    const definitions = llvmText.split("\n")
      .filter((line) => line.includes("define ")).join(" | ");
    fail(`translated LLVM symbol closure differs: ${definitions}`);
  }
  run(available.llc, ["-filetype=obj", "-mtriple=x86_64-pc-windows-msvc",
    "-O3", llvm, "-o", windowsObject]);
  run(available.llc, ["-filetype=obj", "-mtriple=x86_64-unknown-linux-gnu",
    "-relocation-model=pic", "-O3", llvm, "-o", linuxObject]);
  const windowsBytes = await readFile(windowsObject);
  const linuxBytes = await readFile(linuxObject);
  if (windowsBytes.length < 2 || windowsBytes[0] !== 0x64 ||
      windowsBytes[1] !== 0x86)
    fail("Windows x64 COFF object header is invalid");
  if (linuxBytes.length < 4 || linuxBytes[0] !== 0x7f ||
      linuxBytes[1] !== 0x45 || linuxBytes[2] !== 0x4c ||
      linuxBytes[3] !== 0x46)
    fail("Linux x86-64 ELF object header is invalid");
  let emittedExecution = "not-run";
  if (process.platform === "win32") {
    const { findWindowsSdkKernel32 } = await import("./windows-build-support.mjs");
    const sdk = await findWindowsSdkKernel32();
    const adapterSource = resolve(seed, "runtime",
      "w_seed_parallel_entry_windows0.ll");
    const adapterObject = join(directory, "parallel-entry-adapter.obj");
    const executable = join(directory, "parallel-entry.exe");
    run(available.llc, ["-filetype=obj", "-mtriple=x86_64-pc-windows-msvc",
      "-O3", adapterSource, "-o", adapterObject]);
    run(available.lldLink, ["/entry:mainCRTStartup", "/subsystem:console",
      "/nodefaultlib", "/machine:x64", `/out:${executable}`,
      windowsObject, adapterObject, sdk.path, "/Brepro", "/opt:ref",
      "/opt:icf", "/incremental:no"]);
    exactExit(executable, [], 0);
    emittedExecution = "windows-crt-free-runtime-values";
  }
  console.log(`PARALLEL MLIR0: MLIR=23.1.1 tasks=2 runtimeArguments=3 reachableFunctions=3 emittedExecution=${emittedExecution} windowsObjectBytes=${windowsBytes.length} linuxObjectBytes=${linuxBytes.length}`);
} finally {
  await rm(directory, { recursive: true, force: true });
}
