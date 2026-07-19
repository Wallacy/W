; This query is a projection of grammar.js. Tree-sitter remains the structural
; source; editor-specific TextMate rules must not grow an independent grammar.

(comment) @comment

[
  (string_literal)
  (raw_string_literal)
  (multiline_string_literal)
] @string

(number_literal) @number
(unit_identifier) @constant.builtin
(boolean_literal) @boolean

[
  "enum"
  "fn"
  "foreign"
  "object"
  "protocol"
  "struct"
  "type"
] @keyword.type

[
  "break"
  "case"
  "catch"
  "continue"
  "defer"
  "do"
  "else"
  "for"
  "guard"
  "if"
  "return"
  "switch"
  "throw"
  "while"
] @keyword.control

[
  "async"
  "await"
  "const"
  "copy"
  "export"
  "inout"
  "let"
  "mut"
  "panic"
  "ref"
  "spawn"
  "take"
  "throws"
  "try"
  "var"
] @keyword.modifier

[
  "as"
  "from"
  "import"
  "in"
  "where"
] @keyword

(function_declaration name: (identifier) @function)
(call_expression function: (identifier) @function.call)
(call_expression
  function: (member_expression property: (identifier) @function.method.call))

(parameter name: (identifier) @variable.parameter)
(closure_parameter name: (identifier) @variable.parameter)
(argument label: (identifier) @variable.parameter)

(type_identifier) @type

(type_parameter name: (type_identifier) @type.parameter)
(enum_case name: (identifier) @constant)
(enum_literal case: (identifier) @constant)
(enum_pattern case: (identifier) @constant)

(field_declaration name: (identifier) @property)
(member_expression property: (identifier) @property)
(optional_member_expression property: (identifier) @property)
(import_item name: (identifier) @module)
(module_path (identifier) @module)

[
  "=" "+=" "-=" "*=" "/=" "%="
  "+" "-" "*" "/" "%"
  "<<" ">>" "..." "..<" ">.." ">..<"
  "<" "<=" ">" ">=" "==" "!=" "is"
  "&" "^" "|" "&&" "||" "??"
  "!" "~" "?."
  "is"
] @operator

["(" ")" "[" "]" "{" "}"] @punctuation.bracket
["," ":" ";" "."] @punctuation.delimiter
