import { afterAll, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { dialectArgs, probeCDialect } from "./c-dialect.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const temporaryDirectory = fs.mkdtempSync(path.join(os.tmpdir(), "wmeta-reader-"));
const executable = path.join(
  temporaryDirectory,
  process.platform === "win32" ? "wmeta-reference.exe" : "wmeta-reference",
);
const compilerPath = Bun.which(process.env.W_META_CC ?? "gcc");
const dialect = compilerPath === null ? undefined : await probeCDialect(compilerPath);

afterAll(() => fs.rmSync(temporaryDirectory, { recursive: true, force: true }));

test("the independent C reader agrees with every WMeta W0 vector", () => {
  const compiler = compilerPath;
  expect(compiler, "Set W_META_CC to a C23-capable compiler.").not.toBeNull();
  expect(dialect, "Compiler must accept -std=c23 or -std=c2x.").not.toBeUndefined();
  const compilation = Bun.spawnSync([
    compiler,
    ...dialectArgs(dialect),
    "-Wall",
    "-Wextra",
    "-Werror",
    "-O2",
    path.join(toolingDirectory, "wmeta-reference.c"),
    "-o",
    executable,
  ]);
  expect(compilation.exitCode, compilation.stderr.toString()).toBe(0);

  const lines = fs
    .readFileSync(path.join(toolingDirectory, "wmeta-results.snapshot.jsonl"), "utf8")
    .trimEnd()
    .split("\n")
    .slice(1)
    .map((line) => JSON.parse(line));
  expect(lines).toHaveLength(42);

  for (const result of lines) {
    const input = path.join(temporaryDirectory, `${result.caseId}.wmeta`);
    fs.writeFileSync(input, Buffer.from(result.hex, "hex"));
    const execution = Bun.spawnSync([executable, input, result.mode]);
    expect(execution.exitCode, `${result.caseId}: ${execution.stderr}`).toBe(0);
    const actual = execution.stdout.toString().trim();
    const expected = result.status === "accepted" ? "accepted" : result.code;
    expect(actual, result.caseId).toBe(expected);
  }
});
