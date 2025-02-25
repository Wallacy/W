```typescript

module restaurant

makeLunch() async throws -> Meal {
  let veggies = async chopVegetables()
  let meat = async marinateMeat()
  let oven = async preheatOven(temperature: 300)

  let dish = Dish(ingredients: await [try veggies, meat])
  return await oven.cook(dish, duration: .minutes(30))
}

```