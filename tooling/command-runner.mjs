import { loadCommandRegistry, runCommand } from "./check-suite.mjs";

function parseArguments(argv) {
  const options = { command: null, list: false, dryRun: false, help: false, args: [] };
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === "--") {
      options.args = argv.slice(index + 1);
      break;
    }
    if (argument === "--command") {
      if (options.command !== null) throw new Error("--command may be used only once");
      const value = argv[index + 1];
      if (typeof value !== "string" || value.length === 0 || value.startsWith("--")) {
        throw new Error("--command requires a command name");
      }
      options.command = value;
      index += 1;
    } else if (argument === "--list") {
      if (options.list) throw new Error("--list may be used only once");
      options.list = true;
    } else if (argument === "--dry-run") {
      if (options.dryRun) throw new Error("--dry-run may be used only once");
      options.dryRun = true;
    } else if (argument === "--help" || argument === "-h") {
      if (options.help) throw new Error("--help may be used only once");
      options.help = true;
    } else {
      throw new Error(`unknown option: ${String(argument)}`);
    }
  }
  if (options.help && (options.command !== null || options.list || options.dryRun || options.args.length > 0)) {
    throw new Error("--help cannot be combined with another option");
  }
  if (options.list && (options.command !== null || options.dryRun || options.args.length > 0)) {
    throw new Error("--list cannot be combined with another action");
  }
  if (options.dryRun && options.command === null) {
    throw new Error("--dry-run requires --command <name>");
  }
  if (options.args.length > 0 && options.command === null) {
    throw new Error("-- requires --command <name>");
  }
  if (!options.help && !options.list && options.command === null) {
    throw new Error("choose --list or --command <name>");
  }
  return options;
}

const HELP = `usage:
  bun tooling/command-runner.mjs --list
  bun tooling/command-runner.mjs --command <name> [--dry-run] [-- args]

Internal leaf commands live in tooling/command-registry.json. They are
shell-free and are normally selected through bun check --target <target>.
`;

export function main(argv = process.argv.slice(2)) {
  let options;
  try {
    options = parseArguments(argv);
    if (options.help) {
      process.stdout.write(HELP);
      return 0;
    }
    const loaded = loadCommandRegistry();
    if (options.list) {
      for (const [name, command] of Object.entries(loaded.commands)) {
        process.stdout.write(`${name}\t${command.description}\n`);
      }
      return 0;
    }
    return runCommand({
      commandRecords: loaded.commands,
      commandName: options.command,
      dryRun: options.dryRun,
      forwardArgs: options.args,
    });
  } catch (error) {
    process.stderr.write(`command-runner: ${error.message}\n`);
    return 2;
  }
}

if (import.meta.main) process.exit(main());
