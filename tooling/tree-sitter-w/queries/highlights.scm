; This query is a projection of grammar.js. Tree-sitter remains the structural
; source; editor-specific TextMate rules must not grow an independent grammar.

(comment) @comment

[
  (string_literal)
  (raw_string_literal)
  (multiline_string_literal)
  (scalar_literal)
  (byte_literal)
] @string

[
  (number_literal)
  (quantity_literal)
  (unit_suffix_literal)
  (size_literal)
] @number
(boolean_literal) @boolean

[
  "alias"
  "behavior"
  "dimension"
  "enum"
  "entry"
  "extension"
  "fn"
  "foreign"
  "object"
  "protocol"
  "service"
  "struct"
  "test"
  "type"
  "unit"
] @keyword.type

[
  "break"
  "case"
  "catch"
  "continue"
  "defer"
  "deinit"
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
  "atomic"
  "await"
  "capture"
  "const"
  "copy"
  "export"
  "inout"
  "let"
  "mut"
  "package"
  "panic"
  "ref"
  "shared"
  "spawn"
  "static"
  "take"
  "throws"
  "try"
  "unsafe"
  "var"
  "weak"
] @keyword.modifier

[
  "as"
  "any"
  "false"
  "from"
  "get"
  "import"
  "in"
  "init"
  "modify"
  "set"
  "some"
  "storage"
  "true"
] @keyword

(function_declaration name: (identifier) @function)
(call_expression function: (identifier) @function.call)
(call_expression
  function: (member_expression property: (identifier) @function.method.call))

(parameter name: (identifier) @variable.parameter)
(set_accessor parameter: (identifier) @variable.parameter)
(behavior_parameter name: (identifier) @variable.parameter)
(closure_parameter name: (identifier) @variable.parameter)
(argument label: (identifier) @variable.parameter)

(type_identifier) @type

(type_parameter name: (type_identifier) @type.parameter)
(associated_type_requirement name: (type_identifier) @type)
(associated_const_requirement name: (identifier) @constant)
(const_declaration name: (identifier) @constant)
(dimension_declaration name: (type_identifier) @type)
(unit_declaration name: (identifier) @constant)
(enum_case name: (identifier) @constant)
(contextual_member_expression member: (identifier) @property)
(enum_pattern case: (identifier) @constant)

(field_declaration name: (identifier) @property)
(computed_property_declaration name: (identifier) @property)
(property_requirement name: (identifier) @property)
(member_expression property: (identifier) @property)
(optional_member_expression property: (identifier) @property)
(import_item name: (identifier) @module)
(module_path (identifier) @module)

[
  "=" "+=" "-=" "*=" "/=" "%="
  "+" "-" "*" "**" "/" "%" "@"
  "<<" ">>" "..." "..<" ">.." ">..<"
  "<" "<=" ">" ">=" "==" "!=" "is"
  "&" "^" "|" "&&" "||" "??"
  "!" "~" "?."
  "is"
] @operator

["(" ")" "[" "]" "{" "}" "<" ">"] @punctuation.bracket
["," ":" ";" "."] @punctuation.delimiter
