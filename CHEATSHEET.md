# W language cheatsheet

> Design draft. Every example is a complete syntax unit: it declares a feature,
> uses it, and shows an observable result. Some units are design/source evidence
> until the corresponding compiler or runtime feature exists.

## Contents

- [Programs, entries, and execution context](#programs-entries-and-execution-context)
- [Modules, imports, exports, and reexports](#modules-imports-exports-and-reexports)
- [Documentation and tests](#documentation-and-tests)
- [Literals and interpolation](#literals-and-interpolation)
- [Collections and ranges](#collections-and-ranges)
- [Operators and pipe-forward](#operators-and-pipe-forward)
- [Numeric policies and bit primitives](#numeric-policies-and-bit-primitives)
- [Functions, labels, defaults, and rest](#functions-labels-defaults-and-rest)
- [Enums, structs, objects, and extensions](#enums-structs-objects-and-extensions)
- [Protocols, generics, refinements, and specialization](#protocols-generics-refinements-and-specialization)
- [Contracts, refinements, and relations](#contracts-refinements-and-relations)
- [Properties, behaviors, and facets](#properties-behaviors-and-facets)
- [Option, conversion, and type queries](#option-conversion-and-type-queries)
- [Ownership, borrows, and views](#ownership-borrows-and-views)
- [Callable values and captures](#callable-values-and-captures)
- [Control flow and patterns](#control-flow-and-patterns)
- [Errors and cleanup](#errors-and-cleanup)
- [Allocator scopes](#allocator-scopes)
- [Unsafe, addresses, and bit operations](#unsafe-addresses-and-bit-operations)
- [Async, spawn, sync, and await](#async-spawn-sync-and-await)
- [Tasks and cancellation](#tasks-and-cancellation)
- [Bounded task pipelines](#bounded-task-pipelines)
- [Service and transaction pipelines](#service-and-transaction-pipelines)
- [Streams and channels](#streams-and-channels)
- [Shared state, atomics, and locks](#shared-state-atomics-and-locks)
- [Units, matrices, tensors, and SIMD](#units-matrices-tensors-and-simd)
- [Foreign code and ABI](#foreign-code-and-abi)
- [Packages and build roots](#packages-and-build-roots)

## Programs, entries, and execution context

Short body, anonymous `.default` descriptor:

<!-- w-example role=executable use=print observable=effect -->
```w
module hello

entry {
  print("Hello, world!")
}
```

Default function descriptor with an explicit host signature:

<!-- w-example role=logical-contract -->
```w
module commandLine

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(
  args: ProcessArguments,
  context: ProcessContext,
): ProcessExitCode {
  execution#checkCancellation()
  let started = execution.clock().now()
  await execution#yield()
  context.output.write("arguments=${args.count}, started=${started}")
  return .success
}

entry(run)
```

Bounded process count comparisons use the logical `usize` member directly:

The declaration/use is `import std.process` with
`async fn countCase(args: Arguments, ctx: Context): ExitCode`; its body tests
`if args.count == 2`, prints `Exactly two arguments` for the true branch, and
prints `Argument count ${args.count}` otherwise before returning `.success`.

The bounded compiler accepts `==`, `!=`, `<`, `<=`, `>`, and `>=` against a
nonnegative unsuffixed integer literal in either operand order. For example,
`2 > args.count` is the reversed spelling of `args.count < 2`. This does not
generalize the cut to `usize` arithmetic or runtime-computed operands; helper
parameters and returns of type `usize` remain outside this bounded form.

Named function descriptor:

<!-- w-example role=executable use=diagnose observable=effect -->
```w
module diagnostics

fn diagnose() { print("ready") }

entry Diagnostics(diagnose)
```

Named body descriptor:

<!-- w-example role=executable use=print observable=effect -->
```w
module embeddedDiagnostics

entry Diagnostics {
  print("ready")
}
```

## Modules, imports, exports, and reexports

Package import exposes its public modules without flattening them:

<!-- w-example role=logical-contract -->
```w
module packageImport

import std

fn argumentCount(args: process.Arguments): usize {
  io.print("count=${args.count}")
  return args.count
}
```

Direct and wildcard module imports flatten the same exports:

<!-- w-example role=logical-contract -->
```w
module flatImports

import std.process
// Equivalent spelling in another module: import * from std.process

fn succeeded(code: ExitCode): Bool { return code == .success }
```

For the bounded one-module process witness, flat and selective imports resolve
the same semantic HIR and Windows product. Provenance still records the source
spelling and spans separately. The equivalent selective declaration is
`import { Arguments as ProcessArguments, Context as ProcessContext, ExitCode
as ProcessExitCode } from std.process`; it uses `args.count == 2` in the same
body.

A module binding keeps qualification; braces select symbols:

<!-- w-example role=logical-contract -->
```w
module selectedImports

import process from std
import networkUrl from std.url
import {
  Arguments as ProcessArguments,
  ExitCode,
} from std.process

fn inspect(args: ProcessArguments, url: networkUrl.URL): (ExitCode, String) {
  return (.success, "${args.count}:${url}")
}
```

Reexports preserve the same distinction:

<!-- w-example role=logical-contract -->
```w
module publicApi

export * from std.url
export { Arguments as ProcessArguments } from std.process

export fn apiName(): String { return "public-api" }
```

## Documentation and tests

<!-- w-example role=executable use=clamp observable=value -->
```w
/// Limits a value to an inclusive interval.
///
/// call: clamp(4, minimum: 0, maximum: 3)
/// result: 3
/// call: clamp(2, minimum: 0, maximum: 3)
/// result: 2
fn clamp(_ value: i32, minimum: i32, maximum: i32): i32 {
  return value.max(minimum).min(maximum)
}

test "clamp preserves an internal value" for clamp {
  expect clamp(2, minimum: 0, maximum: 3) == 2
}
```

## Contracts, refinements, and relations

WCCP0 selects an intrinsic value invariant `T<(predicate)>`, structured
documentation `contract:` fields for non-module declarations, and
`contracts: [...]` in the module header for static module relations. These are
design/source examples until the parser, resolver, ContractIR, and evidence
gates exist.

Use `contract: <(expression)>` for one inline relation and
`contract: relation(...)` for a reusable relation call. Repeated `contract:`
fields form an unordered conjunction. Module contracts use a nonempty list and
participate in module identity; a leading `/// contract:` on a module is only
prose. A contract-safe `const fn` can be reused by either placement. In a
callable contract, a bare parameter is its logical entry value, `result` is the
successful return, and `after name` is mutable final state. `before name` is an
optional explicit spelling of the same entry value for `inout`, `mut ref`, or a
mutable receiver. The initial direction does not add a public `Proof<C>`, a
top-level `contract` declaration, or a `contract fn` declaration.
Runtime `verify` and `check` spellings remain open; this cheatsheet does not
present them as language syntax.

<!-- w-example role=logical-contract -->
```w
module contract_examples<
  contracts: [apiVersion(current: apiMajor, minimum: 1)],
>

export const apiMajor: u16 = 1

const fn apiVersion(current: u16, minimum: u16): Bool {
  return current >= minimum
}

const fn sameCount(input: Array<i64>, output: Array<i64>): Bool {
  return input.count == output.count
}

const fn balanceTransition(input: i64, amount: i64, output: i64): Bool {
  return output == input.saturatingAdd(amount)
}

type SortedResult<Element: Comparable> = Array<Element><(isSorted(value))>

/// Every implementation targets a supported API and provides a nonnegative rank.
/// contract: apiVersion(current: apiMajor, minimum: 1)
protocol Ranked<Item> {
  const apiMajor: u16

  /// The result is nonnegative.
  /// contract: <(result >= 0)>
  fn rank(item: Item): i32
}

/// Sorts values without changing their element count.
/// contract: <(result.count == values.count)>
/// contract: sameCount(input: values, output: result)
fn sort(values: take Array<i64>): SortedResult<i64> {
  return values
}

/// Applies one total balance transition.
/// contract: balanceTransition(input: before balance, amount: amount, output: after balance)
fn deposit(balance: inout i64, amount: i64) {
  balance = balance.saturatingAdd(amount)
}

test "contract-bearing functions remain callable" for sort {
  let values: Array<i64> = [1, 2, 3]
  let result = sort(values: take values)
  expect result.count == 3

  var balance: i64 = 10
  deposit(balance: inout balance, amount: 7)
  expect balance == 17
}
```

## Literals and interpolation

<!-- w-example role=executable use=literalSummary observable=value -->
```w
fn literalSummary(_ seconds: u64): (String, String, String, String) {
  let integer: i32 = 1_000
  let hexadecimal = 0xff
  let ratio: f64 = 0.5e2
  let enabled: Bool = true
  let inferredText = 'W'
  let scalar: UnicodeScalar = 'λ'
  let byte: u8 = b'W'
  let bytes = b"W"
  let json = '{"value":${seconds},"unit":"s"}'
  let doubleQuoted = "${integer}:${hexadecimal}:${ratio}:${enabled}:${scalar}:${byte}"
  let builtins = "Kitchen ${true}/${false}; table: ${"open"}"
  let directNegative = "Balance ${-7}"
  let raw = #"C:\orders\${seconds}"#
  let rawSingle = #'C:\orders\${seconds}'#
  let multiline = """
    north ${seconds}
    south
    """
  let singleMultiline = '''
    east ${seconds}
    west
    '''
  let rawMultiline = #"""
    ${seconds}
    C:\orders
    """#
  expect inferredText == "W"
  expect bytes.count == 1
  expect builtins == "Kitchen true/false; table: open"
  return (
    json,
    doubleQuoted,
    raw + rawSingle,
    multiline + singleMultiline + rawMultiline,
    directNegative,
  )
}

test "ordinary strings interpolate and raw strings do not" for literalSummary {
  let result = literalSummary(30)
  expect result.0 == #'{"value":30,"unit":"s"}'#
  expect result.2.contains(#'${seconds}'#)
  expect result.3.contains("north 30")
  expect result.3.contains(#'${seconds}'#)
  expect result.4 == "Balance -7"
}
```

Immutable bindings retain their inferred types through later interpolation:

<!-- w-example role=executable use=typedBindingSummary observable=value -->
```w
fn typedBindingSummary() {
  let table = 6 * 7
  let isOpen = true
  let state = "open"
  print("Table ${table}; open: ${isOpen}; state: ${state}")
}

test "typed bindings preserve values" for typedBindingSummary {
  expect output of typedBindingSummary() ==
    "Table 42; open: true; state: open\n"
}
```

## Collections and ranges

<!-- w-example role=executable use=collectionSummary observable=value -->
```w
fn collectionSummary(): (i32, i32, i32, usize, i32) {
  let positional = (1, "north")
  let singleton = (7,)
  let named = (north: 1, east: 2)
  let explicit: [i32; 4] = [1, 2, 3, 4]
  let repeated: [i32; 4] = [0; 4]
  let smaller: [i32; 2] = [8, 9]
  // let larger: [i32; 4] = smaller // error: count is part of the type
  let values = [1, 2, 3, 4]
  let scores = ["north": 7, "south": 9]
  let eager = values
    .filter((value) => value % 2 == 0)
    .map((value) => value * 10)
  let lazy = values.lazy
    .filter((value) => value % 2 == 0)
    .map((value) => value * 10)
    .take(2)
    .collect() // only the explicitly lazy Iterator needs materialization

  var closed = 0
  for value in 1...3 { closed += value }

  var halfOpen = 0
  for value in 1..<3 { halfOpen += value }

  let inner = values[1>..<3]
  let rightClosed = values[1>..3]
  guard let north = scores["north"] else panic("fixture key is missing")
  expect positional.1 == "north"
  expect singleton.0 == 7
  expect explicit[2] == 3
  expect repeated == [0, 0, 0, 0]
  expect smaller.count == 2
  expect eager == lazy
  return (
    named.east,
    north,
    eager[0],
    explicit.count + inner.count + rightClosed.count,
    closed + halfOpen + lazy[0],
  )
}

test "collections expose labels, bounds, and counts" for collectionSummary {
  expect collectionSummary() == (2, 7, 20, 7, 29)
}
```

## Operators and pipe-forward

<!-- w-example role=executable use=addOne,double,renderNumber,clamp,multiply,divide,remainder,checkedI8Sum,checkedU16Product,combineFlags,widenedFlags,mixedFlags,checkedI16Divrem,checkedU16Compound,negateSmall,complementSigned,complementUnsigned,Reading,operatorSummary observable=value -->
```w
fn addOne(_ value: i32): i32 { return value + 1 }
fn double(_ value: i32): i32 { return value * 2 }
fn renderNumber(_ value: i32): String { return "${value}" }
fn clamp(_ value: i32, minimum lower: i32, maximum upper: i32): i32 {
  return value.max(lower).min(upper)
}
fn multiply(_ value: i32, by factor: i32): i32 { return value * factor }
fn divide(_ value: i32, by divisor: i32): i32 { return value / divisor }
fn remainder(_ value: i32, by divisor: i32): i32 { return value % divisor }
fn checkedI8Sum(left: i8, right: i8): i8 { return left + right }
fn checkedU16Product(left: u16, right: u16): u16 { return left * right }
fn combineFlags(left: u8, right: u8): u8 {
  return (left & right) | (left ^ right)
}
fn widenedFlags(left: i8, right: i32): i32 { return left | right }
fn mixedFlags(left: u8, right: i16): i16 { return left | right }
fn checkedI16Divrem(left: i16, right: i16): (i16, i16) {
  return (left / right, left % right)
}
fn checkedU16Compound(left: u16, right: u16): u16 {
  var result = left
  result += right
  result -= 2_u16
  result *= 2_u16
  result /= 2_u16
  result %= right
  return result
}
fn negateSmall(value: i16): i16 { return -value }
fn complementSigned(value: i16): i16 { return ~value }
fn complementUnsigned(value: u8): u8 { return ~value }

object Reading {
  let value: i32

  fn scaled(by factor: i32): Reading {
    return Reading(value: value * factor)
  }

  fn limited(to maximum: i32): Reading {
    return Reading(value: value.min(maximum))
  }

  fn render(): String { return renderNumber(value) }
}

fn operatorSummary(): (String, u8, Bool, u8, i32, i32, i32, Bool, i32) {
  var flags: u8 = 0b0001
  flags |= 0b0100
  flags <<= 1

  let rendered = 20
    |> addOne()
    |> double()
    |> renderNumber()

  let objectFlow = Reading(value: 4)
    |> .scaled(by: 3)
    |> .limited(to: 10)
    |> .render()

  let bounded = 20
    |> clamp(minimum: 0, maximum: 12)
    |> multiply(by: 3)

  let functional = 8
    |> multiply(by: 2)
    |> divide(by: 3)
    |> remainder(by: 5)

  let relation = (flags & 0b1010) == 0b1010 && !false
  let xor = flags ^ 0b0011
  let power = 2 ** 5
  let quotient = 10 / 2
  let remainder = 10 % 3
  let rangeCheck = 2 in 1...3
  let optional: i32? = .none
  let fallback = optional ?? 7
  expect objectFlow == "10"
  expect bounded == 36
  expect functional == 0
  return (rendered, flags, relation, xor, power, quotient, remainder, rangeCheck, fallback)
}

test "operators and pipe-forward produce values" for operatorSummary {
  // Expected: exit 0; stdout is empty.
  var assigned = 8
  assigned += 2
  assigned -= 1
  assigned *= 2
  assigned /= 3
  assigned %= 5
  assigned **= 2
  assigned <<= 1
  assigned >>= 1
  assigned &= 0b0011
  assigned ^= 0b0010
  assigned |= 0b0100

  expect operatorSummary() == ("42", 10, true, 9, 32, 5, 1, true, 7)
  expect checkedI8Sum(-12_i8, 3_i8) == -9_i8
  expect checkedU16Product(1000_u16, 30_u16) == 30000_u16
  expect combineFlags(170_u8, 15_u8) == 175_u8
  expect widenedFlags(-16_i8, 3_i32) == -13_i32
  expect mixedFlags(240_u8, 15_i16) == 255_i16
  expect checkedI16Divrem(-1000_i16, 30_i16) == (-33_i16, -10_i16)
  expect checkedU16Compound(1000_u16, 30_u16) == 8_u16
  expect negateSmall(7_i16) == -7_i16
  expect complementSigned(0_i16) == -1_i16
  expect complementUnsigned(0_u8) == 0xff
  expect assigned == 7
  expect (0b1000 >> 2) == 2
  expect (-8_i8 >> 1_u64) == -4_i8
  expect (0x80_u8 >> 1_u64) == 0x40_u8
  expect (0x20_u8 << 1_u64) == 0x40_u8
  expect (~0_u8) == 0xff
  expect 2 <= 2 && 3 >= 2
  expect 1 != 2 || false
}
```

Ordinary integer `&`, `|`, and `^` apply existing exact integer widening first,
then require both operands to have the same canonical integer type and preserve
that type as the result. Source signedness may differ only through range-safe
exact widening such as `u8 -> i16`; lossy or ambiguous mixing remains invalid.
The seed compiler
covers `i8`/`u8`, `i16`/`u16`, `i32`/`u32`, `i64`/`u64`, and the current
x86-64 `Int`/`UInt` aliases. The exact family witness is
[`integer-bitwise.w`](compiler/seed-c/fixtures/integer-bitwise.w);
its source-local comment declares the expected exit and literal stdout.
Ordinary `<<` and `>>` now have source-backed checked coverage for those same
types. The value and result use exactly the same canonical integer type, and
the count is `UInt` (`u64` on the current x86-64 target). A count at or above
the logical width fails; left shift also fails if the mathematical result does
not fit. Signed right shift is arithmetic and unsigned right shift is logical:

The exact family witness is
[`shifts.w`](compiler/seed-c/fixtures/shifts.w). Named
numeric/shift policies, power, rotations, remaining bit primitives, SIMD,
`usize`/`isize`, 128-bit integers, non-x86-64 alias widths, stable ABI/FFI,
other targets, and equivalent-runtime performance remain open under W-392.

## Numeric policies and bit primitives

<!-- w-example role=logical-contract -->
```w
fn numericPolicies(): (u8, u8, Bool, UInt) {
  let small: u8 = 200
  let wider: u16 = small
  let signed: i16 = small
  expect wider == 200 && signed == 200

  let negative: i16 = -7
  let unsignedSource: u16 = 300
  let saturatedUnsigned: u8 = u8(saturating: negative)
  let saturatedSigned: i8 = i8(saturating: unsignedSource)
  expect saturatedUnsigned == 0
  expect saturatedSigned == 127

  let lowByte: u8 = u8(truncatingBits: negative)
  let widenedNegative: i64 = i64(truncatingBits: negative)
  let signedPattern: i8 = i8(truncatingBits: 255_u8)
  expect lowByte == 249
  expect widenedNegative == -7
  expect signedPattern == -1

  expect (try? u8.checkedAdd(250, 10)) == .none
  expect (try? u8.checkedSubtract(2, 3)) == .none
  expect (try? u8.checkedMultiply(20, 20)) == .none
  expect (try? u8.checkedNegate(1)) == .none
  expect (try? u8.checkedDivide(7, 0)) == .none
  expect (try? u8.checkedRemainder(7, 0)) == .none
  expect (try? u8.checkedPower(4, 4)) == .none
  expect (try? u8.checkedShiftLeft(0x80, 1)) == .none
  expect (try? u8.checkedShiftRight(1, 8)) == .none

  let wrapped = u8.wrappingAdd(250, 10)
  expect wrapped == 4
  expect u8.wrappingSubtract(2, 3) == 255
  expect u8.wrappingMultiply(20, 20) == 144
  expect u8.wrappingNegate(1) == 255
  expect u8.wrappingPower(4, 4) == 0
  expect u8.wrappingShiftLeft(0x80, 1) == 0

  let saturated = u8.saturatingAdd(250, 10)
  expect saturated == 255
  expect u8.saturatingSubtract(2, 3) == 0
  expect u8.saturatingMultiply(20, 20) == 255
  expect u8.saturatingNegate(1) == 0
  expect u8.saturatingPower(4, 4) == 255

  expect u8.overflowingAdd(250, 10) == (4, true)
  expect u8.overflowingSubtract(2, 3) == (255, true)
  expect u8.overflowingMultiply(20, 20) == (144, true)
  expect u8.overflowingNegate(1) == (255, true)
  expect u8.overflowingPower(4, 4) == (0, true)

  expect i32.euclideanDivide(-7, 3) == -3
  expect i32.euclideanRemainder(-7, 3) == 2
  let carry = u8.carryingAdd(250, 5, carry: true)
  let borrow = u8.borrowingSubtract(0, 0, borrow: true)
  let full = u8.fullMultiply(16, 16)
  expect carry == (value: 0, carry: true)
  expect borrow == (value: 255, borrow: true)
  expect full == (high: 1, low: 0)

  expect u8.maskedShiftLeft(1, 9) == 2
  expect u8.maskedShiftRight(0x80, 9) == 0x40
  expect u8.logicalShiftRight(0x80, 1) == 0x40
  expect u8.rotatedLeft(0x81, 1) == 0x03
  expect u8.rotatedRight(0x81, 1) == 0xc0

  let bits = 0x16_u8.toBits()
  expect u8.fromBits(bits) == 0x16
  let negativeZero32: f32 = f32.fromBits(0x80000000_u32)
  let copiedNegativeZero32 = negativeZero32
  expect copiedNegativeZero32.toBits() == 0x80000000_u32
  let nan64: f64 = f64.fromBits(0x7ff8123456789abc_u64)
  let copiedNan64 = nan64
  expect copiedNan64.toBits() == 0x7ff8123456789abc_u64
  let bytes = 0x1234_u16.toBytes(order: .big)
  expect u16.fromBytes(bytes, order: .big) == 0x1234
  expect u8.bitWidth == 8
  expect u8.countOnes(0x16) == 3
  expect u8.countZeros(0x16) == 5
  expect u8.countLeadingZeros(0x16) == 3
  expect u8.countTrailingZeros(0x16) == 1
  expect u8.reversedBits(0x16) == 0x68
  expect u16.reversedBytes(0x1234) == 0x3412

  return (wrapped, saturated, carry.carry, u8.countOnes(wrapped ^ saturated))
}

test "numeric policies name overflow and representation" for numericPolicies {
  expect numericPolicies() == (4, 255, true, 7)
}
```

The selected bit-primitive signatures are:

| API | Signature |
| --- | --- |
| `rotatedLeft` | `static fn rotatedLeft(_ value: Self, _ count: UInt) -> Self` |
| `rotatedRight` | `static fn rotatedRight(_ value: Self, _ count: UInt) -> Self` |
| `countOnes` | `static fn countOnes(_ value: Self) -> UInt` |
| `countZeros` | `static fn countZeros(_ value: Self) -> UInt` |
| `countLeadingZeros` | `static fn countLeadingZeros(_ value: Self) -> UInt` |
| `countTrailingZeros` | `static fn countTrailingZeros(_ value: Self) -> UInt` |
| `reversedBits` | `static fn reversedBits(_ value: Self) -> Self` |
| `reversedBytes` | `static fn reversedBytes(_ value: Self) -> Self` |

Every operation uses the logical width of `Self`. Rotations reduce their
`UInt` count modulo that width; counts return `UInt`, and leading/trailing
zero counts of zero equal the width. Leading counts scan from the
most-significant bit, and trailing counts from the least-significant bit.
Signed values use the full two's-complement bit pattern, including the sign
bit. Reversals preserve `Self`; byte reversal is independent of host endianness.

W-1648 is scoped to the existing W-392 functions on built-in `i8`/`u8`,
`i16`/`u16`, `i32`/`u32`, and `i64`/`u64`. It adds no syntax and makes no
source-backed claim for `Int`/`UInt`, `isize`/`usize`, 128-bit integers, or
other targets. The compact neutral witness is
[`fixed-integer-bit-primitives.w`](compiler/seed-c/fixtures/fixed-integer-bit-primitives.w);
it covers all eight operations across the eight fixed-width types. Its exact
source-to-native output passes the CRT-free Windows x64 and Linux/WSL x64
gates. This is correctness-only evidence; the slice is not performance-ready.

`maskedShiftLeft`, `maskedShiftRight`, and `logicalShiftRight` use the same
`static fn operation(_ value: Self, _ count: UInt) -> Self` shape shown in the
example. The masked policies reduce the count modulo `Self.bitWidth`;
`maskedShiftRight` is arithmetic for signed values and logical for unsigned
values. `logicalShiftRight` always zero-fills and rejects a count at or beyond
the logical width. W-1649 source-backs these three existing APIs exactly for
the built-in `i8`/`u8` through `i64`/`u64` family on the CRT-free Windows x64
and Linux/WSL x64 routes. The neutral witness is
[`fixed-integer-shift-policies.w`](compiler/seed-c/fixtures/fixed-integer-shift-policies.w).
It is correctness-only evidence; aliases, wider integers, other targets, and
performance remain open.

`D(truncatingBits: source)` is the existing total fixed-width integer
conversion. Current seed evidence is correctness-only and covers signed and
unsigned 8/16/32/64-bit integers plus the current x86-64 `Int`/`UInt` aliases.
`usize`/`isize`, 128-bit integers, target-general alias widths, stable ABI/FFI,
other targets, and performance remain outside this slice. Integer `exactly:`
has a separate bounded typed path. Float-to-integer `rounding:` has a bounded
CRT-free process witness: a runtime `Arguments.count` branch selects one of two
`f64` literals for conversion to `i8` with an explicit mode and distinct
normal, non-finite, and out-of-range outcomes. Arbitrary runtime float input,
other public process pairs, general typed process bodies, other `saturating:`
families, and performance remain gaps.

<!-- w-example role=logical-contract -->
```w
fn narrowExactly(_ value: i16): i8 throws NumericConversionError {
  return try i8(exactly: value)
}

test "exact conversion preserves failure as a typed outcome" for narrowExactly {
  expect (try narrowExactly(127)) == 127_i8
  expect (try? narrowExactly(128)) == .none
}
```

The current bounded integer implementation covers all source/destination pairs
among signed and unsigned 8/16/32/64-bit integers and current x86-64
`Int`/`UInt`. Verified HIR preserves distinct normal and
`NumericConversionError.outOfRange` successors. ProductClosure0 now projects
both outcomes for the restricted `native-process@1` root. That exact root now
executes on CRT-free Windows x64 and Linux/WSL x64: cleanup remains LIFO on
both arms, success exits 0, and unhandled out-of-range exits 1 without implicit
output. User-defined direct throws, general typed process bodies, and
performance evidence remain open.

The bounded float-rounding process witness declares and uses the conversion
result. Both branches execute on the public Windows and Linux/WSL routes; the
float operands themselves remain constants, so this is not a throughput
benchmark for runtime float conversion:

<!-- w-example role=logical-contract -->
```w
import { Arguments, Context, ExitCode } from std.process

async fn run(args: Arguments, ctx: Context): ExitCode throws NumericConversionError {
  let rounded = try i8(rounding: if args.count == 0 { 2.5_f64 } else { 3.5_f64 }, mode: .nearestEven)
  print("Rounded ${rounded}")
  return .success
}

entry(run) // no args: "Rounded 2\n"; one arg: "Rounded 4\n"; both exit 0
```

Integer `D(saturating: source)` clamps the mathematical value to the
destination minimum or maximum and is total. Its integer form accepts no
`try` or `nan:` label. The earlier `u8(saturating: negative)` and
`i8(saturating: unsignedSource)` calls document the W-1644 values. The
current bounded implementation covers every source/destination pair among the
signed and unsigned 8/16/32/64-bit integers and x86-64 `Int`/`UInt`; the
compact public witness executes all four signedness quadrants on Windows and
Linux/WSL. Other conversion families and target-general aliases remain gaps.

Fixed-float `fromBits`/`toBits` admits only its matching unsigned carrier:
`f16`/`bf16` use `u16`, `f32` uses `u32`, `f64` uses `u64`, and `f128` uses
`u128`. It reinterprets bits rather than converting a numeric value. Storage,
copy, and round-trip preserve the encoding; arithmetic NaN payloads are not
portable. Byte serialization remains separate and requires an explicit order.
The current seed witness remains limited to f32/f64 on Windows x64 and
Linux/WSL x64.

<!-- w-example role=executable use=strictFloatSummary observable=value -->
```w
fn strictFloatSummary(
  singleLeft: f32,
  singleRight: f32,
  doubleLeft: f64,
  doubleRight: f64,
): (f32, f64, Bool) {
  let single = singleLeft + singleRight
  let double = doubleLeft + doubleRight
  let nan = 0.0_f32 / 0.0_f32
  return (single, double, nan != nan)
}

test "strict floats preserve width and IEEE unordered comparisons" for strictFloatSummary {
  let summary = strictFloatSummary(
    doubleRight: 2.25_f64,
    singleLeft: 1.5_f32,
    doubleLeft: 1.5_f64,
    singleRight: 2.25_f32,
  )
  expect summary.0 == 3.75_f32
  expect summary.1 == 3.75_f64
  expect summary.2
}
```

`f16`, `bf16`, `f32`, `f64`, and IEEE binary128 `f128` are fixed arithmetic
scalars. Low-precision AI elements always name their encoding; bare `f4`,
`f6`, and `f8` are errors.

<!-- w-example role=logical-contract -->
```w
import { BigFloat, BigFloatError, BigFloatLimits } from std.math
import { Tensor } from std.tensor

type Activation = f8<.e4m3fn>
type Gradient = f8<.e5m2>
type Weight = f4<.e2m1fn>
type AuditFloat = BigFloat<precision: 256>
type RuntimeFloat = BigFloat<precision: .dynamic>

fn numericFormats(
  activations: ref Tensor<Activation, shape: [32, 64]>,
  weights: ref Tensor<Weight, shape: [64, 16]>,
): f32 throws NumericConversionError {
  let activation = try Activation(rounding: 1.25_f32, mode: .nearestEven)
  let gradient = try Gradient(rounding: 32.0_f32, mode: .nearestEven)
  let restored = f32(activation) + f32(gradient)

  let product = tensor.matmul<f32>(
    activations,
    weights: weights,
    compute: .tensorFloat32,
    accumulator: f32,
    mode: .strict,
  )
  return restored + product[0, 0]
}

fn runtimePrecisionSummary(
  allocator: ref Allocator,
  limits: ref BigFloatLimits,
): UInt throws BigFloatError {
  let audit: AuditFloat = 3.141592653589793238462643383279502884
  let runtime = try RuntimeFloat(
    exactly: audit,
    precisionBits: 512,
    rounding: .nearestEven,
    allocator: allocator,
    limits: limits,
  )
  return runtime.precisionBits
}
```

`f8` uses one addressable byte. Ordinary scalar/field/`Array` values of `f4`
and `f6` also use an eight-bit carrier; only an explicit packed Tensor,
Quantized, or block-storage contract may use dense sub-byte representation.
`e8m0fnu` is MX block-scale metadata, not a scalar float. Element encoding,
packing, compute policy, accumulator, and target acceleration are independent.

Exact total widening is implicit for `f32 -> f64`, 8/16-bit integers to
`f32`, and 8/16/32-bit integers to `f64`. `D(value)` uses the same route.
Other numeric pairs require a separately selected conversion policy; the
value of one constant does not make a lossy type relation implicit.

<!-- w-example role=executable use=widenForScore,widenForTotal,numericWideningSummary observable=value -->
```w
fn widenForScore(value: i16): f32 { return value }
fn widenForTotal(value: u32): f64 { return value }

fn numericWideningSummary(score: i16, total: u32, single: f32): (f32, f64, f64) {
  let returned = widenForScore(value: score)
  let called = widenForTotal(value: total)
  let explicit = f64(single)
  let mixed = 2_i32 + explicit
  return (returned, called, mixed)
}

test "exact numeric widening is implicit or explicitly total" for numericWideningSummary {
  let summary = numericWideningSummary(score: -123, total: 4_294_967_295, single: 0.5_f32)
  expect summary == (-123.0_f32, 4_294_967_295.0_f64, 2.5_f64)
}
```

## Functions, labels, defaults, and rest

<!-- w-example role=executable use=labelled,join,route,announce observable=value -->
```w
fn labelled(
  _ value: String,
  externalAudit audit: String,
  _ note: String,
  to destination: String,
  title: String = "city",
): String {
  return value + audit + note + destination + title
}

fn join(separator: String, values: String...): String {
  return values.joined(separator: separator)
}

fn route(_ audit: String): String { return "positional:${audit}" }
fn route(audit: String): String { return "labeled:${audit}" }

fn announce(table: i64, isOpen: Bool): String {
  return "Table ${table}; open: ${isOpen}"
}

test "call labels and rest arguments keep their shape" for labelled {
  let labels = labelled("n", externalAudit: "o", "r", title: "h", to: "t")
  let values = ["east", "west"]
  expect labels == "north"
  expect join(separator: "/", values: each values) == "east/west"
  expect route("open") == "positional:open"
  expect route(audit: "open") == "labeled:open"
  // Named arguments may reorder; evaluation still follows source order.
  expect announce(isOpen: true, table: 6 * 7) == "Table 42; open: true"
}
```

## Enums, structs, objects, and extensions

<!-- w-example role=executable use=Place,Counter,Signal,describe observable=value -->
```w
enum Signal {
  quiet
  alert(level: u8)
}

struct Place {
  let id: u64
  var label: String = "square"

  init(id: u64, label: String) {
    self.id = id
    self.label = label
  }

  deinit { print("dropping ${label}") }
}

object Counter {
  var value: i32
  mut fn increment() { value += 1 }
}

extension Place {
  fn describe(): String { return "${id}:${label}" }
}

fn describe(signal: Signal): String {
  return switch signal {
    case .quiet: "quiet"
    case .alert(let level): "alert:${level}"
  }
}

test "nominal declarations expose their members" for Place {
  let place = Place(id: 7, label: "north")
  let counter = Counter(value: 0)
  let signal: Signal = .alert(level: 2)
  let ref { id, ... } = place
  let (description, observedCount) = (place.describe(), counter.value)
  counter.increment()
  expect id == 7
  expect description == "7:north"
  expect observedCount == 0
  expect counter.value == 1
  expect counter.isSameInstance(as: counter)
  expect describe(signal: signal) == "alert:2"
}
```

Nominal aggregate syntax and physical storage are separate. `struct`, `enum`,
and `object` share aggregate lowering. An object declaration or `ref` use does
not imply a heap, header, address, or storage class. `struct` and `enum` use
value defaults, while `object` uses reference and identity defaults. The
compiler materializes an aggregate only when a surviving observable requires
it. `struct` and `object` may declare `init` and `deinit`; enums use cases and
synthesized payload cleanup. A custom `deinit` makes the type non-`Copy`, while
automatic drop glue does not.
`isSameInstance` may fold without allocation when identity is proven.

## Protocols, generics, refinements, and specialization

<!-- w-example role=logical-contract -->
```w
protocol Source<Item> {
  fn item(at index: usize): Item
}

protocol Counted {
  fn count(): usize
}

protocol Catalog<Item>: Source<Item> & Counted {}

struct Shelf<T> {
  let items: Array<T>
}

enum Mode { fast; strict }
alias StringShelf = Shelf<String>
type AllowedMode = Mode<[.strict]>
type Digest = [u8; 32]
type SmallCount = u16<(.member <= 64)>

const DefaultColumns: usize = 4

struct StaticWindow<
  rows: usize<(.member > 0)>,
  columns: usize,
> {
  let values: [[f32; columns]; rows]
}

static const fn zeroWindow<rows: usize>(): StaticWindow<rows: rows, columns: DefaultColumns> {
  return StaticWindow(values: [[0.0; DefaultColumns]; rows])
}

extension<T: Equatable> Shelf<T>: Catalog {
  fn item(at index: usize): T { return items[index] }
  fn count(): usize { return items.count }
}

test "generic conformances preserve the concrete item" for Shelf {
  let shelf: StringShelf = Shelf(items: ["north", "south"])
  let count: SmallCount = try SmallCount(shelf.count())
  let mode: AllowedMode = .strict
  var digest: Digest = [0; 32]
  digest[0] = 0xa5
  let window = zeroWindow<rows: 2>()
  expect shelf.item(at: 1) == "south"
  expect count == 2
  expect mode == .strict
  expect digest[0] == 0xa5
  expect digest != [0; 32]
  expect window.values.count == 2
  expect window.values[0].count == DefaultColumns
}
```

## Properties, behaviors, and facets

<!-- w-example role=executable use=WrappedDegrees,Versioned,VersionedDegrees,Attitude,PropertyModes,PropertyAccessKind,accessName,nudge,overwrite,sampleYaw observable=value -->
```w
behavior WrappedDegrees for u16 {
  var current: u16

  fn normalized(value: u16): u16 { return value % 360_u16 }

  init(initialValue: fn(): u16) { current = normalized(value: initialValue()) }
  get {
    defer { current = normalized(value: current) }
    return current
  }
  mut set(newValue) { current = normalized(value: newValue) }

  export mut fn reset() { current = 0 }
}

behavior Versioned<Value> for Value {
  var epoch: u64
  var replacements: u64
  var reads: u64

  init() {
    epoch = 0
    replacements = 0
    reads = 0
  }

  export let mutationEpoch: u64 { get => epoch }
  export let replacementCount: u64 { get => replacements }
  export let readCount: u64 { get => reads }
  export mut fn resetMutationEpoch() { epoch = 0 }

  mut willGet(kind: PropertyAccessKind) { reads += 1 }
  mut didGet(kind: PropertyAccessKind) {
    if kind == .mutableBorrowed { epoch += 1 }
  }
  mut willSet(current: ref Value, proposed: ref Value) { replacements += 1 }
  mut didSet(current: ref Value) { epoch += 1 }
}

enum PropertyAccessKind {
  value
  borrowed
  mutableBorrowed
}

object PropertyModes {
  var storage: u16 = 1

  let snapshot: u16 { get => storage }
  let borrowed: ref u16 { get => storage }
  var replaceable: u16 {
    get => storage
    set(value) => storage = value
  }
  var borrowedReplaceable: ref u16 {
    get => storage
    set(value) => storage = value
  }
  var direct: mut ref u16 { get => storage }
  var buffered: inout u16 {
    get => storage
    set(value) => storage = value
  }
}

fn accessName(kind: PropertyAccessKind): String {
  let observed: PropertyAccessKind = kind
  return switch observed {
    case .value: "value"
    case .borrowed: "borrowed"
    case .mutableBorrowed: "mutableBorrowed"
  }
}

behavior VersionedDegrees for u16 =
  (degrees: WrappedDegrees, version: Versioned)

fn nudge(value: mut ref u16) { value += 5 }
fn overwrite(value: inout u16) { value = 21 }

struct Attitude {
  var VersionedDegrees yaw: mut ref u16 = 0
  var Versioned remainingCorrections: u16 = 8
  mut fn rotate(by delta: u16) { yaw += delta }
  mut fn sampleYaw(after epoch: u64): u16? {
    if yaw#version.mutationEpoch == epoch { return .none }
    return .some(yaw)
  }
}

test "behavior composition exposes qualified facets" for Attitude {
  var modes = PropertyModes()
  let copied = modes.snapshot
  let ref borrowed = modes.borrowed
  expect copied == 1 && borrowed == 1
  modes.replaceable = 3
  modes.borrowedReplaceable = 5
  nudge(value: mut ref modes.direct)
  overwrite(value: inout modes.buffered)
  expect modes.storage == 21

  var attitude = Attitude()
  // A value result does not make the observer metadata write readonly.
  let remaining = attitude.remainingCorrections
  expect remaining == 8
  expect attitude.remainingCorrections#readCount == 1
  expect attitude.remainingCorrections#mutationEpoch == 0

  attitude.yaw = 350
  attitude.rotate(by: 25)
  expect attitude.yaw == 15
  // The declared mut ref kind stays fixed even when the caller compares a value.
  let beforeReset = attitude.yaw#version.mutationEpoch
  expect beforeReset == 3
  expect attitude.yaw#version.readCount == 2
  expect attitude.yaw#version.replacementCount == 1
  attitude.yaw#degrees.reset()
  expect attitude.yaw == 0
  expect attitude.yaw#version.mutationEpoch == 5
  expect attitude.yaw#version.readCount == 3
  expect attitude.yaw#version.replacementCount == 1
  // Epoch counts mutation admissions; facet reads do not run the logical get.
  nudge(value: mut ref attitude.yaw)
  expect attitude.yaw == 5
  expect attitude.yaw#version.mutationEpoch == 7
  expect attitude.yaw#version.readCount == 5
  expect attitude.yaw#version.replacementCount == 1

  let savedEpoch = attitude.yaw#version.mutationEpoch
  expect attitude.sampleYaw(after: savedEpoch) == .none
  expect attitude.yaw#version.readCount == 5
  attitude.rotate(by: 355)
  expect attitude.sampleYaw(after: savedEpoch) == .some(0)
  let publishedEpoch = attitude.yaw#version.mutationEpoch
  expect publishedEpoch == 9
  expect attitude.sampleYaw(after: publishedEpoch) == .none
  expect attitude.yaw#version.readCount == 7
  // resetMutationEpoch invalidates saved epochs; it is not a global ticket/cache key.
  attitude.yaw#version.resetMutationEpoch()
  let accessKind: PropertyAccessKind = .value
  expect accessName(kind: accessKind) == "value"
}
```

The property declaration selects the access mode; the accessor is always
spelled `get`. `let p: T` returns a value through a read-only getter, and `let p: ref T` returns
a read-only borrow. `var p: T` may replace a value, `var p: ref T` may replace
or borrow it, `var p: mut ref T` exposes a scoped exclusive borrow, and
`var p: inout T` performs copy-in/copy-out through `get` plus `set`. The form
`let p: mut ref T` is invalid for a computed property; a stored `let p: mut ref T`
remains a move-only capability whose reborrow needs exclusive authority over the
enclosing place. `let p: inout T` is never a stored type and is invalid for a
computed `let`; `get ref` and
`get mut ref` are not accessor variants. Runtime stored or computed properties
start with `let` or `var`; `const` is a separate compile-time member without
per-instance storage or a runtime accessor. W does not accept a bare `name: T`
property. Enum payload labels, tuple labels, parameters, and call labels are
not properties and therefore do not use a property binder. Neither do
`build.w` manifest keys or foreign ABI layout members; `foreign c { struct
Header { size: c.size } }` describes C layout, not a W property.
An explicit one-observer application uses `var`, for example
`var Versioned value: u16 = 0`; it synthesizes plain storage and exposes direct
facet paths. A named composition remains the form for multiple behaviors or
reusable aliases. The RHS initializes the plain storage, not observer metadata.
`willGet` and `didGet` are opt-in observer hooks. Their
`PropertyAccessKind` comes from the declared projection (`T`/`inout` value,
`ref` borrowed, `mut ref` mutableBorrowed), even when the caller compares or
copies the result. A `mut` read hook needs exclusive enclosing authority from
`willGet` through getter cleanup and `didGet`; it does not add a hidden lock,
atomic or interior mutation. Epoch counts admissions, and a metadata facet does
not execute the logical get. Set observers count value-in/value-out writeback,
while direct mutable-borrow access does not invoke `willSet` or `didSet`. For
computed or behavior-backed `inout`, the getter must produce an owned,
property-safe `T`; the single-owner non-`Copy` transport below applies only to a
plain direct stored place, not to an accessor get.

## Option, conversion, and type queries

<!-- w-example role=executable use=ReservationKey,LookupResult,inspectKey,metadataSummary,nameOr observable=value -->
```w
struct ReservationKey: Hashable & Reflectable {
  let orderId: u64
}

enum LookupResult: Reflectable {
  found(id: u64)
  missing
}

fn inspectKey(value: ref any Hashable): u64? {
  if value is ReservationKey {
    if let ref key = value as? ReservationKey {
      let staticId = type of ReservationKey
      let dynamicId = type of value
      let ref metadata = info of ReservationKey
      guard metadata.id == staticId && dynamicId == staticId else { return .none }
      return .some(key.orderId)
    }
  }
  return .none
}

fn metadataSummary(): (TypeId, String, TypeKind, TypeId?, usize, TypeId, String) {
  let ref metadata = info of ReservationKey
  let ref property = metadata.properties[0]
  let ref enumMetadata = info of LookupResult
  let ref foundCase = enumMetadata.cases[0]
  return (
    metadata.id,
    copy metadata.name,
    metadata.kind,
    metadata.base,
    metadata.properties.count,
    property.valueType,
    copy foundCase.name,
  )
}

fn nameOr(value: String?): String {
  return value?.trim() ?? "unknown"
}

test "conditional cast keeps the borrowed value" for ReservationKey {
  let key = ReservationKey(orderId: 42)
  let missing: LookupResult = .missing
  let summary = metadataSummary()
  expect inspectKey(value: ref key) == .some(42)
  expect summary.0 == type of ReservationKey
  expect !summary.1.isEmpty
  expect summary.2 == .struct
  expect summary.3 == .none
  expect summary.4 == 1
  expect summary.5 == type of u64
  expect summary.6 == "found"
  let ref keyInfo = info of ReservationKey
  let ref orderId = keyInfo.properties[0]
  let ref resultInfo = info of LookupResult
  let ref found = resultInfo.cases[0]
  expect orderId.name == "orderId"
  expect orderId.mutability == .immutable
  expect orderId.accessMode == .value
  expect !orderId.hasSetter
  expect found.payloadTypes == [type of u64]
  expect resultInfo.cases[1].payloadTypes.isEmpty
  expect missing == .missing
  expect nameOr(value: .some(" W ")) == "W"
  expect nameOr(value: .none) == "unknown"
}
```

## Ownership, borrows, and views

<!-- w-example role=executable use=Point,Ticket,Receipt,translated,ticketLabel,bumpTicket,consumeTicket,readFirst,replaceFirst,consume,window observable=value -->
```w
struct Point: Copy & Equatable {
  let x: i32
  let y: i32
}

struct Receipt {
  let id: u64
}

object Ticket {
  var label: String
}

fn translated(point: Point): Point {
  return Point(x: point.x + 1, y: point.y + 1)
}

fn ticketLabel(ticket: Ticket): String { return copy ticket.label }
fn bumpTicket(ticket: mut ref Ticket) { ticket.label = "bumped" }
fn consumeTicket(ticket: take Ticket): String { return copy ticket.label }

fn readFirst(values: ref Array<String>): ref String { return ref values[0] }

fn replaceFirst(values: inout Array<String>, replacement: String) {
  values[0] = replacement
}

fn consume(value: take String): String { return value }

fn window(values: view Array<String>): view Array<String> {
  return values[1..<3]
}

test "ownership operations are explicit at the call site" for consume {
  let point = Point(x: 1, y: 2)
  let translatedPoint = translated(point: point) // Point is Copy; the value call is implicit.
  var ticket = Ticket(label: "T-7")
  expect ticketLabel(ticket: ticket) == "T-7" // object parameters default to ref.
  bumpTicket(ticket: mut ticket) // `mut objectPlace` is the short form for mut ref.
  bumpTicket(ticket: mut ref ticket) // explicit spelling remains valid.
  let ticketText = consumeTicket(ticket: take ticket)

  var values = ["north", "east", "south"]
  let copied = copy readFirst(values: ref values)
  replaceFirst(values: inout values, replacement: "west")
  let pinned = pin values
  let middle = window(values: values)
  let moved = consume(value: take copied)
  let receipt = Receipt(id: 7)
  let movedReceipt = receipt // non-Copy values move at last use.
  let _ = pinned
  expect point == Point(x: 1, y: 2)
  expect translatedPoint == Point(x: 2, y: 3)
  expect ticketText == "bumped"
  expect moved == "north"
  expect movedReceipt.id == 7
  expect values[0] == "west"
  expect middle.count == 2
}
```

`ref T` is a shared read-only borrow. `mut ref T` is a dependent exclusive
borrow. `mut view T` is an exclusive logical view. `inout T` is a
parameter/call convention: `values: inout values` reserves the source place,
lets the callee mutate a local, and writes back once on normal return,
structured `throw`, or structured cancellation. A computed `var p: inout T`
publishes this capability but is not a stored type; a stored `var p: T` is the
direct place form. Under the exclusive reservation, a single non-`Copy` owner
can move into the callee local and return without hidden clone, retain, or deep
copy. `inout` is not a result, binding mode, stored-field type, capture, or
iteration mode.

Structs and enums are value-semantic and are not automatically `Copy`. `Copy`
is implicit, bounded, and has no hidden allocation or data-dependent graph
traversal; fixed fieldwise traversal of `Copy` fields is allowed. `Duplicable` is
an explicit `copy value` contract that may allocate or traverse and promises
logical independence. In graph terms, `Copy` is always shallow: it never clones
the reachable object graph. A statically bounded traversal of inline `Copy`
fields does not make it a deep copy. An `object` is a singular identity/owner and cannot satisfy `Copy`; sharing uses a
`shared` handle, while `Duplicable` must create a valid new identity. Shared and
weak handles are move-first; `copy handle` explicitly retains the same identity
and never clones its payload, and aggregates containing those owners do not gain
implicit `Copy`. A type may declare a first-party COW strategy for `Duplicable`,
but COW is not a universal String or Array baseline and must document allocator,
budget, failure, cleanup, and cross-domain costs. On a computed property, the
surface spelling is `set(value)`; a behavior body may write `mut set(value)` to
mark mutation of its backing storage.

## Callable values and captures

<!-- w-example role=executable use=CaptureBox,captures observable=value -->
```w
object CaptureBox {
  let value: String
}

fn captures(
  _ copied: String,
  _ borrowed: ref String,
  _ moved: take String,
  _ sharedValue: shared CaptureBox,
): (String, String, String, String?, usize, String) {
  let copyClosure: some fn(): String = <[copy copied]>() => copy copied
  let refClosure: some fn(): String = <[ref borrowed]>() => copy borrowed
  let takeClosure: some take fn(): String = <[take moved]>() => take moved
  let weakClosure = <[weak sharedValue]>() => if let owner = sharedValue {
    .some(copy owner.value)
  } else {
    .none
  }
  var next: usize = 0
  var sequence: some mut fn(): usize = <[take next]>() => {
    next += 1
    return next
  }
  let erased: any fn(String): String =
    <[copy copied]>(value) => (take value) + copied

  let copiedResult = copyClosure()
  let borrowedResult = refClosure()
  let erasedResult = erased("erased:")
  expect copyClosure() == copiedResult
  expect refClosure() == borrowedResult
  expect erased("erased:") == erasedResult

  return (
    copiedResult,
    borrowedResult,
    (take takeClosure)(),
    weakClosure(),
    sequence(),
    erasedResult,
  )
}

test "capture lists preserve ownership modes" for captures {
  let borrowed = "borrowed"
  let moved = "moved"
  let box: shared CaptureBox = CaptureBox(value: "shared")
  expect captures("copied", ref borrowed, take moved, box)
    == ("copied", "borrowed", "moved", .some("shared"), 1, "erased:copied")
}
```

## Control flow and patterns

<!-- w-example role=executable use=Signal,classify,accumulate,nextAvailableSeats,adjustedSeats,branchAdjustedSeats,multiBranchAdjustedState,availability,sign observable=value -->
```w
enum Signal {
  quiet
  alert(level: u8)
}

fn classify(_ signal: Signal): String {
  return switch signal {
    case .quiet: "quiet"
    case .alert(let level) if level > 0: "alert"
    case .alert(_): "silent-alert"
  }
}

fn accumulate(_ rows: Array<Array<i32>>): i32 {
  var total = 0
  matrixRows: for row in rows {
    for value in row {
      if value < 0 { continue matrixRows }
      if value > 50 { break matrixRows }
      total += value
    }
  }

  var attempts = 0
  while attempts < 2 { attempts += 1 }
  repeat { total += 1 } while total < 4

  capped: {
    if total <= 10 { break capped }
    total = 10
  }

  return if total > 0 { total } else { 0 }
}

fn nextAvailableSeats(): i64 {
  var seats = 5
  seats = seats + 1
  return seats
}

fn adjustedSeats(isOpen: Bool): i64 {
  var seats = 5
  let selected = if isOpen { seats + 1 } else { seats - 1 }
  seats = selected
  return seats
}

fn availability(requested: Bool): Bool {
  var open = false
  open = requested
  return open
}

fn sign(value: i64): i64 {
  if value < 0 { return -1 }
  if value == 0 { return 0 }
  return 1
}

fn branchAdjustedSeats(isOpen: Bool): i64 {
  var seats = 5
  if isOpen { seats = seats + 1 }
  else { seats = seats - 1 }
  return seats
}

fn multiBranchAdjustedState(isOpen: Bool): i64 {
  var seats = 5
  var tables = 2
  if isOpen {
    tables = tables + 10
    seats = seats + 1
  } else {
    seats = seats - 1
    tables = tables - 10
  }
  return seats + tables
}

test "control flow returns an observable value" for accumulate {
  let signal: Signal = .alert(level: 1)
  expect classify(signal) == "alert"
  expect accumulate([[1, 2], [-1, 100], [3]]) == 6
  expect nextAvailableSeats() == 6
  expect adjustedSeats(isOpen: true) == 6
  expect adjustedSeats(isOpen: false) == 4
  expect branchAdjustedSeats(isOpen: true) == 6
  expect branchAdjustedSeats(isOpen: false) == 4
  expect multiBranchAdjustedState(isOpen: true) == 18
  expect multiBranchAdjustedState(isOpen: false) == -4
  expect availability(requested: true)
  expect !availability(requested: false)
  expect sign(value: -5) == -1
  expect sign(value: 0) == 0
  expect sign(value: 7) == 1
}
```

## Errors and cleanup

<!-- w-example role=executable use=ParseError,positive,parseAndClose,asyncCleanup,immediate observable=value -->
```w
enum ParseError: Error {
  negative
}

fn positive(_ value: i32): i32 throws ParseError {
  guard value >= 0 else { throw .negative }
  return value
}

fn parseAndClose(_ value: i32, closed: inout Bool): i32 {
  defer { closed = true }
  do {
    return try positive(value)
  } catch .negative {
    return 0
  }
}

async fn asyncCleanup(_ value: i32): i32 {
  defer async { await execution#yield() }
  return value
}

async fn immediate(_ value: i32): i32 { return value }

test "do/catch handles typed errors and defer closes" for parseAndClose {
  let error: ParseError = .negative
  var closed = false
  expect error == .negative
  expect parseAndClose(-1, closed: inout closed) == 0
  expect closed
  expect (try? positive(-1)) == .none
  expect sync immediate(7) == 7
  expect await asyncCleanup(42) == 42
}
```

`sync asyncCleanup(42)` is rejected: its asynchronous cleanup means that the
function has no proven direct entry. `sync` never blocks and never drives a
task to completion.

## Allocator scopes

These fixed scopes assume statically proven, infallible admission in the selected
profile. Dynamic admission requires `try allocator`.

<!-- w-example role=executable use=stage,prepare,edit observable=value -->
```w
fn stage(city: ref String, allocator destination: ref Allocator): String {
  var staged = String(allocator: destination)
  staged.append(city)
  return staged
}

fn edit(value: inout String) { value.append("!") }

fn prepare(city: ref String): (String, usize) {
  var result = copy city
  edit(value: inout result)
  var bytes: usize = 0

  allocator scratch: .fixed<capacity: 256> {
    var staged = stage(city: city, allocator: ref scratch)
    edit(value: inout staged)
    bytes = staged.bytes.count
  }

  allocator .fixed<capacity: 128> {
    let staged = stage(city: city) // The contextual slot uses this allocator.
    expect staged == city
  }

  return (result, bytes)
}

test "allocator scopes bound temporary work" for prepare {
  let city = "city"
  expect prepare(city: ref city) == ("city!", 5)
  expect city == "city"
}
```

Each `staged` owner ends inside its allocator scope. Only the byte count leaves
the scratch scope. The returned `result` was created outside both scopes.

## Unsafe, addresses, and bit operations

<!-- w-example role=logical-contract -->
```w
unsafe fn clearTag<T>(_ pointer: c.ptr<T>, tagMask: Address.Bits): c.ptr<T> {
  let location = pointer.address
  let aligned = location.withBits(location.bits & ~tagMask)
  return unsafe { pointer.withAddress(aligned) }
}

unsafe fn useAlignedByte(_ pointer: c.ptr<c.uchar>): Address.Bits {
  let aligned = unsafe { clearTag(pointer, tagMask: 0x0003) }
  return aligned.address.bits
}
```

`Address` exposes address bits but cannot be fabricated back into a pointer.
`withAddress` retains the original pointer provenance and is valid only when
the caller proves non-null, lifetime, bounds, alignment and access. Normal
profiles add no hidden check; sanitizer profiles may trap a violated `unsafe`
precondition. An unannotated nullable foreign pointer enters W as
`c.ptr<T>?`, never as a safe `ref T`.

## Async, spawn, sync, and await

<!-- w-example role=executable use=FetchError,fetch,ordinary,load observable=value -->
```w
module async_examples<
  domains: [.concurrent(.domain, maximum: 4, capabilities: [.parallel])],
>

enum FetchError: Error { unavailable }

async fn fetch(_ city: String): String throws FetchError {
  return city
}

fn ordinary(): String { return "local" }

async fn load(): String throws FetchError {
  let direct = try sync fetch("north")
  let concurrent = async fetch("east")
  let parallel = spawn<.domain> fetch("south")
  let local = async ordinary()
  let (east, south) = try await (concurrent, parallel)

  return direct
    + east
    + south
    + (await local)
}

test "launchers join through the lexical parent" for load {
  let error: FetchError = .unavailable
  expect error == .unavailable
  expect try await load() == "northeastsouthlocal"
}
```

Execution domains are static admission and placement requirements. The common
host forms are declared once and selected at `spawn`; concurrent pipelines can
inherit the current domain, while parallel pipelines name one explicitly:

<!-- w-example role=logical-contract -->
```w
module domain_examples<
  domains: [
    .serial(.render),
    .concurrent(.physics, maximum: 8, capabilities: [.parallel]),
  ],
>

fn updateFrame(frame: u64): u64 { return frame + 1 }

async fn schedule(frame: u64): u64 {
  let render = spawn<.render> updateFrame(frame: frame)
  let physics = spawn<.physics> updateFrame(frame: frame)
  let (nextFrame, nextPhysics) = await (render, physics)
  return nextFrame + nextPhysics
}

test "domains select admission at the call site" for schedule {
  expect await schedule(frame: 20) == 42
}
```

Names such as `.render`, `.physics`, `.inference`, `.thermal`, and
`.blockingInterop` are product identities, not built-in hardware kinds. W
provides `.main`; module contracts declare `.serial`, `.concurrent`, or
`.accelerated` requirements. I/O, channels, transactions, locks, tests, and
compiler proof phases do not acquire implicit domains.

## Tasks and cancellation

<!-- w-example role=logical-contract -->
```w
module task_examples<
  domains: [.concurrent(.domain, maximum: 4, capabilities: [.parallel])],
>

enum WorkError: Error { failed }

alias TextTask = Task<String, WorkError>
alias TextOutcome = TaskOutcome<String, WorkError>
alias TextSettlement = TaskSettlement<String, WorkError>

async fn work(_ value: String): String throws WorkError { return value }

struct Trace {
  const requestId = TaskLocal<String?>.key(default: .none)
}

async fn traced(_ value: String): String throws WorkError {
  return try await Trace.requestId.withValue(
    .some("request-42"),
    operation: () => try await work(value + Trace.requestId.get()?),
  )
}

async fn cancelAndObserve(): TextOutcome {
  let child: TextTask = async work("cancelable")
  child#cancel(reason: .shutdown)
  return await (take child)#outcome()
}

async fn timed(_ value: String, timeout: TaskTimeout): TextOutcome {
  return await Task.withTimeout(
    for: timeout,
    input: value,
    using: work,
  )
}

async fn first(): TextSettlement {
  let primary: TextTask = async work("primary")
  let fallback: TextTask = spawn<.domain> work("fallback")
  let candidates: [TextTask; 2] = [primary, fallback]
  return await (take candidates).firstSettled()
}

test "firstSettled preserves index and outcome" for first {
  let error: WorkError = .failed
  let settlement = await first()
  let tracedValue = try await traced("value:")
  let canceled = await cancelAndObserve()
  let timedValue = await timed("bounded", timeout: 250<si.ms>)
  expect error == .failed
  expect tracedValue == "value:request-42"
  expect settlement.index < 2
  expect switch settlement.outcome {
    case .success(let value): value == "primary" || value == "fallback"
    case .error(_): false
    case .canceled(_): false
  }
  expect switch canceled {
    case .success(_): true
    case .error(_): false
    case .canceled(_): true
  }
  expect switch timedValue {
    case .success(let value): value == "bounded"
    case .error(_): false
    case .canceled(_): true
  }
}
```

`Task` remains a linear capability. A complete proof may erase its transient
representation and finite root yield markers. W-1582 extends this proof to a
finite acyclic same-module graph of ordinary pure scalar helpers. The helpers
may use verified scalar control and local mutation, but they cannot introduce
effects, suspension, allocation, or runtime ownership. This is representation
evidence and does not claim scheduler behavior or concurrency.

COOP0 is separate compiler-host oracle evidence. It keeps exactly two sibling
scalar async tasks, one or two yields per task, fixed caller-owned frames, and
a deterministic provider/test-profile FIFO trace. The oracle verifies
reserve/publish, program counters, queue transitions, lifecycle, frames, and
outcomes. It does not emit a NativeSubset0 or MLIR0 state machine or provide a
product runtime, scheduler provider, threads, parallelism, cancellation, I/O,
general Task behavior, benchmark, or performance claim. Normal W-1582 elision
remains unchanged.

## Bounded task pipelines

<!-- w-example role=executable use=JobError,process,processAll,collectAll observable=value -->
```w
module pipeline_examples<
  domains: [.concurrent(.domain, maximum: 4, capabilities: [.parallel])],
>

enum JobError: Error { failed }

async fn process(_ value: i32): i32 throws JobError { return value * 2 }

async fn processAll(_ values: take Array<i32>): Array<i32> throws JobError {
  return try await pipeline<
    tasks: .parallel<.domain>,
    limit: 4,
    ordering: .input,
    errors: .failFast,
  > each value in take values {
    commit try process(value)
  }
}

async fn collectAll(
  _ values: take Array<i32>,
): Array<TaskSettlement<i32, JobError>> throws JobError {
  return try await pipeline<
    tasks: .concurrent,
    limit: 4,
    ordering: .completion,
    errors: .collect,
  > each value in take values {
    commit try process(value)
  }
}

test "bounded task pipeline preserves input order" for processAll {
  let error: JobError = .failed
  let settlements = try await collectAll([1, 2, 3])
  expect error == .failed
  expect try await processAll([1, 2, 3]) == [2, 4, 6]
  expect settlements.count == 3
}
```

## Service and transaction pipelines

The service graphs require linked bindings. `checkLinkedOven` assumes
`acquire(temperature: 220)` returns a lease whose `preheat()` result is `220`.
The transaction example requires a `StoreApi` provider with serializable,
read-only transactions. `checkSeededStore` assumes that provider contains
`"north"`. The parameterized examples do not implement their providers.

<!-- w-example role=logical-contract -->
```w
import { Isolation, TransactionAccess } from std.database
import { TransactionFailure, Transactional } from std.runtime.transaction

protocol OvenApi {
  fn requestedTemperature(): u16
  fn preheat(temperature: u16): u16
}

service ovens<key: String>: OvenApi {
  fn requestedTemperature(): u16 { return 220 }
  fn preheat(temperature: u16): u16 { return temperature }
}

async fn prepare(): u16 throws ServiceFailure {
  let oven = ovens.at("primary")
  return try await pipeline {
    let temperature = oven.requestedTemperature()
    let ready = oven.preheat(temperature: temperature)
    commit ready
  }
}

test "dependent service calls commit the terminal value" for prepare {
  expect try await prepare() == 220
}

protocol OvenLeaseApi {
  fn preheat(): u16
}

protocol LeasedOvenApi {
  fn acquire(temperature: u16): ServiceRef<OvenLeaseApi>
}

async fn prepareViaLease(
  oven: ref ServiceRef<LeasedOvenApi>,
): u16 throws ServiceFailure {
  return try await pipeline oven.acquire(temperature: 220).preheat()
}

async fn checkLinkedOven(
  oven: ref ServiceRef<LeasedOvenApi>,
): () throws ServiceFailure {
  expect try await prepareViaLease(oven: oven) == 220
}

enum StoreError: Error {
  unavailable
  service(ServiceFailure)
}

struct StoreContract {
  let isolation: Isolation
  let access: TransactionAccess
}

protocol StoreTransaction {
  async fn read(): String throws StoreError
}

protocol StoreApi: Transactional<StoreTransaction, StoreContract, StoreError> {}

async fn readStoredValue(
  store: ref ServiceRef<StoreApi>,
): String throws TransactionFailure<StoreError, StoreError> {
  return try await pipeline<transaction: {
    isolation: .serializable,
    access: .readOnly,
  }> tx = store {
    let value = try await tx.read()
    commit value
  }
}

async fn checkSeededStore(
  store: ref ServiceRef<StoreApi>,
): () throws TransactionFailure<StoreError, StoreError> {
  let value = try await readStoredValue(store: store)
  expect value == "north"
}
```

The product chooses whether the same service contract is linked locally, as a
component, through IPC, or through the network:

<!-- w-example role=logical-contract -->
```w
// excerpt-kind: manifest-fragment
products: [
  {
    name: "kitchen"
    modules: ["kitchen"]
    servicePolicy: {
      resolution: .startup
      links: [
        .local,
        .component,
        .wrpc(transports: [.ipc, .network]),
      ]
      dynamicRebinding: .deny
    }
    services: [
      {
        binding: "ovens"
        declaration: "kitchen::ovens"
        scope: .process
        mailbox: { items: 64, bytes: 8MiB, inFlight: 1 }
      },
    ]
  },
]
```

## Streams and channels

<!-- w-example role=executable use=RelayError,words,project,relay observable=value -->
```w
enum RelayError<Failure: Error>: Error {
  source(Failure)
  send(ChannelSendError<String><[.closed]>)
}

fn words(_ values: take Array<String>): some Stream<String, Never> {
  return stream <[take values]> {
    for value in take values { yield take value }
  }
}

fn project<Failure: Error>(
  _ source: take some Stream<String, Failure>,
): some Stream<String, Failure> {
  return stream <[take source]> {
    for try await item in take source {
      yield take item
    }
  }
}

async fn relay<Failure: Error>(
  _ source: take some Stream<String, Failure>,
  _ sender: take Channel<send: String>,
): usize throws RelayError<Failure> {
  var cursor = take source
  var count: usize = 0
  while true {
    var next: String? = .none
    do {
      next = try await cursor.next()
    } catch failure {
      throw .source(take failure)
    }
    // Returning ends this sender owner and closes admission if it is last.
    guard let item = take next else { return count }
    do {
      try await sender.send(value: take item)
    } catch failure {
      throw .send(take failure)
    }
    count += 1
  }
}

test "stream projection remains lazy" for project {
  let source = words(["north", "south"])
  var projected = project(take source)
  expect await projected.next() == .some("north")
  expect await projected.next() == .some("south")
  expect await projected.next() == .none
}

test "relay drains after the last sender ends" for relay {
  let (sender, receiver) = Channel<String>.open(capacity: 2)
  let relayed = async relay(words(["east"]), take sender)
  expect try await relayed == 1
  expect await receiver.receive() == .some("east")
  expect await receiver.receive() == .none
}

test "relay returns the item after receiver abort" for relay {
  let (sender, receiver) = Channel<String>.open(capacity: 1)
  let _ = take receiver // Destroying the receiver aborts the channel.
  do {
    let _ = try await relay(words(["held"]), take sender)
    panic("an aborted receiver accepted an item")
  } catch failure {
    let error: RelayError<Never> = take failure
    switch error {
      case .send(.closed(let returnedItem)): expect returnedItem == "held"
      case .source(_): panic("a nonthrowing source failed")
    }
  }
}
```

`RelayError` preserves the rejected payload in its `send` case. Source failures
use a separate case. Cancellation remains a control outcome, not an error case.

A receive endpoint is a mutable cursor. Rebind it as `var` when the scope must
receive, close, and receive again. The `mut` receiver makes overlapping cursor
calls visible to the compiler.

<!-- w-example role=executable use=Channel,ChannelPermit observable=value -->
```w
test "graceful close preserves an issued permit" {
  let (sender, initialReceiver) = Channel<String>.open(capacity: 1)
  var receiver = take initialReceiver
  let permit = try await sender.reserve()
  receiver.close()
  do {
    try (take permit).send(value: "accepted")
  } catch .closed(_) {
    panic("graceful close revoked an issued permit")
  }
  expect await receiver.receive() == .some("accepted")
  expect await receiver.receive() == .none
}
```

<!-- w-example role=executable use=Channel,Task,ChannelPermit observable=value -->
```w
test "canceling a rendezvous returns the permit owner" {
  let (sender, initialReceiver) = Channel<String>.open(capacity: 0)
  var receiver = take initialReceiver
  let pending = async receiver.receive()
  let permit = try await sender.reserve()
  pending#cancel(reason: .userRequest)
  switch await (take pending)#outcome() {
    case .canceled(_): ()
    case .success(_): panic("canceled receive produced an item")
    case .error(_): panic("nonthrowing receive produced an error")
  }
  do {
    try (take permit).send(value: "returned")
    panic("revoked rendezvous permit accepted an item")
  } catch .closed(let returned) {
    expect returned == "returned"
  }
  receiver.close()
  expect await receiver.receive() == .none
}
```

The first example keeps an issued permit through graceful close. The second
revokes only the paired rendezvous permit after receive cancellation. The
channel remains usable until the explicit close.

## Shared state, atomics, and locks

<!-- w-example role=executable use=Ledger,Published,publish observable=value -->
```w
object Ledger {
  var atomic count: usize = 0
  var message: String = ""
}

struct Published: Duplicable {
  let revision: u64
  let value: String
}

fn publish(_ ledger: shared Ledger, _ message: String): (usize, u64) {
  ledger.count.saturatingAdd<.relaxed>(1)
  lock ledger as exclusive {
    exclusive.message = message
  }
  let snapshots = SnapshotCell(Published(revision: 1, value: message))
  snapshots.publish(Published(revision: 2, value: message))
  let revision = snapshots.read((value: ref Published) => value.revision)
  return (ledger.count.load<.acquire>(), revision)
}

test "atomic and lock operations expose their ordering" for Ledger {
  let ledger: shared Ledger = Ledger()
  expect publish(ledger, "stored") == (1, 2)
  expect ledger.message == "stored"
}
```

## Units, matrices, tensors, and SIMD

<!-- w-example role=logical-contract -->
```w
module numerics<
  domains: [
    .accelerated(
      .inference,
      submission: .concurrent,
      maximum: 4,
      fallback: .reject,
    ),
  ],
  kernels: {
    forecast: forecastKernel,
  },
>

import accelerator from std
import tensor from std
import { Limits as TensorLimits, Queue, Tensor } from std.tensor

dimension Distance
unit kilometer: Distance

type FeatureBatch<rows: usize, columns: usize> =
  Tensor<f32, shape: [rows, columns]>

fn forecastKernel<rows: usize, inputs: usize, outputs: usize>(
  features: ref FeatureBatch<rows: rows, columns: inputs>,
  weights: ref Tensor<f32, shape: [inputs, outputs]>,
): FeatureBatch<rows: rows, columns: outputs> {
  return features @ weights
}

async fn forecastOnConfiguredDomain<
  rows: usize,
  inputs: usize,
  outputs: usize,
>(
  features: ref FeatureBatch<rows: rows, columns: inputs>,
  weights: ref Tensor<f32, shape: [inputs, outputs]>,
): FeatureBatch<rows: rows, columns: outputs> throws accelerator.LaunchError {
  let pending = spawn<.inference> forecast(
    features: ref features,
    weights: ref weights,
  )
  return try await pending
}

async fn prepareForConfiguredDomain<
  rows: usize,
  inputs: usize,
  outputs: usize,
>(
  features: take FeatureBatch<rows: rows, columns: inputs>,
  weights: take Tensor<f32, shape: [inputs, outputs]>,
  limits: ref TensorLimits,
): (
  FeatureBatch<rows: rows, columns: inputs>,
  Tensor<f32, shape: [inputs, outputs]>,
) throws tensor.TensorError {
  let deviceFeatures = try await tensor.transfer<to: .inference>(
    source: take features,
    limits: ref limits,
  )
  let deviceWeights = try await tensor.transfer<to: .inference>(
    source: take weights,
    limits: ref limits,
  )
  return (deviceFeatures, deviceWeights)
}

fn numericSummary(): (Quantity<Distance>, i32, i32, [usize; 2], i32, i32, i32) {
  let distance = 12<kilometer>
  let matrix = [[1, 2], [3, 4]]
  let vector = Simd<i32, lanes: 4>([1, 2, 3, 4])
  let doubled = vector + vector
  let sum = vector.wrappingReduceAdd()
  let product = vector.saturatingReduceMultiply()
  let xor = vector.reduceBitXor()
  let features: FeatureBatch<rows: 1, columns: 2> = [[1.0, 2.0]]
  let weights: Tensor<f32, shape: [2, 1]> = [[1.0], [0.5]]
  let result = forecastKernel(features: features, weights: weights)
  return (distance, matrix[1][0], doubled[3], result.shape, sum, product, xor)
}

test "numeric types preserve dimensions and lanes" for numericSummary {
  let result = numericSummary()
  expect result.0 == 12<kilometer>
  expect result.1 == 3
  expect result.2 == 8
  expect result.3 == [1, 1]
  expect result.4 == 10
  expect result.5 == 24
  expect result.6 == 4
}
```

O caminho estático comum importa labels de kernel nominalmente. A identidade
continua sendo a do módulo de origem; `predict` é apenas o nome local:

<!-- w-example role=logical-contract -->
```w
module numerics_batch<
  domains: [
    .accelerated(
      .inference,
      submission: .serial,
      maximum: 2,
      fallback: .reject,
    ),
  ],
>

import kernel { forecast as predict } from numerics
import accelerator from std
import { FeatureBatch, Tensor } from numerics

async fn predictBatch<rows: usize, inputs: usize, outputs: usize>(
  features: ref FeatureBatch<rows: rows, columns: inputs>,
  weights: ref Tensor<f32, shape: [inputs, outputs]>,
): FeatureBatch<rows: rows, columns: outputs> throws accelerator.LaunchError {
  let pending = spawn<.inference> predict(
    features: ref features,
    weights: ref weights,
  )
  return try await pending
}
```

O caminho dinâmico avançado usa a projeção qualificada e mantém o import
ordinário da função separado para a chamada host:

<!-- w-example role=logical-contract -->
```w
module numerics_client

import accelerator from std
import { FeatureBatch, Tensor, forecastKernel } from numerics
import { Queue } from std.tensor
import kernel numerics as kernels

async fn forecastOnSelectedQueue<
  rows: usize,
  inputs: usize,
  outputs: usize,
>(
  features: ref FeatureBatch<rows: rows, columns: inputs>,
  weights: ref Tensor<f32, shape: [inputs, outputs]>,
  queue: ref Queue,
  limits: ref accelerator.Limits,
): FeatureBatch<rows: rows, columns: outputs> throws accelerator.LaunchError {
  var launch = try await accelerator.open<module: kernels>(
    on: ref queue,
    limits: ref limits,
  )
  defer async { let _ = try? await (take launch).close() }
  return try await kernels.forecast.launch(
    using: ref launch,
    features: ref features,
    weights: ref weights,
  )
}
```

## Foreign code and ABI

<!-- w-example role=logical-contract -->
```w
foreign c from "stdlib.h" {
  fn abs(_ value: c.int): c.int
}

export foreign c {
  struct w_result { value: c.int }
}

export unsafe fn<abi: .c> w_add(_ left: c.int, _ right: c.int): w_result {
  return w_result(value: left + right)
}

unsafe fn<lang: .c> c_add(_ left: c.int, _ right: c.int): c.int {
  return left + right;
}

unsafe fn callC(_ left: i32, _ right: i32): i32 {
  return abs(c_add(left, right))
}

test "foreign calls remain inside unsafe" for callC {
  expect unsafe { callC(20, 22) } == 42
  expect unsafe { w_add(20, 22).value } == 42
}
```

## Packages and build roots

`build.w` is data, so it stays separate from module source. A single
independently publishable package can omit the local build coordinator only
when selection, resolution, recipe, and deployment are unambiguous. Every
package record has an exact local `root`, excluded from public package
identity. Package-authored build requirements, profiles, and recipes remain
inside the package. Multiple packages require one local-only `build`
coordinator; package record order never changes package recipe identity.

<!-- w-example role=logical-contract -->
```w
// excerpt-kind: manifest-fragment
package {
  schema: "w.package/1"
  root: "."
  name: "last-light/restaurant"
  version: "0.1.0"
  edition: "2026"
  moduleSets: [{ name: "app", root: "src", include: ["*.w"] }]
  products: [{
    name: "last-light-native"
    kind: .executable
    module: "app"
    entry: "LastLightTui"
  }]
  build: {
    profiles: [
      {
        name: "debug"
        optimize: .none
        checks: .full
        cpuPolicy: .portable
      },
      {
        name: "release"
        optimize: .speed
        checks: .safe
        cpuPolicy: .portable
      },
    ]
    recipes: [
      {
        name: "benchmark-x86-v3"
        baseProfile: "release"
        kind: .benchmark
        cpuPolicy: .explicit
        cpu: "x86-64-v3"
        features: ["+avx2", "+fma"]
      },
      {
        name: "compat-x86-v2"
        baseProfile: "release"
        cpuPolicy: .explicit
        cpu: "x86-64-v2"
        features: []
      },
    ]
  }
}
```

There are exactly two W program base profiles: `debug` and `release`, with
`release` as the default. Benchmark is a recipe; `size` is an orthogonal preset;
proof and distribution are assurance/admission policies; sanitizer and PGO are
instrumentation lanes. Debug defaults to no optimization and full checks;
release defaults to speed optimization and safe checks. Release strips its
primary by default for efficient execution. A separate debug sidecar is not
requested by default and can be selected independently for either profile;
distribution requires the stronger audit package and receipt. Required safety
checks apply to both profiles. Build configuration is authored in `build.w`; a
recipe pins CPU/features rather than inferring them from the compiler host.

`size` defaults to `.performance`. The opt-in `.compact` preset overlays its
size-oriented defaults only where individual fields are not set; explicit
`build.w` fields win, and no preset weakens target hardening or runtime closure.

For W 1.0, the selected x86_64 `.portable` design baseline is x86-64-v3.
Older hardware is selected as a separate compatibility recipe/product; it
never weakens the primary artifact. Future ISA levels and exact
microarchitectures remain explicit until measurements justify promotion.

The artifact selection below composes the release base profile with a pinned
benchmark recipe:

```w
// excerpt-kind: manifest-fragment
source: .product(
  "last-light-native",
  target: "x86_64-unknown-linux-gnu",
  profile: "release",
  recipe: "benchmark-x86-v3",
  size: .compact,
  debug: .sidecar,
  packing: "single-process",
)
```

This is `build.w` data, not a compiler CLI spelling. Here a compact size preset
and a separately selected sidecar compose with the release base profile. A
host-tuned benchmark recipe must record its exact target CPU and feature set;
that recipe is local-only and is not a distributable portable target.
`pie` and RELRO are ELF-specific target settings: Windows PE uses its
ASLR/DEP-oriented target hardening contract. Reproducibility is required by the
future product contract, but is not yet implemented or evidenced.

An aggregate build root lists each package directly and uses the coordinator
only for local selection and orchestration:

<!-- w-example role=logical-contract -->
```w
// excerpt-kind: manifest-fragment
package {
  schema: "w.package/1"
  root: "packages/core"
  name: "example/core"
  products: [{ name: "core", kind: .library, module: "core" }]
}

package {
  schema: "w.package/1"
  root: "packages/server"
  name: "example/server"
  products: [{ name: "server", kind: .executable, module: "server" }]
}

build {
  schema: "w.build/1"
  default: { package: "example/server", product: "server" }
  patches: []
  resolution: {
    schema: "w.resolution/1"
    resolver: "w.resolver/1"
    contexts: []
    packages: []
  }
  deployments: []
}
```

There is no `workspace` record or workspace identity. Roots must be exact local
paths beneath the selected `build.w`; globs, escapes, ancestor/cwd scanning,
nested build roots, and configuration cycles are rejected. The coordinator may
not weaken package-authored requirements. Local patches are rejected by
publication, and receipts bind package and resolved build-plan identities
separately.

```text
w context --build build.w       # inspect one explicitly selected build root
w build example/server:server   # exact package/product selection
w build all                     # only when the caller explicitly requests all
w run last-light/restaurant:last-light-native -- --tui
```

The normative contract and implementation status remain in [DESIGN.md](DESIGN.md).
The seed compiler has bounded BOOL0 evidence for the existing `!`, `&&`, and
`||` operators: verified HIR join block arguments and incoming Bool edges lower
to `llvm.xor`, `llvm.cond_br`, and `llvm.br ^join(%operand : i1)`. The compiler
short-circuit fixture passed the Linux/WSL and native Windows gates with exact
stdout; this is compiler-lifecycle correctness evidence, not general CFG or
performance evidence.
W-1539 adds the bounded scalar-if value cut: `if condition { scalar } else {
scalar }` is valid only in scalar `return` and immutable `let` initializer
contexts, with a Bool condition and matching `i64` or Bool arms. HIR11 carries
one typed join argument per diamond; MLIR14/Windows5 emits real
`llvm.cond_br`/typed `llvm.br` CFG and never `llvm.select`. The compiler
fixture passed both conditions with exact `Open 5; closed 2\n` on the public
Windows route. W-390 keeps runtime `+/-` checked and outside this witness;
missing else/non-Bool/mismatch use `W-PARSE-0021`/`W-SEM-0001`/`W-TYPE-0120`,
and unsupported effectful/aggregate forms remain rejected. This is
compiler-lifecycle correctness evidence only; no general CFG, target or
performance claim follows.

<!-- w-example role=logical-contract -->
```w
fn choose(outer: Bool, inner: Bool, open: i64, middle: i64, closed: i64): i64 {
  return if outer { if inner { open } else { middle } } else { closed }
}
```

W-1540 adds ARITH0 for checked signed-`i64` runtime `+`, `-`, and `*` through
the verified HIR route, currently HIR15, and MLIR15/Windows6. LLVM
signed-overflow intrinsics and a trap boundary
terminate overflowed processes nonzero before later success output. Helpers are
reachability-only. Constant overflow and faulting constant `/` or `%` fail
closed, while safe constant forms emit `llvm.sdiv`/`llvm.srem`; dynamic/runtime
forms remain outside the cut. The short-entry compiler fixture produces
`Open 6; closed 1\n` on the Linux/WSL LLVM 23.1.1 route only. There is no native Windows,
`PanicEvent`, runtime payload, cleanup, timing, or benchmark result claim.
Unary negation, power, other widths, named numeric APIs, and general panic
runtime remain gaps. W-1541 implements the bounded `entry {}` path introduced
through frontend15 and HIR13; the current HIR schema is HIR15. The canonical
Hello fixture executes through public
Linux/WSL `w run`; `entry(functionName)` remains valid. This is
compiler-lifecycle correctness-only evidence.
