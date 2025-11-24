# W Language Technical Specification (Full Draft)

> **Status**: Living Specification (Includes Experimental Ideas)
> **Target**: Systems Programming, High-Performance Web Services, Embedded Systems.
> **Backend**: C11/C23 (Primary), LLVM (Future).

---

## 1. Core Philosophy & Architecture

### 1.1 The "Simple & Powerful C"
W is designed to be a superset of capabilities over C, providing modern ergonomics without sacrificing the raw performance or memory layout control of C.
*   **Zero Global GC**: Memory is managed via deterministic Module Arenas.
*   **Unified Types**: All data structures are variants of Enums (Tagged Unions).
*   **Structured Concurrency**: Async/Await is built-in, not a library.

### 1.2 Compilation Pipeline
1.  **Source**: `.w` files (UTF-8).
2.  **Parser**: Generates AST. Handles "Bash-style" syntax for DSLs.
3.  **Analysis**: Type checking, Escape Analysis (Stack vs Arena), Hash Generation (XXH3).
4.  **Transpilation**: Emits optimized C code.
    *   W Modules -> C Structs (Singletons).
    *   W Functions -> C Functions (with context pointers).
    *   W Async -> State Machines / Protothreads.

---

## 2. Syntax & Variables (Experimental)

### 2.1 Variable Declaration
*   `const`: Compile-time constant (C const).
*   `let`: Immutable variable (Swift-like).
*   `var`: Mutable variable.

### 2.2 Function Syntax & Destructuring
W explores aggressive syntactic sugar for argument handling.

**Implicit Argument Destructuring:**
```typescript
fn({ a: string, b: number }) {
    // 'a' and 'b' are available directly in scope
    return 0
}
```

**Variable Destructuring Syntax:**
```typescript
fn(args: SomeType) {
    let { a, b } = args
    return false
}

// Experimental "Dollar" Syntax for Destructuring
fn(SomeType) {
    let ${ a, b } // Infers from argument type
    return `${x}-${a}`
}
```

**Return Type Destructuring:**
```typescript
type RType = { c: number, d: string }
fn fName(FType): RType {
    let ${ a, b }
    return { c: 0, d: "ok" }
}
```

### 2.3 Default Values & Optionals
```typescript
type FType2 = { a: string?, b: int }
fn(FType2) {
    let ${ a ?? 'default', b == 0 ? 2 : b }
    return `${a}-${b}`
}
```

---

## 3. Type System: "Everything is an Enum"

### 3.1 The Unified Model
In W, the distinction between `struct`, `enum`, and `object` is syntactic sugar over a single internal representation: the **Tagged Union**.

*   **Enum**: A type with multiple variants.
*   **Struct**: An enum with a single variant (implicit).
*   **Object**: A struct/enum with associated methods.

**Internal Layout (C Representation):**
```c
struct W_Object {
    uint64_t type_tag; // Contains Type ID + State Flags (Uninitialized/Empty/etc)
    union {
        int64_t as_int;
        double as_float;
        struct { ... } as_complex_variant;
        void* as_pointer;
    } data;
};
```

### 3.2 Numeric Types as Enums
Numbers themselves can be thought of as enums wrapping underlying C types.
```typescript
enum number {
    float(var f: float),
    int(var i: int),
    bigInt(var b: long[])
}
```
*   **Adaptive Integers**: `int` maps to `int_fast32_t` or `int_fast64_t`.
*   **Bounds Checking**: `declare type Port = int using(max_value: 65535)`.

### 3.3 Hashing & Identifiers (XXH3)
*   **Concept**: Use XXH3 (64-bit or 128-bit) to hash all property names and identifiers at compile time.
*   **Benefit**: No string comparisons at runtime. O(1) dispatch.
*   **Bijective**: XXH3 is bijective for small lengths, guaranteeing no collisions for standard identifiers.
*   **Implementation**: The compiler replaces `obj.method` with `obj[0x1234ABCD]`.

---

## 4. Memory Management: Module Arenas

### 4.1 The Arena Model
*   **Isolation**: Each Module instance has its own linear memory allocator (Arena/Region).
*   **Allocation**: `new` allocates from the current module's arena.
*   **Deallocation**:
    *   **Stack**: Automatic (scope-based).
    *   **Heap**: **Bulk deallocation**. You do not free individual objects. You `flush` the module.
    *   `process.flush(module)`: Resets the arena pointer to zero. Instant cleanup.

### 4.2 Memory Properties
```typescript
module.memory.max = 128M      // Hard limit
module.memory.base = 64M      // Initial allocation
module.memory.current         // Current usage
module.memory.flush()         // Manual flush
```
*   `process.memory` is the sum of all `module.memory`.

### 4.3 Stack & Heap Experiments
*   **HeapStack**: A parallel stack structure for extending the normal stack without modifying the OS stack.
*   **Split-Stack**: Investigating split-stack support for massive concurrency.
*   **Linux Stack Manipulation**: Possibility to treat the stack as part of the `module.memory` ringbuffer for flexibility.
*   **Auto Allocation**: `let a = [auto]` - Compiler attempts to guess the best allocation strategy (Stack vs Heap) based on escape analysis.

---

## 6. Concurrency: Structured & Affinity

### 6.1 Async/Await Implementation
*   **No OS Threads per Task**: Uses **Protothreads** (Duff's Device style state machines) or **Stackless Coroutines**.
*   **Context**: Async functions receive a context pointer containing their state machine variables (spilled registers).
*   **`async let`**: Spawns a coroutine.
*   **`sync`**: Runs the coroutine loop inline until completion (blocking).

### 6.2 Computer Units (CU) & Affinity
*   **Spec**: 1 CU = 128MB RAM + 1 vCPU + 1 KV Storage.
*   **Affinity**:
    ```typescript
    async<.cpu(1)> let audioTask = processAudio()
    ```
    Pins the coroutine to a specific OS thread/core.
*   `process.cpu`: Access to CPU topology.

---

## 5. Module System & Packages

### 5.1 Hierarchy
*   `namespace` > `module` > `file`.
*   Modules are Singletons. Importing `math` gives you the reference to the allocated `math` struct.
*   **Global Access**: Modules can be referenced by their canonical name globally if configured.

### 5.2 Import Syntax
```typescript
import { http } from std        // Standard Lib
include 'network' as net        // Include namespace
import * as utils from 'utils'  // Namespace import
import math from 'https://...'  // Deno-style URL import
```

### 5.3 Package Levels (GStreamer-like)
1.  **std**: Level 1 - Core language support (Guaranteed).
2.  **library**: Level 2 - Shared support (Language + Community).
3.  **modules**: Level 3 - Community supported.
4.  **packages**: Level 4 - Experimental/No guarantee.

### 5.4 Package Manager (`package.w`)
Uses "Bash-style" syntax for declarative configuration.
```typescript
package
 .name "MyServer"
 .version "1.0.0"

deps
 .add "http" "std"
 .add "utils" "github.com/user/utils" .hash "abc1234"
```

---

## 7. Advanced Control Flow

### 7.1 The "Ultimate" Switch
Designed to be the most flexible pattern matcher, inspired by Swift but more powerful.
```typescript
switch (value) {
    case 1: ...                 // Literal
    case 1..10: ...             // Range
    case .success(let data): ... // Enum extraction
    case is String: ...         // Type check
    case ~= /regex/: ...        // Regex match
    case {a: 1, b: _}: ...      // Struct pattern
    default: ...
}
```
*   **Multi-variable Switch**: `switch (var1, var2) { case (1, 2): ... }`
*   **Handlers**: Passing functions to validate cases.
*   **Interleave**: Mixing `do-while` with `switch` (Duff's Device style) for state machines.

### 7.2 Snapshots
Functions marked `snapshot` return deterministic/cached data. Used for:
1.  **Testing**: Mocking complex backends.
2.  **PGO**: Providing profile data for optimization.
3.  **Dev**: Speeding up iteration.
```typescript
snapshot fn getUser() { return { name: "Mock" } }
```

---

## 8. Services & IPC
*   **`service` Keyword**: Defines a contract for remote execution.
*   **Broker**: The runtime includes a message broker.
*   **Transport**: Can be in-memory (same process), pipe (same machine), or TCP (network).
```typescript
service Auth {
    fn login(u: string, p: string): bool
}
// Usage
await .service.call(.login("user", "pass"))
```

---

## 9. C Interoperability
*   **`c_module`**: A block allowing raw C code.
*   **ABI Compatibility**: W structs are standard C structs.
*   **Header Gen**: The compiler emits `.h` files for all exported W symbols.
*   **Pointers**: `int* a` syntax supported for low-level C interop.

---

## 10. Implementation Roadmap & Gaps
To produce the W language, the following components are required:

1.  **Grammar Definition (EBNF)**: *Missing*. Needs a formal grammar file for the parser (Lemon or Bison).
2.  **Runtime Library (`libw`)**: *Missing*. Needs the C implementation of:
    *   The Arena Allocator.
    *   The Coroutine Scheduler.
    *   The Hash Map (XXH3).
3.  **Compiler Bootstrap**:
    *   Phase 1: Write Transpiler in TypeScript or Python (easier to prototype).
    *   Phase 2: Rewrite in W (Self-hosting).
4.  **Standard Library**:
    *   `std/io`: File/Console.
    *   `std/net`: HTTP/TCP.
    *   `std/mem`: Low-level memory access.