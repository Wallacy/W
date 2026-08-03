# Primeiros exemplos do restaurante W

> **Arquivo histórico · fonte:** commit `18cd0f3` de 9 de maio de 2021

Estes três sketches são a origem confirmada pelo Git do exemplo de restaurante.
Eles preservam a intenção de contrastar trabalho sequencial e concorrente, mas
não são sintaxe normativa: usam fence TypeScript, assinatura anterior, `async`
como expressão e o nome possivelmente incompleto `makeDiner`.

## `W/cheff.md`

```w
module restaurant

makeDinner() async throws -> Meal {
    let veggies = try await chopVegetables()
    let meat = await marinateMeat()
    let oven = try await preheatOven(temperature: 350)

    let dish = Dish(ingredients: [veggies, meat])
    return try await oven.cook(dish, duration: .hours(3))
}
```

## `W/assistant.md`

```w
module restaurant

makeLunch() async throws -> Meal {
  let veggies = async chopVegetables()
  let meat = async marinateMeat()
  let oven = async preheatOven(temperature: 300)

  let dish = Dish(ingredients: await [try veggies, meat])
  return await oven.cook(dish, duration: .minutes(30))
}
```

## `W/city.md`

```w
module city

import { makeDiner } from restaurant

await makeDiner()
```

## Linhagem posterior

Os arquivos passaram por `W/_w_/` e no commit `928d3c9` aparecem vazios antes de
serem removidos. O exemplo atual, deliberadamente reescrito contra a baseline,
está em [`W/examples/restaurant/`](../../W/examples/restaurant) quando presente.
