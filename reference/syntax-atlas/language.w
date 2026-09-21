// atlas:begin source-roots-imports
module atlas_language<
  domains: [.serial],
  kernels: { forecast: forecastKernel },
  contracts: [apiVersion(current: apiMajor, minimum: 1)],
>

import std.text
import text from std
import * from atlas.prelude
import { normalize as normalizeText } from std.text
import kernel { forecast as predict } from atlas.models
import kernel atlas.models as models
export * from atlas.foundation
export { FoundationPlace as BasePlace } from atlas.foundation
import domain { District } from atlas.domain
import service { RemoteCatalog<key: String> } from atlas.catalog
import service atlas.catalog as catalog
// atlas:end source-roots-imports

// atlas:begin data-declarations
export struct Place<ID> : Hashable {
  let id: ID
  var label: String = "square"
  var storedCount: usize = 0

  let summary: usize { get => storedCount }
  let labelView: ref String { get => label }

  var replaceable: usize {
    get => storedCount
    set(value) { storedCount = value }
  }

  var replaceableView: ref String {
    get => label
    set(value) { label = value }
  }

  init(id: ID, label: String) {
    self.id = id
    self.label = label
  }

  var title: mut ref String {
    get => label
    set(value) { label = value }
  }

  var buffered: inout usize {
    get => storedCount
    set(value) { storedCount = value }
  }

  fn describe(): String {
    return label
  }

  deinit {
    print("released ${label}")
  }
}

object Ward {
  var name: String
  fn rename(_ value: String) {
    name = value
  }
}

/// A directory keeps one reusable static relation.
/// contract: apiVersion(current: apiMajor, minimum: 1)
protocol Directory<Key> {
  type Value: Hashable
  const empty: Bool
  const apiMajor: u16
  fn lookup(_ key: Key): Value;
  fn isEmpty(): Bool
  var count: usize { get set }
}

extension Directory {
  fn isEmpty(): Bool {
    return count == 0
  }
}

service Catalog<key: String>: Directory {
  alias Value = String
  const empty: Bool = false
  const apiMajor: u16 = 1
  fn lookup(_ key: String): String {
    return key
  }
  var count: usize = 0

  fn find(_ key: String): String {
    return key
  }
}

enum Signal: Error {
  quiet;
  alert(level: u8);
}

type PlaceId = String
alias MaybePlace = Place<String>?
dimension Distance
unit kilometer: Distance

extension Place {
  fn cityBlock(): String {
    return label
  }
}

behavior Initialized for Place {
  var current: Place
  init(initialValue: fn(): Place) {
    current = initialValue()
  }
  get {
    return current
  }
  set(proposed: Place) {
    current = proposed
  }
}

behavior Versioned<Value> for Value {
  var epoch: u64
  var reads: u64

  init() {
    epoch = 0
    reads = 0
  }
  export let mutationEpoch: u64 { get => epoch }
  export let readCount: u64 { get => reads }
  mut willGet(kind: PropertyAccessKind) { reads += 1 }
  mut didSet(current: ref Value) { epoch += 1 }
}

// A named composition remains useful for multiple behaviors and aliases.
behavior VersionedPlace for Place<String> =
  (value: Initialized, version: Versioned)

struct VersionedPlaceBox {
  var VersionedPlace place: Place<String> = Place(id: "north", label: "square")
  var Versioned visits: u16 = 0
}

const DefaultLabel: String = "square"
test "place label" for Place {
  let place = Place(id: "north", label: DefaultLabel)
  expect place.describe() == DefaultLabel
}

test "qualified facet path" for VersionedPlaceBox {
  var box = VersionedPlaceBox()
  box.place.title = "avenue"
  let epoch = box.place#version.mutationEpoch
  let title = (box.place#value).title
  expect epoch == 1
  expect title == "avenue"
}

test "direct observer facets" for VersionedPlaceBox {
  var box = VersionedPlaceBox()
  let visits = box.visits
  expect visits == 0
  expect box.visits#readCount == 1
  expect box.visits#mutationEpoch == 0
}

struct AtlasMarker {}
export { AtlasMarker, VersionedPlaceBox }
// atlas:end data-declarations

// atlas:begin callables-and-foreign
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

/// Sorts values without changing their element count.
/// contract: <(result.count == values.count)>
/// contract: sameCount(input: values, output: result)
fn sortValues(values: take Array<i64>): SortedResult<i64> {
  return values
}

/// Applies one total balance transition.
/// contract: balanceTransition(input: before balance, amount: amount, output: after balance)
fn deposit(balance: inout i64, amount: i64) {
  balance = balance.saturatingAdd(amount)
}

export fn describe<ID, _ limit: usize>(_ value: ID, each labels: String...): String throws Signal {
  return labels[0]
}

export static const fn makePlace<ID>(_ value: ID): Place<ID> {
  return Place(id: value, label: "center")
}

fn chooseLabel(
  primary: ref String,
  fallback: ref String,
): ref String borrows(0: [primary, fallback]) {
  return if primary.bytes.count > 0 { primary } else { fallback }
}

export mut fn rename(_ place: Place<String>, _ value: String) {
  place.title = value
}

fn labelShapes(
  _ order: String,
  audit: String,
  externalAudit audit: String,
  to destination: String,
  _ title: String = "city",
  each tags: String...,
): String {
  return order
}

type Handler = any mut async fn(inout String, take Place<String>): String throws Signal
type ForeignHandler = unsafe fn<abi: .c>(c.ptr<c.char>, c.int): c.int

foreign c from "atlas.h" {
  type AtlasHandle
  struct AtlasPoint {
    north: c.int
    east: c.int
  }
  fn<abi: .c> atlas_hash(data: c.ptr<c.char>): c.int;
}

export unsafe fn<abi: .c> atlas_version(): c.int {
  return 1
}

unsafe fn<lang: .c> c_hash(data: c.ptr<c.char>): c.int {
}
// atlas:end callables-and-foreign

// atlas:begin types-and-contracts
struct Matrix<Element, rows: usize, columns: usize> {
  let cells: [Element; rows]
}

type Tile = Matrix<u8, rows: 4, columns: 4>
type SmallText = Array<u8><(.count <= 64)>
type SortedResult<Element: Comparable> = Array<Element><(isSorted(value))>
type Allowed = Signal<[.quiet, .alert]>
type Settings = Config<{mode: .strict, retries: 2}>
type Location = (district: String, number: u16)
type Digest = [u8; 32]
type Callback = some fn(String): String
type SharedPlace = shared Place<String>
type WeakPlace = weak Place<String>
type ViewPlace = view Place<String>
type TrainingActivation = f8<.e4m3fn>
type TrainingGradient = f8<.e5m2>
type PackedWeight = f4<.e2m1fn>
type AuditFloat = BigFloat<precision: 256>
type RuntimeFloat = BigFloat<precision: .dynamic>

fn retainActivation(value: TrainingActivation): TrainingActivation {
  return value
}

fn makeDigest(): Digest {
  let location: Location = (district: "north", number: 4)
  let bytes: Digest = [0; 32]
  let value = if location.number > 0 { bytes } else { bytes }
  let first = location.0
  let empty = ()
  let maker = makePlace<String>
  let made = maker("east")
  let _ = (first, empty, made)
  return value
}
// atlas:end types-and-contracts

// atlas:begin patterns
fn classify(_ signal: Signal): String {
  let Signal.alert(level: let level) = signal
  return switch signal {
    case .quiet: "quiet"
    case .alert(let level) if level > 0: "alert"
    case .alert(let level): "alert"
  }
}

fn unpack(_ place: Place<String>): String {
  let Place(id, ...) = place
  let { id: inferredId, ... } = place
  let (id, label) = (id, "center")
  return inferredId + label
}

fn classifyRange(_ value: i32): String {
  return switch value {
    case 0...3: "low"
    case 4..<8: "mid"
    case _: "high"
  }
}
// atlas:end patterns

// atlas:begin literals-and-collections
fn values(): () {
  let count = 1_000
  let ratio = 0.5e2
  let distance = 9.81<m/s^2>
  let speed = 12km
  let bytes = 64KiB
  let text = "city ${count}"
  let single = 'city ${count}'
  let raw = #"raw city ${count}"#
  let rawSingle = #'raw city ${count}'#
  let multiline = """north
south"""
  let rawMultiline = #"""north ${count}
south"""#
  let scalar = 'N'
  let byte = b'\x4e'
  let enabled = true
  let point = (north: 1, east: 2)
  let list = [1, 2, 3]
  let map = ["north": 1]
  let repeated = [0; 4]
  let selected = (point).north
  let _ = (
    count, ratio, distance, speed, bytes, text, single, raw, rawSingle,
    multiline, rawMultiline, scalar, byte, enabled, list, map, repeated,
    selected,
  )
}
// atlas:end literals-and-collections

// atlas:begin entry-declaration
fn runAtlas() {
  values()
}

entry Atlas(runAtlas)

entry Diagnostics {
  print("atlas diagnostics ready")
}
// atlas:end entry-declaration
