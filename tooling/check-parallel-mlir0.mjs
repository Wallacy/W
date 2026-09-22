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
      ldLld: get("ld.lld.exe"),
    };
  }
  const get = (name) => Bun.which(name) ?? fail(`${name} is unavailable`);
  const result = {
    mlirOpt: get("mlir-opt"), mlirTranslate: get("mlir-translate"),
    llc: get("llc"), ldLld: get("ld.lld"),
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

  const measuredEmitted = run(witness, ["--emit-parallel-entry1-mlir"]);
  if (measuredEmitted.stderr.length !== 0 ||
      measuredEmitted.stdout.includes(Buffer.from([0])))
    fail("measured emitter produced stderr or embedded NUL");
  const measuredText = measuredEmitted.stdout.toString("utf8");
  for (let task = 0; task < 5; task += 1)
    if (!measuredText.includes(
      `func.func @w_seed_parallel_task_${task}(%arg0: i64) -> i64`))
      fail(`measured artifact omits task ${task}`);

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

  const measuredInput = join(directory, "parallel-entry1.mlir");
  const measuredLowered = join(directory, "parallel-entry1-lowered.mlir");
  const measuredLlvm = join(directory, "parallel-entry1.ll");
  const measuredWindowsObject = join(directory, "parallel-entry1.obj");
  const measuredLinuxObject = join(directory, "parallel-entry1.o");
  await writeFile(measuredInput, measuredEmitted.stdout);
  run(available.mlirOpt, [measuredInput, "-o", measuredLowered,
    "--convert-arith-to-llvm", "--convert-func-to-llvm",
    "--reconcile-unrealized-casts", "--canonicalize", "--cse",
    "--verify-each"]);
  run(available.mlirTranslate, ["--mlir-to-llvmir", measuredLowered,
    "-o", measuredLlvm]);
  const measuredLlvmText = await readFile(measuredLlvm, "utf8");
  for (let task = 0; task < 5; task += 1)
    if (!new RegExp(`define i64 @w_seed_parallel_task_${task}\\(i64 `, "u")
      .test(measuredLlvmText))
      fail(`translated measured LLVM omits task ${task}`);
  run(available.llc, ["-filetype=obj", "-mtriple=x86_64-pc-windows-msvc",
    "-O3", measuredLlvm, "-o", measuredWindowsObject]);
  run(available.llc, ["-filetype=obj", "-mtriple=x86_64-unknown-linux-gnu",
    "-relocation-model=pic", "-O3", measuredLlvm, "-o",
    measuredLinuxObject]);
  const measuredWindowsBytes = await readFile(measuredWindowsObject);
  const measuredLinuxBytes = await readFile(measuredLinuxObject);
  if (measuredWindowsBytes.length < 2 || measuredWindowsBytes[0] !== 0x64 ||
      measuredWindowsBytes[1] !== 0x86)
    fail("measured Windows x64 COFF object header is invalid");
  if (measuredLinuxBytes.length < 4 || measuredLinuxBytes[0] !== 0x7f ||
      measuredLinuxBytes[1] !== 0x45 || measuredLinuxBytes[2] !== 0x4c ||
      measuredLinuxBytes[3] !== 0x46)
    fail("measured Linux x86-64 ELF object header is invalid");

  const processEmitted = run(witness, ["--emit-process-parallel-mlir"]);
  if (processEmitted.stderr.length !== 0 || processEmitted.stdout.length === 0 ||
      processEmitted.stdout.includes(Buffer.from([0])))
    fail("process emitter produced stderr, no bytes, or an embedded NUL");
  const processText = processEmitted.stdout.toString("utf8");
  for (const marker of [
    "w-seed-mlir0-process-parallel-2",
    "llvm.target_triple = \"x86_64-unknown-linux-gnu\"",
    "llvm.func @w_seed_process_parallel_entry",
    "llvm.func @w_seed_parallel_task_0",
    "llvm.call @w_seed_process_arguments_is_empty(%p0)",
    "llvm.call @w_fn_0(%parallel_buffer, %parallel_cursor, %v2)",
    "llvm.call @w_seed_parallel_launch_task_0(%parallel_frame, %call1)",
    "llvm.call @w_seed_parallel_join_task_0(%parallel_frame, %parallel_result)",
  ]) if (!processText.includes(marker))
    fail(`process artifact omits ${marker}`);
  for (const forbidden of [
    "llvm.call @w_fn_2(%parallel_buffer",
    "llvm.call @w_seed_parallel_launch_task_0(%parallel_frame, 40",
  ]) if (processText.includes(forbidden))
    fail(`process artifact unexpectedly contains ${forbidden}`);

  const processInput = join(directory, "process-parallel.mlir");
  const processLowered = join(directory, "process-parallel-lowered.mlir");
  const processLlvm = join(directory, "process-parallel.ll");
  const processLinuxObject = join(directory, "process-parallel.o");
  await writeFile(processInput, processEmitted.stdout);
  run(available.mlirOpt, [processInput, "-o", processLowered,
    "--convert-arith-to-llvm", "--convert-func-to-llvm",
    "--reconcile-unrealized-casts", "--canonicalize", "--cse",
    "--verify-each"]);
  run(available.mlirTranslate, ["--mlir-to-llvmir", processLowered,
    "-o", processLlvm]);
  const processLlvmText = await readFile(processLlvm, "utf8");
  if (!processLlvmText.includes("define i32 @w_seed_process_parallel_entry") ||
      !processLlvmText.includes("define i64 @w_seed_parallel_task_0") ||
      !processLlvmText.includes("call i1 @w_seed_parallel_launch_task_0") ||
      !processLlvmText.includes("call i1 @w_seed_parallel_join_task_0"))
    fail("translated process LLVM omits explicit launch/join");
  const processRoot = processLlvmText.match(
    /define i32 @w_seed_process_parallel_entry\([^\{]+\) \{([\s\S]*?)\n\}/u);
  if (!processRoot || !processRoot[1].includes("call i64 @w_fn_0") ||
      !processRoot[1].includes("call i1 @w_seed_parallel_launch_task_0") ||
      !processRoot[1].includes("call i1 @w_seed_parallel_join_task_0") ||
      processRoot[1].includes("call i64 @w_fn_2"))
    fail("translated process root does not preserve private task edge");
  run(available.llc, ["-filetype=obj", "-mtriple=x86_64-unknown-linux-gnu",
    "-relocation-model=pic", "-O3", processLlvm, "-o", processLinuxObject]);
  const processLinuxBytes = await readFile(processLinuxObject);
  if (processLinuxBytes.length < 4 || processLinuxBytes[0] !== 0x7f ||
      processLinuxBytes[1] !== 0x45 || processLinuxBytes[2] !== 0x4c ||
      processLinuxBytes[3] !== 0x46)
    fail("process Linux x86-64 ELF object header is invalid");
  let processLinuxExecution = "not-run";
  if (process.platform === "win32" && Bun.which("wsl.exe")) {
    const linuxAdapterSource = resolve(seed, "runtime",
      "w_seed_process_parallel_linux0.S");
    const linuxAdapterObject = join(directory,
      "process-parallel-linux-adapter.o");
    const linuxExecutable = join(directory, "process-parallel-linux");
    const toWslPath = (path) => run("wsl.exe", ["-d", "Ubuntu", "--",
      "wslpath", "-a", path.replaceAll("\\", "/")])
      .stdout.toString().trim();
    const linuxAdapterSourceWsl = toWslPath(linuxAdapterSource);
    const linuxAdapterObjectWsl = toWslPath(linuxAdapterObject);
    run("wsl.exe", ["-d", "Ubuntu", "--", "gcc", "-c", "-x",
      "assembler-with-cpp", linuxAdapterSourceWsl, "-o",
      linuxAdapterObjectWsl]);
    run(available.ldLld, ["-pie", "--no-dynamic-linker", "-e", "_start",
      "--gc-sections", "-z", "noexecstack", "-s", processLinuxObject,
      linuxAdapterObject, "-o", linuxExecutable]);
    const linuxExecutableWsl = toWslPath(linuxExecutable);
    exactExit("wsl.exe", ["-d", "Ubuntu", "--", linuxExecutableWsl], 0);
    exactExit("wsl.exe", ["-d", "Ubuntu", "--", linuxExecutableWsl,
      "payload"], 0);
    exactExit("wsl.exe", ["-d", "Ubuntu", "--", linuxExecutableWsl, "!"], 3);
    processLinuxExecution = "windows-cross-link+linux-wsl-crt-free";
  } else if (process.platform === "linux") {
    const linuxCompiler = Bun.which("gcc") ?? fail("gcc is unavailable");
    const linuxAdapterSource = resolve(seed, "runtime",
      "w_seed_process_parallel_linux0.S");
    const linuxAdapterObject = join(directory,
      "process-parallel-linux-adapter.o");
    const linuxExecutable = join(directory, "process-parallel-linux");
    run(linuxCompiler, ["-c", "-x", "assembler-with-cpp",
      linuxAdapterSource, "-o", linuxAdapterObject]);
    run(available.ldLld, ["-pie", "--no-dynamic-linker", "-e", "_start",
      "--gc-sections", "-z", "noexecstack", "-s", processLinuxObject,
      linuxAdapterObject, "-o", linuxExecutable]);
    exactExit(linuxExecutable, [], 0);
    exactExit(linuxExecutable, ["payload"], 0);
    exactExit(linuxExecutable, ["!"], 3);
    processLinuxExecution = "native-linux-crt-free";
  }
  let processWindowsObjectBytes = 0;
  let processExecution = "not-run";
  if (process.platform === "win32") {
    const processWindowsEmitted = run(
      witness, ["--emit-process-parallel-windows-mlir"]);
    if (processWindowsEmitted.stderr.length !== 0 ||
        processWindowsEmitted.stdout.length === 0 ||
        processWindowsEmitted.stdout.includes(Buffer.from([0])))
      fail("Windows process emitter produced stderr, no bytes, or an embedded NUL");
    const processWindowsText = processWindowsEmitted.stdout.toString("utf8");
    for (const marker of [
      "w-seed-mlir0-process-parallel-2",
      "llvm.target_triple = \"x86_64-pc-windows-msvc\"",
      "llvm.func @w_seed_process_parallel_entry",
      "llvm.func @w_seed_parallel_task_0",
      "llvm.call @w_seed_parallel_launch_task_0",
      "llvm.call @w_seed_parallel_join_task_0",
    ]) if (!processWindowsText.includes(marker))
      fail(`Windows process artifact omits ${marker}`);

    const processWindowsInput = join(directory, "process-parallel-windows.mlir");
    const processWindowsLowered = join(directory,
      "process-parallel-windows-lowered.mlir");
    const processWindowsLlvm = join(directory, "process-parallel-windows.ll");
    const processWindowsObject = join(directory, "process-parallel-windows.obj");
    const processAdapterSource = resolve(seed, "runtime",
      "w_seed_process_parallel_windows0.ll");
    const processAdapterObject = join(directory,
      "process-parallel-windows-adapter.obj");
    const processExecutable = join(directory, "process-parallel-windows.exe");
    await writeFile(processWindowsInput, processWindowsEmitted.stdout);
    run(available.mlirOpt, [processWindowsInput, "-o", processWindowsLowered,
      "--convert-arith-to-llvm", "--convert-func-to-llvm",
      "--reconcile-unrealized-casts", "--canonicalize", "--cse",
      "--verify-each"]);
    run(available.mlirTranslate, ["--mlir-to-llvmir", processWindowsLowered,
      "-o", processWindowsLlvm]);
    const processWindowsLlvmText = await readFile(processWindowsLlvm, "utf8");
    if (!processWindowsLlvmText.includes(
          "define i32 @w_seed_process_parallel_entry") ||
        !processWindowsLlvmText.includes(
          "define i64 @w_seed_parallel_task_0") ||
        !processWindowsLlvmText.includes(
          "call i1 @w_seed_parallel_launch_task_0") ||
        !processWindowsLlvmText.includes(
          "call i1 @w_seed_parallel_join_task_0"))
      fail("translated Windows process LLVM omits explicit launch/join");
    run(available.llc, ["-filetype=obj", "-mtriple=x86_64-pc-windows-msvc",
      "-O3", processWindowsLlvm, "-o", processWindowsObject]);
    run(available.llc, ["-filetype=obj", "-mtriple=x86_64-pc-windows-msvc",
      "-O3", processAdapterSource, "-o", processAdapterObject]);
    const { findWindowsSdkKernel32 } = await import("./windows-build-support.mjs");
    const sdk = await findWindowsSdkKernel32();
    run(available.lldLink, ["/entry:mainCRTStartup", "/subsystem:console",
      "/nodefaultlib", "/machine:x64", `/out:${processExecutable}`,
      processWindowsObject, processAdapterObject, sdk.path, "/Brepro",
      "/opt:ref", "/opt:icf", "/incremental:no"]);
    exactExit(processExecutable, [], 0);
    exactExit(processExecutable, ["payload"], 0);
    exactExit(processExecutable, ["!"], 3);
    processWindowsObjectBytes = (await readFile(processWindowsObject)).length;
    processExecution = "windows-crt-free-empty-nonempty-provider-fault";
  }
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
  console.log(`PARALLEL MLIR0: MLIR=23.1.1 tasks=2 measuredTasks=5 processRoot=1 runtimeArguments=3 reachableFunctions=3 emittedExecution=${emittedExecution} processExecution=${processExecution} processLinuxExecution=${processLinuxExecution} windowsObjectBytes=${windowsBytes.length} linuxObjectBytes=${linuxBytes.length} measuredWindowsObjectBytes=${measuredWindowsBytes.length} measuredLinuxObjectBytes=${measuredLinuxBytes.length} processWindowsObjectBytes=${processWindowsObjectBytes} processLinuxObjectBytes=${processLinuxBytes.length}`);
} finally {
  await rm(directory, { recursive: true, force: true });
}
