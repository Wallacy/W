# W Language Quick Reference

**W** is a modern, safe, and expressive programming language designed as a superset of C. It combines C's performance with modern features inspired by TypeScript, Swift, Zig, and Rust, focusing on memory safety, concurrency, and interoperability with C. W generates pure C (likely C2y) as its output.

**Version**: Pre-release (in development)  
**Website**: [W Language GitHub (TBD)](https://github.com/your-username/w-lang)  
**License**: MIT  
**Contribute**: [GitHub Repository (TBD)](https://github.com/your-username/w-lang)

---

## Basics

### Hello World

```w
func main() {
  print("Hello, World!")
}
```

- **Compile**: `w build main.w` (generates C, then compiles with GCC/Clang).
- **Run**: `./main` (after build).

### Comments

```w
// Single-line comment

/*
Multi-line
comment
*/
```

---

## Variables

### Declare Variables

```w
let x: Int = 42             // Explicit type, immutable
var y: String = "Hello"     // Mutable variable
let z = 100                 // Type inferred, immutable
```

- `let`: Immutable (read-only).
- `var`: Mutable.
- Type inference available; use explicit types for clarity or restrictions.

### Constants (Compile-Time)

```w
const PI: Float = 3.14159   // Compile-time constant
```

- `const` for values known at compile time.

---

## Data Types

### Primitive Types

```w
let num: Int = 42          // Integer
let dec: Float = 3.14      // Floating-point
let text: String = "Hi"    // String
let flag: Bool = true      // Boolean
let char: Char = 'a'       // Character
```

### Custom Types

```w
type Name = String<maxLength: 20>  // Restricted type
let name: Name = "John"           // Only strings up to 20 chars
```

- Use `type` for aliases or restrictions (e.g., length, pattern).

### Arrays

```w
let numbers: Int[] = [1, 2, 3]
let mixed: [Int, String] = [42, "Hi"]  // Heterogeneous array
```

- Arrays are immutable by default; use `var` for mutability.

### Structs

```w
struct Point {
  x: Float
  y: Float
}

let p = Point { x: 10.0, y: 20.0 }
```

- Value types, allocated on stack or embedded.

### Classes

```w
class User {
  let name: String
  init(name: String) {
    this.name = name
  }
}

let user = User { name: "Alice" }  // Reference type, managed by ARC
```

- Reference types, managed by ARC (Automatic Reference Counting).

### Enums

```w
enum Color {
  case Red
  case Green
  case Blue(String)
}

let color = Color.Red
```

- Can carry associated values.

---

## Control Flow

### If Statement

```w
let x = 10
if (x > 5) {
  print("Greater than 5")
} else if (x == 5) {
  print("Equal to 5")
} else {
  print("Less than 5")
}
```

- Supports ternary-like syntax:

```w
let msg = x > 5 ? "Big" : "Small"
```

### Switch Statement

```w
let value = 3
switch (value) {
  case 1: print("One")
  case 2, 3: print("Two or Three")  // Multiple cases
  default: print("Other")
}
```

- No fall-through by default; use explicit `fallthrough` if needed:

```w
switch (value) {
  case 1: print("One"); fallthrough
  case 2: print("One or Two")
  default: print("Other")
}
```

### For Loop

```w
for i in 0..<5 {  // Range [0, 5)
  print(i)        // Prints 0, 1, 2, 3, 4
}

for item in [1, 2, 3] {
  print(item)     // Iterates over array
}
```

- Step sizes with ranges:

```w
for i in 0..<10, 2 {  // Steps of 2: 0, 2, 4, 6, 8
  print(i)
}
```

### While Loop

```w
var count = 0
while (count < 5) {
  print(count)
  count += 1
}
```

### Do-While Loop

```w
var count = 0
do {
  print(count)
  count += 1
} while (count < 5)
```

---

## Functions

### Function Declaration

```w
func add(a: Int, b: Int) -> Int {
  return a + b
}

async func fetch(url: String) throws -> String {  // Async with error handling
  let response = await http.get(url)
  return response
}
```

- `->` for return type.
- `async` for asynchronous functions; use `await`.
- `throws` for error-prone functions; handle with `try`.

### Parameters and Modifiers

```w
func process(data: ref String) -> storage String {  // Reference and storage
  data += " processed"
  return data
}
```

- `ref`: Reference with ARC.
- `storage`: Transfers ownership.
- `cow`: Copy-on-write for sharing.

---

## Arithmetic Operations

### Basic Arithmetic

```w
let a = 10
let b = 5

let sum = a + b      // Addition: 15
let diff = a - b     // Subtraction: 5
let prod = a * b     // Multiplication: 50
let quot = a / b     // Division: 2
let mod = a % b      // Modulus: 0
```

- All operators follow C-like precedence and associativity.

### Compound Assignment

```w
var x = 10
x += 5    // x = 15
x -= 3    // x = 12
x *= 2    // x = 24
x /= 4    // x = 6
x %= 2    // x = 0
```

---

## Memory Management

### ARC (Reference Types)

```w
class Data {
  let value: String
}

func use(data: ref Data) {
  print(data.value)  // Retains data
}  // Data released when ref count reaches 0
```

- Use `ref`, `storage`, or `cow` to control ownership.

### Copy-on-Write (Cow)

```w
func modify(data: cow String) -> cow String {
  return data + " modified"  // Copies only if modified
}
```

- Efficient for immutable sharing; copies on mutation.

---

## Concurrency

### Async/Await

```w
async func download(url: String) throws -> String {
  let response = await http.get(url)
  return response.text()
}

func main() {
  try await print(download("https://example.com"))
}
```

- `async` for coroutines; `await` to wait for results.

### Parallelism (Spawn)

```w
func compute() -> Int {
  return 42  // Heavy computation
}

func main() {
  let task = spawn compute()  // Runs in separate thread
  let result = await task
  print(result)
}
```

- `spawn` for background threads; `await` for results.

---

## Modules

### Module Declaration

```w
module Math {
  export func add(a: Int, b: Int) -> Int {
    return a + b
  }
}

module Main {
  import { add } from "Math"

  func main() {
    print(add(3, 4))  // 7
  }
}
```

- Modules are singletons; use `#config` for settings.

### Module Configuration

```w
module Network {
  #config {
    threads = [.background, .network]
    memory = .rcu  // Read-Copy-Update for concurrency
  }
}
```

---

## Interoperability with C

### Calling C Functions

```w
import { printf } from "c:stdio.h"

func sayHello(name: String) {
  printf("Hello, %s!\n", name)
}
```

- Maps W types (e.g., `String` → `char*`) to C; uses ARC/structs for safety.

---

## Error Handling

### Try/Catch (Planned)

```w
func risky() throws -> Int {
  if (someCondition) {
    throw "Error occurred"
  }
  return 42
}

func main() {
  try {
    let result = risky()
    print(result)
  } catch (error) {
    print(`Error: ${error}`)
  }
}
```

- `throws` for functions that can throw; `try` for handling.

---

## Build System

### Build Command

```w
// build.w
module Build {
  func main() {
    let sources = glob("src/*.w")
    let objects = sources.map(f => compile(f, "c"))
    link(objects, "my_program")
  }
}
```

- Run `w build` to compile W code to C, then link with GCC/Clang.
- Uses [libuv](https://github.com/libuv/libuv) for async operations.

---

## Tips & Tricks

- **Type Inference**: Use `let` without types when possible; W infers them.
- **Safety**: Use restricted types (e.g., `String<maxLength: 20>`) to catch errors early.
- **Performance**: Avoid unnecessary `cow` copies; prefer `ref` for ARC.

---

## References

- **[pthread](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html)**: For threading in C backend.
- **[Clang/LLVM](https://clang.llvm.org/)**: Compiler backend.
- **[Tree-sitter](https://tree-sitter.github.io/)**: For parsing (AST grammar TBD).
- **[mimalloc](https://github.com/microsoft/mimalloc)**: Potential allocator for ARC.

---

## Contribute

- **Repository**: [W Language GitHub (TBD)](https://github.com/your-username/w-lang)
- **License**: MIT (see [LICENSE](https://github.com/your-username/w-lang/blob/main/LICENSE))
- **Issues/Features**: Open a pull request or issue on GitHub.
