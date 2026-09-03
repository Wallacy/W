module pattern_surface
import { PartySize, ServiceStage } from domain

struct Guest {
  let name: String
  let note: String
}

struct Order {
  let id: usize
  let guest: Guest
  let stage: ServiceStage
  let guests: usize
  let route: Route
  let newField: String
  let newFieldValue: String
}

enum Route {
  menu
  unknown
}

fn patternSurfaceEntry(_ order: take Order): String {
  let Order(id: orderId, guest: Guest(name, ...), ...) = take order
  return name
}

fn fieldSetSurface(_ order: Order): Array<String> {
  let newField = order.newField
  let openFields = [newField]
  return openFields
}

fn tupleSurface(_ stage: ServiceStage, _ guests: usize): PartySize {
  return switch (stage, guests) {
    case (.accepted, 1...4): .intimate
    case (_, _): .regular
  }
}

fn routeSurface(_ route: Route): String {
  return switch route {
    case .menu: "show"
    case .unknown: "reject"
  }
}
