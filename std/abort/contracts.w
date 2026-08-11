// Portable Web abort adapter over structured W cancellation.
//
// AbortSignal is an observation handle. It does not grant cancellation,
// network, clock, or timer authority. AbortController owns the only public
// authority that can transition its signal. Native task cancellation remains
// a control outcome and is only mirrored into this adapter at Web boundaries.

import { TaskTimeout } from std.runtime.task

export type AbortSourceLimit = usize<(1...1_024)>

export enum AbortExternalReason: Copy {
  peerDisconnected
  transportReset
}

// The reason is a bounded, owned snapshot. The provider publishes the first
// reason once. A later native Cancellation can contain additional causes, but
// it does not mutate this Web-facing snapshot.
export enum AbortReason: Error & Copy {
  requested(CancellationReason)
  timeout
  cancellation(Cancellation)
  external(AbortExternalReason)
}

export enum AbortSignalCombineError: Error {
  sourceLimitExceeded(maximumSources: AbortSourceLimit, found: usize)
}

// Raw handles and state transitions belong to a versioned runtime provider.
// The provider stores the reason before publishing the terminal state, uses
// release/acquire synchronization, and removes timers, waiters, and dependent
// registrations exactly once. Per-result registrations use maximumSources;
// total live states and registrations use provider allocation/admission.
foreign intrinsic from "std.abort-state@1" {
  type AbortSignalHandle
  type AbortControllerHandle

  fn stdAbortSignalAlready(reason: AbortReason): AbortSignalHandle
  fn stdAbortSignalTimeout(timeout: TaskTimeout): AbortSignalHandle
  fn stdAbortSignalAny(
    named maximumSources: AbortSourceLimit,
    _ sources: ref AbortSignal...,
  ): AbortSignalHandle throws AbortSignalCombineError

  fn stdAbortSignalAborted(handle: ref AbortSignalHandle): Bool
  fn stdAbortSignalReason(handle: ref AbortSignalHandle): AbortReason?
  fn stdAbortSignalDuplicate(handle: ref AbortSignalHandle): AbortSignalHandle
  async fn stdAbortSignalWait(handle: ref AbortSignalHandle): AbortReason
  fn stdAbortSignalDrop(handle: inout AbortSignalHandle)

  fn stdAbortControllerCreate(): (AbortControllerHandle, AbortSignalHandle)
  fn stdAbortControllerAbort(
    named authority: ref AbortControllerHandle,
    named reason: AbortReason,
  )
  fn stdAbortControllerDrop(authority: inout AbortControllerHandle)
}

export struct AbortSignal: Duplicable {
  handle: AbortSignalHandle

  init(validatedHandle: AbortSignalHandle) {
    self.handle = validatedHandle
  }

  export static fn abort(
    reason: AbortReason = .requested(.userRequest),
  ): AbortSignal {
    let handle = unsafe { stdAbortSignalAlready(reason) }
    return AbortSignal(validatedHandle: handle)
  }

  export static fn timeout(for timeout: TaskTimeout): AbortSignal {
    // Zero is already aborted. A positive timeout owns a timer resource in the
    // signal state. Creator/root cancellation does not abort this signal. A
    // timer-budget admission failure publishes .cancellation and requests the
    // current structural cancellation.
    let handle = unsafe { stdAbortSignalTimeout(timeout) }
    return AbortSignal(validatedHandle: handle)
  }

  // Before registration, the provider validates direct argument count, records
  // the first lexical aborted input, flattens and deduplicates pending leaves,
  // then validates unique pending-leaf count. A failed bound registers nothing.
  // After both pass, the recorded terminal input wins or each pending leaf gets
  // one registration. The result owns leaves and source registrations are
  // non-owning, so nested any forms a DAG without a refcount cycle. An empty
  // list never aborts. Future races use one transition.
  export static fn any(
    maximumSources: AbortSourceLimit,
    _ sources: ref AbortSignal...,
  ): AbortSignal throws AbortSignalCombineError {
    let handle = unsafe {
      try stdAbortSignalAny(
        maximumSources: maximumSources,
        each sources,
      )
    }
    return AbortSignal(validatedHandle: handle)
  }

  export aborted: Bool {
    get => unsafe { stdAbortSignalAborted(handle) }
  }

  export reason: AbortReason? {
    get => unsafe { stdAbortSignalReason(handle) }
  }

  export fn duplicate(): AbortSignal {
    let duplicate = unsafe { stdAbortSignalDuplicate(handle) }
    return AbortSignal(validatedHandle: duplicate)
  }

  export fn throwIfAborted(): () throws AbortReason {
    guard let reason = unsafe {
      stdAbortSignalReason(handle)
    } else return

    throw reason
  }

  // A committed abort completion returns its reason. Otherwise, waiting task
  // cancellation removes this waiter and propagates as a control outcome.
  export async fn wait(): AbortReason {
    return unsafe { await stdAbortSignalWait(handle) }
  }

  deinit {
    unsafe { stdAbortSignalDrop(inout handle) }
  }
}

// The controller is move-only. Borrowed access can be shared because the
// provider transition is atomic. Dropping the controller does not abort; if
// the last authority disappears, a pending signal can remain pending forever.
export struct AbortController {
  authority: AbortControllerHandle
  exposedSignal: AbortSignal

  export init() {
    let (authority, signal) = unsafe { stdAbortControllerCreate() }
    self.authority = authority
    self.exposedSignal = AbortSignal(validatedHandle: signal)
  }

  export signal: ref AbortSignal {
    get => exposedSignal
  }

  export fn abort(
    reason: AbortReason = .requested(.userRequest),
  ) {
    unsafe {
      stdAbortControllerAbort(
        authority: authority,
        reason: reason,
      )
    }
  }

  deinit {
    unsafe { stdAbortControllerDrop(inout authority) }
  }
}
