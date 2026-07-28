// Candidate executable grammar for W's surface syntax.
// W/DESIGN.md owns language decisions.

const DECLARATION_KEYWORDS = [
  "alias",
  "behavior",
  "dimension",
  "enum",
  "entry",
  "extension",
  "fn",
  "foreign",
  "object",
  "protocol",
  "service",
  "struct",
  "test",
  "type",
  "unit",
];

const CONTROL_KEYWORDS = [
  "break",
  "cancel",
  "case",
  "catch",
  "continue",
  "defer",
  "deinit",
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
  "atomic",
  "await",
  "capture",
  "const",
  "copy",
  "export",
  "inout",
  "let",
  "mut",
  "package",
  "panic",
  "ref",
  "shared",
  "spawn",
  "static",
  "take",
  "throws",
  "try",
  "unsafe",
  "var",
  "weak",
];

const OTHER_KEYWORDS = [
  "any",
  "as",
  "false",
  "from",
  "get",
  "import",
  "in",
  "init",
  "is",
  "modify",
  "on",
  "set",
  "some",
  "storage",
  "true",
  "where",
];

// Keep this inventory next to the grammar. Highlight projections should use the
// same spellings, and corpus tests make accidental additions visible.
const KEYWORDS = [
  ...DECLARATION_KEYWORDS,
  ...CONTROL_KEYWORDS,
  ...MODIFIER_KEYWORDS,
  ...OTHER_KEYWORDS,
];

const BINARY_OPERATORS = [
  ["@", 12],
  ["*", 12],
  ["/", 12],
  ["%", 12],
  ["+", 11],
  ["-", 11],
  ["<<", 10],
  [">>", 10],
  [">..<", 9],
  [">..", 9],
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
        optional("export"),
        "import",
        choice(
          seq(
            field("items", $.named_imports),
            "from",
            field("module", $.module_path),
          ),
          seq(
            field("items", $.legacy_namespace_import),
            "from",
            field("module", $.module_path),
          ),
          seq(
            field("module", $.module_path),
            optional(seq("as", field("alias", $.identifier))),
          ),
        ),
        optional(";"),
      ),

    named_imports: ($) => seq("{", commaSep1($.import_item), optional(","), "}"),
    import_item: ($) =>
      seq(field("name", $.identifier), optional(seq("as", field("alias", $.identifier)))),
    legacy_namespace_import: ($) =>
      prec(
        1,
        seq(
          field("name", $.identifier),
          "as",
          field("alias", $.identifier),
        ),
      ),
    module_path: ($) => seq($.identifier, repeat(seq(".", $.identifier))),

    _declaration: ($) =>
      choice(
        $.function_declaration,
        $.struct_declaration,
        $.object_declaration,
        $.service_declaration,
        $.enum_declaration,
        $.protocol_declaration,
        $.type_declaration,
        $.alias_declaration,
        $.dimension_declaration,
        $.unit_declaration,
        $.extension_declaration,
        $.behavior_declaration,
        $.entry_declaration,
        $.foreign_declaration,
        $.const_declaration,
        $.test_declaration,
      ),

    declaration_prefix: (_) => choice("export", "package"),

    function_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        optional("static"),
        optional("unsafe"),
        optional("mut"),
        optional("async"),
        "fn",
        optional($.language_tag),
        field("name", $.identifier),
        optional($.type_parameters),
        field("parameters", $.parameter_list),
        optional(seq(":", field("return_type", $.type))),
        optional(seq("throws", field("error_type", $.type))),
        choice(field("body", $.block), optional(";")),
      ),

    language_tag: ($) => seq("<", field("language", $.identifier), ">"),

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
      choice(
        seq(
          "const",
          field("name", $.identifier),
          ":",
          field("value_type", $.type),
        ),
        seq(
          field("name", $._type_identifier),
          optional(seq(":", field("constraint", $.type))),
        ),
      ),

    struct_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "struct",
        field("name", $._type_identifier),
        optional($.type_parameters),
        optional($.conformance_clause),
        $.type_body,
      ),
    object_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "object",
        field("name", $._type_identifier),
        optional($.type_parameters),
        optional($.conformance_clause),
        $.type_body,
      ),
    service_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "service",
        field("name", $._type_identifier),
        optional($.type_parameters),
        "as",
        field("api", $.type),
        $.type_body,
      ),
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

    conformance_clause: ($) => seq(":", commaSep1($.type)),
    type_body: ($) =>
      seq(
        "{",
        repeat(
          choice(
            $.field_declaration,
            $.function_declaration,
            $.const_declaration,
            $.deinit_declaration,
          ),
        ),
        "}",
      ),
    protocol_body: ($) => seq("{", repeat($.function_declaration), "}"),
    enum_body: ($) => seq("{", repeat(choice($.enum_case, $.function_declaration)), "}"),

    field_declaration: ($) =>
      seq(
        optional("export"),
        optional("var"),
        optional(field("storage_modifier", choice("atomic", $.behavior_identifier))),
        field("name", $.identifier),
        optional(seq(":", field("type", $.type))),
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
        optional($.type_parameters),
        "=",
        field("value", $.type),
        optional(
          seq(
            "where",
            field("predicate", $._expression),
          ),
        ),
        optional(";"),
      ),

    alias_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "alias",
        field("name", $._type_identifier),
        optional($.type_parameters),
        "=",
        field("value", $.type),
        optional(";"),
      ),

    dimension_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "dimension",
        field("name", $._type_identifier),
        optional(";"),
      ),

    unit_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "unit",
        field("name", $.identifier),
        choice(
          seq(":", field("dimension", $.type)),
          seq("=", field("value", $._expression)),
        ),
        optional(";"),
      ),

    extension_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "extension",
        field("extended_type", $.type),
        optional($.conformance_clause),
        $.type_body,
      ),

    behavior_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "behavior",
        field("name", $._type_identifier),
        optional($.type_parameters),
        "for",
        field("logical_type", $.type),
        $.behavior_body,
      ),

    behavior_body: ($) =>
      seq(
        "{",
        repeat(
          choice(
            $.behavior_storage_declaration,
            $.behavior_value_declaration,
            $.behavior_accessor,
            $.function_declaration,
          ),
        ),
        "}",
      ),
    behavior_storage_declaration: ($) =>
      seq(
        "storage",
        "var",
        field("name", $.identifier),
        ":",
        field("type", $.type),
        optional(seq("=", field("value", $._expression))),
        optional(";"),
      ),
    behavior_value_declaration: ($) =>
      seq(field("name", $.identifier), optional(";")),
    behavior_accessor: ($) =>
      seq(
        optional("mut"),
        field("kind", choice("init", "get", "set", "modify")),
        optional($.behavior_parameter_list),
        field("body", $.block),
      ),
    behavior_parameter_list: ($) =>
      seq("(", commaSep($.behavior_parameter), optional(","), ")"),
    behavior_parameter: ($) =>
      seq(
        field("name", $.identifier),
        optional(seq(":", field("type", $.type))),
      ),

    entry_declaration: ($) =>
      seq(
        "entry",
        choice(
          field("body", $.block),
          seq(
            field("name", $._type_identifier),
            "{",
            repeat($.entry_binding),
            "}",
          ),
        ),
      ),
    entry_binding: ($) =>
      seq(
        field("slot", $.module_path),
        "=",
        field("handler", $.identifier),
        optional(";"),
      ),

    deinit_declaration: ($) => seq("deinit", field("body", $.block)),

    foreign_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "foreign",
        field("language", $.identifier),
        optional(seq("from", field("source", $.string_literal))),
        "{",
        repeat(
          choice(
            $.foreign_type_declaration,
            $.foreign_struct_declaration,
            $.function_declaration,
          ),
        ),
        "}",
      ),
    foreign_type_declaration: ($) => seq("type", field("name", $._type_identifier), optional(";")),
    foreign_struct_declaration: ($) =>
      seq(
        "struct",
        field("name", $._type_identifier),
        "{",
        repeat($.foreign_field_declaration),
        "}",
      ),
    foreign_field_declaration: ($) =>
      seq(
        field("name", $.identifier),
        ":",
        field("type", $.type),
        optional(";"),
      ),

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

    test_declaration: ($) =>
      seq(
        "test",
        field("name", $.string_literal),
        optional(seq("for", field("subject", $.identifier))),
        field("body", $.block),
      ),

    type: ($) =>
      prec.right(
        seq(
          optional(field("qualifier", choice("any", "some", "shared", "weak"))),
          choice(
            seq(
              field("base", $.type_name),
              optional($.type_arguments),
            ),
            field("base", $.tuple_type),
            field("base", $.fixed_array_type),
          ),
          optional("?"),
        ),
      ),
    type_name: ($) =>
      prec.right(seq(field("name", $._type_identifier), repeat(seq(".", field("member", $._type_identifier))))),
    _type_identifier: ($) => alias($.identifier, $.type_identifier),
    type_arguments: ($) =>
      seq("<", commaSep1($.type_argument), optional(","), ">"),
    type_argument: ($) =>
      choice(
        $.type,
        $.number_literal,
        seq(
          field("label", $.identifier),
          ":",
          field("value", $.static_argument_value),
        ),
      ),
    static_argument_value: ($) =>
      choice(
        $.type,
        $.number_literal,
        $.boolean_literal,
        $.string_literal,
        $.static_array_literal,
      ),
    static_array_literal: ($) =>
      seq(
        "[",
        commaSep(choice($.identifier, $.number_literal, $.static_array_literal)),
        optional(","),
        "]",
      ),
    fixed_array_type: ($) =>
      seq(
        "[",
        field("element", $.type),
        ";",
        field("count", choice($.identifier, $.number_literal)),
        "]",
      ),
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
        $.do_statement,
        $.break_statement,
        $.continue_statement,
        $.cancel_statement,
        $.expression_statement,
      ),

    binding_declaration: ($) =>
      seq(
        optional(field("task_kind", choice("async", "spawn"))),
        optional(
          seq(
            "on",
            field("execution_domain", $.enum_literal),
          ),
        ),
        field("kind", choice("let", "var")),
        optional(field("storage_modifier", choice("atomic", $.behavior_identifier))),
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
    cancel_statement: ($) =>
      seq(
        "cancel",
        field("task", $._expression),
        optional(seq(",", "reason", ":", field("reason", $._expression))),
        optional(";"),
      ),
    defer_statement: ($) => seq("defer", optional("async"), $.block),
    guard_statement: ($) => seq("guard", choice($.optional_binding, $._expression), "else", choice($.block, $._statement)),
    if_statement: ($) =>
      prec.right(seq("if", choice($.optional_binding, $._expression), $.block, optional(seq("else", choice($.if_statement, $.block))))),
    while_statement: ($) => seq("while", $._expression, $.block),
    for_statement: ($) => seq("for", field("pattern", $.pattern), "in", field("value", $._expression), $.block),
    optional_binding: ($) =>
      seq(
        field("kind", choice("let", "var")),
        field("name", $.identifier),
        "=",
        field("value", $._expression),
      ),

    switch_expression: ($) =>
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
        $.range_pattern,
        $.identifier,
        $.enum_pattern,
        $.tuple_pattern,
        $.number_literal,
        $.string_literal,
        $.boolean_literal,
        seq("let", $.identifier),
        "_",
      ),
    range_pattern: ($) =>
      choice(
        prec(
          1,
          seq(
            field("lower", $.number_literal),
            field("operator", choice("...", "..<", ">..", ">..<")),
            field("upper", $.number_literal),
          ),
        ),
        seq(field("operator", choice("...", "..<")), field("upper", $.number_literal)),
        seq(field("lower", $.number_literal), field("operator", choice("...", ">.."))),
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
        $.generic_application_expression,
        $.member_expression,
        $.optional_member_expression,
        $.index_expression,
        $.closure_expression,
        $.capture_expression,
        $.unsafe_expression,
        $.switch_expression,
        $.parenthesized_expression,
        $.tuple_expression,
        $.array_literal,
        $.map_literal,
        $.enum_literal,
        $.quantity_literal,
        $.unit_suffix_literal,
        $.size_literal,
        $.number_literal,
        $.string_literal,
        $.raw_string_literal,
        $.multiline_string_literal,
        $.scalar_literal,
        $.byte_literal,
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
        prec.right(
          14,
          seq(field("left", $._expression), field("operator", "**"), field("right", $._expression)),
        ),
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
    generic_application_expression: ($) =>
      prec.left(
        16,
        seq(
          field("function", choice($.identifier, $.member_expression)),
          field("arguments", $.generic_call_arguments),
        ),
      ),
    generic_call_arguments: ($) =>
      seq(
        token.immediate("<"),
        commaSep1(
          choice(
            $.type,
            seq(
              field("label", $.identifier),
              ":",
              field("value", $.static_argument_value),
            ),
          ),
        ),
        optional(","),
        ">",
      ),
    argument_list: ($) => seq("(", commaSep($.argument), optional(","), ")"),
    argument: ($) =>
      seq(optional(seq(field("label", $.identifier), ":")), field("value", $._expression)),
    member_expression: ($) =>
      prec.left(15, seq(field("object", $._expression), ".", field("property", $.identifier))),
    optional_member_expression: ($) =>
      prec.left(15, seq(field("object", $._expression), "?.", field("property", $.identifier))),
    index_expression: ($) =>
      prec.left(
        15,
        seq(
          field("object", $._expression),
          "[",
          commaSep1(field("index", $._expression)),
          optional(","),
          "]",
        ),
      ),

    closure_expression: ($) =>
      prec.right(seq($.closure_parameters, "=>", choice($._expression, $.block))),
    capture_expression: ($) =>
      prec.right(
        seq(
          "capture",
          "(",
          commaSep($.capture_item),
          optional(","),
          ")",
          $.closure_expression,
        ),
      ),
    capture_item: ($) =>
      seq(
        field("mode", choice("copy", "ref", "take", "weak")),
        field("name", $.identifier),
      ),
    unsafe_expression: ($) => prec.right(seq("unsafe", $.block)),
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

    quantity_literal: (_) =>
      token(
        prec(
          2,
          /-?[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?<[^>\r\n]+>/,
        ),
      ),
    unit_suffix_literal: (_) => token(/[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?(?:C|F|km)/),
    size_literal: (_) => token(/[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?(?:B|KiB|MiB|GiB)/),
    number_literal: (_) =>
      token(
        choice(
          /0[xX][0-9a-fA-F](?:_?[0-9a-fA-F])*(?:_[A-Za-z][A-Za-z0-9]*)?/,
          /0[bB][01](?:_?[01])*(?:_[A-Za-z][A-Za-z0-9]*)?/,
          /[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?(?:_[A-Za-z][A-Za-z0-9]*)?/,
        ),
      ),
    string_literal: (_) => token(/"([^"\\\r\n]|\\.)*"/),
    raw_string_literal: (_) => token(/#"(?:[^"]|"[^#])*"#/),
    multiline_string_literal: (_) => token(/"""([^"\r]|"[^"\r]|""[^"\r])*"""/),
    scalar_literal: (_) => token(/'(?:[^'\\\r\n]|\\.)'/),
    byte_literal: (_) => token(/b'(?:[\x20-\x26\x28-\x7e]|\\(?:x[0-9A-Fa-f]{2}|[\\'nrt0]))'/),
    boolean_literal: (_) => choice("true", "false"),

    // Exact keyword tokens win in their syntactic positions. `word` lets
    // Tree-sitter build the keyword table from this shared identifier token.
    identifier: (_) => /[A-Za-z_][A-Za-z0-9_]*/,
    behavior_identifier: (_) => /[A-Z][A-Za-z0-9_]*/,

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
