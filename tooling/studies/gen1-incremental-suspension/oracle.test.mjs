import { describe, expect, test } from "bun:test";
import { compareGen1Lowerings, runGen1Program } from "../../gen1-incremental-suspension-machine.mjs";

describe("GEN1 study oracle", () => {
  test("the two independent lowerings preserve a simple pull", () => {
    const operations = [{ op: "open", owner: "source" }, { op: "pullAcquire", owner: "source", token: "p1" }, { op: "pullNone", owner: "source", token: "p1" }];
    expect(compareGen1Lowerings(operations).equivalence.equivalent).toBe(true);
  });
  test("a late custom token is rejected by derived terminal state", () => {
    const result = runGen1Program([{ op: "open", owner: "source" }, { op: "resumeAcquire", owner: "source", token: "r1" }, { op: "resumeNone", owner: "source", token: "r1" }, { op: "resumeAcquire", owner: "source", token: "r2" }]);
    expect(result).toMatchObject({ status: "rejected", reason: "resume-terminal", operation: 3 });
  });
});
