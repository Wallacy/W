/// Fixture lexical da superfície integrada. Não é um programa executável.
import std.http
import { Order } from restaurant.domain
import std.tensor as tensor

export type GuestCount = u16<(1...4096)>
export type ShortMessage = String<(.graphemes.count <= 120)>

struct StagePath<const stages: StaticList<ServiceStage>> {
  orderId: OrderId
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

export service LastLightRestaurant as RestaurantApi {
  pantry: ServiceRef<PantryApi>
  var Lazy calibration = loadCalibration()
  var atomic completed: u64 = 0

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

foreign c from "last_light_probe.h" {
  type ll_probe
  fn ll_probe_close(probe: c.ptr<ll_probe>)
}

entry LastLight {
  process.main = run
  http.fetch = fetch
}
