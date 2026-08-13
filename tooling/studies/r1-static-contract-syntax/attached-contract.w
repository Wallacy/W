// R1S1 static-contract-syntax fixture derived from generics.w::Matrix.

module static_contract_syntax
import { Dish } from domain
import { Tensor } from std.tensor

type Menu = Array<Dish>
type Menus = Array<Array<Dish>>
type Scores = Tensor<f32, shape: [8, 4]>

fn staticSyntaxEntry(rows: usize, columns: usize): usize {
  return rows * columns
}

test "attached contracts keep nested shape" for staticSyntaxEntry {
  expect staticSyntaxEntry(8, 4) == 32
}
