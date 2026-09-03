import * from std

export struct DishRoute {
  let raw: u32
}

export fn routeDish(
  id: DishRoute,
  fallback: String,
): String {
  return fallback
}
