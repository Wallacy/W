import { createHash } from "node:crypto";
import { createReadStream } from "node:fs";
import { mkdir, open, readFile, readdir, lstat, readlink, writeFile } from "node:fs/promises";
import { spawn } from "node:child_process";
import { dirname, extname, isAbsolute, join, relative, resolve, sep } from "node:path";
import { once } from "node:events";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const manifestPath = join(root, "tooling", "llvm-target-pack.json");
const workflowPath = join(root, ".github", "workflows", "build-llvm-target-pack.yml");
const manifest = JSON.parse(await readFile(manifestPath, "utf8"));

export function validateInputs({ host, tag, commit, sourceSha256 }) {
  const hostConfig = manifest.hosts.find((item) => item.id === host);
  if (!hostConfig) throw new Error(`Unsupported host '${host}'.`);
  if (!new RegExp(manifest.source.tagPattern, "u").test(tag ?? "")) {
    throw new Error("LLVM tag must be an exact stable llvmorg-X.Y.Z tag.");
  }
  if (!manifest.source.commitHexLengths.includes((commit ?? "").length) || !/^[0-9a-f]+$/iu.test(commit)) {
    throw new Error("Expected commit must be a full hexadecimal Git commit ID.");
  }
  if (!/^[0-9a-f]{64}$/iu.test(sourceSha256 ?? "")) {
    throw new Error("Expected source archive SHA-256 must contain exactly 64 hexadecimal characters.");
  }
  return {
    host: hostConfig,
    tag,
    version: tag.slice("llvmorg-".length),
    commit: commit.toLowerCase(),
    sourceSha256: sourceSha256.toLowerCase(),
  };
}

export function cmakeConfigureArguments({ sourceDir, buildDir, installDir, host }) {
  const components = [...manifest.distributionComponents, ...manifest.developmentComponents];
  const definitions = [
    ["CMAKE_BUILD_TYPE", manifest.cmake.buildType],
    ["CMAKE_INSTALL_PREFIX", installDir],
    ["LLVM_ENABLE_PROJECTS", manifest.projects.join(";")],
    ["LLVM_TARGETS_TO_BUILD", host.targets.join(";")],
    ["LLVM_DISTRIBUTION_COMPONENTS", components.join(";")],
    ["LLVM_INCLUDE_TESTS", "OFF"],
    ["CLANG_INCLUDE_TESTS", "OFF"],
    ["MLIR_INCLUDE_TESTS", "OFF"],
    ["LLVM_INCLUDE_EXAMPLES", "OFF"],
    ["LLVM_INCLUDE_BENCHMARKS", "OFF"],
    ["LLVM_INCLUDE_DOCS", "OFF"],
    ["LLVM_ENABLE_ASSERTIONS", "OFF"],
    ["LLVM_ENABLE_ZLIB", "OFF"],
    ["LLVM_ENABLE_ZSTD", "OFF"],
    ["LLVM_ENABLE_TERMINFO", "OFF"],
    ["LLVM_ENABLE_LIBEDIT", "OFF"],
    ["LLVM_ENABLE_LIBXML2", "OFF"],
    ["LLVM_ENABLE_LIBPFM", "OFF"],
    ["LLVM_ENABLE_CURL", "OFF"],
    ["LLVM_ENABLE_FFI", "OFF"],
    ["LLVM_ENABLE_Z3_SOLVER", "OFF"],
    ["LLVM_INSTALL_TOOLCHAIN_ONLY", "OFF"],
    ["LLVM_INSTALL_UTILS", "OFF"],
    ["MLIR_INSTALL_AGGREGATE_OBJECTS", "OFF"],
    ["LLVM_USE_RELATIVE_PATHS_IN_FILES", "ON"],
    ["LLVM_USE_RELATIVE_PATHS_IN_DEBUG_INFO", "ON"],
  ];
  return [
    "-S", join(sourceDir, "llvm"),
    "-B", buildDir,
    "-G", host.generator,
    ...host.generatorArguments,
    ...definitions.map(([key, value]) => `-D${key}=${value}`),
  ];
}

export function parseBuiltTargets(output) {
  return new Set(output.trim().split(/\s+/u).filter(Boolean));
}

export function verifyBuiltTargets(output, expectedTargets) {
  const actual = parseBuiltTargets(output);
  const missing = expectedTargets.filter((target) => !actual.has(target));
  if (missing.length > 0) throw new Error(`LLVM target smoke is missing: ${missing.join(", ")}.`);
  return [...actual].sort(compareText);
}

export async function createSha256Inventory(directory) {
  const entries = await scanTree(directory);
  const inventory = [];
  for (const entry of entries) {
    if (entry.type === "directory" || entry.path === manifest.packaging.sha256Inventory) continue;
    if (entry.type === "symlink") {
      inventory.push({
        path: entry.path,
        type: "symlink",
        linkTarget: entry.linkTarget,
        sha256: createHash("sha256").update(entry.linkTarget).digest("hex"),
      });
      continue;
    }
    inventory.push({
      path: entry.path,
      type: "file",
      sizeBytes: entry.sizeBytes,
      sha256: await hashFile(entry.fullPath),
    });
  }
  return inventory;
}

export async function writeDeterministicTar(directory, archivePath) {
  const entries = await scanTree(directory);
  const output = await open(archivePath, "wx");
  try {
    for (const entry of entries) {
      const tarPath = entry.type === "directory" ? `${entry.path}/` : entry.path;
      const mode = entry.type === "directory" ? 0o755 : entry.type === "symlink" ? 0o777 : entry.mode;
      const header = createTarHeader(tarPath, {
        mode,
        size: entry.type === "file" ? entry.sizeBytes : 0,
        type: entry.type,
        linkTarget: entry.linkTarget,
      });
      await writeAll(output, header);
      if (entry.type === "file") {
        let written = 0;
        for await (const chunk of createReadStream(entry.fullPath)) {
          const bytes = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
          await writeAll(output, bytes);
          written += bytes.length;
        }
        if (written !== entry.sizeBytes) throw new Error(`File changed while packaging: ${entry.path}`);
        const padding = (512 - (entry.sizeBytes % 512)) % 512;
        if (padding > 0) await writeAll(output, Buffer.alloc(padding));
      }
    }
    await writeAll(output, Buffer.alloc(1024));
  } finally {
    await output.close();
  }
}

function compareText(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}

async function scanTree(directory) {
  const base = resolve(directory);
  const entries = [];
  async function visit(current) {
    const children = await readdir(current, { withFileTypes: true });
    children.sort((left, right) => compareText(left.name, right.name));
    for (const child of children) {
      const fullPath = join(current, child.name);
      const path = relative(base, fullPath).split(sep).join("/");
      if (!path || path.startsWith("../") || isAbsolute(path)) throw new Error(`Unsafe package path '${path}'.`);
      const stats = await lstat(fullPath);
      if (stats.isDirectory()) {
        entries.push({ path, type: "directory", fullPath, mode: 0o755, sizeBytes: 0 });
        await visit(fullPath);
      } else if (stats.isSymbolicLink()) {
        const linkTarget = await readlink(fullPath);
        const resolvedTarget = resolve(dirname(fullPath), linkTarget);
        const targetRelative = relative(base, resolvedTarget);
        if (isAbsolute(linkTarget) || targetRelative === ".." || targetRelative.startsWith(`..${sep}`)) {
          throw new Error(`Package symlink escapes its root: ${path} -> ${linkTarget}`);
        }
        entries.push({ path, type: "symlink", fullPath, linkTarget, mode: 0o777, sizeBytes: 0 });
      } else if (stats.isFile()) {
        const executableByExtension = [".exe", ".bat", ".cmd"].includes(extname(child.name).toLowerCase());
        const mode = (stats.mode & 0o111) !== 0 || executableByExtension ? 0o755 : 0o644;
        entries.push({ path, type: "file", fullPath, mode, sizeBytes: stats.size });
      } else {
        throw new Error(`Unsupported package entry type at '${path}'.`);
      }
    }
  }
  await visit(base);
  entries.sort((left, right) => compareText(left.path, right.path));
  return entries;
}

function createTarHeader(path, { mode, size, type, linkTarget }) {
  const header = Buffer.alloc(512);
  const { name, prefix } = splitTarPath(path);
  writeTarString(header, 0, 100, name);
  writeTarOctal(header, 100, 8, mode);
  writeTarOctal(header, 108, 8, 0);
  writeTarOctal(header, 116, 8, 0);
  writeTarOctal(header, 124, 12, size);
  writeTarOctal(header, 136, 12, 0);
  header.fill(0x20, 148, 156);
  header[156] = type === "directory" ? 0x35 : type === "symlink" ? 0x32 : 0x30;
  if (type === "symlink") writeTarString(header, 157, 100, linkTarget);
  writeTarString(header, 257, 6, "ustar\0");
  writeTarString(header, 263, 2, "00");
  writeTarString(header, 345, 155, prefix);
  const checksum = header.reduce((sum, value) => sum + value, 0);
  writeTarString(header, 148, 8, `${checksum.toString(8).padStart(6, "0")}\0 `);
  return header;
}

function splitTarPath(path) {
  if (Buffer.byteLength(path, "utf8") <= 100) return { name: path, prefix: "" };
  const separators = [];
  for (let i = 0; i < path.length; i += 1) if (path[i] === "/") separators.push(i);
  for (const index of separators.reverse()) {
    const prefix = path.slice(0, index);
    const name = path.slice(index + 1);
    if (Buffer.byteLength(prefix, "utf8") <= 155 && Buffer.byteLength(name, "utf8") <= 100) {
      return { name, prefix };
    }
  }
  throw new Error(`Path is not representable in deterministic ustar: ${path}`);
}

function writeTarString(buffer, offset, length, value) {
  const bytes = Buffer.from(value, "utf8");
  if (bytes.length > length) throw new Error(`Tar string exceeds ${length} bytes.`);
  bytes.copy(buffer, offset);
}

function writeTarOctal(buffer, offset, length, value) {
  if (!Number.isSafeInteger(value) || value < 0) throw new Error("Tar numeric fields must be non-negative safe integers.");
  const digits = value.toString(8);
  if (digits.length > length - 1) throw new Error("Tar numeric field is too large.");
  writeTarString(buffer, offset, length, `${digits.padStart(length - 1, "0")}\0`);
}

async function writeAll(fileHandle, bytes) {
  let offset = 0;
  while (offset < bytes.length) {
    const { bytesWritten } = await fileHandle.write(bytes, offset, bytes.length - offset);
    if (bytesWritten === 0) throw new Error("Could not make progress while writing package archive.");
    offset += bytesWritten;
  }
}

async function hashFile(filePath) {
  const hash = createHash("sha256");
  for await (const chunk of createReadStream(filePath)) hash.update(chunk);
  return hash.digest("hex");
}

async function hashCommandOutput(command, args, cwd) {
  const hash = createHash("sha256");
  const child = spawn(command, args, { cwd, stdio: ["ignore", "pipe", "pipe"], windowsHide: true });
  let stderr = "";
  child.stdout.on("data", (chunk) => hash.update(chunk));
  child.stderr.setEncoding("utf8");
  child.stderr.on("data", (chunk) => { stderr += chunk; });
  const [code] = await once(child, "close");
  if (code !== 0) throw new Error(`${command} ${args.join(" ")} failed (${code}): ${stderr.trim()}`);
  return hash.digest("hex");
}

function run(command, args, options = {}) {
  return new Promise((resolvePromise, reject) => {
    const child = spawn(command, args, {
      cwd: options.cwd,
      env: options.env ?? process.env,
      stdio: "inherit",
      windowsHide: true,
    });
    child.once("error", reject);
    child.once("close", (code, signal) => {
      if (code === 0) resolvePromise();
      else reject(new Error(`${command} ${args.join(" ")} failed (${signal ?? code}).`));
    });
  });
}

function capture(command, args, options = {}) {
  return new Promise((resolvePromise, reject) => {
    const child = spawn(command, args, {
      cwd: options.cwd,
      env: options.env ?? process.env,
      stdio: ["ignore", "pipe", "pipe"],
      windowsHide: true,
    });
    let stdout = "";
    let stderr = "";
    child.stdout.setEncoding("utf8");
    child.stderr.setEncoding("utf8");
    child.stdout.on("data", (chunk) => { stdout += chunk; });
    child.stderr.on("data", (chunk) => { stderr += chunk; });
    child.once("error", reject);
    child.once("close", (code, signal) => {
      if (code === 0) resolvePromise(stdout.trim());
      else reject(new Error(`${command} ${args.join(" ")} failed (${signal ?? code}): ${stderr.trim()}`));
    });
  });
}

function hostPlatformAndArchitecture() {
  const platform = { win32: "windows", linux: "linux", darwin: "macos" }[process.platform];
  const architecture = { x64: "x86_64", arm64: "arm64" }[process.arch];
  return { platform, architecture };
}

async function gitArchiveSha256(sourceDir, commit) {
  return hashCommandOutput("git", ["archive", "--format=tar", commit], sourceDir);
}

async function writeJson(filePath, value) {
  await writeFile(filePath, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

async function smokeTool(toolPath, version, environment) {
  const output = await capture(toolPath, manifest.smoke.versionArguments, { env: environment });
  if (!output.includes(version)) throw new Error(`${toolPath} version did not include expected ${version}: ${output}`);
  return output.split(/\r?\n/u).map((line) => line.trim()).filter(Boolean).slice(0, 3);
}

async function prepareBuildEnvironment(hostConfig, temporaryDirectory) {
  if (hostConfig.platform !== "windows") return process.env;

  const programFilesX86 = process.env["ProgramFiles(x86)"] ?? "C:\\Program Files (x86)";
  const vswhere = join(programFilesX86, "Microsoft Visual Studio", "Installer", "vswhere.exe");
  const installationPath = await capture(vswhere, [
    "-latest",
    "-products", "*",
    "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
    "-property", "installationPath",
  ]);
  if (!installationPath) throw new Error("vswhere did not find a Visual Studio installation with the x64 MSVC toolset.");

  const developerCommand = join(installationPath, "Common7", "Tools", "VsDevCmd.bat");
  const environmentFile = join(temporaryDirectory, "msvc-environment.txt");
  const setupScript = join(temporaryDirectory, "load-msvc-environment.cmd");
  await writeFile(setupScript, [
    "@echo off",
    `@call "${developerCommand}" -arch=x64 -host_arch=x64 >nul 2>&1`,
    "@if errorlevel 1 exit /b 1",
    `@set > "${environmentFile}"`,
    "",
  ].join("\r\n"), "utf8");
  await run(process.env.ComSpec ?? "cmd.exe", ["/d", "/c", `call "${setupScript}"`], { cwd: temporaryDirectory });

  const environment = { ...process.env };
  const lines = (await readFile(environmentFile, "utf8")).split(/\r?\n/u);
  for (const line of lines) {
    const separator = line.indexOf("=");
    if (separator < 1) continue;
    const name = line.slice(0, separator);
    if (!/^[A-Za-z_][A-Za-z0-9_()]*$/u.test(name)) continue;
    const existingName = Object.keys(environment).find((key) => key.toLowerCase() === name.toLowerCase());
    if (existingName && existingName !== name) delete environment[existingName];
    environment[name] = line.slice(separator + 1);
  }
  if (!environment.VCToolsInstallDir) throw new Error("VsDevCmd did not expose VCToolsInstallDir; the MSVC environment was not initialized.");
  return environment;
}

async function buildPack(rawInputs) {
  const inputs = validateInputs(rawInputs);
  const actualHost = hostPlatformAndArchitecture();
  if (actualHost.platform !== inputs.host.platform || actualHost.architecture !== inputs.host.architecture) {
    throw new Error(`Runner mismatch for ${inputs.host.id}: got ${actualHost.platform}/${actualHost.architecture}.`);
  }
  const runnerTemp = process.env.RUNNER_TEMP;
  if (!runnerTemp) throw new Error("RUNNER_TEMP must be set; this build entry point is CI-only.");
  const runId = (process.env.GITHUB_RUN_ID ?? "manual").replace(/[^A-Za-z0-9-]/gu, "-");
  const attempt = (process.env.GITHUB_RUN_ATTEMPT ?? "1").replace(/[^A-Za-z0-9-]/gu, "-");
  const buildRoot = join(runnerTemp, `w-llvm-target-pack-${inputs.host.id}-${runId}-${attempt}`);
  await mkdir(buildRoot, { recursive: false });
  const sourceDir = join(buildRoot, "llvm-project");
  const buildDir = join(buildRoot, "build");
  const installDir = join(buildRoot, "pack");
  const outputDir = join(buildRoot, "output");
  await mkdir(outputDir, { recursive: false });

  console.log(`Fetching official llvm-project ${inputs.tag} and checking its commit.`);
  await run("git", ["init", sourceDir]);
  await run("git", ["-C", sourceDir, "remote", "add", "origin", manifest.source.repository]);
  await run("git", ["-C", sourceDir, "fetch", "--no-tags", "--depth=1", "origin", `refs/tags/${inputs.tag}:refs/tags/${inputs.tag}`]);
  const resolvedCommit = await capture("git", ["-C", sourceDir, "rev-parse", "--verify", `refs/tags/${inputs.tag}^{commit}`]);
  if (resolvedCommit.toLowerCase() !== inputs.commit) {
    throw new Error(`Tag ${inputs.tag} resolved to ${resolvedCommit}, not the expected ${inputs.commit}.`);
  }
  await run("git", ["-C", sourceDir, "checkout", "--detach", inputs.commit]);
  const sourceSha256 = await gitArchiveSha256(sourceDir, inputs.commit);
  if (sourceSha256 !== inputs.sourceSha256) {
    throw new Error(`Source archive SHA-256 mismatch: got ${sourceSha256}, expected ${inputs.sourceSha256}.`);
  }

  const hostConfig = inputs.host;
  const configureArguments = cmakeConfigureArguments({ sourceDir, buildDir, installDir, host: hostConfig });
  const buildEnvironment = await prepareBuildEnvironment(hostConfig, buildRoot);
  console.log("Configuring the scoped Release development distribution.");
  await run("cmake", configureArguments, { env: buildEnvironment });
  const cacheText = await readFile(join(buildDir, "CMakeCache.txt"), "utf8");
  const compilerReceipt = {
    c: cacheValue(cacheText, "CMAKE_C_COMPILER_ID") && cacheValue(cacheText, "CMAKE_C_COMPILER_VERSION")
      ? `${cacheValue(cacheText, "CMAKE_C_COMPILER_ID")} ${cacheValue(cacheText, "CMAKE_C_COMPILER_VERSION")}` : "not exposed by CMake cache",
    cxx: cacheValue(cacheText, "CMAKE_CXX_COMPILER_ID") && cacheValue(cacheText, "CMAKE_CXX_COMPILER_VERSION")
      ? `${cacheValue(cacheText, "CMAKE_CXX_COMPILER_ID")} ${cacheValue(cacheText, "CMAKE_CXX_COMPILER_VERSION")}` : "not exposed by CMake cache",
  };
  const cmakeVersion = await capture("cmake", ["--version"], { env: buildEnvironment });
  const ninjaVersion = hostConfig.generator === "Ninja" ? await capture("ninja", ["--version"], { env: buildEnvironment }) : null;
  const buildArgs = ["--build", buildDir, "--target", "distribution", "--parallel", String(hostConfig.parallelJobs)];
  const installArgs = ["--build", buildDir, "--target", manifest.cmake.distributionInstallTarget, "--parallel", String(hostConfig.parallelJobs)];
  console.log(`Building only the configured development distribution (${hostConfig.targets.join(", ")}).`);
  await run("cmake", buildArgs, { env: buildEnvironment });
  await run("cmake", installArgs, { env: buildEnvironment });

  const executableSuffix = hostConfig.platform === "windows" ? ".exe" : "";
  const binDir = join(installDir, "bin");
  const environment = { ...buildEnvironment };
  const oldPath = environment.PATH ?? environment.Path ?? "";
  environment.PATH = [binDir, oldPath].filter(Boolean).join(hostConfig.platform === "windows" ? ";" : ":");
  if (hostConfig.platform === "linux") environment.LD_LIBRARY_PATH = [join(installDir, "lib"), environment.LD_LIBRARY_PATH].filter(Boolean).join(":");
  if (hostConfig.platform === "macos") environment.DYLD_LIBRARY_PATH = [join(installDir, "lib"), environment.DYLD_LIBRARY_PATH].filter(Boolean).join(":");

  const toolVersions = {};
  for (const tool of manifest.tools) {
    const toolPath = join(binDir, `${tool}${executableSuffix}`);
    toolVersions[tool] = await smokeTool(toolPath, inputs.version, environment);
  }
  const targetProbe = await capture(join(binDir, `llvm-config${executableSuffix}`), ["--targets-built"], { env: environment });
  const builtTargets = verifyBuiltTargets(targetProbe, hostConfig.targets);

  await writeFile(join(installDir, "build-config.json"), await readFile(manifestPath));
  const manifestSha256 = await hashFile(manifestPath);
  const workflowSha256 = await hashFile(workflowPath);
  const receipt = {
    schema: "w-llvm-target-pack-build-receipt-1",
    status: "manual-ci-bootstrap-only",
    claimBoundary: "not an accepted toolchain pin, signed W release, cross-run reproducibility result, end-user package, or complete runtime-dependency receipt",
    source: {
      repository: manifest.source.repository,
      tag: inputs.tag,
      commit: inputs.commit,
      gitArchiveSha256: sourceSha256,
    },
    host: {
      id: hostConfig.id,
      runner: hostConfig.runner,
      platform: hostConfig.platform,
      architecture: hostConfig.architecture,
      imageOS: process.env.ImageOS ?? null,
      imageVersion: process.env.ImageVersion ?? null,
      compiler: compilerReceipt,
    },
    build: {
      configuration: manifest.cmake.buildType,
      generator: hostConfig.generator,
      cmakeVersion: cmakeVersion.split(/\r?\n/u)[0],
      ninjaVersion,
      cmakeArguments: configureArguments,
      buildTargets: ["distribution", manifest.cmake.distributionInstallTarget],
      projects: manifest.projects,
      llvmTargets: hostConfig.targets,
      tools: manifest.tools,
      distributionComponents: manifest.distributionComponents,
      developmentComponents: manifest.developmentComponents,
      llvmTargetsBuiltOutput: targetProbe,
      llvmTargetsBuilt: builtTargets,
      toolVersions,
    },
    configuration: {
      manifest: "tooling/llvm-target-pack.json",
      manifestSha256,
      workflow: ".github/workflows/build-llvm-target-pack.yml",
      workflowSha256,
    },
  };
  await writeJson(join(installDir, manifest.packaging.receipt), receipt);
  await writeJson(join(installDir, manifest.packaging.sbomInputs), {
    schema: "w-llvm-target-pack-sbom-inputs-1",
    status: "inputs-only-not-a-complete-dependency-sbom",
    sourceComponent: {
      name: "llvm-project",
      version: inputs.version,
      purl: `pkg:github/llvm/llvm-project@${inputs.commit}`,
      repository: manifest.source.repository,
      commit: inputs.commit,
      sourceArchiveSha256: sourceSha256,
      licenseReview: "required-per-file; not inferred from the repository-level license",
    },
    packedTools: manifest.tools,
    developmentComponents: manifest.developmentComponents,
    fileInventory: manifest.packaging.sha256Inventory,
    dependencyClosure: "not measured by this bootstrap scaffold",
  });
  await writeJson(join(installDir, manifest.packaging.provenanceInputs), {
    schema: "w-llvm-target-pack-provenance-inputs-1",
    status: "unsigned-provenance-inputs-only",
    subject: `${inputs.tag}-${hostConfig.id}`,
    source: {
      repository: manifest.source.repository,
      tag: inputs.tag,
      commit: inputs.commit,
      gitArchiveSha256: sourceSha256,
    },
    builder: {
      workflow: process.env.GITHUB_WORKFLOW ?? null,
      workflowRef: process.env.GITHUB_WORKFLOW_REF ?? null,
      repository: process.env.GITHUB_REPOSITORY ?? null,
      event: process.env.GITHUB_EVENT_NAME ?? null,
      ref: process.env.GITHUB_REF ?? null,
      sourceRevision: process.env.GITHUB_SHA ?? null,
      runId: process.env.GITHUB_RUN_ID ?? null,
      runAttempt: process.env.GITHUB_RUN_ATTEMPT ?? null,
      runnerOs: process.env.RUNNER_OS ?? null,
      runnerArchitecture: process.env.RUNNER_ARCH ?? null,
      runnerImageOS: process.env.ImageOS ?? null,
      runnerImageVersion: process.env.ImageVersion ?? null,
    },
    workflowSha256,
    buildManifestSha256: manifestSha256,
    actionPins: manifest.actionPins,
    buildReceipt: manifest.packaging.receipt,
    sbomInputs: manifest.packaging.sbomInputs,
  });
  const inventory = await createSha256Inventory(installDir);
  await writeJson(join(installDir, manifest.packaging.sha256Inventory), {
    schema: "w-llvm-target-pack-sha256-inventory-1",
    entries: inventory,
  });

  const archiveName = `w-llvm-development-${inputs.version}-${hostConfig.id}.tar`;
  const archivePath = join(outputDir, archiveName);
  await writeDeterministicTar(installDir, archivePath);
  const archiveSha256 = await hashFile(archivePath);
  const archiveSidecar = `${archivePath}.sha256`;
  await writeFile(archiveSidecar, `${archiveSha256}  ${archiveName}\n`, "utf8");
  if (process.env.GITHUB_OUTPUT) {
    await writeFile(process.env.GITHUB_OUTPUT, `archive-path=${archivePath}\narchive-sha256-path=${archiveSidecar}\n`, { flag: "a" });
  }
  console.log(`Created ${archivePath} (SHA-256 ${archiveSha256}).`);
}

function cacheValue(cacheText, name) {
  const escapedName = name.replace(/[.*+?^${}()|[\]\\]/gu, "\\$&");
  return cacheText.match(new RegExp(`^${escapedName}:[^=]*=(.*)$`, "mu"))?.[1] ?? null;
}

function parseArguments(args) {
  if (args[0] === "--help" || args.length === 0) {
    console.log("Usage: bun tooling/llvm-target-pack.mjs build --host <host-id>\nRequired environment: W_LLVM_TAG, W_LLVM_EXPECTED_COMMIT, W_LLVM_EXPECTED_SOURCE_SHA256, RUNNER_TEMP.");
    process.exit(0);
  }
  if (args[0] !== "build") throw new Error("The only supported operation is 'build'.");
  if (args.length !== 3 || args[1] !== "--host") throw new Error("Usage: bun tooling/llvm-target-pack.mjs build --host <host-id>.");
  return validateInputs({
    host: args[2],
    tag: process.env.W_LLVM_TAG,
    commit: process.env.W_LLVM_EXPECTED_COMMIT,
    sourceSha256: process.env.W_LLVM_EXPECTED_SOURCE_SHA256,
  });
}

if (import.meta.main) {
  try {
    const inputs = parseArguments(process.argv.slice(2));
    await buildPack(inputs);
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    process.exitCode = 1;
  }
}
