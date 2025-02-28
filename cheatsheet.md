# W Language Cheatsheet

## Basic Syntax

**Comments:**

```typescript
// Single-line comment

/*
 * Multi-line
 * comment
 */
```

**Variable Declaration:**

```typescript
let immutableName = "W Lang" // Immutable variable (constant)
const constantName = "W Lang" // Constant variable, block scope
var mutableName = "W Lang"   // Mutable variable, block scope
global const PI = 3.14159         // Global constant (module level)
```
`const` declares block-scoped constants, while `global const` declares module-scoped constants. `let` declares block-scoped immutable variables, and `var` declares block-scoped mutable variables.

**Data Types:**

```typescript
Int         // Integer (architecture-dependent size, default i32/i64)
Int<size>   // Integer with specific size (e.g., Int<16>, Int<32>, Int<64>)
Int<bitSize: size> // Integer with specific size in bits (e.g., Int<bitSize: 16>)
Float       // Floating-point (default double)
Float<size> // Floating-point with specific size (e.g., Float<32>, Float<64>)
String      // UTF-8 String
Char        // Unicode Character
Bool        // Boolean (true or false)
Void        // Empty type (no return)
```
`Int`, `Float`, `String`, `Char`, `Bool`, and `Void` are primitive types. `Int` and `Float` can have specified sizes.

**Optional Types:**

```typescript
let user: String? = nil  // String variable that can be null
let age: Int?          // Optional Int variable, initial value nil
```
Optional types are declared with `?` and can contain `nil`.

**Restricted Types:**

```typescript
type Username = String<maxLength: 20, pattern: /^[a-zA-Z0-9_]+$/>
type ValidAge = Int<range: 0...120>
type CPF = String<maxLength: 12; mask:CPF, inputType:Number>; // String type with CPF mask
type CPFType = String<maxLength: 12; mask:CPF, inputType:Number>; // Type alias for CPF
type Email = String<pattern: /^[\w-\.]+@([\w-]+\.)+[\w-]{2,4}$/>
type Password = String<minLength: 8>
type HexColor = String<pattern: /^#([0-9A-F]{3}){1,2}$/i> // Hexadecimal Colors
type BRPhone = String<mask: '(99) 99999-9999', inputType: Number> // BR Phone
type Percentage = Float<range: 0.0...1.0> // Percentages between 0 and 1
type UUID = String<pattern: /^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$/> // UUIDs
type ISOData = String<pattern: /^\d{4}-\d{2}-\d{2}$/> // Dates in ISO 8601 format (YYYY-MM-DD)
type Hour24h = String<pattern: /^(?:[01]\d|2[0-3]):[0-5]\d$/ > // Hours in 24h format (HH:MM)
type MonetaryValue = Float<range: 0...> // Non-negative monetary values
type BRPostalCode = String<mask: '99999-999', inputType: Number> // Brazilian postal code
type BRVehiclePlate = String<maxLength: 8, pattern: /^[A-Z]{3}\d[A-Z0-9]\d{2}$/> // Vehicle plates in BR format

```
Types can be restricted with `maxLength`, `pattern`, `range`, `minLength`, `mask`, and `inputType`. `type` keyword creates type aliases.

**Operators:**

*   **Arithmetic:** `+`, `-`, `*`, `/`, `%`
    ```typescript
    10 + 5  // 15
    20 - 3  // 17
    7 * 6   // 42
    50 / 5  // 10
    10 % 3  // 1
    ```
*   **Comparison:** `==`, `!=`, `>`, `<`, `>=`, `<=`
    ```typescript
    10 == 10 // true
    5 != 3   // true
    8 > 2    // true
    1 < 0    // false
    5 >= 5   // true
    2 <= 1   // false
    ```
*   **Logical:** `&&`, `||`, `!`
    ```typescript
    true && false // false
    true || false // true
    !true        // false
    ```
*   **Assignment:** `=`, `+=`, `-=`, `*=`, `/=`, `%=`
    ```typescript
    var x = 10
    x += 5 // x is now 15
    x -= 3 // x is now 12
    x *= 2 // x is now 24
    x /= 4 // x is now 6
    x %= 5 // x is now 1
    ```
*   **Null Coalescing:** `??` (e.g., `userName ?? "Anonymous"`)
    ```typescript
    let name: String? = nil
    let displayName = name ?? "Visitor" // displayName is "Visitor"
    ```
*   **Optional Chaining:** `?.` (e.g., `user?.name`)
    ```typescript
    class User {
        let name: String?
    }
    let user: User? = nil
    let userName = user?.name // userName is nil
    ```
*   **Range Operators:** `..` (inclusive), `..<` (upper exclusive), `>..` (lower exclusive), `>..<` (both exclusive)
    ```typescript
    for (i in [1..3]) { print(i) }      // 1 2 3
    for (i in [1..<3]) { print(i) }     // 1 2
    for (i in [2>..5]) { print(i) }     // 3 4 5
    for (i in [2>..<5]) { print(i) }    // 3 4
    for (i in [1..7, 2]) { print(i) }   // 1 3 5 7
    ```
*   **Optional Operators:** `?+`, `?-`, `?*`, `?/`, `?%`, `?>`, `?<`, `?>=`, `?<=`, `?+=`, `?-=` (conditional operators for optionals)
    ```typescript
    let a: Int? = 10
    let b: Int? = nil
    print(a ?+ 5) // Optional(15)
    print(b ?+ 5) // nil
    ```

**Strings and String Interpolation:**

Strings can be declared with `"` , `'` or `` ` ``.

```typescript
print("Hello, ${name}! Today is \"${date.DayOfWeek}\", it's ${date:HH:mm} now.")
print('Hello, ${name}! Today is "${date.DayOfWeek}", it\'s ${date:HH:mm} now.')
print(`Hello, ${name}! Today is "${date.DayOfWeek}", it's ${date:HH:mm} now.`)

print($`Hello, {name}! Today is {date.DayOfWeek}, it's {date:HH:mm} now.`) // Positional interpolation

print(@"Hello, ${name}! Today is ${date.DayOfWeek}, it's ${date:HH:mm} now."); // Literal string, no interpolation

print(#"Hello, ${name}!
      Today is ${date.DayOfWeek}
      it's ${date:HH:mm} now.") // Multiline string with identation

print(#$`Hello, {name}!
      Today is {date.DayOfWeek}
      it's {date:HH:mm} now.`) // Multiline and positional interpolation

print("Hello, Joe! "
      "Today is friday, it's 12/05/198814h25 "
      "now its a good time") // Implicit string concatenation

```
Strings support interpolation with `${}`.  `$`` enables positional interpolation. `@` disables interpolation for literal strings. `#"` enables multiline strings with indentation based on `#` position. `#$`` enables multiline and positional interpolation. Strings can be implicitly concatenated by placing them side-by-side.

**Whitespace Handling:**

Whitespace is generally ignored outside of strings.

## Control Structures

**Conditional `if`:**

```typescript
let x = 10
if (x > 5) {
  print("Greater than 5")
} else if (x == 5) {
  print("Equal to 5")
} else {
  print("Less than 5")
}
```

**Conditional `guard`:**

```typescript
func processValue(value: Int?) -> String? {
  guard let val = value else {
    return nil // Early return if value is nil
  }
  // Continue processing if value is not nil
  return "Processed value: ${val}"
}
```

```typescript
func example() {
  guard cond return "reason1" // Early return with value
  guard cond2 return "reason2"
  guard cond3 return "reason3"

  ...thing...
  return ...
}
```
`guard` statements ensure conditions are met before proceeding, providing early exits. `guard <bool_expression> return <...>` provides a shorthand for early return with a value.

**Conditional `if let`:**

```typescript
const provider = getProvider() // Provider | undefined
if let prov = provider {
  prov.doIt() // prov is now type Provider and not undefined
}
```
`if let` provides a way to safely unwrap optionals and check for non-null values.

**Conditional `if ~= condition`:**

```typescript
var bb = [...]

if bb ~= 'someElementMatch' { // Checks if 'someElementMatch' is present in bb
  // ...
}

if bb ~= someFunc { // Checks if someFunc returns true for any element in bb
 // ...
}

if bb ~= (b) => { // Checks if the lambda returns true for any element in bb
 // ...
}
```
`if ~= condition` provides pattern matching capabilities for collections, checking for element presence or conditions met by elements.

**Switch Statement:**

```typescript
let value = 3
switch (value) {
  case 1: print("One")
  case 2, 3: print("Two or Three")  // Multiple cases
  default: print("Other")
}
```

**Loop `for`:**

```typescript
for (var i = 0; i < 5; i++) {
  print(i)        // Prints 0, 1, 2, 3, 4
}

for (item in collection) { // for-in for collections (arrays, etc - to be defined)
  print(item)     // Iterates over array
}

for (value in [1..5]) {  // Inclusive range: 1, 2, 3, 4, 5
  print(value)
}

for (value in [1..<5]) { // Upper exclusive range: 1, 2, 3, 4
  print(value)
}

for (value in [1>..5]) { // Lower exclusive range: 2, 3, 4, 5
  print(value)
}

for (value in [1>..<5, 2]) { // Range with step: 3
  print(valor)
}
```

**Loop `while`:**

```typescript
var count = 0
while (count < 5) {
  print(count)
  count += 1
}
```

**Loop `do-while`:**

```typescript
var count = 0
do {
  print(count)
  count += 1
} while (count < 5)
```

## Functions, Closures, and Lambdas

In W, functions and closures share the same syntactic basis. Functions are essentially named closures. Lambdas are anonymous *inline* functions.

**Named Function Declaration:**

```typescript
func functionName(parameter1: Type1, parameter2: Type2): ReturnType {
  // Function body
  return returnValue
}
// Functions without return type (implicitly return Void)
func functionWithoutReturn(parameter: Type) {
  // Function body
}

//Functions with parameter names
func greet(name person: String, age: Int) {
    print("Hello, ${person}! You are ${age} years old.")
}
greet(name: "Ana", age: 30)

//Functions with variadic arguments
func sum(numbers: Int...) -> Int {
    var total = 0
    for (number in numbers) {
        total += number
    }
    return total
}

print(sum(1, 2, 3, 4, 5)) // Prints 15

// Functions with code in other languages
func<C> cFunction(parameter: Int): Int {
    // Inline C code
    return parameter * 2;
}

func<asm ("MYFUNC")> asmFunction() Int // External assembly function declaration

```
Functions are declared using `func` or `fn` keyword.  Functions can have named parameters and variadic arguments. Functions can also contain inline code in other languages like C, using `func<C>`. External assembly functions can be declared using `func<asm ("MYFUNC")>`.

**Anonymous Functions (Closures):**

```typescript
let myAnonymousFunction = (parameter: Type): ReturnType {
  // Anonymous function body
  return returnValue
}

// Shorthand for anonymous functions with a single-line body (Lambdas)
let shortFunction = (parameter: Type) => returnValue // When not using `{ }` pair, `=>` must be used

//Examples
let double = (x: Int) => x * 2
print(double(5)) // Prints 10

let greeting = (name: String) => {
    return "Hello, ${name}!"
}
print(greeting("Carlos")) // Prints "Hello, Carlos!"
```

**Functions as Closures with Explicit Capture:**

```typescript
func counter(init: Int) -> (Int) -> Int { // Returns a function (closure)
  var count = init // Local variable to the counter function
  return (increment: Int) { |let c = count| // Captures 'count' by copy
    return (c + increment)
  }
}

let myCounter = counter(init: 10)
print(myCounter(5)) // 15
print(myCounter(3)) // 18

// Capture by weak reference
func observer(value: Int) -> () -> Int? {
    var observedValue: Int? = value // Using optional to simulate weak reference
    return () { |weak observedValue|
        return observedValue
    }
}

var initialValue = 10
let myObserver = observer(valorInicial)
print(myObserver()) // Prints Optional(10)
initialValue = 20 // Modifying initialValue does not affect observedValue
print(myObserver()) // Prints Optional(10)

func counter(init: Int) {
  let count = init
  return (add: Int) => { |let c = count, self| // Captures 'count' by copy and 'self' by strong reference
    return (c + add)
  }
}

func counter(init: Int) {
  let count = init
  return (add: Int) => { |weak object| // Captures 'object' by weak reference
    return (count ?+ add)
  }
}

func counter(init: Int) {
  let count = init
  return somador(add: Int) { |count| // Named closure, 'somador' name is lost in return type
    return (count + add)
  }
}
```
Closures can capture variables explicitly using `|capture_list|`. Capture modes include copy (`let`), strong reference (`self`), and weak reference (`weak`). Captures can be named using `|let captureName = variable, ...|`.

**Asynchronous Functions (`async`/`await`) with Modifiers:**

```typescript
async func asynchronousFunction(): String {
  // Asynchronous code
  let result = await someAsynchronousOperation()
  return result
}

func callAsyncFunction() async {
  let result = await asynchronousFunction()
  print(result)
}
```

**Parallel Functions (`spawn`/`await`) with Modifiers:**

```typescript
async func heavyTask(): String { // Spawn can only be done in async type functions.
  // Computationally intensive operation
  return "Heavy task completed"
}

func executeInParallel() async {
  let task = spawn heavyTask()
  let result = await task
  print(result)
}

func executeInParallelInBackground() async {
  let task = spawn<.background> heavyTask()
  let result = await task
  print(result)
}

async<.max(8)> func makeDinner() throws: Meal { // Max threads for async function
  let veggies = try chopVegetables()
  let meat = marinateMeat()
  let oven = try preheatOven(temperature: 350)

  let dish = Dish(ingredients: [veggies, meat])
  return try oven.cook(dish, duration: .hours(3))
}
```
`async` and `await` keywords are used for asynchronous operations. `spawn` creates parallel tasks. Modifiers like `<.background>` and `<.max(threads)>` can be used to configure execution context.

**Parameter Modifiers:**

```typescript
func processData( String<.ref>, file: String<.storage>, cache: String<.cow>) {
  // ref: Mutable reference (in-out)
  // storage: Ownership transfer
  // cow: Copy-on-write
}
```
Parameter modifiers control ownership and mutability of arguments passed to functions.

**CallbackType:**

```typescript
func greet(callback fptr: (name: String) -> Void) {
    fptr("World");
}

func sayHello(name: String) {
    print("Hello, ${name}!\n")
}

func main() {
    greet(fptr: sayHello)
}

func some(a: Int, callback b: (value: Int) -> Void){ // Callback with type and argument name
  ...
  b(2);
}

func some(a: Int, callback x: add){ // Callback with function signature
  ...
  x(2);
}

type CallbackTypeAlias = (value: Int) -> Void // Callback type alias

func some(a: Int, callback x: CallbackTypeAlias){ // Callback with type alias
  ...
  x(2);
}
```
`callback` modifier defines function pointer types, enabling interoperability with C-style callbacks and event systems. `CallbackType` is a special type for function pointers.

**Functions with Side Effects:**

```typescript
let count = 0

mut action(){ // 'mut' keyword marks function with side effects
  count++
}

return mut () => { // Anonymous function with side effects
  count++
}
```
`mut` keyword marks functions that have side effects, modifying variables outside their local scope.

**Function Configuration:**

```typescript
declare configX = <W, .gpu, .heap> // Declare a function configuration type

fn<configX> call() {} // Apply configuration type to a function
```
Function configurations can be declared using `declare configX = <...>` and applied to functions using `fn<configX>`.

**Asm Functions:**

```typescript
fn<asm ("MYFUNC")> asmFunction() Int // External assembly function declaration
```
External assembly functions can be declared using `fn<asm ("MYFUNC")>`.

## Modules

**Module Declaration:**

```typescript
module ModuleName { // Module declaration
  // Functions, constants, types, etc.
  export func moduleFunction() { ... }
  export const MODULE_CONSTANT = 123

  export { // Exporting multiple items in a block
    moduleFunction,
    MODULE_CONSTANT,
  }

  export default { // Export default for default module value
    version: "1.0.0"
  }
}
```
Modules are declared using the `module` keyword and act as singletons. Module names are case-sensitive and lowercase ASCII.

**Module Import:**

```typescript
import { moduleFunction, MODULE_CONSTANT } from "ModuleName" // Selective import
import ModuleNamespace from "ModuleName" // Imports module as namespace
import * as ModuleAlias from "ModuleName" // Imports with alias
import { a,b,c } as lili from 'lulu' // Import with alias and selection
include 'bababa' // Include module content in current module namespace
include 'bababa' as nanana // Include module with namespace renaming
include ninini from 'hahaha' // Include specific export from module
import * from 'bababa' // Include all exports, same as include 'bababa'
import lilili from lololo // Module rename, module names are global variables
import math from 'https://libs.w.org/supermath@1.2.3/math.w' // Import from URL with version
import math from 'supermath' // Import using defined name
import { thing } from someModule(args) // Import with module function call
import { thing } from someModule --arg like -cli=true // Import with module CLI arguments
import 'supermath' from 'https://libs.w.org/supermath@1.2.4/math' // Import with extension type preference (.a, .dyn, .w)
import a from "a" // Import module 'a' as 'a' variable
import type a from "a" // Import only type definitions from module 'a'
import type { someClass } from "a" // Import specific type definitions from module 'a'
import fork { func as func_alt } from 'X' // Import forked module with alias
import { X , Y , Z } from 'alphabet' with { config, ....} // Import with configuration override
```
Modules are imported using `import` and `include` keywords. `import` makes the module available as a variable, while `include` merges the module's content into the current namespace. Variations include selective imports, aliases, URL imports, and conditional imports. `fork import` creates a new instance of a module. `import ... with { config, ... }` allows overriding module configurations during import.

**Module Configurations:**

```typescript
module MyModule {
  #config { // Inline configurations
    threads = [.background, .network] // Dedicated threads
    memory = .rcu                   // RCU memory management
    dynamic.maxSize = 1G        // Dynamic memory limit
  }

  #config threads = [.background, .network] // Shorthand for single config
  #config memory = .rcu
  #config dynamic.maxSize = 1G
}

module Network {
    #config {
        dynamic.maxSize = 1G        // Dynamic memory limit
        threads = [.background, .network]
        callPolice = .rcu           // Read-Copy-Update for concurrency
        #threads: .module // Module dedicated threads
        #dynamicThreads: 100 // Dynamic threads limit for module
    }
}
```
Module configurations are defined using `#config` blocks, controlling threads, memory management, and other resources. Configurations can be inline or shorthand.

**Module Entry Points:**

```typescript
module MainModule {
  export func main(args: String[]) { // Main entry point for executables
    print("Executing main module with arguments: ${args}")
  }

  export func fetch(request: Request, context: Context) -> Response { // Entry point similar to Cloudflare Workers
    return new Response("W module response!")
  }

  export default func() { // Default export as alternative entry point
    print("Default module entry point")
  }

  export entry { // Entry block for multiple entry points
    main(){
      // ...
    }
    cli(){
      // ...
    }
    ipc(req, conn){
      // ...
    }
    ...
  }
}
```
Modules can define multiple entry points, including `main` for executables, `fetch` for service workers, and `default` for general module entry. `export entry { ... }` block allows defining multiple named entry points.

**Module Lifecycle:**

```typescript
module SomeModule {
  module.init = initFunctionName // Custom module initialization function name
  module.deinit = deinitFunctionName // Custom module deinitialization function name

  export func init() { // Default module initialization function
    // ...
  }

  export func deinit() { // Default module deinitialization function
    // ...
  }

  export func release(moduleName: String) // Unload a module by name
  export func release(moduleVar: ModuleType) // Unload a module by variable
}
```
Modules have a lifecycle with `init` and `deinit` functions. `module.init` and `module.deinit` can be used to customize the names of these functions. `release()` function unloads a module.

**Module Naming:**

Module names should be ASCII, lowercase, and case-sensitive. Real name of variables and functions in WC is `moduleName_variableOrFunc`.

**Module Compilation:**

Modules are compiled as static libraries. The main module is linked with a template main function that calls the module's entry point and constructor.

**Module Export:**

```typescript
module SomeModule {
  export callThing // Export function
  export callThing as thing // Export function with alias
  export { callThing as thing } // Export function with alias in block
  export hide_export { callThing_SF } // Hide export from module interface
}
```
`export` keyword is used to make functions, constants, and types available outside the module. `hide_export` block hides exports from the module's public interface.

## Objects (Classes and Structs)

In W, `class` and `struct` are keywords used to declare complex types. `class` declares reference types, and `struct` declares value types. `object` keyword is used to define protocols or interfaces, and for generic object references that can be either class or struct instances.

**`class` Declaration (Reference Type):**

```typescript
class ClassName : SuperClass, Protocol1, Protocol2 { // Class declaration with inheritance and protocols
  public let immutableProperty: Type // Public immutable property
  private var mutableProperty: Type // Private mutable property

  init(parameter1: Type, parameter2: Type) { // Constructor
    this.immutableProperty = parameter1
    this.mutableProperty = parameter2
  }

    init.complete(param1: Type, param2: Type, param3: Type) { // Named constructor
        this.immutableProperty = param1
        this.mutableProperty = param2
        // ... other initializations
  }

  public func classMethod() { // Public method
    // ...
  }

  mut public func mutableMethod() { // Public mutable method
      this.mutableProperty = newValue //allowed only in mut func
  }

  static constants->{ // Static constants block
    undefined = 'undefined'
    null = 'null'
    unitialized = 'undefined' // e.g
  }

  Property1:: SomeType // Default property for type association
  Property2: SomeType // Regular property
  Property3:2: AnotherType // Property with index 2 for layout purposes
  Property1:-: SomeType // Property with default layout

  _id: .pointer // ID property with pointer type
  _state: .state(name: string, age:number) // State property for persistence

}

// Instantiation
let object = ClassName(parameter1: value1, parameter2: value2)
let completeObject = ClassName.complete(param1: val1, param2: val2, param3: val3)
```
`class` keyword declares reference types. Classes support inheritance, protocols, constructors (`init` and named `init.name`), methods (public and private, mutable with `mut`), static constants, and property configurations. Properties can be declared as `public` or `private`. Property configurations include default properties (`::`), indexed properties (`:index:`), and default layout properties (`:-:`). `_id` and `_state` properties are special properties for object identity and persistence.

**`struct` Declaration (Value Type):**

```typescript
struct StructName {  // Struct declaration
  let immutableProperty: Type
  var mutableProperty: Type

//Constructors are not mandatory in Structs
//If all properties have default values, you can use the struct without an explicit constructor.

  // Methods (structs can also have methods)
  func structMethod() {
    // ...
  }

  // Mutable methods in structs (return a new copy)
  mut func mutableMethod() -> StructName {
      var copy = this //implicit copy
      copy.mutableProperty = newValue
      return copy
  }
}

// Instantiation
let myStruct = StructName(immutableProperty: value1, mutableProperty: value2)
let modifiedStruct = myStruct.mutableMethod() // Returns a new instance
```
`struct` keyword declares value types. Structs are similar to classes but are value types, copied on assignment. Structs can have methods, including mutable methods that return a new copy of the struct. Constructors are optional for structs.

**`enum` Declaration (Enumerated Type):**

```typescript
enum DayOfWeek(var dayNumber: Int) { // Enum declaration with associated value
  MONDAY(1), TUESDAY(2), WEDNESDAY(3), THURSDAY(4),
  FRIDAY(5), SATURDAY(6), SUNDAY(7)
}

let day: DayOfWeek = .MONDAY // Enum case assignment

enum TargetType { // Enum without associated value
    /// A target that contains code for the Swift package’s functionality.
    case regular
    /// A target that contains code for an executable's main module.
    case executable
    /// A target that contains tests for the Swift package’s other targets.
    case test
    /// A target that adapts a library on the system to work with Swift packages.
    case system
    /// A target that references a binary artifact.
    case binary
    /// A target that provides a package plugin.
    case plugin
}

let target: Target = .executable // Enum case assignment

enum number() { // Enum as type with cases as constructors
  float(var f: float),
  int(var f: int),
  long(var f: long),
  ...
  bitInt(var f: long[]))
}

var thing: number = .float(2) // Enum case constructor usage
var thing: number = 2.0 // Shorthand for enum case constructor

enum PackageDependencies { // Enum for package dependencies
  .package(url: String, from: String)
}

let packageDep: PackageDependencies = .package(url: "https://github.com/gringoireDM/EnumKit.git", from: "1.1.0") // Enum case with associated values
```
`enum` keyword declares enumeration types. Enums can have associated values, and cases can be used as constructors. Enums can also be used to define types with enumerable constructors.

**`object` Declaration (Protocol/Interface):**

```typescript
protocol Drawable { // Protocol declaration using 'protocol' keyword
    func draw()
    var color: String { get } // Read-only property
    //var size: Int { get set } //error, protocols cannot have stored properties
}

// Protocol conformance
class Circle : Drawable { //":" indicates inheritance and/or protocol conformance
    let radius: Float
    var color: String = "blue" //must have var here

    init(radius: Float) {
        this.radius = radius
    }

    func draw() {
        print("Drawing a circle of radius ${this.radius} and color ${this.color}")
    }
}

let myCircle: Drawable = Circle(radius: 5.0) // Protocol type usage
myCircle.draw()
print(myCircle.color)
```
`protocol` keyword declares interfaces. `object` keyword can be used as a synonym for `protocol` when defining interfaces. Protocols define contracts that classes and structs can conform to. Protocols cannot have stored properties, only computed properties and methods.

**Object Methods:**

```typescript
object SomeObject {
  func someMethod() {
    // ...
  }
}

let obj = SomeObject()
obj.someMethod() // Method call

// Object Call Syntax Sugar
// In C backend: object_someMethod(obj);

```
Methods are functions associated with classes and structs. Object call syntax `obj.method()` is syntactic sugar for `object_method(obj)`.

## Interoperability with C

**Importing C Libraries:**

```typescript
import { c_function } from "c:c_library.h" // Imports C function
import { c_type } from "c:c_library.h"   // Imports C type
```

**Calling C Functions:**

```typescript
func useCFunction() {
  let cResult: Int = c_function(wArgument)
  // ...
}
```

**C Module:**

```typescript
module CModule {
  #C::MyNamespace { // C code block with namespace
    #include <stdio.h>
    int c_function(int arg) {
      printf("Hello from C! Arg: %d\n", arg);
      return arg * 2;
    }
  }

  export func callCFunction(val: Int) -> Int {
    return #C::MyNamespace.c_function(val) // Call C function with namespace
  }
}
```
`#C::NamespaceName { ... }` blocks allow embedding C code directly within W modules, with optional namespace specification.

## Build System

**`build.w` File:**

```typescript
module Build {
  func main() {
    let sources = glob("src/*.w")
    let objects = sources.map(f => compile(f, "c"))
    link(objects, "w_program")
  }
}
```

**`w build` Commands:**

*   `w build` - Compiles the project.
*   `w run` - Compiles and runs.
*   `w test` - Executes tests (to be defined).
*   `w package` - Packages for distribution.
*   `w release` - Packages for release, using git tags for versioning.

## Testing and Debugging

**Test Files:**

Test files are named with `.test.w` extension (e.g., `file.test.w`). They are treated as libraries imported in debug mode.

**Debug Files:**

Debug files are named with `.debug.w` extension (e.g., `file.debug.w`). They are similar to test files but allow side effects and are used for debugging and demonstrations.

**In-Place Tests:**

```typescript
//@(2,1) == 3 // In-place test comment
fn someFunc(Int a, Int b){
 return a + b
}

fn someFunc(Int a, Int b){
 @(2,1) == 3 // In-place test assertion
 return a + b
}

// Some Docs about that func
// @a: Int, bla bla bla
// @b: Int, bla bla bla
// @@: Int, sum of bla bla bla
// @(2,2) == 4 // In-place test with JSDocs like arguments and result

// Some Docs about that func
// @a: String, bla bla bla
// @@: SomeComplexObject, bla bla bla
// @a = "Very Complex sample"
// @@ = SerializedForm from SomeComplexObject
// @(a) == @return // In-place test with complex arguments and results

@Test("Continents mentioned in videos", arguments: [ // Test function with arguments
  "A Beach",
  "By the Lake",
  "Camping in the Woods"
])
func mentionedContinents(videoName: String) async throws {
  let videoLibrary = try await VideoLibrary()
  let video = try #require(await videoLibrary.video(named: videoName))
  #expect(video.mentionedContinents.count <= 3)
}

@debug("variable that will actually change in debug the first time this file runs") // Debug variable annotation
mut someFunc(string a){
 @debug("I can also call myself whenever the function is executed in debug") // Debug function call annotation
 @debug someOtherFunc() // Debug function call annotation
 #debug ("a") == Result.OK // Debug assertion with Result type
 return new SomeComplexObject()
}

var a = someFunc()
if (a == Result.OK){ // Check Result.OK
  a.value == "blabla" // Access value from Result.OK
}

guard let b = someFunc, where b == Result.err(504){ // Guard with Result.err check
  //....
  // outside guard b is Result<T>.value
}

if let c = someFunc() { // if let with Result.OK check
  //.... c is value T
  // if let only enters if result is okay, so b is always value
}
```
In-place tests can be added as comments or code annotations using `@(...) == ...` syntax. `@Test` annotation defines test functions with arguments. `@debug` annotation marks code for debug-only execution. `Result<T>` type is implicitly used in debug mode to handle function results, with `Result.OK` and `Result.err` cases.

## Comptime

**Comptime Functions:**

```typescript
var someValue = #func() // #func is called at compile time

var someValue = #func("static string") // comptime function with static string argument
var value = someInput() using(max_value:30000) // using clause helps compiler find bounds
var value = someInput() using(max_value:30000, boundChecking: true) // boundChecking forces runtime test
declare type myInt = int using(max_value:10000) // declare type with using clause
declare type myInt = int32 using(boundChecking: true) // declare type with boundChecking
declare type age = int using(max_value:1000) // declare type with using clause

guard someNumber { // guard block for runtime overflow checking
  // overflow in runtime, can be handled here.
}

var obj = SomeObject { literalPro1 = value, literalProp2 = 42, ...} // Wlon literal object initialization

var someArray = [{someLabel, 33}, 44, 55, {lolo, 66}] // Wlon array with labels

```
`#func()` executes a function at compile time. Comptime functions can be used for code generation and static computations. `using` clause helps the compiler infer type bounds. `declare type ... using(...)` declares types with bounds and runtime checks. `guard someNumber { ... }` block handles runtime overflow checks. Wlon (W Literal Object Notation) is used for literal object and array initialization, and for comptime function return values.

## Services

**Service Declaration:**

```typescript
service NameOfService { // Service declaration
  name: idLikeName // default service name
  context: Network | Process | Thread // default service context
  protocol: Http | TCP | WS | SharedMemory | Custom // default service protocol
  onStart(){ // default onStart handler
    //
  }

  anyFunction(anyArg){ // Service function

  }

  handler(genericProtocolArg){ // default message handler
    genericProtocolArg.type
    genericProtocolArg.data

    return data
  }
  async call(.methodToCall(args)) // RPC/ITC call
  send(genericSendArg) // Send generic message
  send(.context(ctx)) // Send context message
  async receive(.context(ctx)) // Receive context message

  onFinish(){ // default onFinish handler
    //
  }
}
```
Services are declared using the `service` keyword and provide a unified way to handle IPC/ITC, RPC, and other communication methods. Services define lifecycle handlers (`onStart`, `onFinish`), message handlers (`handler`), and functions for communication (`call`, `send`, `receive`).
