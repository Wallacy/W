// Typed target escapes for the last restaurant before the end of the Universe.

module system_escapes

import device from std
import { ThreadLocal } from std.runtime.thread

// W-1237: safe native TLS stores only Copy values without drop obligations.
struct NativeCounters {
  const samples = ThreadLocal<u64>.key(initial: 0)
}

export fn noteNativeSample(): u64 {
  return NativeCounters.samples.write((count: inout u64) => {
    count += 1
    return count
  })
}

// W-1234: MMIO provenance and access behavior come from DeviceContext.
export fn horizonStatus(ctx: ref device.Context): u32 throws device.Error {
  let status: device.MmioRegister<u32, access: .readOnly> =
    try ctx.mmio.register(named: .horizonStatus)
  return status.load()
}

// W-1239: every effect outside the typed signature belongs to this contract.
unsafe fn<Asm,
  targets: [.aarch64],
  clobbers: [],
  memory: .none,
  stack: .preserved,
  unwind: .never,
  volatile: true,
>
readCycleCounter(): u64 {
  // Opaque Asm body: mrs x0, cntvct_el0
}
