// Mobile lifecycle adapter. UI remains native to the platform.

import std.mobile as mobile
import { RestaurantApi, RestaurantError, lastLight } from restaurant

enum MobileError: Error {
  restaurant(RestaurantError)
  service(ServiceFailure)
  state(mobile.StateError)
}

async fn start(ctx: mobile.Context): mobile.StartResult throws MobileError {
  let menu = try await lastLight.menu()
  await ctx.state.publish("menu", value: menu)
  return .ready
}

async fn resume(ctx: mobile.Context): () {
  await ctx.state.publish("lifecycle", value: "active")
}

async fn suspend(ctx: mobile.Context): () {
  await ctx.services.drain(deadline: ctx.deadline)
}

async fn notification(
  event: mobile.NotificationEvent,
  ctx: mobile.Context,
): mobile.NotificationResult {
  return try await ctx.notifications.dispatch(event)
}

entry LastLightMobile(start)
