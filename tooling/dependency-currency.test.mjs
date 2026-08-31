import fs from "node:fs";
import { describe, expect, test } from "bun:test";
import {
  DEPENDENCY_CLASSES,
  checkRenderedDependencies,
  loadDependencyCurrency,
  renderDependencyCurrency,
  validateDependencyCurrency,
} from "./dependency-currency.mjs";

const source = loadDependencyCurrency();

function copySource() {
  return structuredClone(source);
}

function entry(value, id) {
  return value.dependencies.find((candidate) => candidate.id === id);
}

function errorsAfter(mutator) {
  const value = copySource();
  mutator(value);
  return validateDependencyCurrency(value).errors;
}

function expectError(errors, text) {
  expect(errors.some((error) => error.includes(text))).toBe(true);
}

describe("dependency currency catalog", () => {
  test("validates the checked-in catalog and generated projection", () => {
    expect(validateDependencyCurrency(source).errors).toEqual([]);
    expect(source.$schema).toBe("w-dependency-currency-1");
    expect(source.observedAt).toBe("2026-08-31");
    expect(source.dependencies.map((item) => item.classification)).toEqual(
      expect.arrayContaining(DEPENDENCY_CLASSES),
    );
    const document = fs.readFileSync("DEPENDENCIES.md", "utf8");
    expect(checkRenderedDependencies(document, source)).toEqual([]);
    expect(renderDependencyCurrency(source)).toBe(document);
  });

  test("requires every managed dependency to use the exact latest stable selector", () => {
    expectError(errorsAfter((value) => {
      entry(value, "tree-sitter-cli").selected.selector = "latest";
    }), "tree-sitter-cli.selected.selector must not be floating or a range");
    expectError(errorsAfter((value) => {
      entry(value, "tree-sitter-cli").latestStable.version = "0.26.13";
    }), "tree-sitter-cli.latestStable.version must be 0.27.0");
    expectError(errorsAfter((value) => {
      entry(value, "actions-checkout").selected.sha = "deadbeef";
    }), "actions-checkout.selected.sha must be 3d3c42e5aac5ba805825da76410c181273ba90b1");
    expectError(errorsAfter((value) => {
      entry(value, "actions-setup-bun").selected.tag = "v2";
    }), "actions-setup-bun.selected.tag must be v2.2.0");
  });

  test("rejects prerelease, range, and floating policy drift", () => {
    expectError(errorsAfter((value) => {
      value.policy.managedDependency.selector = "range";
    }), "policy.managedDependency.selector must be exact");
    expectError(errorsAfter((value) => {
      entry(value, "bun-runtime").selected.stability = "nightly";
    }), "bun-runtime.selected.stability must be stable");
    expectError(errorsAfter((value) => {
      entry(value, "vscode-vsce-on-demand").selected.selector = "@vscode/vsce@^3.9.2";
    }), "vscode-vsce-on-demand.selected.selector must not be floating or a range");
  });

  test("preserves historical evidence and blocks promotion", () => {
    expectError(errorsAfter((value) => {
      entry(value, "mlir0-llvm-clang").current.version = "23.1.0";
    }), "MLIR0 current evidence must remain 20.1.2");
    expectError(errorsAfter((value) => {
      entry(value, "mlir0-llvm-clang").selected.promotion = "promoted";
    }), "MLIR0 successor promotion must remain blocked");
    expectError(errorsAfter((value) => {
      entry(value, "unicode-ucd").selected.version = "16.0.0";
    }), "Unicode UCD current and selected snapshots must remain 17.0.0");
    expectError(errorsAfter((value) => {
      entry(value, "rust-correctness-baseline").requirements.preserveCurrent = false;
    }), "rust-correctness-baseline.requirements.preserveCurrent must be true");
  });

  test("keeps compatibility floors separate from managed currency", () => {
    expectError(errorsAfter((value) => {
      entry(value, "cmake-floor").requirements.mustNotRaiseForCurrency = false;
    }), "cmake-floor.requirements.mustNotRaiseForCurrency must be true");
    expectError(errorsAfter((value) => {
      entry(value, "ninja-build-requirement").selected = null;
    }), "ninja-build-requirement.selected must preserve its floor or recipe");
    expectError(errorsAfter((value) => {
      entry(value, "vscode-engine-floor").successor = { version: "2.0.0" };
    }), "vscode-engine-floor.successor must remain null");
  });

  test("keeps external evaluations non-authoritative", () => {
    expectError(errorsAfter((value) => {
      entry(value, "portable-mlir-toolchain").status = "current";
    }), "portable-mlir-toolchain.status must be evaluation-only");
    expectError(errorsAfter((value) => {
      entry(value, "portable-mlir-toolchain").requirements.promotionAllowed = true;
    }), "portable-mlir-toolchain.requirements.promotionAllowed must be false");
    expectError(errorsAfter((value) => {
      entry(value, "portable-mlir-toolchain").limitations = [];
    }), "portable-mlir-toolchain.limitations must declare external evaluation limits");
  });

  test("rejects duplicate IDs, invalid paths, and invalid sources", () => {
    expectError(errorsAfter((value) => {
      value.dependencies[1].id = value.dependencies[0].id;
    }), "catalog contains duplicate dependency id");
    expectError(errorsAfter((value) => {
      entry(value, "bun-runtime").locations = ["../package.json"];
    }), "locations contains an invalid repository path");
    expectError(errorsAfter((value) => {
      entry(value, "bun-runtime").source.urls[0] = "http://example.invalid";
    }), "source.urls[0] must be an HTTPS URL");
    expectError(errorsAfter((value) => {
      entry(value, "mlir0-llvm-clang").source.urls[0] = "https://example.invalid/mlir";
    }), "source.urls must include the official release URL for llvmorg-23.1.0");
  });

  test("cross-checks lock and platform files against catalog selections", () => {
    expectError(errorsAfter((value) => {
      entry(value, "tree-sitter-cli").selected.integrity = "sha512-catalog-drift";
    }), "selected Tree-sitter integrity");
    expectError(errorsAfter((value) => {
      entry(value, "mlir0-llvm-clang").selected.commit = "0000000000000000000000000000000000000000";
    }), "tooling/platform-support.json successorCommit must match the selected MLIR successor");
  });

  test("rejects manual projection drift", () => {
    const document = fs.readFileSync("DEPENDENCIES.md", "utf8");
    const drifted = document.replace("# Dependency currency", "# Changed dependency currency");
    expect(checkRenderedDependencies(drifted, source).join("\n")).toContain("stale or manually edited");
  });
});
