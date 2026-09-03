import * from std

/* export struct CommentGhost {} */
export struct RouteTable {
  let label: String

  fn nestedLookup(): String {
    return "import ghost from hidden_comment"
  }
}

export const marker: String = "export struct StringGhost {}"
