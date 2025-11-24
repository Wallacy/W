# W Language Cheatsheet

## Variables & Constants
```typescript
const PI = 3.14159       // Compile-time constant
let name = "Wallacy"     // Immutable variable
var count = 0            // Mutable variable
```

## Types
Everything in W is fundamentally an Enum or a Struct.
```typescript
// Basic Types (conceptually enums)
let i: int = 42
let f: float = 3.14
let s: string = "Hello"

// Enums with Associated Values (Swift-like)
enum Status {
    success,
    error(code: int, message: string),
    loading(progress: float)
}

// Objects (Structs with methods/properties)
object User {
    let id: int
    var name: string
}
```

## Special States
```typescript
if value == .undefined { ... }
if value == .empty     { ... }

// Type constants
int.constants.empty
string.constants.unitialized
```

## Numeric Bounds
```typescript
declare type Age = int using(max_value: 120)
var score = someInput() using(max_value: 30000)

guard score {
  // runtime overflow handling
}
```

## Strings
```typescript
print("Hello, ${name}!")
print(`Today is "${date.DayOfWeek}"`)

// Global interpolation
print($`Hello, {name}! Today is {date:HH:mm}`)

// Multiline aligned
print(#"Hello, ${name}!
      Today is ${date.DayOfWeek}
      it's ${date:HH:mm} now.")

// Literal (no interpolation)
print(@"Hello, ${name}!")
```

## Functions
```typescript
// Basic function
fn add(a: int, b: int): int {
    return a + b
}

// Implicit argument destructuring
fn createUser({ name: string, age: number }) {
    // 'name' and 'age' are available directly
}

// Multiple returns / Destructuring
fn getCoords(): {x: int, y: int} {
    return { x: 10, y: 20 }
}

let { x, y } = getCoords()
```

## Control Flow
```typescript
// Pattern Matching
if value ~= 'pattern' { ... }

// If Let (Unwrapping)
if let user = findUser() {
    // user is valid here
}

// The Ultimate Switch
switch value {
    case 1: print("One")
    case 2..10: print("Range")
    case .success(let data): print(data)
    case is String: print("It's a string")
    case {x: 0, y: _}: print("On X axis")
    case ~= /^[a-z]+$/: print("Regex match")
    default: break
}
```

## Modules & Imports
Modules are singletons with their own memory arena.
```typescript
import { http } from std        // Import specific symbol
include 'network' as net        // Include module with namespace
import * as utils from 'utils'  // Import all as namespace

// URL Imports
import math from 'https://libs.w.org/supermath@1.2.3/math.w'
define 'supermath' import 'https://libs.w.org/supermath@1.2.3/math.w'

// Module Lifecycle
module.memory = 64M             // Set max memory for this module
process.flush(module)           // Clear memory
```

## Concurrency
Structured concurrency is built-in.
```typescript
// Async function
fn fetchData() async: Data { ... }

// Async Let (Start task in background)
async let data = fetchData()

// Await result
process(await data)

// Force Synchronous (if needed)
let result = sync fetchData()

// CPU Affinity & Config
async<.cpu(1)>    let heavyTask = compute()
async<.cpus(0, 2)> let multiCoreTask = process()
async<.max(8)>    let job = bigJob()
```

## Services
```typescript
service Logger {
  fn log(msg: string) { ... }
}

let resp = await .service.call(.log("hi"))
```

## CLI Commands
```bash
w run main.w          # Run a script
w build main.w        # Compile to executable
w test                # Run tests
w install             # Install dependencies from package.w
```