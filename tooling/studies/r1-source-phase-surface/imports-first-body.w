module source_phase_surface
import { Dish, Order } from domain
import kitchen
import { Temperature, degC } from units

fn sourcePhaseEntry(): Temperature {
  return 180<degC>
}

fn runConsole(_ order: Order): Dish {
  return Dish(orderId: order.id, course: order.course, label: "phase")
}
