/// Fixture lexical da superfície integrada. Não é um programa executável.
module restaurant<
  domains: [
    .concurrent(.compute, maximum: 4, capabilities: [.parallel]),
  ],
>

import iec from std
import std.http
import { Order } from domain
import { Tensor } from std.tensor
import service {
  OvenApi as ovens<key: OvenId>,
} from contracts

export type GuestCount = u16<(1...4096)>
export type ShortMessage = String<(.graphemes.count <= 120)>

struct StagePath<
  _ stages: StaticList<ServiceStage><(isValidStagePath(.member))>,
> {
  let orderId: OrderId
}

protocol Source<Item> {
  fn first(): ref Item?
}

protocol Catalog<Item>: Source<Item> & Counted {}

extension<T: Display & Equatable> Shelf<T>: Catalog {
  alias Item = T
  fn first(): ref T? { return values.first }
}

alias WorkStage =
  ServiceStage<[.reserving, .preparing, .serving]>

alias ContinuingOutcome<T> =
  KitchenOutcome<T><[.ready, .delayed]>

alias RecoverableServiceFault =
  ServiceFault<[.ingredientsMissing, .delayed]>

enum OvenSessionState {
  idle
  ready
}

struct OvenSession<_ state: OvenSessionState> {
  let id: u16
}

extension OvenSession<.idle> {
  take fn activate(): OvenSession<.ready> {
    return OvenSession<.ready>(id: id)
  }
}

fn reserveCourse(): WorkStage throws RecoverableServiceFault {
  return .preparing
}

struct ReservationKey: Hashable & Reflectable {
  let orderId: OrderId
  let course: Course
}

fn kitchenLoad(_ kitchens: u16, _ courses: Course...): u32 {
  return 0
}

fn announce(_ messages: ref String...): usize {
  return 0
}

fn restValues(): () {
  let planned = [.nebulaBroth, .horizonCake]
  let load = kitchenLoad(2, courses: each planned)
  let ref info = info of ReservationKey
  let loader: fn(u16, Course...): u32 = kitchenLoad
}

type SensorCallback =
  unsafe fn<abi: .c>(c.ptr<void>, c.int): ()

dimension Applause
unit clap: Applause
unit ovation = 1_000<clap>

const serviceTemperature = 180<degC>
const commandLimit = 64<iec.KiB>

fn lexicalValues(): () {
  let scalar = 'W'
  let byte = b'\n'
  let packet = b"WPKG\x00\x01"
  let rawPath = #"C:\last-light\${literal}"#
  let rawCard = #"""${literal}
C:\last-light"""#
}

struct ServiceFlow {
  var stage: ServiceStage
  const first = ServiceFlow(initialStage: .accepted)

  export init(initialStage: ServiceStage) {
    self.stage = initialStage
  }

  let isTerminal: Bool {
    get => stage in (.completed, .cancelled)
  }

  mut fn advance(to next: ServiceStage): self {
    stage = next
  }

  take fn finish(): ServiceStage {
    return stage
  }
}

object FixtureMetrics {
  var atomic completed: u64 = 0
}

object FixtureLedger {
  let entries: shared Array<String>

  init() {
    self.entries = []
  }

  fn snapshot(): Array<String> {
    return lock entries as values { copy values }
  }
}

export service lastLight: RestaurantApi {
  let pantry: ServiceRef<PantryApi>
  var Lazy calibration = loadCalibration()
  var completed: u64 = 0

  static fn serviceName(): String { return "last-light" }

  mut async fn place(_ order: take Order): Receipt throws RestaurantError {
    let ref Order(course, ...) = order
    let stock = async pantry.reserve(course)
    let plan = spawn<.compute> optimize(order)
    plan#cancel(reason: .menuChanged)
    let (stock, plan) = try await (stock, plan)

    defer async { await stock.release() }
    return try await cook(take order, stock: stock, plan: plan)
  }
}

fn score(
  _ observations: ref Tensor<f32, shape: [2, 3]>,
  _ weights: ref Tensor<f32, shape: [3, 4]>,
): Tensor<f32, shape: [2, 4]> {
  return observations @ weights
}

fn welcome(
  _ arrival: ref Arrival,
  using greeter: some fn(ref Arrival): Welcome,
): Welcome {
  return greeter(arrival)
}

fn explicitCaptureFixture(_ gate: usize): some fn(ref Arrival): Welcome {
  return <[copy gate]> (arrival) => Welcome(orderId: arrival.orderId, gate: gate)
}

fn firstOrder(_ values: ref Array<Order>): ref Order? {
  let ref first = values.first?
  return .some(first)
}

fn visibleOrders(_ values: ref Array<Order>): view Array<Order> {
  return values[1..<values.count]
}

fn reprioritize(
  _ values: inout Array<Order>,
): mut view Array<Order> {
  let mut view pending: Array<Order> = values[1..<values.count]
  return mut view pending
}

fn commandWord(_ source: ref String): view String throws Utf8BoundaryError {
  return try source.view(bytes: 0..<4)
}

fn optionalGuestName(_ input: ref String): GuestName? {
  return try? GuestName(input)
}

const fn buildOpcodes(): Map<String, u8> {
  var result = Map<String, u8>()
  result["heat"] = 0x02_u8
  result["serve"] = 0xff_u8
  return result
}

const instructionOpcodes = buildOpcodes()

struct ConstCell {
  var value: u8

  const init(value: u8) {
    self.value = value
  }

  const mut fn replace(_ value: u8) {
    self.value = value
  }
}

fn recoverOrder(_ source: ref String): Order throws AppError {
  let result = Result.capture(() => try parse(source))

  do {
    return try result
  } catch .invalid(let token) if token.isRecoverable {
    return try repair(source)
  } catch error {
    throw .parse(error)
  }
}

fn collectionForms() {
  let digest: [u8; 32] = [0; 32]
  var counts: Map<Course, u32> = [.horizonCake: 1]
  var diagnosticBits: u32 = 0

  if let mut ref count = counts[.horizonCake] {
    count += 1
  }

  for ref order in orders { inspect(order) }
  for mut ref order in mutableOrders { order.update() }
  for copy code in statusCodes { send(code) }
  for order in take pendingOrders { serve(take order) }

  inspectOrders: {
    scanOrders: for ref order in orders {
      for ref item in order.items {
        diagnosticBits <<= 1
        diagnosticBits |= item.statusBits
        if item.isInvalid { break inspectOrders }
        if item.endsOrder { continue scanOrders }
      }
    }
  }

  repeat {
    diagnosticBits >>= 1
  } while diagnosticBits > 0
}

struct Route {
  let handler: any mut fn(Arrival): Welcome
  let finalize: any take fn(): Receipt
}

fn numericForms(): () {
  let binary = 0b1111_0000_u8
  let octal = 0o755_u16
  let hexadecimal = 0xff_40_00_u32
  let exponent = 6.022_140_76e23_f64
  let signedExponent = 1.0e-9_f32
  let amount = 6.022e23<1/mol>
  let setpoint = -40<degC>
}

fn pinState(_ state: take BellState): Pinned<BellState> throws AllocationError {
  return try pin take state
}

fn decodeMenu(allocator memory: ref Allocator, _ payload: ref Bytes): Menu throws AllocationError {
  allocator scratch: .fixed<capacity: 8<iec.MiB>> {
    let parsed = try Menu.parse(payload)
    return try (take parsed).rehome(allocator: memory)
  }
}

protocol Stream<Item, Failure: Error> {
  mut async fn next(): Item? throws Failure
}

async fn drainOrders(
  _ input: take Channel<receive: Order>,
): () {
  var orders = take input

  for await order in orders {
    serve(take order)
  }
}

async fn inspectLines<E: Error>(
  _ source: take some Stream<view String, E>,
): () throws E {
  var lines = take source

  for try await line in lines {
    inspect(line)
  }
}

async fn reserveTable(
  _ ledger: ref ServiceRef<TableLedgerApi>,
  _ request: take ReservationRequest,
): ReservationReceipt throws BookingError {
  return try await pipeline<transaction: {
    isolation: .serializable,
    access: .readWrite,
  }> tx = ledger {
    let reservation = try await tx.reserve(take request)
    let receipt = try await tx.confirm(take reservation)
    commit receipt
  }
}

foreign c from "last_light_probe.h" {
  type ll_probe
  fn ll_probe_close(_ probe: c.ptr<ll_probe>)
}

export foreign c {
  const LL_STATUS_OK_V1: c.int = 0

  struct ll_status_v1 {
    code: c.int
  }
}

export unsafe fn<abi: .c> ll_status_v1_create(code: c.int): ll_status_v1 {
  return ll_status_v1(code: code)
}

entry LastLight(run)
