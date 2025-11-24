# The W Programming Language

**W** is a modern systems programming language designed for performance, predictability, and ergonomics. It combines the low-level control of C with the high-level expressiveness of Swift and TypeScript.

> **Slogan**: "A simple and powerful C" — For C what TypeScript is for JavaScript.

## Why W?

### 1. Predictable Memory (No GC)
In W, memory is managed per-module. Each module acts as an isolated memory arena. You define the limits (`module.memory = 64M`), and the language ensures you stay within them. There is no global Garbage Collector pausing your world. Instead, you "flush" modules when you're done with a unit of work (like a request), instantly reclaiming memory.

### 2. Everything is an Enum
W unifies types around the concept of Enums (Tagged Unions). Whether it's a primitive `int`, a complex `struct`, or a `class`, they are all treated as variants of data. This allows for powerful pattern matching and a consistent type system.

### 3. Structured Concurrency
Concurrency is not an afterthought. With `async let`, you spawn tasks that are structurally bound to their scope. The compiler ensures you handle their lifecycle, making race conditions and deadlocks significantly harder to write.

### 4. C-Interop First
W compiles to C. This means you can drop W into any existing C/C++ project, use any C library without overhead, and run on any platform that has a C compiler.

## Key Features

*   **Module-based Memory**: No GC, no manual `malloc/free` hell. Just arenas.
*   **Pattern Matching**: Expressive `switch` and `if let` constructs.
*   **Explicit States**: Types handle `uninitialized`, `undefined`, `null`, and `empty` states explicitly.
*   **Hot-Code Optimization**: Designed to keep hot paths small and cache-friendly.
*   **Services & IPC**: First-class support for services and inter-process communication.
*   **Computer Units**: Runtime model based on isolated units of computation (Memory/CPU/Storage).

## Getting Started

### Hello World
```typescript
import { io } from std

fn main() {
    io.print("Hello, World!")
}
```

### A Simple Module
```typescript
// user.w
export object User {
    let id: int
    var name: string
}

export fn create(name: string): User {
    return User(id: 1, name: name)
}
```

### Using Memory Limits
```typescript
// server.w
module.memory = 128M // Hard limit for this request handler

import { http } from std

fn handleRequest(req: http.Request) {
    // All allocations here happen in the module's arena
    let data = processData(req)
    // ...
}
// When the module is flushed, all 'data' is freed instantly.
```

## Documentation
*   [**Cheatsheet**](cheatsheet.md): Quick syntax reference.
*   [**Technical Specification**](techspec.md): Deep dive into architecture, memory model, and implementation details.

## Roadmap
1.  **Prototype**: Current phase. Defining syntax and core semantics.
2.  **Bootstrap**: Building the self-hosting compiler.
3.  **Standard Library**: Building `std` with core IO and networking.
4.  **Ecosystem**: Package manager and tooling.
