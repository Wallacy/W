// atlas:begin source-roots-imports
module atlas_language<
  domains: [.serial],
>

import std.text
import { String as Text } from std.text
export * from atlas.foundation
export { FoundationPlace as BasePlace } from atlas.foundation
import domain { District } from atlas.domain
import service { RemoteCatalog<key: String> } from atlas.catalog
import service atlas.catalog as catalog
// atlas:end source-roots-imports

// atlas:begin data-declarations
export struct Place<ID> : Hashable {
  id: ID
  var label: String = "square"

  init(id: ID, label: String) {
    self.id = id
    self.label = label
  }

  var title: String {
    get => label
    set(value) { label = value }
    modify { label }
  }

  fn describe(): String {
    return label
  }

  deinit {
    label
  }
}

object Ward {
  var name: String
  fn rename(value: String) {
    name = value
  }
}

protocol Directory<Key> {
  type Value: Hashable
  const empty: Bool
  fn lookup(key: Key): Value;
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
  fn lookup(key: String): String {
    return key
  }
  var count: usize = 0

  fn find(key: String): String {
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
}

behavior Versioned<Value> for Value {
  var epoch: u64

  init() { epoch = 0 }
  export mutationEpoch: u64 { get => epoch }
  mut didSet(current: ref Value) { epoch += 1 }
  mut didModify(current: ref Value) { epoch += 1 }
}

// Facet observers enter through a named composition; a direct observer
// application is not a property declaration form.
behavior VersionedPlace for Place<String> =
  (value: Initialized, version: Versioned)

struct VersionedPlaceBox {
  var VersionedPlace place: Place<String> = Place(id: "north", label: "square")
}

const DefaultLabel: String = "square"
test "place label" for Place {
  let place = Place(id: "north", label: DefaultLabel)
  place.describe()
}

test "qualified facet path" for VersionedPlaceBox {
  var box = VersionedPlaceBox()
  box.place.title = "avenue"
  let epoch = box.place#version.mutationEpoch
  let title = (box.place#value).title
  epoch
  title
}
// atlas:end data-declarations

// atlas:begin callables-and-foreign
export fn describe<ID, _ limit: usize>(value: ID, each labels: String...): String throws Signal {
  return labels[0]
}

export static const fn makePlace<ID>(value: ID): Place<ID> {
  return Place(id: value, label: "center")
}

export mut fn rename(place: Place<String>, value: String) {
  place.title = value
}

fn labelShapes(
  order: String,
  named audit: String,
  _ note: String,
  to destination: String,
  title: String = "city",
  each tags: String...,
): String {
  return order
}

type Handler = any mut async fn(inout String, take Place<String>): String throws Signal
type ForeignHandler = unsafe fn<abi: .c>(c.ptr<c.char>, c.int): c.int

unsafe fn<lang: .c> c_hash(data: c.ptr<c.char>): c.int {
}
// atlas:end callables-and-foreign

// atlas:begin types-and-contracts
struct Matrix<Element, rows: usize, columns: usize> {
  cells: [Element; rows]
}

type Tile = Matrix<u8, rows: 4, columns: 4>
type SmallText = Array<u8><(.count <= 64)>
type Allowed = Signal<[.quiet, .alert]>
type Settings = Config<{mode: .strict, retries: 2}>
type Location = (district: String, number: u16)
type Digest = [u8; 32]
type Callback = some fn(String): String
type SharedPlace = shared Place<String>
type WeakPlace = weak Place<String>
type ViewPlace = view Place<String>

fn makeDigest(): Digest {
  let location: Location = (district: "north", number: 4)
  let bytes: Digest = [0; 32]
  let value = if location.number > 0 { bytes } else { bytes }
  return value
}
// atlas:end types-and-contracts

// atlas:begin patterns
fn classify(signal: Signal): String {
  let Signal.alert(level: let level) = signal
  return switch signal {
    case .quiet: "quiet"
    case .alert(let level) if level > 0: "alert"
    case .alert(let level): "alert"
  }
}

fn unpack(place: Place<String>): String {
  let Place(id, ...) = place
  let (id, label) = (id, "center")
  return id + label
}

fn classifyRange(value: i32): String {
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
  let speed = 12<km>
  let bytes = 64<KiB>
  let text = "city"
  let raw = #"raw city"#
  let rawInterpolated = #"city #${count}"#
  let multiline = """north
south"""
  let scalar = 'N'
  let byte = b'\x4e'
  let enabled = true
  let point = (north: 1, east: 2)
  let list = [1, 2, 3]
  let map = ["north": 1]
  let repeated = [0; 4]
  let selected = (point).north
  count
  ratio
  distance
  speed
  bytes
  text
  raw
  multiline
  scalar
  byte
  enabled
  list
  map
  repeated
  selected
}
// atlas:end literals-and-collections

// atlas:begin entry-declaration
fn runAtlas() {
  values()
}

entry Atlas(runAtlas)
// atlas:end entry-declaration
