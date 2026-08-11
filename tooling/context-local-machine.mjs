export class ContextLocalModelError extends Error {
  constructor(code) {
    super(code)
    this.name = "ContextLocalModelError"
    this.code = code
  }
}

function fail(code) {
  throw new ContextLocalModelError(code)
}

function requireName(value, code = "W-CONTEXT-0001") {
  if (typeof value !== "string" || value.length === 0) fail(code)
}

function requireTask(state, id) {
  const task = state.tasks[id]
  if (!task) fail("contextTaskMissing")
  return task
}

function requireTaskKey(state, id) {
  const key = state.taskKeys[id]
  if (!key) fail("contextTaskKeyMissing")
  return key
}

function requireThreadKey(state, id) {
  const key = state.threadKeys[id]
  if (!key) fail("contextThreadKeyMissing")
  return key
}

function currentBinding(task, key) {
  const stack = task.bindings[key] ?? []
  return stack.length === 0 ? null : stack[stack.length - 1]
}

function readThreadSlot(state, thread, key) {
  state.threadSlots[thread] ??= {}
  if (!(key.id in state.threadSlots[thread])) {
    state.threadSlots[thread][key.id] = key.initial
    state.trace.push(`tls-init:${thread}:${key.id}`)
  }
  return state.threadSlots[thread][key.id]
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "declareTaskKey": {
      requireName(operation.key)
      requireName(operation.symbol)
      requireName(operation.type)
      if (!new Set(["moduleConst", "associatedConst"]).has(operation.context)) {
        fail("W-CONTEXT-0001")
      }
      if (operation.defaultConst !== true) fail("W-CONTEXT-0001")
      if (operation.shareable !== true) fail("W-CONTEXT-0002")
      if (operation.mutable === true || operation.authority === true) {
        fail("W-CONTEXT-0003")
      }
      if (state.taskKeys[operation.key]) fail("W-CONTEXT-0001")
      if (Object.values(state.taskKeys).some((key) => key.symbol === operation.symbol)) {
        fail("W-CONTEXT-0001")
      }
      state.taskKeys[operation.key] = {
        id: operation.key,
        identity: `${operation.symbol}:${operation.type}`,
        symbol: operation.symbol,
        type: operation.type,
        default: operation.default,
      }
      state.trace.push(`task-key:${operation.key}`)
      return
    }

    case "beginTaskScope": {
      const task = requireTask(state, operation.task)
      const key = requireTaskKey(state, operation.key)
      requireName(operation.scope)
      if (state.scopes[operation.scope]) fail("contextScopeDuplicate")
      if (!new Set(["move", "copy"]).has(operation.ownership)) {
        fail("W-CONTEXT-0004")
      }
      if (operation.ownership === "copy" && operation.copyable !== true) {
        fail("W-CONTEXT-0004")
      }
      if (operation.hiddenCopy === true || operation.hiddenRetain === true) {
        fail("W-CONTEXT-0004")
      }
      const binding = {
        id: `${operation.scope}:${key.id}`,
        key: key.id,
        value: operation.value,
        ownerScope: operation.scope,
      }
      task.bindings[key.id] ??= []
      task.bindings[key.id].push(binding)
      task.scopeStack.push(operation.scope)
      state.scopes[operation.scope] = {
        id: operation.scope,
        task: task.id,
        key: key.id,
        bindingId: binding.id,
        children: [],
        phase: "active",
      }
      state.trace.push(`task-bind:${task.id}:${binding.id}`)
      return
    }

    case "getTaskLocal": {
      const task = requireTask(state, operation.task)
      const key = requireTaskKey(state, operation.key)
      const binding = currentBinding(task, key.id)
      state.taskReads.push({
        task: task.id,
        key: key.id,
        bindingId: binding?.id ?? null,
        value: binding?.value ?? key.default,
      })
      return
    }

    case "spawnChild": {
      const parent = requireTask(state, operation.parent)
      const scope = state.scopes[operation.scope]
      requireName(operation.child)
      if (!scope || scope.task !== parent.id || scope.phase !== "active") {
        fail("contextScopeMissing")
      }
      if (!new Set(["asyncLet", "taskGroup", "spawn"]).has(operation.form)) {
        fail("W-CONTEXT-0005")
      }
      if (state.tasks[operation.child]) fail("contextTaskDuplicate")
      const bindings = {}
      for (const [key, stack] of Object.entries(parent.bindings)) {
        const binding = stack[stack.length - 1]
        if (binding) bindings[key] = [binding]
      }
      state.tasks[operation.child] = {
        id: operation.child,
        bindings,
        scopeStack: [],
        parentScope: scope.id,
        thread: null,
        phase: "active",
      }
      scope.children.push(operation.child)
      state.trace.push(`task-child:${parent.id}->${operation.child}`)
      return
    }

    case "settleChild": {
      const child = requireTask(state, operation.child)
      if (child.phase !== "active" || child.parentScope === null) {
        fail("contextChildNotActive")
      }
      const scope = state.scopes[child.parentScope]
      if (!scope) fail("contextScopeMissing")
      child.phase = "settled"
      child.bindings = {}
      scope.children = scope.children.filter((id) => id !== child.id)
      state.trace.push(`task-child-settled:${child.id}`)
      return
    }

    case "crossBoundary": {
      const parent = requireTask(state, operation.parent)
      requireName(operation.target)
      if (!new Set(["service", "wire", "device", "foreignCallback", "hostEntry"]).has(operation.kind)) {
        fail("contextBoundaryUnknown")
      }
      if (operation.inherit === true) fail("W-CONTEXT-0005")
      if (state.tasks[operation.target]) fail("contextTaskDuplicate")
      state.tasks[operation.target] = {
        id: operation.target,
        bindings: {},
        scopeStack: [],
        parentScope: null,
        thread: null,
        phase: "active",
      }
      state.trace.push(`task-boundary:${parent.id}->${operation.target}:${operation.kind}`)
      return
    }

    case "closeTaskScope": {
      const task = requireTask(state, operation.task)
      const scope = state.scopes[operation.scope]
      if (!scope || scope.task !== task.id || scope.phase !== "active") {
        fail("contextScopeMissing")
      }
      if (task.scopeStack[task.scopeStack.length - 1] !== scope.id) {
        fail("contextScopeNotLifo")
      }
      if (operation.operationSettled !== true || scope.children.length !== 0) {
        fail("W-CONTEXT-0006")
      }
      if (operation.dependenciesClosed !== true) fail("W-CONTEXT-0007")
      if (!new Set(["success", "error", "canceled", "panicBoundary"]).has(operation.outcome)) {
        fail("contextOutcomeUnknown")
      }
      const stack = task.bindings[scope.key]
      const binding = stack?.[stack.length - 1]
      if (!binding || binding.id !== scope.bindingId) fail("contextBindingMismatch")
      stack.pop()
      task.scopeStack.pop()
      scope.phase = "closed"
      scope.outcome = operation.outcome
      state.trace.push(`task-pop:${task.id}:${scope.bindingId}:${operation.outcome}`)
      return
    }

    case "escapeTaskLocalDependency":
      requireTask(state, operation.task)
      requireTaskKey(state, operation.key)
      fail("W-CONTEXT-0007")

    case "declareThreadKey": {
      requireName(operation.key, "W-TLS-0001")
      requireName(operation.symbol, "W-TLS-0001")
      requireName(operation.type, "W-TLS-0001")
      if (operation.context !== "associatedConst" && operation.context !== "moduleConst") {
        fail("W-TLS-0001")
      }
      if (operation.initialConst !== true || operation.copy !== true || operation.hasDrop === true) {
        fail("W-TLS-0001")
      }
      if (operation.nativeTls !== true || operation.fiberEmulation === true) {
        fail("W-TLS-0002")
      }
      if (state.threadKeys[operation.key]) fail("W-TLS-0001")
      if (Object.values(state.threadKeys).some((key) => key.symbol === operation.symbol)) {
        fail("W-TLS-0001")
      }
      state.threadKeys[operation.key] = {
        id: operation.key,
        identity: `${operation.symbol}:${operation.type}`,
        symbol: operation.symbol,
        type: operation.type,
        initial: operation.initial,
      }
      state.trace.push(`tls-key:${operation.key}`)
      return
    }

    case "placeTaskOnThread": {
      const task = requireTask(state, operation.task)
      requireName(operation.thread, "contextThreadMissing")
      task.thread = operation.thread
      state.trace.push(`task-thread:${task.id}:${operation.thread}`)
      return
    }

    case "readThreadLocal": {
      const task = requireTask(state, operation.task)
      const key = requireThreadKey(state, operation.key)
      if (task.thread === null) fail("contextThreadMissing")
      state.threadReads.push({
        task: task.id,
        thread: task.thread,
        key: key.id,
        value: readThreadSlot(state, task.thread, key),
      })
      return
    }

    case "writeThreadLocal": {
      const task = requireTask(state, operation.task)
      const key = requireThreadKey(state, operation.key)
      if (task.thread === null) fail("contextThreadMissing")
      if (operation.maySuspend === true || operation.dependencyEscapes === true) {
        fail("W-TLS-0003")
      }
      const previous = readThreadSlot(state, task.thread, key)
      let next
      if (operation.action === "increment") next = previous + 1
      else if (operation.action === "set") next = operation.value
      else fail("contextThreadActionUnknown")
      state.threadSlots[task.thread][key.id] = next
      state.threadWrites.push({
        task: task.id,
        thread: task.thread,
        key: key.id,
        previous,
        next,
        outcome: operation.throws === true ? "error" : "success",
      })
      return
    }

    case "endThread": {
      requireName(operation.thread, "contextThreadMissing")
      const keys = Object.keys(state.threadSlots[operation.thread] ?? {})
      delete state.threadSlots[operation.thread]
      state.trace.push(`tls-end:${operation.thread}:${keys.length}:drops=0`)
      return
    }

    default:
      fail("contextOperationUnknown")
  }
}

function publicState(state) {
  return {
    taskKeys: Object.values(state.taskKeys).map((key) => ({
      id: key.id,
      identity: key.identity,
      default: key.default,
    })),
    taskReads: state.taskReads,
    activeBindings: Object.values(state.tasks).flatMap((task) =>
      Object.values(task.bindings).flatMap((stack) => stack.map((binding) => ({
        task: task.id,
        key: binding.key,
        bindingId: binding.id,
        value: binding.value,
      })))),
    openScopes: Object.values(state.scopes)
      .filter((scope) => scope.phase === "active")
      .map((scope) => ({ id: scope.id, children: scope.children })),
    threadKeys: Object.values(state.threadKeys).map((key) => ({
      id: key.id,
      identity: key.identity,
      initial: key.initial,
    })),
    threadReads: state.threadReads,
    threadWrites: state.threadWrites,
    threadSlots: state.threadSlots,
    trace: state.trace,
  }
}

export function runContextLocalOperations(operations) {
  const state = {
    taskKeys: {},
    tasks: {
      root: {
        id: "root",
        bindings: {},
        scopeStack: [],
        parentScope: null,
        thread: null,
        phase: "active",
      },
    },
    scopes: {},
    taskReads: [],
    threadKeys: {},
    threadSlots: {},
    threadReads: [],
    threadWrites: [],
    trace: [],
  }

  let error = null
  try {
    for (const operation of operations) applyOperation(state, operation)
  } catch (caught) {
    if (!(caught instanceof ContextLocalModelError)) throw caught
    error = caught.code
  }

  return {
    status: error === null ? "accepted" : "rejected",
    error,
    state: publicState(state),
  }
}
