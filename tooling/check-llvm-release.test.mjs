import { describe, expect, test } from "bun:test";
import { compareCatalog, compareVersions, latestStableRelease } from "./check-llvm-release.mjs";

const catalog = {
  dependencies: [{
    id: "mlir0-llvm-clang",
    latestStable: { version: "23.1.1", tag: "llvmorg-23.1.1" },
  }],
};

describe("LLVM stable release watch", () => {
  test("selects the newest numeric stable release and ignores prereleases", () => {
    const release = latestStableRelease([
      { tag_name: "llvmorg-24.1.0-rc1", prerelease: true },
      { tag_name: "llvmorg-23.1.1", html_url: "https://example/23.1.1" },
      { tag_name: "llvmorg-22.1.9", html_url: "https://example/22.1.9" },
      { tag_name: "llvmorg-23.1.2", html_url: "https://example/23.1.2" },
    ]);
    expect(release.version).toBe("23.1.2");
    expect(release.tag).toBe("llvmorg-23.1.2");
  });

  test("distinguishes a current catalog from a required update", () => {
    expect(compareCatalog(catalog, { version: "23.1.1", tag: "llvmorg-23.1.1" }).status).toBe("current");
    expect(compareCatalog(catalog, { version: "23.1.2", tag: "llvmorg-23.1.2" }).status).toBe("update-required");
    expect(compareVersions("24.1.0", "23.1.9")).toBeGreaterThan(0);
  });

  test("fails closed for a missing catalog entry or stable release", () => {
    expect(compareCatalog({ dependencies: [] }, { version: "23.1.1", tag: "llvmorg-23.1.1" }).status).toBe("invalid");
    expect(compareCatalog(catalog, null).status).toBe("invalid");
  });
});
