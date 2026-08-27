import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import {
  BYTE_SCAN_MAX_BYTES,
  deterministicByteScanCases,
  expectedByteScanOutput,
  loadByteScanDocuments,
  validateByteScanManifest,
  validateLanguageCatalog,
} from "./byte-scan-view-machine.mjs";
import { oracleForPath, runOracleCli } from "./byte-scan-view-oracle.mjs";

const root = path.resolve(import.meta.dir, "..");
const documents = loadByteScanDocuments();
const errors = [];

function report(message) {
  console.log("BMD3 byte-scan correctness: " + message);
}

function fail(message) {
  errors.push(message);
}

function independentOutput(bytes, delimiter) {
  let matches = 0n;
  for (const byte of bytes) if (byte === delimiter) matches += 1n;
  return JSON.stringify({ bytes: String(bytes.length), matches: matches.toString(10) });
}

async function spawnCapture(command, args) {
  const child = Bun.spawn([command, ...args], { stdout: "pipe", stderr: "pipe" });
  const [stdout, stderr, exitCode] = await Promise.all([
    new Response(child.stdout).arrayBuffer(),
    new Response(child.stderr).arrayBuffer(),
    child.exited,
  ]);
  return {
    stdout: new Uint8Array(stdout),
    stderr: new TextDecoder().decode(stderr),
    exitCode,
  };
}

function stdoutText(bytes) {
  return new TextDecoder("utf-8", { fatal: true }).decode(bytes);
}

async function existingFile(candidates) {
  for (const candidate of candidates) {
    try {
      const stat = await fs.stat(candidate);
      if (stat.isFile()) return candidate;
    } catch {
      // Try the next platform-specific output path.
    }
  }
  return undefined;
}

function baselineManifest(id) {
  return documents.manifest.baselines.find((baseline) => baseline.id === id);
}

function expandStep(step, replacements) {
  return step.map((token) => replacements[token] ?? token);
}

async function prepareCases(directory) {
  const cases = deterministicByteScanCases(true);
  const overMaximum = Buffer.alloc(BYTE_SCAN_MAX_BYTES + 1, 17);
  cases.push({ id: "over-maximum", kind: "invalid-bound", delimiter: 17, bytes: overMaximum });
  const paths = [];
  for (const item of cases) {
    const inputPath = path.join(directory, item.id + ".bin");
    await fs.writeFile(inputPath, item.bytes);
    const independent = item.bytes.length <= BYTE_SCAN_MAX_BYTES
      ? independentOutput(item.bytes, item.delimiter)
      : undefined;
    if (independent !== undefined && independent !== expectedByteScanOutput(item.bytes, item.delimiter)) {
      fail(item.id + " host independent oracle disagrees with the byte contract.");
    }
    if (independent !== undefined && await oracleForPath(inputPath, item.delimiter) !== independent) {
      fail(item.id + " bounded host oracle disagrees with the independent oracle.");
    }
    paths.push({ ...item, inputPath, expected: independent });
  }
  return paths;
}

async function compileC11(directory) {
  const cmake = Bun.which("cmake");
  if (!cmake) {
    report("C11 SKIP (cmake not available; no compiler claim made)");
    return undefined;
  }
  const sourceDirectory = path.join(directory, "c11-source");
  const buildDirectory = path.join(directory, "c11-build");
  await fs.mkdir(sourceDirectory, { recursive: true });
  await fs.copyFile(path.join(root, "benchmarks", "byte-scan-view", "byte_scan_view.c"), path.join(sourceDirectory, "byte_scan_view.c"));
  await fs.copyFile(path.join(root, "benchmarks", "byte-scan-view", "CMakeLists.txt"), path.join(sourceDirectory, "CMakeLists.txt"));
  const recipe = baselineManifest("c11");
  const replacements = { "<source-dir>": sourceDirectory, "<build-dir>": buildDirectory };
  const configureStep = expandStep(recipe.compileSteps[0], replacements);
  const configure = await spawnCapture(cmake, configureStep.slice(1));
  if (configure.exitCode !== 0) {
    fail("C11 CMake configure failed with an available cmake: " + configure.stderr.trim().slice(0, 1200));
    return undefined;
  }
  const buildStep = expandStep(recipe.compileSteps[1], replacements);
  const build = await spawnCapture(cmake, buildStep.slice(1));
  if (build.exitCode !== 0) {
    fail("C11 CMake build failed with an available compiler: " + build.stderr.trim().slice(0, 1200));
    return undefined;
  }
  const executable = await existingFile([
    path.join(buildDirectory, "Release", "byte_scan_view.exe"),
    path.join(buildDirectory, "byte_scan_view.exe"),
    path.join(buildDirectory, "byte_scan_view"),
  ]);
  if (!executable) {
    fail("C11 build completed without a discoverable executable.");
    return undefined;
  }
  report("C11 compiled through CMake with its selected available compiler");
  return { id: "c11", executable };
}

async function compileRust(directory) {
  const rustc = Bun.which("rustc");
  if (!rustc) {
    report("Rust SKIP (rustc not available; no compiler claim made)");
    return undefined;
  }
  const sourcePath = path.join(directory, "byte_scan_view.rs");
  const executable = path.join(directory, process.platform === "win32" ? "byte_scan_view-rust.exe" : "byte_scan_view-rust");
  await fs.copyFile(path.join(root, "benchmarks", "byte-scan-view", "byte_scan_view.rs"), sourcePath);
  const recipe = baselineManifest("rust");
  const compileStep = expandStep(recipe.compileSteps[0], { "<source>": sourcePath, "<executable>": executable });
  const compile = await spawnCapture(rustc, compileStep.slice(1));
  if (compile.exitCode !== 0) {
    fail("Rust rustc failed with an available toolchain: " + compile.stderr.trim().slice(0, 1200));
    return undefined;
  }
  report("Rust compiled with rustc --edition=2021 -C opt-level=2 (no Cargo or lockfile)");
  return { id: "rust", executable };
}

async function checkBaseline(baseline, cases) {
  for (const item of cases) {
    const result = await spawnCapture(baseline.executable, [item.inputPath, String(item.delimiter)]);
    if (item.expected === undefined) {
      if (result.exitCode === 0) fail(baseline.id + " accepted an input larger than 64 MiB.");
      if (result.stdout.length !== 0) fail(baseline.id + " wrote stdout before rejecting an oversized input.");
      continue;
    }
    if (result.exitCode !== 0) {
      fail(baseline.id + " failed a valid " + item.id + " input: " + result.stderr.trim());
      continue;
    }
    let actual;
    try {
      actual = stdoutText(result.stdout);
    } catch {
      fail(baseline.id + " emitted non-UTF-8 stdout for " + item.id + ".");
      continue;
    }
    if (actual !== item.expected) fail(baseline.id + " emitted non-canonical output for " + item.id + ".");
    if (result.stdout.length !== Buffer.byteLength(item.expected, "utf8")) {
      fail(baseline.id + " emitted unexpected stdout bytes for " + item.id + ".");
    }
  }
  report(baseline.id + " exact stdout/exit checks complete for " + cases.filter((item) => item.expected !== undefined).length + " valid inputs and one bound rejection");
}

async function expectRejected(label, operation) {
  try {
    await operation();
    fail(label + " unexpectedly succeeded.");
  } catch {
    // Rejection is the required all-or-nothing result.
  }
}

async function checkInvalidInputs(directory, validCase, baselines) {
  const missingPath = path.join(directory, "does-not-exist.bin");
  await expectRejected("host oracle missing path", () => oracleForPath(missingPath, 10));
  await expectRejected("host oracle out-of-range delimiter", () => runOracleCli([validCase.inputPath, "256"]));
  for (const baseline of baselines) {
    for (const [label, inputPath, delimiter] of [
      ["missing path", missingPath, "10"],
      ["out-of-range delimiter", validCase.inputPath, "256"],
      ["invalid delimiter", validCase.inputPath, "not-a-byte"],
      ["non-canonical delimiter", validCase.inputPath, "01"],
    ]) {
      const result = await spawnCapture(baseline.executable, [inputPath, delimiter]);
      if (result.exitCode === 0) fail(baseline.id + " accepted " + label + ".");
      if (result.stdout.length !== 0) fail(baseline.id + " wrote stdout for " + label + ".");
    }
  }
  report("invalid delimiter/path checks complete with no partial stdout");
}

async function main() {
  const catalogErrors = validateLanguageCatalog(documents.catalog);
  const manifestErrors = validateByteScanManifest(documents.manifest, documents.catalog);
  for (const error of catalogErrors) fail("catalog: " + error);
  for (const error of manifestErrors) fail("manifest: " + error);
  if (errors.length > 0) {
    for (const error of errors) console.error("BMD3 byte-scan correctness: " + error);
    process.exitCode = 1;
    return;
  }

  const directory = await fs.mkdtemp(path.join(os.tmpdir(), "w-bmd3-byte-scan-"));
  try {
    const cases = await prepareCases(directory);
    report("host oracle exact checks complete for " + cases.filter((item) => item.expected !== undefined).length + " valid inputs");
    const baselines = (await Promise.all([compileC11(directory), compileRust(directory)])).filter(Boolean);
    for (const baseline of baselines) await checkBaseline(baseline, cases);
    await checkInvalidInputs(directory, cases.find((item) => item.expected !== undefined), baselines);
    if (baselines.length === 0 && errors.length === 0) report("all native baseline checks SKIP (toolchains unavailable; structural/oracle checks remain the only evidence)");
  } finally {
    await fs.rm(directory, { recursive: true, force: true });
  }
  if (errors.length > 0) {
    for (const error of errors) console.error("BMD3 byte-scan correctness: " + error);
    process.exitCode = 1;
    return;
  }
  report("PASS (correctness only; no W timings, results, or performance claims recorded)");
}

await main();
