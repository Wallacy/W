module data_declaration_surface
import { Course, DishLabel, Money } from domain
import { Ingredient, ReservationId } from kitchen

export struct MenuItem {
  export let course: Course
  export let label: DishLabel
  export let price: Money
}

export object StockReservation {
  let id: ReservationId
  let ingredients: Array<Ingredient>

  export fn ingredientCount(): usize {
    return ingredients.count
  }

  export mut fn release() {
    ingredients = []
  }
}

fn dataSurfaceEntry(_ item: MenuItem, _ reservation: StockReservation): Bool {
  return item.price.minorUnits >= 0 && reservation.ingredientCount() >= 0
}
