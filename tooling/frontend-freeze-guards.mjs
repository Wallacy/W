export const EXPECTED_FRONTEND_FAMILIES = ["G0", "G1", "G2", "G3", "G4", "G5"]

export function duplicateValues(values) {
  const seen = new Set()
  const duplicates = new Set()
  for (const value of values) {
    if (seen.has(value)) duplicates.add(value)
    seen.add(value)
  }
  return [...duplicates]
}

export function isExpectedEcho(left, right) {
  return left === right
}

export function digestMatches(actual, expected) {
  return typeof actual === "string" && typeof expected === "string" && actual === expected
}

export function symbolOccurrenceCount(text, symbol) {
  if (typeof text !== "string" || typeof symbol !== "string" || symbol.length === 0) return 0
  return text.split(symbol).length - 1
}

export function corpusGuardErrors(corpus, expectedFamilies = EXPECTED_FRONTEND_FAMILIES) {
  const errors = []
  if (corpus?.$schema !== "w-frontend-freeze-cases-1") errors.push("invalid schema")
  if (corpus?.status !== "design-oracle-input") errors.push("invalid status")
  if (!Array.isArray(corpus?.families) || corpus.families.length !== expectedFamilies.length) {
    errors.push("families must contain exactly one entry for each expected family")
    return errors
  }

  const ids = []
  const families = []
  for (const [index, entry] of corpus.families.entries()) {
    const owner = "families[" + index + "]"
    ids.push(entry?.id)
    families.push(entry?.family)
    if (!expectedFamilies.includes(entry?.family)) errors.push(owner + ".family must be G0-G5")
    if (!new RegExp("^FZ0-" + entry?.family + "-[a-z0-9-]+$").test(entry?.id || "")) {
      errors.push(owner + ".id must agree with its family")
    }
    if (typeof entry?.construction !== "string" || entry.construction.trim() === "") {
      errors.push(owner + ".construction must be non-empty")
    }
  }
  for (const id of duplicateValues(ids)) errors.push("duplicate family id " + id)
  for (const family of duplicateValues(families)) errors.push("duplicate family " + family)
  for (const family of expectedFamilies) if (!families.includes(family)) errors.push("missing family " + family)
  return errors
}
