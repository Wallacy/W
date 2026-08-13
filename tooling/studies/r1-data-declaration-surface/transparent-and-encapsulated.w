module data_declaration_surface
import { Course, DishLabel, Money } from domain
import { Ingredient, ReservationId } from kitchen

export struct MenuItem {
  course: Course
  label: DishLabel
  price: Money
}

export object StockReservation {
  id: ReservationId
  ingredients: Array<Ingredient>

  export fn ingredientCount(): usize {
    return ingredients.count
  }

  export mut fn release() {
    ingredients = []
  }
}

fn dataSurfaceEntry(item: MenuItem, reservation: StockReservation): Bool {
  return item.price.minorUnits >= 0 && reservation.ingredientCount() >= 0
}
