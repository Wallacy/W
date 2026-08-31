import assert from "node:assert/strict";
import { execFileSync, spawnSync } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { dialectArgs, dialectDisclosure, probeCDialect } from "./c-dialect.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const sourcePath = path.join(toolingDirectory, "wire-reference.c");
const compiler = process.env.W_CC || "gcc";
const probe = spawnSync(compiler, ["--version"], { stdio: "ignore" });
const compilerAvailable = !probe.error && probe.status === 0;
const dialect = compilerAvailable ? await probeCDialect(compiler) : undefined;
const dialectAvailable = dialect !== undefined;

test(
  "C MenuKey codec matches the wWire seed vectors",
  {
    skip: !compilerAvailable
      ? `${compiler} is not available`
      : (!dialectAvailable ? "compiler accepts neither C23 nor c2x" : false),
  },
  () => {
    const temporaryDirectory = fs.mkdtempSync(
      path.join(os.tmpdir(), "w-wire-reference-"),
    );
    const executableName = process.platform === "win32"
      ? "wire-reference.exe"
      : "wire-reference";
    const executablePath = path.join(temporaryDirectory, executableName);

    try {
      const compilation = spawnSync(
        compiler,
        [sourcePath, ...dialectArgs(dialect), "-Wall", "-Wextra", "-Werror", "-O2", "-o", executablePath],
        { encoding: "utf8" },
      );
      assert.equal(
        compilation.status,
        0,
        compilation.stderr || compilation.error?.message || "C compilation failed",
      );

      const output = execFileSync(executablePath, { encoding: "utf8" })
        .trim()
        .split(/\r?\n/);
      assert.deepEqual(output, [
        "exact.absent=00 2A 00",
        "exact.present=01 2A 00 01",
        "compatible.absent=01 01 03 02 2A 00",
        "compatible.present=02 01 03 02 01 01 01 2A 00 01",
      ]);
      assert.ok(dialectDisclosure(dialect));
    } finally {
      fs.rmSync(temporaryDirectory, { recursive: true, force: true });
    }
  },
);
