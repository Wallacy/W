const builds = [
  {
    name: "browser",
    target: "browser" as const,
    entrypoints: ["w-syntax.js", "app.js", "book.js", "playground.js"],
  },
  {
    name: "server",
    target: "bun" as const,
    entrypoints: ["server.ts"],
  },
];

let failed = false;

for (const build of builds) {
  const result = await Bun.build({
    entrypoints: build.entrypoints,
    target: build.target,
    write: false,
  });

  if (!result.success) {
    failed = true;
    console.error(`${build.name}: falhou`);
    for (const log of result.logs) console.error(log);
    continue;
  }

  console.log(`${build.name}: ${result.outputs.length} output(s) em memória`);
}

if (failed) process.exit(1);
