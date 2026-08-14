import * from std

export struct DishRoute {
  raw: u32
}

export fn routeDish(
  id: DishRoute,
  named fallback: String,
): String {
  return fallback
}
