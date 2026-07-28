// W Working Draft — pseudocódigo pedagógico, não executável.
// `print`/`readLine` são nomes T1 do mapa da edição; console capability segue visível.

import { CakeRequest, OrderId, Receipt } from restaurant.domain
import { FrontDeskError, RestaurantApi } from restaurant.front_desk

enum TerminalCommand {
  cake
  status
  quit
  unknown(String)
}

export enum TerminalError: Error {
  frontDesk(FrontDeskError)
}

fn parseCommand(line: ref String): TerminalCommand {
  let command = line.trimmed().lowercased()
  if command == "cake" {
    return .cake
  }
  if command == "status" {
    return .status
  }
  if command == "quit" {
    return .quit
  }
  return .unknown(command)
}

async fn submitCake(restaurant: ServiceRef<RestaurantApi>): Receipt throws TerminalError {
  let request = CakeRequest(
    orderId: OrderId(value: "terminal-demo"),
    flavor: .chocolate,
    portions: 8,
    message: "W: prazer para humanos",
  )

  do {
    return try await restaurant.place(.cake(request))
  } catch let error {
    throw .frontDesk(error)
  }
}

export async fn runTerminal(restaurant: ServiceRef<RestaurantApi>): Void throws TerminalError {
  print(#"Restaurante W — cake | status | quit"#)
  var running = true

  while running {
    guard let line = readLine(prompt: "> ") else {
      return
    }

    switch parseCommand(line) {
      case .cake:
        let receipt = try await submitCake(restaurant)
        print("pedido ${receipt.orderId.value}: ${receipt.dish.label}")
      case .status:
        print("terminal e HTTP compartilham uma RestaurantApi serial")
      case .quit:
        running = false
      case .unknown(let command):
        print("comando desconhecido: ${command}")
    }
  }
}
