// Mobile lifecycle adapter. UI remains native to the platform.

import { RestaurantApi, RestaurantError } from restaurant.restaurant

const restaurantService = ServiceBinding<RestaurantApi>(name: "last-light")

enum MobileError: Error {
  restaurant(RestaurantError)
  service(ServiceFailure)
  state(StateError)
}

async fn start(ctx: MobileAppContext): AppStartResult throws MobileError {
  let restaurant = try await ctx.services.get(restaurantService)
  let menu = try await restaurant.menu()
  await ctx.state.publish("menu", value: menu)
  return .ready
}

async fn resume(ctx: MobileAppContext): () {
  await ctx.state.publish("lifecycle", value: "active")
}

async fn suspend(ctx: MobileAppContext): () {
  await ctx.services.drain(deadline: ctx.deadline)
}

async fn notification(
  event: NotificationEvent,
  ctx: MobileAppContext,
): NotificationResult {
  return try await ctx.notifications.dispatch(event)
}

entry LastLightMobile(start) {
  app.resume = resume
  app.suspend = suspend
  app.notification = notification
}
