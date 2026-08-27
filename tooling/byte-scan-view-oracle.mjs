import fs from "node:fs/promises";
import { Buffer } from "node:buffer";
import path from "node:path";
import {
  BYTE_SCAN_MAX_BYTES,
  calculateByteScan,
  canonicalByteScanOutput,
  deterministicByteScanCases,
} from "./byte-scan-view-machine.mjs";

export { BYTE_SCAN_MAX_BYTES, calculateByteScan, canonicalByteScanOutput, deterministicByteScanCases };

export async function oracleForPath(inputPath, delimiter) {
  const absolutePath = path.resolve(inputPath);
  if (!Number.isInteger(delimiter) || delimiter < 0 || delimiter > 255) {
    throw new RangeError("delimiter must be a byte");
  }
  const handle = await fs.open(absolutePath, "r");
  const buffer = Buffer.allocUnsafe(64 * 1024);
  let bytes = 0n;
  let matches = 0n;
  try {
    for (;;) {
      const { bytesRead } = await handle.read(buffer, 0, buffer.length, null);
      if (bytesRead === 0) break;
      bytes += BigInt(bytesRead);
      if (bytes > BigInt(BYTE_SCAN_MAX_BYTES)) {
        throw new RangeError("byte-scan input exceeds 64 MiB");
      }
      for (let index = 0; index < bytesRead; index += 1) {
        if (buffer[index] === delimiter) matches += 1n;
      }
    }
  } finally {
    await handle.close();
  }
  return canonicalByteScanOutput(bytes, matches);
}

export async function runOracleCli(argumentsList) {
  if (argumentsList.length !== 2) throw new Error("usage: byte-scan-view-oracle <path> <delimiter>");
  const [inputPath, delimiterText] = argumentsList;
  if (!/^(?:0|[1-9][0-9]{0,2})$/u.test(delimiterText) || Number(delimiterText) > 255) {
    throw new Error("delimiter must be a decimal byte");
  }
  return oracleForPath(inputPath, Number(delimiterText));
}

if (import.meta.main) {
  try {
    process.stdout.write(await runOracleCli(process.argv.slice(2)));
  } catch (error) {
    process.stderr.write(String(error?.message ?? error) + "\n");
    process.exitCode = 1;
  }
}
