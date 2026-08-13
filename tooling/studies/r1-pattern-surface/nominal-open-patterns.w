module pattern_surface
import { PartySize, ServiceStage } from domain

struct Guest {
  name: String
  note: String
}

struct Order {
  id: usize
  guest: Guest
  stage: ServiceStage
  guests: usize
  route: Route
  newField: String
  newFieldValue: String
}

enum Route {
  menu
  unknown
}

fn patternSurfaceEntry(order: take Order): String {
  let Order(id: orderId, guest: Guest(name, ...), ...) = take order
  return name
}

fn fieldSetSurface(order: Order): Array<String> {
  let newField = order.newField
  let openFields = [newField]
  return openFields
}

fn tupleSurface(stage: ServiceStage, guests: usize): PartySize {
  return switch (stage, guests) {
    case (.accepted, 1...4): .intimate
    case (_, _): .regular
  }
}

fn routeSurface(route: Route): String {
  return switch route {
    case .menu: "show"
    case .unknown: "reject"
  }
}
