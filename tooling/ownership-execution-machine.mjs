const FORMS = new Set(["asyncLet", "spawn"])
const CAPTURE_MODES = new Set(["take", "copy", "ref", "inout"])
const STRATEGIES = new Set(["inline", "queued"])
const ACCESS_MODES = new Set(["read", "write"])
const OUTCOMES = new Set(["success", "error", "canceled"])

export class OwnershipExecutionModelError extends Error {
  constructor(code) {
    super(code)
    this.name = "OwnershipExecutionModelError"
    this.code = code
  }
}

function fail(code) {
  throw new OwnershipExecutionModelError(code)
}

function requireString(value, code) {
  if (typeof value !== "string" || value.length === 0) fail(code)
}

function requireOwner(state, ownerId) {
  const owner = state.owners[ownerId]
  if (!owner || owner.location === "dropped") fail("W-OWNERSHIP-0010")
  return owner
}

function requireParentOwner(state, ownerId) {
  const owner = requireOwner(state, ownerId)
  if (owner.location !== "parent") fail("W-OWNERSHIP-0010")
  return owner
}

function requireTask(state, taskId) {
  const task = state.tasks[taskId]
  if (!task) fail("compositionTaskMissing")
  return task
}

function requireParentRunning(state) {
  if (state.parentSuspended) fail("compositionParentSuspended")
}

function reservationsForOwner(state, ownerId) {
  return Object.values(state.loans).filter(
    (loan) => loan.owner === ownerId && loan.phase !== "released",
  )
}

function activeLoanForTask(state, taskId, ownerId) {
  return Object.values(state.loans).find(
    (loan) => loan.task === taskId && loan.owner === ownerId && loan.phase === "active",
  )
}

function dropOwner(state, ownerId, reason) {
  const owner = requireOwner(state, ownerId)
  if (reservationsForOwner(state, ownerId).length > 0) fail("W-BORROW-0008")
  if (owner.dropCount !== 0) fail("compositionDropRepeated")
  owner.location = "dropped"
  owner.dropCount = 1
  state.drops.push({ owner: ownerId, reason })
  state.events.push(`drop:${ownerId}:${reason}`)
}

function releaseLoans(state, task, phase) {
  for (const loanId of task.loanIds) {
    const loan = state.loans[loanId]
    if (!loan || loan.phase !== "active") fail("compositionLoanDrainInvalid")
    loan.phase = phase
    state.events.push(`loan:${loanId}:${phase}`)
  }
}

function closeTaskStaging(state, task, reason, loanPhase) {
  for (const capture of task.captures) {
    if (capture.mode === "take" || capture.mode === "copy") {
      dropOwner(state, capture.carriedOwner, reason)
    }
  }
  releaseLoans(state, task, loanPhase)
}

function validateCapturePlan(state, taskId, captures) {
  if (!Array.isArray(captures)) fail("compositionCaptureListMissing")
  const sourceOwners = new Set()
  const createdOwners = new Set()

  for (const capture of captures) {
    if (!CAPTURE_MODES.has(capture.mode)) fail("compositionCaptureModeInvalid")
    requireString(capture.owner, "compositionCaptureOwnerMissing")
    if (sourceOwners.has(capture.owner)) fail("W-OWNERSHIP-0010")
    sourceOwners.add(capture.owner)

    const owner = requireParentOwner(state, capture.owner)
    if (reservationsForOwner(state, capture.owner).length > 0) {
      fail(capture.mode === "ref" ? "W-BORROW-0002" : "W-BORROW-0006")
    }

    if (capture.mode === "take") {
      if (owner.transferable !== true) fail("W-OWNERSHIP-0010")
      continue
    }

    if (capture.mode === "copy") {
      if (owner.duplicable !== true || owner.transferable !== true) {
        fail("W-OWNERSHIP-0010")
      }
      requireString(capture.as, "compositionCopyBindingMissing")
      if (state.owners[capture.as] || createdOwners.has(capture.as)) {
        fail("compositionCopyBindingDuplicate")
      }
      createdOwners.add(capture.as)
      continue
    }

    if (capture.referentStable !== true) fail("W-BORROW-0004")
    if (capture.originSurvives !== true) fail("W-BORROW-0003")
    if (capture.mode === "ref" && owner.shareable !== true) fail("W-OWNERSHIP-0010")
    if (capture.mode === "inout" && owner.transferable !== true) {
      fail("W-OWNERSHIP-0010")
    }
  }

  if (state.tasks[taskId]) fail("compositionTaskDuplicate")
}

function verifyState(state) {
  if (state.parentSuspended) {
    const owner = state.owners[state.parentSuspension?.owner]
    if (!owner || owner.location !== "parent") fail("compositionParentSuspensionOwner")
  }

  for (const [ownerId, owner] of Object.entries(state.owners)) {
    if (owner.location === "dropped" && owner.dropCount !== 1) {
      fail(`compositionDroppedOwnerInvalid:${ownerId}`)
    }
    if (owner.location !== "dropped" && owner.dropCount !== 0) {
      fail(`compositionLiveOwnerDropped:${ownerId}`)
    }
  }

  for (const [loanId, loan] of Object.entries(state.loans)) {
    const owner = state.owners[loan.owner]
    const task = state.tasks[loan.task]
    if (!task || (loan.phase !== "released" && (!owner || owner.location === "dropped"))) {
      fail(`compositionLoanReferentMissing:${loanId}`)
    }
    if (loan.phase === "active" && !["staged", "published", "active", "suspended", "settled"].includes(task.phase)) {
      fail(`compositionActiveLoanPhase:${loanId}`)
    }
    if (loan.phase === "drained" && task.phase !== "committed") {
      fail(`compositionDrainedLoanPhase:${loanId}`)
    }
    if (loan.phase === "released" && !["joined", "evaluationFailed"].includes(task.phase)) {
      fail(`compositionReleasedLoanPhase:${loanId}`)
    }
  }

  for (const [taskId, task] of Object.entries(state.tasks)) {
    if (task.phase === "evaluationFailed" && task.handlePublished) {
      fail(`compositionEvaluationHandle:${taskId}`)
    }
    if (["published", "active", "suspended", "settled", "committed", "joined"].includes(task.phase) && !task.handlePublished) {
      fail(`compositionHandleMissing:${taskId}`)
    }
    if (task.phase === "committed" && task.cleanupCount !== 1) {
      fail(`compositionCommitBeforeCleanup:${taskId}`)
    }
    if (task.phase === "joined" && (!task.handleConsumed || task.cleanupCount !== 1)) {
      fail(`compositionJoinInvalid:${taskId}`)
    }
    for (const capture of task.captures) {
      if (capture.mode !== "take" && capture.mode !== "copy") continue
      const owner = state.owners[capture.carriedOwner]
      if (task.phase === "staged" && owner?.location !== `staging:${taskId}`) {
        fail(`compositionStagingOwnerMismatch:${taskId}`)
      }
      if (["published", "active", "suspended", "settled"].includes(task.phase) && owner?.location !== `child:${taskId}`) {
        fail(`compositionChildOwnerMismatch:${taskId}`)
      }
      if (task.phase === "committed") {
        const expected = task.returnedOwners.includes(capture.carriedOwner)
          ? `outcome:${taskId}`
          : "dropped"
        if (owner?.location !== expected) fail(`compositionOutcomeOwnerMismatch:${taskId}`)
      }
    }
  }

  if (state.scope === "closed") {
    if (Object.values(state.tasks).some((task) => !["joined", "evaluationFailed"].includes(task.phase))) {
      fail("compositionClosedScopeHasTask")
    }
    if (Object.values(state.loans).some((loan) => loan.phase !== "released")) {
      fail("compositionClosedScopeHasLoan")
    }
    if (Object.values(state.owners).some((owner) => owner.location !== "dropped")) {
      fail("compositionClosedScopeHasOwner")
    }
  }
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "createOwner": {
      requireParentRunning(state)
      requireString(operation.owner, "compositionOwnerMissing")
      if (state.scope !== "open") fail("compositionScopeClosed")
      if (state.owners[operation.owner]) fail("compositionOwnerDuplicate")
      state.owners[operation.owner] = {
        value: operation.value ?? operation.owner,
        location: "parent",
        transferable: operation.transferable === true,
        shareable: operation.shareable === true,
        duplicable: operation.duplicable === true,
        dropCount: 0,
      }
      state.events.push(`owner:${operation.owner}:parent`)
      return
    }

    case "directAccess": {
      requireParentRunning(state)
      const owner = requireParentOwner(state, operation.owner)
      const access = operation.access ?? "read"
      if (!ACCESS_MODES.has(access)) fail("compositionAccessModeInvalid")
      const reservations = reservationsForOwner(state, operation.owner)
      if (reservations.some((loan) => loan.mode === "inout")) fail("W-BORROW-0002")
      if (access !== "read" && reservations.some((loan) => loan.mode === "ref")) {
        fail("W-BORROW-0006")
      }
      if (access === "write") owner.value = operation.value ?? owner.value
      state.events.push(`parent:${access}:${operation.owner}`)
      return
    }

    case "suspendParent": {
      if (state.parentSuspended) fail("compositionParentAlreadySuspended")
      requireParentOwner(state, operation.owner)
      if (!new Set(["ref", "inout"]).has(operation.mode)) {
        fail("compositionParentSuspensionMode")
      }
      if (operation.referentStable !== true) fail("W-BORROW-0004")
      if (operation.originSurvives !== true) fail("W-BORROW-0003")
      const reservations = reservationsForOwner(state, operation.owner)
      if (reservations.some((loan) => loan.mode === "inout")) {
        fail("W-BORROW-0002")
      }
      if (operation.mode === "inout" && reservations.length > 0) {
        fail("W-BORROW-0002")
      }
      state.parentSuspended = true
      state.parentSuspension = { owner: operation.owner, mode: operation.mode }
      state.events.push(`parent:suspend:${operation.owner}`)
      return
    }

    case "resumeParent": {
      if (!state.parentSuspended) fail("compositionParentNotSuspended")
      state.events.push(`parent:resume:${state.parentSuspension.owner}`)
      state.parentSuspended = false
      state.parentSuspension = null
      return
    }

    case "stageChild": {
      requireParentRunning(state)
      if (state.scope !== "open") fail("compositionScopeClosed")
      requireString(operation.task, "compositionTaskMissing")
      if (!FORMS.has(operation.form)) fail("compositionFormInvalid")
      if (!STRATEGIES.has(operation.strategy ?? "queued")) fail("compositionStrategyInvalid")
      if (operation.form === "spawn") requireString(operation.domain, "W-PLACEMENT-0002")
      if (operation.form === "asyncLet" && operation.domain && operation.domain !== "current") {
        fail("W-PLACEMENT-0001")
      }
      if (operation.detached === true) fail("compositionDetachedChildRejected")
      validateCapturePlan(state, operation.task, operation.captures)

      const task = {
        form: operation.form,
        domain: operation.form === "spawn" ? operation.domain : "current",
        phase: "staged",
        strategy: operation.strategy ?? "queued",
        captures: [],
        loanIds: [],
        returnedOwners: [],
        outcome: null,
        cancelRequested: false,
        bodyStarted: false,
        cleanupCount: 0,
        handlePublished: false,
        handleConsumed: false,
        frameReclaimed: false,
        resultBindings: {},
      }
      state.tasks[operation.task] = task

      for (const capture of operation.captures) {
        const owner = state.owners[capture.owner]
        if (capture.mode === "take") {
          owner.location = `staging:${operation.task}`
          task.captures.push({ mode: "take", sourceOwner: capture.owner, carriedOwner: capture.owner })
          continue
        }
        if (capture.mode === "copy") {
          state.owners[capture.as] = {
            value: owner.value,
            location: `staging:${operation.task}`,
            transferable: owner.transferable,
            shareable: owner.shareable,
            duplicable: owner.duplicable,
            dropCount: 0,
          }
          task.captures.push({ mode: "copy", sourceOwner: capture.owner, carriedOwner: capture.as })
          continue
        }

        const loanId = `loan-${state.nextLoanId++}`
        state.loans[loanId] = {
          owner: capture.owner,
          task: operation.task,
          mode: capture.mode,
          phase: "active",
        }
        task.loanIds.push(loanId)
        task.captures.push({ mode: capture.mode, sourceOwner: capture.owner, loanId })
      }
      state.events.push(`task:${operation.task}:staged`)
      return
    }

    case "failEvaluation": {
      const task = requireTask(state, operation.task)
      if (task.phase !== "staged") fail("compositionEvaluationPhase")
      closeTaskStaging(state, task, "evaluation-failure", "released")
      task.phase = "evaluationFailed"
      state.events.push(`task:${operation.task}:evaluation-failed`)
      return
    }

    case "admitChild": {
      const task = requireTask(state, operation.task)
      if (task.phase !== "staged") fail("compositionAdmissionPhase")
      task.handlePublished = true
      if (operation.accepted !== true) {
        closeTaskStaging(state, task, "admission-rejection", "drained")
        task.phase = "committed"
        task.outcome = "canceled"
        task.cleanupCount = 1
        state.events.push(`task:${operation.task}:admission-rejected`)
        return
      }

      for (const capture of task.captures) {
        if (capture.mode === "take" || capture.mode === "copy") {
          state.owners[capture.carriedOwner].location = `child:${operation.task}`
        }
      }
      task.phase = "published"
      state.events.push(`task:${operation.task}:published`)
      return
    }

    case "startChild": {
      const task = requireTask(state, operation.task)
      if (task.phase !== "published") fail("compositionStartPhase")
      task.phase = "active"
      task.bodyStarted = true
      state.happensBefore.push(`stage:${operation.task}->start:${operation.task}`)
      state.physicalTrace.push(`${task.strategy}:start:${operation.task}`)
      state.events.push(`task:${operation.task}:active`)
      return
    }

    case "suspendChild": {
      const task = requireTask(state, operation.task)
      if (task.phase !== "active") fail("compositionSuspendPhase")
      task.phase = "suspended"
      state.physicalTrace.push(`${task.strategy}:suspend:${operation.task}`)
      state.events.push(`task:${operation.task}:suspended`)
      return
    }

    case "resumeChild": {
      const task = requireTask(state, operation.task)
      if (task.phase !== "suspended") fail("compositionResumePhase")
      task.phase = "active"
      state.physicalTrace.push(`${task.strategy}:resume:${operation.task}`)
      state.events.push(`task:${operation.task}:active`)
      return
    }

    case "childAccess": {
      const task = requireTask(state, operation.task)
      if (task.phase !== "active") fail("compositionChildNotActive")
      const capture = task.captures.find(
        (item) => item.sourceOwner === operation.owner || item.carriedOwner === operation.owner,
      )
      if (!capture) fail("compositionOwnerNotCaptured")
      const access = operation.access ?? "read"
      if (!ACCESS_MODES.has(access)) fail("compositionAccessModeInvalid")
      if (capture.mode === "ref" && access !== "read") fail("W-BORROW-0009")
      if (capture.mode === "ref" || capture.mode === "inout") {
        if (!activeLoanForTask(state, operation.task, capture.sourceOwner)) {
          fail("compositionLoanNotActive")
        }
        if (access === "write") state.owners[capture.sourceOwner].value = operation.value
      } else {
        const owner = requireOwner(state, capture.carriedOwner)
        if (owner.location !== `child:${operation.task}`) fail("compositionChildOwnerMissing")
        if (access === "write") owner.value = operation.value
      }
      state.events.push(`child:${operation.task}:${access}:${operation.owner}`)
      return
    }

    case "cancelChild": {
      const task = requireTask(state, operation.task)
      if (!task.handlePublished || task.handleConsumed) fail("compositionCancelHandleUnavailable")
      task.cancelRequested = true
      state.events.push(`task:${operation.task}:cancel-requested`)
      return
    }

    case "settleChild": {
      const task = requireTask(state, operation.task)
      if (!new Set(["published", "active"]).has(task.phase)) {
        fail("compositionSettlePhase")
      }
      if (!OUTCOMES.has(operation.outcome)) fail("compositionOutcomeInvalid")
      if (task.phase === "published" && operation.outcome !== "canceled") {
        fail("compositionBodyNotStarted")
      }
      if (operation.outcome !== "canceled" && !task.bodyStarted) {
        fail("compositionBodyNotStarted")
      }
      if (operation.outcome === "canceled" && !task.cancelRequested && operation.structural !== true) {
        fail("compositionCancellationMissing")
      }
      const returnedOwners = operation.returnOwners ?? []
      if (!Array.isArray(returnedOwners)) fail("compositionReturnedOwnerList")
      if (operation.outcome !== "success" && returnedOwners.length > 0) {
        fail("compositionNonSuccessOwner")
      }
      if (new Set(returnedOwners).size !== returnedOwners.length) {
        fail("compositionReturnedOwnerDuplicate")
      }
      for (const ownerId of returnedOwners) {
        const capture = task.captures.find(
          (item) => item.carriedOwner === ownerId && ["take", "copy"].includes(item.mode),
        )
        if (!capture || state.owners[ownerId]?.location !== `child:${operation.task}`) {
          fail("compositionReturnedOwnerInvalid")
        }
      }
      task.returnedOwners = [...returnedOwners]
      task.outcome = operation.outcome
      task.phase = "settled"
      state.events.push(`task:${operation.task}:settled:${operation.outcome}`)
      return
    }

    case "finishCleanup": {
      const task = requireTask(state, operation.task)
      if (task.cleanupCount !== 0) fail("compositionCleanupRepeated")
      if (task.phase !== "settled") fail("compositionCleanupPhase")
      for (const capture of task.captures) {
        if (capture.mode !== "take" && capture.mode !== "copy") continue
        if (task.returnedOwners.includes(capture.carriedOwner)) {
          state.owners[capture.carriedOwner].location = `outcome:${operation.task}`
        } else {
          dropOwner(state, capture.carriedOwner, `cleanup:${operation.task}`)
        }
      }
      releaseLoans(state, task, "drained")
      task.cleanupCount = 1
      task.phase = "committed"
      state.events.push(`task:${operation.task}:committed`)
      return
    }

    case "reclaimFrame": {
      const task = requireTask(state, operation.task)
      if (!new Set(["committed", "joined"]).has(task.phase)) fail("compositionFrameReclaimEarly")
      if (task.frameReclaimed) fail("compositionFrameReclaimRepeated")
      task.frameReclaimed = true
      state.events.push(`task:${operation.task}:frame-reclaimed`)
      return
    }

    case "joinChild": {
      const task = requireTask(state, operation.task)
      if (task.handleConsumed) fail("compositionJoinRepeated")
      if (task.phase !== "committed") fail("compositionJoinBeforeCleanup")
      const bindings = operation.bindings ?? {}
      if (typeof bindings !== "object" || bindings === null || Array.isArray(bindings)) {
        fail("compositionResultBindingsInvalid")
      }
      if (Object.keys(bindings).length !== task.returnedOwners.length) {
        fail("compositionResultBindingCount")
      }
      for (const ownerId of task.returnedOwners) {
        const newBinding = bindings[ownerId]
        requireString(newBinding, "compositionResultBindingMissing")
        if (newBinding !== ownerId && state.owners[newBinding]) {
          fail("compositionResultBindingDuplicate")
        }
        const owner = state.owners[ownerId]
        if (!owner || owner.location !== `outcome:${operation.task}`) {
          fail("compositionOutcomeOwnerMissing")
        }
        if (newBinding !== ownerId) {
          delete state.owners[ownerId]
          state.owners[newBinding] = owner
        }
        owner.location = "parent"
        task.resultBindings[ownerId] = newBinding
      }
      for (const loanId of task.loanIds) {
        const loan = state.loans[loanId]
        if (loan.phase !== "drained") fail("compositionJoinLoanNotDrained")
        loan.phase = "released"
        state.events.push(`loan:${loanId}:released`)
      }
      task.handleConsumed = true
      task.phase = "joined"
      state.happensBefore.push(`commit:${operation.task}->join:${operation.task}`)
      state.events.push(`task:${operation.task}:joined`)
      return
    }

    case "scopeExit": {
      if (state.parentSuspended) fail("compositionParentSuspendedAtClose")
      const cooperative = operation.cooperative === true
      if (!cooperative && Object.values(state.tasks).some(
        (task) => ["active", "suspended"].includes(task.phase),
      )) {
        fail("compositionScopeExitNeedsProgress")
      }
      for (const [taskId, task] of Object.entries(state.tasks)) {
        if (task.phase === "evaluationFailed" || task.phase === "joined") continue

        if (task.phase === "staged") {
          closeTaskStaging(state, task, "scope-exit-staging", "released")
          task.phase = "evaluationFailed"
          state.events.push(`task:${taskId}:evaluation-failed`)
          continue
        }

        if (["published", "active", "suspended"].includes(task.phase)) {
          task.cancelRequested = true
          if (task.phase === "suspended") {
            task.phase = "active"
            state.physicalTrace.push(`${task.strategy}:resume:${taskId}`)
          }
          task.returnedOwners = []
          task.outcome = "canceled"
          task.phase = "settled"
          state.events.push(`task:${taskId}:cancel-requested`)
          state.events.push(`task:${taskId}:settled:canceled`)
        }

        if (task.phase === "settled") {
          for (const capture of task.captures) {
            if (capture.mode !== "take" && capture.mode !== "copy") continue
            if (task.returnedOwners.includes(capture.carriedOwner)) {
              state.owners[capture.carriedOwner].location = `outcome:${taskId}`
            } else {
              dropOwner(state, capture.carriedOwner, `cleanup:${taskId}`)
            }
          }
          releaseLoans(state, task, "drained")
          task.cleanupCount = 1
          task.phase = "committed"
          state.events.push(`task:${taskId}:committed`)
        }

        if (task.phase === "committed") {
          for (const ownerId of task.returnedOwners) {
            const owner = state.owners[ownerId]
            if (!owner || owner.location !== `outcome:${taskId}`) {
              fail("compositionOutcomeOwnerMissing")
            }
            owner.location = "parent"
          }
          for (const loanId of task.loanIds) {
            const loan = state.loans[loanId]
            if (loan.phase !== "drained") fail("compositionJoinLoanNotDrained")
            loan.phase = "released"
            state.events.push(`loan:${loanId}:released`)
          }
          for (const ownerId of task.returnedOwners) {
            dropOwner(state, ownerId, `scope-exit-result:${taskId}`)
          }
          task.handleConsumed = true
          task.phase = "joined"
          state.happensBefore.push(`commit:${taskId}->join:${taskId}`)
          state.events.push(`task:${taskId}:joined`)
        }
      }

      for (const [ownerId, owner] of Object.entries(state.owners)) {
        if (owner.location === "parent") dropOwner(state, ownerId, "scope-exit-parent")
      }
      state.scope = "closed"
      state.events.push("scope:closed")
      return
    }

    case "dropOwner": {
      requireParentRunning(state)
      requireParentOwner(state, operation.owner)
      dropOwner(state, operation.owner, operation.reason ?? "parent-scope")
      return
    }

    case "closeScope": {
      if (state.parentSuspended) fail("compositionParentSuspendedAtClose")
      if (Object.values(state.tasks).some((task) => !["joined", "evaluationFailed"].includes(task.phase))) {
        fail("compositionScopeHasUnjoinedChild")
      }
      if (Object.values(state.loans).some((loan) => loan.phase !== "released")) {
        fail("compositionScopeHasLoan")
      }
      if (Object.values(state.owners).some((owner) => owner.location !== "dropped")) {
        fail("compositionScopeHasLiveOwner")
      }
      state.scope = "closed"
      state.events.push("scope:closed")
      return
    }

    default:
      fail(`compositionOperationUnknown:${operation.op}`)
  }
}

function projectState(state) {
  return {
    scope: state.scope,
    parentSuspended: state.parentSuspended,
    owners: Object.fromEntries(
      Object.entries(state.owners)
        .sort(([left], [right]) => left.localeCompare(right))
        .map(([id, owner]) => [id, {
          value: owner.value,
          location: owner.location,
          dropCount: owner.dropCount,
        }]),
    ),
    tasks: Object.fromEntries(
      Object.entries(state.tasks)
        .sort(([left], [right]) => left.localeCompare(right))
        .map(([id, task]) => [id, {
          form: task.form,
          domain: task.domain,
          phase: task.phase,
          outcome: task.outcome,
          bodyStarted: task.bodyStarted,
          cancelRequested: task.cancelRequested,
          cleanupCount: task.cleanupCount,
          handleConsumed: task.handleConsumed,
          frameReclaimed: task.frameReclaimed,
          resultBindings: task.resultBindings,
        }]),
    ),
    loans: Object.fromEntries(
      Object.entries(state.loans)
        .sort(([left], [right]) => left.localeCompare(right))
        .map(([id, loan]) => [id, { ...loan }]),
    ),
    drops: state.drops,
    happensBefore: state.happensBefore,
    events: state.events,
  }
}

export function runOwnershipExecutionOperations(operations) {
  const state = {
    scope: "open",
    parentSuspended: false,
    parentSuspension: null,
    owners: {},
    tasks: {},
    loans: {},
    drops: [],
    happensBefore: [],
    events: [],
    physicalTrace: [],
    nextLoanId: 1,
  }

  try {
    for (const operation of operations) {
      applyOperation(state, operation)
      verifyState(state)
    }
    return {
      status: "accepted",
      error: null,
      state: projectState(state),
      physical: { trace: state.physicalTrace },
    }
  } catch (error) {
    if (!(error instanceof OwnershipExecutionModelError)) throw error
    return {
      status: "rejected",
      error: error.code,
      state: projectState(state),
      physical: { trace: state.physicalTrace },
    }
  }
}
