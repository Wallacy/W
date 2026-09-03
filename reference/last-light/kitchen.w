// Thermal control and resource contracts for the Last Light kitchen.

import si from std
import { Course, Dish, GuestCount, Probability } from domain
import { Energy, Mass, PhysicalDuration, Power, Temperature, energy } from units

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
  let course: Course
  let ingredients: Array<Ingredient>
  let target: Temperature
  let duration: PhysicalDuration
  let energyBudget: Energy
}

export struct Mixture {
  let course: Course
  let mass: Mass
  let homogeneity: Probability
}

export struct KitchenPlan {
  let recipe: Recipe
  let minimumAroma: Probability
  let duration: PhysicalDuration
  let energyBudget: Energy
}

export struct OvenTelemetry {
  let ovenId: OvenId
  let temperature: Temperature
  let power: Power
  let duty: DutyCycle
  let sampleSequence: u64
}

export struct OvenReady {
  let ovenId: OvenId
  let token: u64
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
  let id: ReservationId
  export let ingredients: Array<Ingredient>
  let releaser: ServiceRef<PantryLeaseApi>

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

export protocol OvenObserverApi {
  async fn status(): OvenTelemetry throws OvenError
}

export protocol OvenLeaseApi: OvenObserverApi {
  async fn preheat(): OvenReady throws OvenError
  async fn bake(
    mixture: take Mixture,
    readiness: take OvenReady,
  ): Dish throws OvenError
  async fn close()
}

export protocol OvenApi {
  async fn telemetry(): OvenTelemetry throws OvenError
  async fn acquire(
    target: Temperature,
    duration: PhysicalDuration,
  ): ServiceRef<OvenLeaseApi> throws OvenError
}

export struct PidController {
  let proportionalGain: f64
  let integralGain: f64
  let derivativeGain: f64
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

  export let isIdle: Bool {
    get => accumulatedError == 0.0 && previousError == 0.0
  }
}

fn integralMayAdvance(rawDuty: f64, error controllerError: f64): Bool {
  return switch rawDuty {
    case 0.0...1.0: true
    case ..<0.0 if controllerError > 0.0: true
    case 1.0>.. if controllerError < 0.0: true
    case _: false
  }
}

export fn controlDuty(
  controller: inout PidController,
  target: Temperature,
  measured: Temperature,
  interval: PhysicalDuration,
): DutyCycle {
  let error = (target - measured).value(in: si.deltaK)
  let seconds = interval.value(in: si.s)
  let candidateIntegral = controller.accumulatedError + error * seconds
  let derivative = (error - controller.previousError) / seconds
  let rawDuty = controller.proportionalGain * error
    + controller.integralGain * candidateIntegral
    + controller.derivativeGain * derivative
  let duty = try DutyCycle((0.0...1.0).clamp(rawDuty))

  if integralMayAdvance(rawDuty: rawDuty, error: error) {
    controller.accumulatedError = candidateIntegral
  }

  controller.previousError = error
  return duty
}

export fn expectedEnergy(
  telemetry: ref OvenTelemetry,
  during duration: PhysicalDuration,
): Energy {
  return expectedEnergy(power: telemetry.power, duty: telemetry.duty, during: duration)
}

export fn expectedEnergy(
  power: Power,
  duty dutyCycle: DutyCycle,
  during duration: PhysicalDuration,
): Energy {
  return energy(power: power * dutyCycle, during: duration)
}

export fn mix(
  ingredients: ref Array<Ingredient>,
  recipe: ref Recipe,
): Mixture throws KitchenError {
  guard !ingredients.isEmpty else throw .emptyStock
  return Mixture(course: recipe.course, mass: ingredients.totalMass(), homogeneity: ingredients.homogeneity())
}

test "anti-windup uses range patterns and guards" for controlDuty {
  expect integralMayAdvance(rawDuty: 0.4, error: 1.0)
  expect integralMayAdvance(rawDuty: -0.2, error: 1.0)
  expect !integralMayAdvance(rawDuty: 1.2, error: 1.0)
}

test "a PID controller starts with no accumulated error" {
  let controller = try PidController(
    proportionalGain: 0.8,
    integralGain: 0.1,
    derivativeGain: 0.02,
  )

  expect controller.isIdle
}

test "an overloaded function value uses an explicit call shape" for expectedEnergy {
  let estimator: some fn(Power, DutyCycle, PhysicalDuration): Energy =
    (power, duty, duration) => expectedEnergy(power: power, duty: duty, during: duration)

  expect estimator(2<si.W>, try DutyCycle(0.5), 3<si.s>) == 3<si.J>
}
