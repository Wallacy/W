/// Fixture lexical da superfície integrada. Não é um programa executável.
import std.http
import { Order } from restaurant.domain
import std.tensor as tensor

export type GuestCount = u16 where (value in 1...4096)

dimension Applause
unit clap: Applause
unit ovation = 1_000<clap>

const serviceTemperature = 180<degC>
const commandLimit = 64<KiB>
let scalar = 'W'
let byte = b'\n'
let rawPath = #"C:\last-light\${literal}"#

export service LastLightRestaurant as RestaurantApi {
  let pantry: ServiceRef<PantryApi>
  var Lazy calibration = loadCalibration()
  var atomic completed: u64 = 0

  mut async fn place(order: take Order): Receipt throws RestaurantError {
    async on .network let stock = pantry.reserve(order.course)
    spawn on .compute let plan = optimize(order)
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

foreign c from "last_light_probe.h" {
  type ll_probe
  fn ll_probe_close(probe: c.ptr<ll_probe>)
}

entry LastLight {
  process.main = run
  http.fetch = fetch
}
