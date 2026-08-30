import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const skippedDirectories = new Set([".git", "node_modules"]);

function collectMarkdownFiles(directory, files = []) {
  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    if (entry.isDirectory() && skippedDirectories.has(entry.name)) continue;

    const entryPath = path.join(directory, entry.name);
    if (entry.isDirectory()) collectMarkdownFiles(entryPath, files);
    else if (entry.isFile() && entry.name.endsWith(".md")) files.push(entryPath);
  }
  return files;
}

function markdownSlug(text) {
  return text
    .replace(/<[^>]+>/g, "")
    .replace(/!\[([^\]]*)\]\([^)]*\)/g, "$1")
    .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1")
    .replace(/[`*_~]/g, "")
    .trim()
    .toLocaleLowerCase("en-US")
    .replace(/[^\p{L}\p{M}\p{N}\s_-]/gu, "")
    .replace(/\s+/g, "-");
}

const anchorCache = new Map();

function markdownAnchors(filePath) {
  const cached = anchorCache.get(filePath);
  if (cached) return cached;

  const anchors = new Set();
  const duplicates = new Map();
  const lines = fs.readFileSync(filePath, "utf8").split(/\r?\n/);

  for (const line of lines) {
    const heading = line.match(/^#{1,6}\s+(.+?)\s*#*\s*$/);
    if (heading) {
      const base = markdownSlug(heading[1]);
      const duplicate = duplicates.get(base) ?? 0;
      anchors.add(duplicate === 0 ? base : `${base}-${duplicate}`);
      duplicates.set(base, duplicate + 1);
    }

    for (const explicit of line.matchAll(/<(?:a|span)\s+(?:id|name)=["']([^"']+)["']/gi)) {
      anchors.add(explicit[1].toLocaleLowerCase("en-US"));
    }
  }

  anchorCache.set(filePath, anchors);
  return anchors;
}

function decode(value, source, lineNumber, failures) {
  try {
    return decodeURIComponent(value);
  } catch {
    failures.push(`${source}:${lineNumber}: invalid URL encoding: ${value}`);
    return null;
  }
}

const failures = [];
const files = collectMarkdownFiles(repositoryRoot).sort();
const linkPattern = /!?\[[^\]]*\]\((<[^>]+>|[^)\s]+)(?:\s+(?:"[^"]*"|'[^']*'))?\)/g;

for (const filePath of files) {
  const relativeSource = path.relative(repositoryRoot, filePath);
  const lines = fs.readFileSync(filePath, "utf8").split(/\r?\n/);
  let fence = null;

  for (const [index, sourceLine] of lines.entries()) {
    const fenceMarker = sourceLine.match(/^\s*(```+|~~~+)/)?.[1];
    if (fenceMarker) {
      if (!fence) fence = fenceMarker[0];
      else if (fence === fenceMarker[0]) fence = null;
      continue;
    }
    if (fence) continue;

    const line = sourceLine.replace(/`[^`]*`/g, "");
    for (const match of line.matchAll(linkPattern)) {
      let target = match[1];
      if (target.startsWith("<") && target.endsWith(">")) target = target.slice(1, -1);
      if (/^[a-z][a-z0-9+.-]*:/i.test(target) || target.startsWith("//")) continue;

      const hashIndex = target.indexOf("#");
      const rawPath = hashIndex === -1 ? target : target.slice(0, hashIndex);
      const rawAnchor = hashIndex === -1 ? "" : target.slice(hashIndex + 1);
      const decodedPath = decode(rawPath.split("?", 1)[0], relativeSource, index + 1, failures);
      const decodedAnchor = decode(rawAnchor, relativeSource, index + 1, failures);
      if (decodedPath === null || decodedAnchor === null) continue;

      const targetPath = decodedPath
        ? path.resolve(path.dirname(filePath), decodedPath)
        : filePath;

      if (!targetPath.startsWith(repositoryRoot + path.sep) && targetPath !== repositoryRoot) {
        failures.push(`${relativeSource}:${index + 1}: local link escapes the repository: ${target}`);
        continue;
      }
      if (!fs.existsSync(targetPath)) {
        failures.push(`${relativeSource}:${index + 1}: missing local target: ${target}`);
        continue;
      }

      if (decodedAnchor && fs.statSync(targetPath).isFile() && targetPath.endsWith(".md")) {
        const anchor = decodedAnchor.toLocaleLowerCase("en-US");
        if (!markdownAnchors(targetPath).has(anchor)) {
          failures.push(`${relativeSource}:${index + 1}: missing local anchor: ${target}`);
        }
      }
    }
  }
}

if (failures.length > 0) {
  console.error(failures.join("\n"));
  process.exit(1);
}

console.log(`Markdown links: ${files.length} maintained files checked.`);
