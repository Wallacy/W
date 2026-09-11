import fs from "node:fs";
import path from "node:path";

export const LLVM_RELEASES_API = "https://api.github.com/repos/llvm/llvm-project/releases?per_page=30";
const TAG_PATTERN = /^llvmorg-(\d+)\.(\d+)\.(\d+)$/u;

export function compareVersions(left, right) {
  const a = left.split(".").map(Number);
  const b = right.split(".").map(Number);
  for (let index = 0; index < 3; index += 1) {
    if (a[index] !== b[index]) return a[index] - b[index];
  }
  return 0;
}

export function latestStableRelease(releases) {
  const stable = releases
    .filter((release) => release?.draft !== true && release?.prerelease !== true)
    .map((release) => {
      const match = TAG_PATTERN.exec(release?.tag_name ?? "");
      if (!match) return null;
      return {
        version: `${match[1]}.${match[2]}.${match[3]}`,
        tag: release.tag_name,
        url: release.html_url,
        publishedAt: release.published_at,
      };
    })
    .filter(Boolean)
    .sort((left, right) => compareVersions(right.version, left.version));
  return stable[0] ?? null;
}

export function compareCatalog(catalog, release) {
  const entry = catalog.dependencies?.find((candidate) => candidate.id === "mlir0-llvm-clang");
  if (!entry) return { status: "invalid", message: "mlir0-llvm-clang is missing from the dependency catalog." };
  if (!release) return { status: "invalid", message: "The official release feed contains no stable llvmorg-X.Y.Z release." };
  const recorded = entry.latestStable?.version;
  if (recorded === release.version && entry.latestStable?.tag === release.tag) {
    return { status: "current", recorded, release };
  }
  if (typeof recorded === "string" && compareVersions(recorded, release.version) > 0) {
    return { status: "invalid", message: `The catalog version ${recorded} is newer than the official stable release ${release.version}.` };
  }
  return {
    status: "update-required",
    recorded,
    release,
    message: `LLVM ${release.version} (${release.tag}) is stable; the catalog records ${recorded ?? "no version"}.`,
  };
}

async function main() {
  const root = path.resolve(import.meta.dirname, "..");
  const catalog = JSON.parse(fs.readFileSync(path.join(root, "tooling", "dependency-currency.json"), "utf8"));
  const headers = {
    Accept: "application/vnd.github+json",
    "User-Agent": "w-language-llvm-release-watch",
    "X-GitHub-Api-Version": "2022-11-28",
  };
  if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  const response = await fetch(LLVM_RELEASES_API, { headers });
  if (!response.ok) {
    process.stderr.write(`LLVM release watch could not read the official feed: HTTP ${response.status}.\n`);
    process.exitCode = 2;
    return;
  }
  const result = compareCatalog(catalog, latestStableRelease(await response.json()));
  if (result.status === "current") {
    process.stdout.write(`LLVM release watch: current at ${result.recorded}.\n`);
    return;
  }
  process.stderr.write(`LLVM release watch: ${result.message}\n`);
  if (result.status === "update-required") {
    process.stderr.write("Resolve the annotated tag object and peeled commit, run the isolated compatibility probe, update exact pins and native plans, then run dependency, platform, compiler, and benchmark gates. Do not rewrite historical evidence.\n");
    process.exitCode = 1;
    return;
  }
  process.exitCode = 2;
}

if (import.meta.main) await main();
