// W Working Draft — pseudocódigo pedagógico, não executável.
// Dinheiro usa inteiro em minor units; binary float não participa da conta.

import { OrderId } from restaurant.domain

export enum Currency {
  brl
  usd
  eur
}

export struct Money {
  minorUnits: i64
  currency: Currency
}

export struct BillLine {
  label: String
  quantity: Int
  unitPrice: Money
}

export struct Invoice {
  orderId: OrderId
  subtotal: Money
  service: Money
  tax: Money
  total: Money
}

export enum BillingError: Error {
  emptyBill
  invalidQuantity(Int)
  currencyMismatch(expected: Currency, received: Currency)
  invalidBasisPoints(Int)
}

fn add(left: Money, right: Money): Money throws BillingError {
  guard left.currency == right.currency else {
    throw .currencyMismatch(expected: left.currency, received: right.currency)
  }
  return Money(minorUnits: left.minorUnits + right.minorUnits, currency: left.currency)
}

fn multiply(price: Money, quantity: Int): Money throws BillingError {
  guard quantity > 0 else {
    throw .invalidQuantity(quantity)
  }
  return Money(minorUnits: price.minorUnits * quantity, currency: price.currency)
}

// Half away from zero é política de domínio explícita; não herda rounding do host.
fn basisPoints(amount: Money, points: Int): Money throws BillingError {
  guard points >= 0 && points <= 10_000 else {
    throw .invalidBasisPoints(points)
  }

  let scaled = amount.minorUnits * points
  var rounded = scaled / 10_000
  let remainder = scaled % 10_000
  if remainder >= 5_000 {
    rounded += 1
  }
  if remainder <= -5_000 {
    rounded -= 1
  }
  return Money(minorUnits: rounded, currency: amount.currency)
}

export fn closeBill(
  orderId: OrderId,
  lines: ref List<BillLine>,
  servicePoints: Int,
  taxPoints: Int,
): Invoice throws BillingError {
  guard lines.count > 0 else {
    throw .emptyBill
  }

  var subtotal = Money(minorUnits: 0, currency: lines[0].unitPrice.currency)
  for line in lines {
    subtotal = try add(subtotal, right: try multiply(line.unitPrice, quantity: line.quantity))
  }

  let service = try basisPoints(subtotal, points: servicePoints)
  let taxable = try add(subtotal, right: service)
  let tax = try basisPoints(taxable, points: taxPoints)
  let total = try add(taxable, right: tax)
  return Invoice(orderId: orderId, subtotal: subtotal, service: service, tax: tax, total: total)
}
