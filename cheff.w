```typescript

module restaurant

makeDinner() async throws -> Meal {
    let veggies = try await chopVegetables()
    let meat = await marinateMeat()
    let oven = try await preheatOven(temperature: 350)
  
    let dish = Dish(ingredients: [veggies, meat])
    return try await oven.cook(dish, duration: .hours(3))
}

```