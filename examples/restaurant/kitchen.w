// Thermal control and resource contracts for the Last Light kitchen.

import std.clock
import std.si
import { Course, Dish, GuestCount, Probability } from restaurant.domain
import { Duration, Energy, Mass, Power, Temperature, energy } from restaurant.units

export type ReservationId = u64
export type OvenId = u16
export type DutyCycle = f64<(0.0...1.0)>

export enum Ingredient {
  cometFlour
  vacuumButter
  ionizedSugar
  quietWater
  horizonFruit
}

export struct Recipe {
  course: Course
  ingredients: Array<Ingredient>
  target: Temperature
  duration: Duration
  energyBudget: Energy
}

export struct Mixture {
  course: Course
  mass: Mass
  homogeneity: Probability
}

export struct KitchenPlan {
  recipe: Recipe
  minimumAroma: Probability
  duration: Duration
  energyBudget: Energy
}

export struct OvenTelemetry {
  ovenId: OvenId
  temperature: Temperature
  power: Power
  duty: DutyCycle
  sampledAt: Instant
}

export struct OvenReady {
  ovenId: OvenId
  deadline: Instant
}

export enum PantryError: Error {
  unavailable(Ingredient)
  insufficient(found: u32, required: u32)
  reservationExpired(ReservationId)
  service(ServiceFailure)
}

export enum OvenError: Error {
  offline(OvenId)
  sensorFailure(OvenId)
  unsafeTemperature(Temperature)
  leaseClosed
}

export enum ControllerGain {
  proportional
  integral
  derivative
}

export enum KitchenError: Error {
  emptyStock
  recipeMismatch(expected: Course, found: Course)
  lowAroma(found: Probability, required: Probability)
  energyBudgetExceeded(found: Energy, limit: Energy)
  invalidControllerGain(kind: ControllerGain, value: f64)
}

export object StockReservation {
  id: ReservationId
  export ingredients: Array<Ingredient>
  releaser: ServiceRef<PantryLeaseApi>

  export take async fn release() throws PantryError {
    try await releaser.release(id)
  }
}

export protocol PantryLeaseApi {
  async fn release(reservationId: ReservationId) throws PantryError
}

export protocol PantryApi {
  async fn reserve(course: Course, guests: GuestCount): StockReservation throws PantryError
}

export protocol OvenLeaseApi {
  async fn preheat(): OvenReady throws OvenError
  async fn bake(mixture: take Mixture, until deadline: Instant): Dish throws OvenError
  async fn close()
}

export protocol OvenApi {
  async fn telemetry(): OvenTelemetry throws OvenError
  async fn acquire(target: Temperature, duration: Duration): ServiceRef<OvenLeaseApi> throws OvenError
}

export struct PidController {
  proportionalGain: f64
  integralGain: f64
  derivativeGain: f64
  var accumulatedError: f64
  var previousError: f64

  export init(
    proportionalGain: f64,
    integralGain: f64,
    derivativeGain: f64,
  ) throws KitchenError {
    guard proportionalGain.isFinite && proportionalGain >= 0.0 else {
      throw .invalidControllerGain(kind: .proportional, value: proportionalGain)
    }
    guard integralGain.isFinite && integralGain >= 0.0 else {
      throw .invalidControllerGain(kind: .integral, value: integralGain)
    }
    guard derivativeGain.isFinite && derivativeGain >= 0.0 else {
      throw .invalidControllerGain(kind: .derivative, value: derivativeGain)
    }

    self.proportionalGain = proportionalGain
    self.integralGain = integralGain
    self.derivativeGain = derivativeGain
    self.accumulatedError = 0.0
    self.previousError = 0.0
  }

  export isIdle: Bool {
    get => accumulatedError == 0.0 && previousError == 0.0
  }
}

fn integralMayAdvance(rawDuty: f64, error: f64): Bool {
  return switch rawDuty {
    case 0.0...1.0: true
    case ..<0.0 if error > 0.0: true
    case 1.0>.. if error < 0.0: true
    case _: false
  }
}

export fn controlDuty(
  controller: inout PidController,
  target: Temperature,
  measured: Temperature,
  interval: Duration,
): DutyCycle {
  let error = (target - measured).value(in: si.deltaK)
  let seconds = interval.value(in: si.s)
  let candidateIntegral = controller.accumulatedError + error * seconds
  let derivative = (error - controller.previousError) / seconds
  let rawDuty = controller.proportionalGain * error
    + controller.integralGain * candidateIntegral
    + controller.derivativeGain * derivative
  let duty = try DutyCycle((0.0...1.0).clamp(rawDuty))

  if integralMayAdvance(rawDuty, error: error) {
    controller.accumulatedError = candidateIntegral
  }

  controller.previousError = error
  return duty
}

export fn expectedEnergy(telemetry: ref OvenTelemetry, during duration: Duration): Energy {
  return energy(telemetry.power * telemetry.duty, during: duration)
}

export fn mix(ingredients: ref Array<Ingredient>, recipe: ref Recipe): Mixture throws KitchenError {
  guard !ingredients.isEmpty else throw .emptyStock
  return Mixture(course: recipe.course, mass: ingredients.totalMass(), homogeneity: ingredients.homogeneity())
}

test "anti-windup uses range patterns and guards" for controlDuty {
  expect integralMayAdvance(0.4, error: 1.0)
  expect integralMayAdvance(-0.2, error: 1.0)
  expect !integralMayAdvance(1.2, error: 1.0)
}

test "a PID controller starts with no accumulated error" {
  let controller = try PidController(
    proportionalGain: 0.8,
    integralGain: 0.1,
    derivativeGain: 0.02,
  )

  expect controller.isIdle
}
