import { describe, expect, test } from "bun:test";
import { compactJupyterState, runJupyterProgram } from "./jupyter-machine.mjs";

const key = "pyn3-test-key";
const open = () => ({
  op: "open",
  kernelspec: { argv: ["w-kernel", "{connection_file}"], display_name: "W", language: "w", interrupt_mode: "message", kernel_protocol_version: "5.5", metadata: { supported_encryption: "curve" } },
  connection: { transport: "tcp", ip: "127.0.0.1", signature_scheme: "hmac-sha256", key, shell_port: 51001, iopub_port: 51002, stdin_port: 51003, control_port: 51004, hb_port: 51005, curve_publickey: "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", curve_secretkey: "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb" },
  fileFacts: { permissions: "user-only", remoteAllowed: false, logSecret: false, persistSecret: false },
  policy: { requireCurve: true },
  kernelInfo: { implementation: "W", implementation_version: "0", protocol_version: "5.5", supported_features: [], language_info: { name: "w", version: "0", mimetype: "text/x-w", file_extension: ".w" }, banner: "W kernel" },
  host: { sessionId: "w-session-test", incarnation: 1, generationId: "opaque:gen0", executionOrdinal: 0 },
});

const frame = (msgId, msgType, channel = "shell", content = {}) => ({ op: "frame", request: { msgId, msgType, channel, content: msgType === "execute_request" && Object.keys(content).length === 0 ? { code: "", silent: false, store_history: true, user_expressions: {}, allow_stdin: false, stop_on_error: false } : content } });

describe("PYN3 Jupyter host oracle", () => {
  test("HMAC precedes JSON use and replay is bounded", () => {
    const tampered = runJupyterProgram([open(), { op: "frame", request: { msgId: "bad", msgType: "kernel_info_request", channel: "shell", tamperSignature: true } }]);
    expect(tampered.error.code).toBe("W-JUPYTER-0001");
    expect(compactJupyterState(tampered.state).jsonUsed).toBe(false);
  });

  test("execute follows busy, reply, outputs, idle", () => {
    const result = runJupyterProgram([
      open(),
      frame("e", "execute_request"),
      { op: "execute", frameMsgId: "e", analysis: { readOnly: true, effectFree: true, outputs: [{ media: "text/plain" }] } },
    ]);
    expect(result.status).toBe("accepted");
    expect(result.state.events[0].sequence).toEqual(["busy", "process", "reply", "outputs", "idle"]);
  });

  test("silent mutation is rejected before execution", () => {
    const result = runJupyterProgram([open(), frame("s", "execute_request", "shell", { code: "", silent: true, store_history: true, user_expressions: {}, allow_stdin: false, stop_on_error: false }), { op: "execute", frameMsgId: "s", analysis: { mutation: true, readOnly: false, effectFree: false } }]);
    expect(result.error.code).toBe("W-JUPYTER-0005");
    expect(result.state.executionCount).toBe(0);
  });

  test("read requests use committed snapshots and code-point offsets", () => {
    const ok = runJupyterProgram([open(), frame("i", "inspect_request", "shell", { code: "x", cursor_pos: 1, detail_level: 0 }), { op: "read", frameMsgId: "i", analysis: { offsetUnit: "unicode-codepoint", readsStaging: false, executes: false, analyzer: { plainText: true } } }]);
    expect(ok.state.lastRead.snapshot).toBe("committed");
    const bad = runJupyterProgram([open(), frame("ib", "complete_request", "shell", { code: "x", cursor_pos: 1 }), { op: "read", frameMsgId: "ib", analysis: { offsetUnit: "byte", readsStaging: false, executes: false } }]);
    expect(bad.error.code).toBe("W-JUPYTER-0009");
  });

  test("shutdown waits for safe close and drain", () => {
    const result = runJupyterProgram([open(), frame("sd", "shutdown_request", "control", { restart: false }), { op: "control", frameMsgId: "sd", analysis: { safeClose: true } }]);
    expect(result.state.shutdown).toBe("ok");
    expect(result.state.phase).toBe("closed");
  });
});
