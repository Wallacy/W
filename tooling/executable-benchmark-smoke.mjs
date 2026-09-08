import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import {
  loadExecutableDocuments,
  validateExecutableCatalog,
} from "./executable-benchmark-machine.mjs";
import { dialectArgs, dialectDisclosure, probeCDialect } from "./c-dialect.mjs";
import { C_RELEASE_FLAGS, RUST_RELEASE_FLAGS } from "./executable-release-recipes.mjs";

const root = path.resolve(import.meta.dir, "..");
const documents = loadExecutableDocuments();
const errors = [];

function report(message) {
  console.log("executable correctness: " + message);
}

function fail(message) {
  errors.push(message);
}

async function capture(command, args, cwd = root) {
  const child = Bun.spawn([command, ...args], {
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  });
  const [stdout, stderr, exitCode] = await Promise.all([
    new Response(child.stdout).arrayBuffer(),
    new Response(child.stderr).arrayBuffer(),
    child.exited,
  ]);
  return {
    stdout: new Uint8Array(stdout),
    stderr: new Uint8Array(stderr),
    exitCode,
  };
}

function text(bytes) {
  return new TextDecoder("utf-8", { fatal: true }).decode(bytes);
}

const CORRECTNESS_WORKLOAD_IDS = Object.freeze(["hello", "restaurant-branch"]);

function workloadFor(id) {
  const workload = documents.catalog.workloads.find((item) => item.id === id);
  if (!workload) throw new Error("missing correctness workload: " + id);
  return workload;
}

function sourcePath(workloadId, language) {
  const source = workloadFor(workloadId).sources.find((item) => item.language === language);
  if (!source) throw new Error(`missing ${language} source for ${workloadId}`);
  return path.resolve(root, source.path);
}

function executablePath(directory, workloadId, language) {
  return path.join(directory, `${workloadId}-${language}${process.platform === "win32" ? ".exe" : ""}`);
}

async function checkExecutable(workloadId, language, executable, compileArgs, directory) {
  const compile = await capture(executable, compileArgs, root);
  if (compile.exitCode !== 0) {
    let detail = "";
    try { detail = text(compile.stderr).trim().slice(0, 1200); } catch { detail = "non-UTF-8 diagnostics"; }
    fail(`${language} ${workloadId} compile failed with an available toolchain: ${detail}`);
    return;
  }
  const run = await capture(executablePath(directory, workloadId, language), [], root);
  let output;
  let errorOutput;
  try {
    output = text(run.stdout);
    errorOutput = text(run.stderr);
  } catch {
    fail(`${language} ${workloadId} emitted non-UTF-8 stdout or stderr.`);
    return;
  }
  const oracle = workloadFor(workloadId).oracle;
  if (run.exitCode !== oracle.exitCode) fail(`${language} ${workloadId} exit code does not match the exact-output oracle.`);
  if (output !== oracle.stdout) fail(`${language} ${workloadId} stdout does not match the exact-output oracle.`);
  if (errorOutput !== oracle.stderr) fail(`${language} ${workloadId} stderr does not match the exact-output oracle.`);
  report(`${language} ${workloadId} exact output/exit check passed`);
}

async function main() {
  const catalogErrors = validateExecutableCatalog(documents.catalog, documents);
  for (const error of catalogErrors) fail("catalog: " + error);
  if (errors.length > 0) {
    for (const error of errors) console.error("executable correctness: " + error);
    process.exitCode = 1;
    return;
  }
  report("W source/oracle is declared; execution and timing remain deferred to M3b");
  const directory = await fs.mkdtemp(path.join(os.tmpdir(), "w-executable-correctness-"));
  try {
    const cCandidates = ["gcc", "clang", "cc"].map((name) => Bun.which(name)).filter(Boolean);
    if (cCandidates.length === 0) {
      report("C SKIP (gcc/clang/cc unavailable; no compiler claim made)");
    } else {
      let c;
      let dialect;
      for (const candidate of cCandidates) {
        const targetProbe = await capture(candidate, ["-dumpmachine"]);
        let target = "";
        try { target = text(targetProbe.stdout).trim(); } catch { target = ""; }
        if (targetProbe.exitCode !== 0 || target !== "x86_64-w64-mingw32") continue;
        const candidateDialect = await probeCDialect(candidate);
        if (candidateDialect) {
          c = candidate;
          dialect = candidateDialect;
          break;
        }
      }
      if (!c || !dialect) {
        report("C SKIP (x86_64-w64-mingw32 C23/c2x compiler unavailable; no ABI or compiler claim made)");
      } else {
        for (const workloadId of CORRECTNESS_WORKLOAD_IDS) {
          const executable = executablePath(directory, workloadId, "c");
          await checkExecutable(workloadId, "c", c, [...dialectArgs(dialect), ...C_RELEASE_FLAGS, sourcePath(workloadId, "c"), "-o", executable], directory);
        }
        report("C release recipe: " + dialectDisclosure(dialect) + " with -O3, LTO, section GC, and stripped symbols");
      }
    }
    const rustc = Bun.which("rustc");
    if (!rustc) {
      report("Rust SKIP (rustc unavailable; no compiler claim made)");
    } else {
      for (const workloadId of CORRECTNESS_WORKLOAD_IDS) {
        const executable = executablePath(directory, workloadId, "rust");
          await checkExecutable(workloadId, "rust", rustc, [sourcePath(workloadId, "rust"), "--edition=2024", ...RUST_RELEASE_FLAGS, "-o", executable], directory);
      }
      report("Rust release recipe: edition 2024, O3, fat LTO, one codegen unit, panic abort, and stripped symbols");
    }
  } finally {
    await fs.rm(directory, { recursive: true, force: true });
  }
  if (errors.length > 0) {
    for (const error of errors) console.error("executable correctness: " + error);
    process.exitCode = 1;
    return;
  }
  report("PASS (correctness only; no W timing, result, or performance claim recorded)");
}

await main();
