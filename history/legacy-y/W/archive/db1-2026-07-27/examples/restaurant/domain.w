// W Working Draft — pseudocódigo pedagógico, não executável.
// Tipos de domínio são values; nenhum import cria serviço ou autoridade.

export struct OrderId {
  value: String
}

export struct TableId {
  value: String
}

export enum CakeFlavor {
  chocolate
  vanilla
  carrot
}

export enum SoupKind {
  tomato
  onion
  pumpkin
}

export enum SaladKind {
  garden
  caesar
  fruit
}

export struct CakeRequest {
  orderId: OrderId
  flavor: CakeFlavor
  portions: Int
  message: String?
}

export struct SoupRequest {
  orderId: OrderId
  kind: SoupKind
  portions: Int
}

export struct SaladRequest {
  orderId: OrderId
  kind: SaladKind
  portions: Int
}

export enum DishKind {
  cake
  soup
  salad
}

export struct DishSummary {
  kind: DishKind
  portions: Int
  label: String
}

export struct Cake {
  summary: DishSummary
}

export struct Soup {
  summary: DishSummary
}

export struct Salad {
  summary: DishSummary
}

export struct Receipt {
  orderId: OrderId
  dish: DishSummary
}

export struct BirthdayTableRequest {
  tableId: TableId
  cake: CakeRequest
  soup: SoupRequest
  salad: SaladRequest
}

export struct TableReceipt {
  tableId: TableId
  cake: Receipt
  soup: Receipt
  salad: Receipt
}
