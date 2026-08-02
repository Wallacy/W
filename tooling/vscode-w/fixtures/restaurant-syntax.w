/// Fixture lexical da superfície integrada. Não é um programa executável.
module restaurant<
  domains: [
    .concurrent(.compute, maximum: 4, capabilities: [.parallel]),
  ],
>

import std.http
import { Order } from domain
import reflect from std
import Tensor from std.tensor
import service {
  OvenApi as ovens<key: OvenId>,
} from contracts

export type GuestCount = u16<(1...4096)>
export type ShortMessage = String<(.graphemes.count <= 120)>

struct StagePath<
  const _ stages: StaticList<ServiceStage><(isValidStagePath(.member))>,
> {
  orderId: OrderId
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

struct OvenSession<const _ state: OvenSessionState> {
  id: u16
}

extension OvenSession<.idle> {
  take fn activate(): OvenSession<.ready> {
    return OvenSession<.ready>(id: id)
  }
}

fn reserveCourse(): WorkStage throws RecoverableServiceFault

struct ReservationKey: Hashable & reflect.Reflectable {
  orderId: OrderId
  course: Course
}

fn kitchenLoad(kitchens: u16, courses: Course...): u32
fn announce(_ messages: ref String...): usize

fn restValues(): () {
  let planned = [.nebulaBroth, .horizonCake]
  let load = kitchenLoad(2, courses: each planned)
  let ref info = reflect.info<ReservationKey>()
  let loader: fn(u16, Course...): u32 = kitchenLoad
}

type SensorCallback =
  unsafe fn<abi: .c>(c.ptr<void>, c.int): ()

dimension Applause
unit clap: Applause
unit ovation = 1_000<clap>

const serviceTemperature = 180<degC>
const commandLimit = 64<KiB>

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

  isTerminal: Bool {
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

export service lastLight: RestaurantApi {
  pantry: ServiceRef<PantryApi>
  var Lazy calibration = loadCalibration()
  var completed: u64 = 0

  static fn serviceName(): String { return "last-light" }

  mut async fn place(order: take Order): Receipt throws RestaurantError {
    let ref Order(course, ...) = order
    async<.network> let stock = pantry.reserve(course)
    spawn<.compute> let plan = optimize(order)
    plan.cancel(reason: .menuChanged)
    let (stock, plan) = try await (stock, plan)

    defer async { await stock.release() }
    return try await cook(take order, stock: stock, plan: plan)
  }
}

fn score(
  observations: ref Tensor<f32, shape: [2, 3]>,
  weights: ref Tensor<f32, shape: [3, 4]>,
): Tensor<f32, shape: [2, 4]> {
  return observations @ weights
}

fn welcome(
  arrival: ref Arrival,
  using greeter: some fn(ref Arrival): Welcome,
): Welcome {
  return greeter(arrival)
}

fn firstOrder(values: ref Array<Order>): ref Order? {
  let ref first = values.first?
  return .some(first)
}

fn visibleOrders(values: ref Array<Order>): view Array<Order> {
  return values[1..<values.count]
}

fn reprioritize(
  values: inout Array<Order>,
): inout view Array<Order> {
  let inout pending: view Array<Order> = values[1..<values.count]
  return pending
}

fn commandWord(source: ref String): view String throws Utf8BoundaryError {
  return try source.view(bytes: 0..<4)
}

fn optionalGuestName(input: ref String): GuestName? {
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

  const mut fn replace(value: u8) {
    self.value = value
  }
}

fn recoverOrder(source: ref String): Order throws AppError {
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

  if let inout count = counts[.horizonCake] {
    count += 1
  }

  for ref order in orders { inspect(order) }
  for inout order in mutableOrders { order.update() }
  for copy code in statusCodes { send(code) }
  for order in take pendingOrders { serve(take order) }
}

struct Route {
  handler: any mut fn(Arrival): Welcome
  finalize: any take fn(): Receipt
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

fn pinState(state: take BellState): Pinned<BellState> throws AllocationError {
  return try pin take state
}

fn decodeMenu(payload: ref Bytes, memory: ref Allocator): Menu throws AllocationError {
  region scratch(using: memory, limit: 8<MiB>) {
    let parsed = try Menu.parse(payload, using: scratch)
    return try (take parsed).rehome(using: memory)
  }
}

protocol Stream<Item, Failure: Error> {
  mut async fn next(): Item? throws Failure
}

async fn drainOrders(
  input: take Channel<Order><.receive>,
): () {
  var orders = take input

  for await order in orders {
    serve(take order)
  }
}

async fn inspectLines<E: Error>(
  source: take some Stream<view String, E>,
): () throws E {
  var lines = take source

  for try await line in lines {
    inspect(line)
  }
}

foreign c from "last_light_probe.h" {
  type ll_probe
  fn ll_probe_close(probe: c.ptr<ll_probe>)
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
