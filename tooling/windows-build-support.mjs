import { existsSync } from "node:fs"
import { lstat, readdir } from "node:fs/promises"
import { homedir } from "node:os"
import { join, resolve } from "node:path"

function uniquePaths(values) {
  const seen = new Set()
  return values.filter((value) => {
    if (typeof value !== "string" || value.length === 0) return false
    const normalized = resolve(value)
    if (seen.has(normalized.toLowerCase())) return false
    seen.add(normalized.toLowerCase())
    return true
  })
}

function run(command, args, options = {}) {
  return Bun.spawnSync({
    cmd: [command, ...args],
    cwd: options.cwd,
    stdout: "pipe",
    stderr: "pipe",
    windowsHide: true,
  })
}

function vswhereCandidates() {
  const programFilesX86 = process.env["ProgramFiles(x86)"]
  const programFiles = process.env.ProgramFiles
  return uniquePaths([
    Bun.which("vswhere.exe"),
    programFilesX86 === undefined ? undefined :
      join(programFilesX86, "Microsoft Visual Studio", "Installer", "vswhere.exe"),
    programFiles === undefined ? undefined :
      join(programFiles, "Microsoft Visual Studio", "Installer", "vswhere.exe"),
  ])
}

export function findVisualStudio() {
  for (const vswhere of vswhereCandidates()) {
    if (!existsSync(vswhere)) continue
    const result = run(vswhere, [
      "-latest",
      "-products", "*",
      "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
      "-property", "installationPath",
    ])
    if (result.exitCode !== 0) continue
    const installationPath = result.stdout.toString().split(/\r?\n/u)
      .map((line) => line.trim()).find((line) => line.length > 0)
    if (installationPath === undefined) continue
    const devCommand = join(installationPath, "Common7", "Tools", "VsDevCmd.bat")
    if (existsSync(devCommand)) return {
      installationPath: resolve(installationPath),
      devCommand: resolve(devCommand),
      vswhere: resolve(vswhere),
    }
  }

  const explicit = process.env.VSINSTALLDIR
  if (explicit !== undefined) {
    const installationPath = resolve(explicit)
    const devCommand = join(installationPath, "Common7", "Tools", "VsDevCmd.bat")
    if (existsSync(devCommand)) return {
      installationPath,
      devCommand: resolve(devCommand),
      vswhere: null,
    }
  }
  throw new Error("Visual Studio with the x64 C++ workload was not found by vswhere")
}

export async function findWindowsSdkKernel32() {
  const roots = []
  for (const variable of ["WindowsSdkDir", "ProgramFiles(x86)",
    "ProgramW6432", "ProgramFiles"]) {
    if (process.env[variable] === undefined) continue
    roots.push(variable === "WindowsSdkDir"
      ? resolve(process.env[variable])
      : join(process.env[variable], "Windows Kits", "10"))
  }
  roots.push(join(homedir(), "AppData", "Local", "Microsoft", "Windows Kits", "10"))
  for (const sdkRoot of uniquePaths(roots)) {
    let entries
    try {
      entries = await readdir(join(sdkRoot, "Lib"), { withFileTypes: true })
    } catch (error) {
      if (error?.code === "ENOENT") continue
      throw error
    }
    for (const version of entries.filter((entry) => entry.isDirectory())
      .map((entry) => entry.name).sort().reverse()) {
      const library = join(sdkRoot, "Lib", version, "um", "x64", "kernel32.lib")
      try {
        const stats = await lstat(library)
        if (stats.isFile() && !stats.isSymbolicLink()) return {
          path: resolve(library),
          root: resolve(sdkRoot),
          version,
        }
      } catch (error) {
        if (error?.code !== "ENOENT") throw error
      }
    }
  }
  throw new Error("Windows SDK kernel32.lib was not found by explicit probes")
}

export function runWithVisualStudio(vsDevCmd, command, args, options = {}) {
  return run("cmd.exe", ["/d", "/s", "/c", "call", vsDevCmd,
    "-arch=x64", ">nul", "&&", command, ...args], options)
}
