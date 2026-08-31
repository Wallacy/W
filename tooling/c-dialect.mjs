const probeSource = "int main(void) { return 0; }\n"
const c23ProbeSource = "#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L\n#error C23 mode must define __STDC_VERSION__ >= 202311L\n#endif\n" + probeSource

const dialects = [
  { flag: "-std=c23", name: "C23", final: true, source: c23ProbeSource },
  { flag: "-std=c2x", name: "c2x-preview", final: false, source: probeSource },
]

function commandParts(command) {
  if (Array.isArray(command) && command.length > 0) return [...command]
  if (typeof command === "string" && command.length > 0) return [command]
  throw new TypeError("C dialect probe requires a compiler command")
}

async function probe(command, dialect, options) {
  const child = Bun.spawn([
    ...commandParts(command),
    ...(options.args ?? []),
    dialect.flag,
    "-x",
    "c",
    "-fsyntax-only",
    "-",
  ], {
    cwd: options.cwd,
    env: options.env,
    stdin: "pipe",
    stdout: "pipe",
    stderr: "pipe",
  })
  child.stdin.write(dialect.source)
  child.stdin.end()
  const [, , exitCode] = await Promise.all([
    new Response(child.stdout).arrayBuffer(),
    new Response(child.stderr).arrayBuffer(),
    child.exited,
  ])
  return exitCode === 0
}

/**
 * Select the strongest accepted C dialect without silently falling back to C11.
 * The command may be a compiler path or a command prefix such as wsl.exe.
 */
export async function probeCDialect(command, options = {}) {
  for (const dialect of dialects) {
    if (await probe(command, dialect, options)) return { ...dialect }
  }
  return undefined
}

export function dialectArgs(dialect) {
  if (!dialect || typeof dialect.flag !== "string") {
    throw new TypeError("C dialect is required")
  }
  return [dialect.flag]
}

export function dialectDisclosure(dialect) {
  if (!dialect || typeof dialect.name !== "string") {
    throw new TypeError("C dialect is required")
  }
  return dialect.final
    ? "C23"
    : "c2x-preview (correctness-only; not a final C23 result)"
}

export function requireCDialect(dialect, context = "C compiler") {
  if (!dialect) {
    throw new Error(
      `${context} accepts neither -std=c23 nor -std=c2x; C11 recovery must be explicit`,
    )
  }
  return dialect
}
