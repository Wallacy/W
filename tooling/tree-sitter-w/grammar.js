// Candidate executable grammar for W's surface syntax.
// W/spec/syntax.md and W/STATUS.md still own language decisions.

const DECLARATION_KEYWORDS = [
  "enum",
  "fn",
  "foreign",
  "object",
  "protocol",
  "struct",
  "type",
];

const CONTROL_KEYWORDS = [
  "break",
  "case",
  "catch",
  "continue",
  "defer",
  "do",
  "else",
  "for",
  "guard",
  "if",
  "return",
  "switch",
  "throw",
  "while",
];

const MODIFIER_KEYWORDS = [
  "async",
  "await",
  "const",
  "copy",
  "export",
  "inout",
  "let",
  "mut",
  "panic",
  "ref",
  "spawn",
  "take",
  "throws",
  "try",
  "var",
];

const OTHER_KEYWORDS = ["as", "false", "from", "import", "in", "is", "true", "where"];

// Keep this inventory next to the grammar. Highlight projections should use the
// same spellings, and corpus tests make accidental additions visible.
const KEYWORDS = [
  ...DECLARATION_KEYWORDS,
  ...CONTROL_KEYWORDS,
  ...MODIFIER_KEYWORDS,
  ...OTHER_KEYWORDS,
];

const BINARY_OPERATORS = [
  ["*", 12],
  ["/", 12],
  ["%", 12],
  ["+", 11],
  ["-", 11],
  ["<<", 10],
  [">>", 10],
  ["..<", 9],
  ["...", 9],
  ["<", 8],
  ["<=", 8],
  [">", 8],
  [">=", 8],
  ["is", 8],
  ["in", 8],
  ["==", 7],
  ["!=", 7],
  ["&", 6],
  ["^", 5],
  ["|", 4],
  ["&&", 3],
  ["||", 2],
  ["??", 1],
];

const ASSIGNMENT_OPERATORS = ["=", "+=", "-=", "*=", "/=", "%="];

module.exports = grammar({
  name: "w",

  extras: ($) => [/[\s\uFEFF\u2060\u200B]/, $.comment],

  word: ($) => $.identifier,

  conflicts: ($) => [
    [$._expression, $.closure_parameter],
    [$._type_identifier, $._expression],
    [$._type_identifier, $._expression, $.closure_parameter],
  ],

  rules: {
    source_file: ($) => repeat(choice($.import_statement, $._declaration)),

    import_statement: ($) =>
      seq(
        "import",
        field("items", choice($.named_imports, $.namespace_import)),
        "from",
        field("module", $.module_path),
        optional(";"),
      ),

    named_imports: ($) => seq("{", commaSep1($.import_item), optional(","), "}"),
    import_item: ($) =>
      seq(field("name", $.identifier), optional(seq("as", field("alias", $.identifier)))),
    namespace_import: ($) =>
      seq(field("name", $.identifier), "as", field("alias", $.identifier)),
    module_path: ($) => seq($.identifier, repeat(seq(".", $.identifier))),

    _declaration: ($) =>
      choice(
        $.function_declaration,
        $.struct_declaration,
        $.object_declaration,
        $.enum_declaration,
        $.protocol_declaration,
        $.type_declaration,
        $.foreign_declaration,
        $.const_declaration,
      ),

    declaration_prefix: ($) =>
      choice(seq(repeat1($.attribute), optional("export")), "export"),

    attribute: ($) =>
      seq("@", field("name", $.identifier), optional($.argument_list)),

    function_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        optional("mut"),
        "fn",
        field("name", $.identifier),
        optional($.type_parameters),
        field("parameters", $.parameter_list),
        optional(seq(":", field("return_type", $.type))),
        optional("async"),
        optional(seq("throws", field("error_type", $.type))),
        choice(field("body", $.block), optional(";")),
      ),

    parameter_list: ($) => seq("(", commaSep($.parameter), optional(","), ")"),
    parameter: ($) =>
      seq(
        choice(
          field("name", $.identifier),
          seq(field("label", choice($.identifier, "_")), field("name", $.identifier)),
        ),
        ":",
        optional(field("ownership", choice("ref", "inout", "take"))),
        field("type", $.type),
        optional(seq("=", field("default", $._expression))),
      ),

    type_parameters: ($) => seq("<", commaSep1($.type_parameter), optional(","), ">"),
    type_parameter: ($) =>
      seq(field("name", $._type_identifier), optional(seq(":", field("constraint", $.type)))),

    struct_declaration: ($) =>
      seq(optional($.declaration_prefix), "struct", field("name", $._type_identifier), optional($.type_parameters), $.type_body),
    object_declaration: ($) =>
      seq(optional($.declaration_prefix), "object", field("name", $._type_identifier), optional($.type_parameters), $.type_body),
    protocol_declaration: ($) =>
      seq(optional($.declaration_prefix), "protocol", field("name", $._type_identifier), optional($.type_parameters), $.protocol_body),
    enum_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "enum",
        field("name", $._type_identifier),
        optional($.type_parameters),
        optional(seq(":", field("conformance", $.type))),
        $.enum_body,
      ),

    type_body: ($) => seq("{", repeat(choice($.field_declaration, $.function_declaration)), "}"),
    protocol_body: ($) => seq("{", repeat($.function_declaration), "}"),
    enum_body: ($) => seq("{", repeat(choice($.enum_case, $.function_declaration)), "}"),

    field_declaration: ($) =>
      seq(
        repeat($.attribute),
        optional("export"),
        optional("var"),
        field("name", $.identifier),
        ":",
        field("type", $.type),
        optional(seq("=", field("value", $._expression))),
        optional(";"),
      ),

    enum_case: ($) =>
      seq(
        field("name", $.identifier),
        optional(seq("(", commaSep1($.enum_case_parameter), optional(","), ")")),
        optional(";"),
      ),
    enum_case_parameter: ($) =>
      choice(
        field("type", $.type),
        seq(field("label", $.identifier), ":", field("type", $.type)),
      ),

    type_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "type",
        field("name", $._type_identifier),
        "=",
        field("value", $.type),
        optional(seq("where", field("predicate", $._expression))),
        optional(";"),
      ),

    foreign_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "foreign",
        field("language", $.identifier),
        optional(seq("from", field("source", $.string_literal))),
        "{",
        repeat(choice($.foreign_type_declaration, $.function_declaration)),
        "}",
      ),
    foreign_type_declaration: ($) => seq("type", field("name", $._type_identifier), optional(";")),

    const_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "const",
        field("name", $.identifier),
        optional(seq(":", field("type", $.type))),
        "=",
        field("value", $._expression),
        optional(";"),
      ),

    type: ($) =>
      prec.right(
        seq(
          field("base", choice($.type_name, $.tuple_type)),
          optional($.type_arguments),
          optional("?"),
        ),
      ),
    type_name: ($) =>
      prec.right(seq(field("name", $._type_identifier), repeat(seq(".", field("member", $._type_identifier))))),
    _type_identifier: ($) => alias($.identifier, $.type_identifier),
    type_arguments: ($) => seq("<", commaSep1($.type), optional(","), ">"),
    tuple_type: ($) => seq("(", commaSep1($.type), optional(","), ")"),

    block: ($) => seq("{", repeat($._statement), "}"),
    _statement: ($) =>
      choice(
        $.binding_declaration,
        $.return_statement,
        $.throw_statement,
        $.defer_statement,
        $.guard_statement,
        $.if_statement,
        $.while_statement,
        $.for_statement,
        $.switch_statement,
        $.do_statement,
        $.break_statement,
        $.continue_statement,
        $.expression_statement,
      ),

    binding_declaration: ($) =>
      seq(
        optional(field("task_kind", choice("async", "spawn"))),
        field("kind", choice("let", "var")),
        field("pattern", $.pattern),
        optional(seq(":", field("type", $.type))),
        optional(seq("=", field("value", $._expression))),
        optional(";"),
      ),

    return_statement: ($) =>
      prec.right(seq("return", optional($._expression), optional(";"))),
    throw_statement: ($) => seq("throw", $._expression, optional(";")),
    break_statement: (_) => seq("break", optional(";")),
    continue_statement: (_) => seq("continue", optional(";")),
    defer_statement: ($) => seq("defer", $.block),
    guard_statement: ($) => seq("guard", choice($.optional_binding, $._expression), "else", choice($.block, $._statement)),
    if_statement: ($) =>
      prec.right(seq("if", choice($.optional_binding, $._expression), $.block, optional(seq("else", choice($.if_statement, $.block))))),
    while_statement: ($) => seq("while", $._expression, $.block),
    for_statement: ($) => seq("for", field("pattern", $.pattern), "in", field("value", $._expression), $.block),
    optional_binding: ($) => seq("let", field("name", $.identifier), "=", field("value", $._expression)),

    switch_statement: ($) =>
      seq("switch", field("value", $._expression), "{", repeat1($.switch_case), "}"),
    switch_case: ($) =>
      seq(
        "case",
        field("pattern", $.pattern),
        optional(seq("where", field("guard", $._expression))),
        ":",
        repeat($._statement),
      ),

    do_statement: ($) => seq("do", $.block, repeat1($.catch_clause)),
    catch_clause: ($) => seq("catch", optional(field("pattern", $.pattern)), $.block),

    expression_statement: ($) => seq($._expression, optional(";")),

    pattern: ($) =>
      choice(
        $.identifier,
        $.enum_pattern,
        $.tuple_pattern,
        seq("let", $.identifier),
        "_",
      ),
    enum_pattern: ($) =>
      prec.right(seq(".", field("case", $.identifier), optional(seq("(", commaSep1($.pattern), optional(","), ")")))),
    tuple_pattern: ($) => seq("(", commaSep1($.pattern), optional(","), ")"),

    _expression: ($) =>
      choice(
        $.assignment_expression,
        $.binary_expression,
        $.unary_expression,
        $.panic_expression,
        $.call_expression,
        $.member_expression,
        $.optional_member_expression,
        $.index_expression,
        $.closure_expression,
        $.parenthesized_expression,
        $.tuple_expression,
        $.array_literal,
        $.map_literal,
        $.enum_literal,
        $.size_literal,
        $.number_literal,
        $.string_literal,
        $.raw_string_literal,
        $.multiline_string_literal,
        $.boolean_literal,
        $.identifier,
      ),

    assignment_expression: ($) =>
      prec.right(
        0,
        seq(field("left", $._expression), field("operator", choice(...ASSIGNMENT_OPERATORS)), field("right", $._expression)),
      ),

    binary_expression: ($) =>
      choice(
        ...BINARY_OPERATORS.map(([operator, precedence]) =>
          prec.left(
            precedence,
            seq(field("left", $._expression), field("operator", operator), field("right", $._expression)),
          ),
        ),
      ),

    unary_expression: ($) =>
      prec.right(13, seq(field("operator", choice("!", "~", "-", "try", "await", "copy", "take", "inout", "ref")), field("operand", $._expression))),

    panic_expression: ($) => prec(15, seq("panic", field("arguments", $.argument_list))),

    call_expression: ($) =>
      prec.left(15, seq(field("function", $._expression), field("arguments", $.argument_list))),
    argument_list: ($) => seq("(", commaSep($.argument), optional(","), ")"),
    argument: ($) =>
      seq(optional(seq(field("label", $.identifier), ":")), field("value", $._expression)),
    member_expression: ($) =>
      prec.left(15, seq(field("object", $._expression), ".", field("property", $.identifier))),
    optional_member_expression: ($) =>
      prec.left(15, seq(field("object", $._expression), "?.", field("property", $.identifier))),
    index_expression: ($) =>
      prec.left(15, seq(field("object", $._expression), "[", field("index", $._expression), "]")),

    closure_expression: ($) =>
      prec.right(seq($.closure_parameters, "=>", choice($._expression, $.block))),
    closure_parameters: ($) => seq("(", commaSep($.closure_parameter), optional(","), ")"),
    closure_parameter: ($) => seq(field("name", $.identifier), optional(seq(":", field("type", $.type)))),

    parenthesized_expression: ($) => seq("(", $._expression, ")"),
    tuple_expression: ($) =>
      seq(
        "(",
        choice(
          seq(field("label", $.identifier), ":", field("value", $._expression)),
          seq($.tuple_element, ",", optional(seq(commaSep1($.tuple_element), optional(",")))),
        ),
        ")",
      ),
    tuple_element: ($) => seq(optional(seq(field("label", $.identifier), ":")), field("value", $._expression)),
    array_literal: ($) => seq("[", commaSep($._expression), optional(","), "]"),
    map_literal: ($) => seq("[", commaSep1($.map_entry), optional(","), "]"),
    map_entry: ($) => seq(field("key", $._expression), ":", field("value", $._expression)),
    enum_literal: ($) => prec(16, seq(".", field("case", $.identifier))),

    size_literal: ($) => seq(field("value", $.number_literal), field("unit", $.unit_identifier)),
    unit_identifier: (_) => token(choice("B", "KiB", "MiB", "GiB")),
    number_literal: (_) =>
      token(
        choice(
          /0[xX][0-9a-fA-F](?:_?[0-9a-fA-F])*(?:_[A-Za-z][A-Za-z0-9]*)?/,
          /0[bB][01](?:_?[01])*(?:_[A-Za-z][A-Za-z0-9]*)?/,
          /[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?(?:_[A-Za-z][A-Za-z0-9]*)?/,
        ),
      ),
    string_literal: (_) => token(/"([^"\\\r\n]|\\.)*"/),
    raw_string_literal: (_) => token(/r"[^"\r\n]*"/),
    multiline_string_literal: (_) => token(/"""([^"\r]|"[^"\r]|""[^"\r])*"""/),
    boolean_literal: (_) => choice("true", "false"),

    // Exact keyword tokens win in their syntactic positions. `word` lets
    // Tree-sitter build the keyword table from this shared identifier token.
    identifier: (_) => /[A-Za-z_][A-Za-z0-9_]*/,

    comment: (_) =>
      token(choice(seq("//", /[^\r\n]*/), /\/\*[^*]*\*+([^/*][^*]*\*+)*\//)),
  },
});

function commaSep(rule) {
  return optional(commaSep1(rule));
}

function commaSep1(rule) {
  return seq(rule, repeat(seq(",", rule)));
}
