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
  "region",
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
  "pin",
  "ref",
  "shared",
  "spawn",
  "static",
  "take",
  "throws",
  "try",
  "unsafe",
  "var",
  "view",
  "weak",
];

const OTHER_KEYWORDS = [
  "any",
  "as",
  "each",
  "false",
  "from",
  "get",
  "import",
  "in",
  "init",
  "is",
  "modify",
  "set",
  "some",
  "storage",
  "true",
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
    [$._type_identifier, $.pattern],
    [$.tuple_type, $.unit_literal],
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
        optional("const"),
        optional("unsafe"),
        optional(field("receiver_modifier", choice("mut", "take"))),
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

    language_tag: ($) =>
      seq(
        "<",
        choice(
          field("language", $.identifier),
          seq("lang", ":", field("language", $.contextual_member_expression)),
        ),
        ">",
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
        field("type", alias($.non_borrowed_type, $.type)),
        optional(field("rest", $.rest_marker)),
        optional(seq("=", field("default", $._expression))),
      ),
    rest_marker: (_) => "...",

    type_parameters: ($) => seq("<", commaSep1($.type_parameter), optional(","), ">"),
    type_parameter: ($) =>
      choice(
        seq(
          "const",
          optional(field("external_label", "_")),
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
      seq(
        optional($.declaration_prefix),
        "protocol",
        field("name", $._type_identifier),
        optional($.primary_associated_types),
        optional($.conformance_clause),
        $.protocol_body,
      ),
    enum_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "enum",
        field("name", $._type_identifier),
        optional($.type_parameters),
        optional(seq(":", field("conformance", $.type))),
        $.enum_body,
      ),

    primary_associated_types: ($) =>
      seq(
        "<",
        commaSep1(
          seq(
            field("name", $._type_identifier),
            optional(seq(":", field("constraint", $.type))),
          ),
        ),
        optional(","),
        ">",
      ),
    conformance_clause: ($) => seq(":", $.type),
    type_body: ($) =>
      seq(
        "{",
        repeat(
          choice(
            $.field_declaration,
            $.computed_property_declaration,
            $.initializer_declaration,
            $.function_declaration,
            $.const_declaration,
            $.alias_declaration,
            $.deinit_declaration,
          ),
        ),
        "}",
      ),
    protocol_body: ($) =>
      seq(
        "{",
        repeat(
          choice(
            $.function_declaration,
            $.property_requirement,
            $.associated_type_requirement,
            $.associated_const_requirement,
          ),
        ),
        "}",
      ),
    enum_body: ($) =>
      seq(
        "{",
        repeat(
          choice(
            $.enum_case,
            $.computed_property_declaration,
            $.function_declaration,
            $.const_declaration,
            $.alias_declaration,
          ),
        ),
        "}",
      ),

    associated_type_requirement: ($) =>
      seq(
        "type",
        field("name", $._type_identifier),
        optional(seq(":", field("constraint", $.type))),
        optional(";"),
      ),
    associated_const_requirement: ($) =>
      seq(
        "const",
        field("name", $.identifier),
        ":",
        field("type", $.type),
        optional(";"),
      ),

    initializer_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        optional("const"),
        optional("unsafe"),
        "init",
        field("parameters", $.parameter_list),
        optional(seq("throws", field("error_type", $.type))),
        field("body", $.block),
      ),

    field_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        optional("var"),
        optional(field("storage_modifier", choice("atomic", $.behavior_identifier))),
        field("name", $.identifier),
        optional(seq(":", field("type", $.type))),
        optional(seq("=", field("value", $._expression))),
        optional(";"),
      ),

    computed_property_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        optional("var"),
        field("name", $.identifier),
        ":",
        field("type", $.type),
        field("accessors", $.property_accessor_body),
      ),
    property_accessor_body: ($) =>
      seq(
        "{",
        field("getter", $.get_accessor),
        optional(field("setter", $.set_accessor)),
        optional(field("modifier", $.modify_accessor)),
        "}",
      ),
    get_accessor: ($) => seq("get", $.accessor_implementation),
    set_accessor: ($) =>
      seq(
        "set",
        "(",
        field("parameter", $.identifier),
        ")",
        $.accessor_implementation,
      ),
    modify_accessor: ($) => seq("modify", field("body", $.block)),
    accessor_implementation: ($) =>
      choice(
        field("body", $.block),
        seq("=>", field("value", $._expression), optional(";")),
      ),

    property_requirement: ($) =>
      seq(
        optional("var"),
        field("name", $.identifier),
        ":",
        field("type", $.type),
        "{",
        "get",
        optional("set"),
        optional("modify"),
        "}",
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
        optional($.type_parameters),
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
          optional(
            field(
              "qualifier",
              choice("any", "some", "shared", "weak", "ref", "inout", "view", seq("inout", "view")),
            ),
          ),
          $._type_core,
          repeat(seq("&", field("composition", $._type_core))),
          optional("?"),
        ),
      ),
    non_borrowed_type: ($) =>
      prec.right(
        seq(
          optional(field("qualifier", choice("any", "some", "shared", "weak", "view"))),
          $._type_core,
          repeat(seq("&", field("composition", $._type_core))),
          optional("?"),
        ),
      ),
    _type_core: ($) =>
      choice(
        seq(
          field("base", $.type_name),
          repeat($.type_arguments),
        ),
        field("base", $.tuple_type),
        field("base", $.fixed_array_type),
        field("base", $.function_type),
      ),
    type_name: ($) =>
      prec.right(seq(field("name", $._type_identifier), repeat(seq(".", field("member", $._type_identifier))))),
    _type_identifier: ($) => alias($.identifier, $.type_identifier),
    type_arguments: ($) =>
      seq("<", commaSep1($.type_argument), optional(","), ">"),
    type_argument: ($) =>
      choice(
        $.contract_expression_argument,
        $.static_record_literal,
        $.static_array_literal,
        $.type,
        $.number_literal,
        $.contextual_member_expression,
        seq(
          field("label", $.identifier),
          ":",
          field("value", $.static_argument_value),
        ),
      ),
    static_argument_value: ($) =>
      choice(
        $.contract_expression_argument,
        $.static_record_literal,
        prec(1, $.identifier),
        $.type,
        $.number_literal,
        $.boolean_literal,
        $.string_literal,
        $.contextual_member_expression,
        $.static_array_literal,
      ),
    contract_expression_argument: ($) =>
      seq(
        "(",
        field("expression", choice($._expression, $.one_sided_range_expression)),
        ")",
      ),
    static_record_literal: ($) =>
      seq(
        "{",
        commaSep(
          seq(
            field("name", $.identifier),
            ":",
            field("value", $.static_argument_value),
          ),
        ),
        optional(","),
        "}",
      ),
    static_array_literal: ($) =>
      seq(
        "[",
        commaSep($.static_argument_value),
        optional(","),
        "]",
      ),
    fixed_array_type: ($) =>
      seq(
        "[",
        field("element", $.type),
        ";",
        field("count", $._expression),
        "]",
      ),
    tuple_type: ($) => seq("(", commaSep($.type), optional(","), ")"),
    function_type: ($) =>
      choice(
        prec.right(
          3,
          seq(
            optional("unsafe"),
            field("callable_mode", choice("mut", "take")),
            optional("async"),
            $._function_type_signature,
          ),
        ),
        prec.right(
          2,
          seq(
            optional("unsafe"),
            optional("async"),
            $._function_type_signature,
          ),
        ),
      ),
    _function_type_signature: ($) =>
      prec.right(
        seq(
          "fn",
          optional(field("contract", $.type_arguments)),
          "(",
          commaSep($.function_type_parameter),
          optional(","),
          ")",
          optional(seq(":", field("return_type", $.type))),
          optional(seq("throws", field("error_type", $.type))),
        ),
      ),
    function_type_parameter: ($) =>
      seq(
        optional(field("ownership", choice("ref", "inout", "take"))),
        field("type", alias($.non_borrowed_type, $.type)),
        optional(field("rest", $.rest_marker)),
      ),

    block: ($) => seq("{", repeat($._statement), "}"),
    _statement: ($) =>
      choice(
        $.binding_declaration,
        $.return_statement,
        $.throw_statement,
        $.defer_statement,
        $.guard_statement,
        $.region_statement,
        $.if_statement,
        $.while_statement,
        $.for_statement,
        $.do_statement,
        $.break_statement,
        $.continue_statement,
        $.expression_statement,
      ),

    binding_declaration: ($) =>
      seq(
        optional(
          seq(
            field("task_kind", choice("async", "spawn")),
            optional(field("task_contract", $.task_contract)),
          ),
        ),
        field("kind", choice("let", "var")),
        optional(field("storage_modifier", choice("atomic", $.behavior_identifier))),
        optional(field("pattern_ownership", choice("ref", "inout"))),
        field("pattern", $.pattern),
        optional(seq(":", field("type", $.type))),
        optional(seq("=", field("value", $._expression))),
        optional(";"),
      ),
    task_contract: ($) =>
      seq(
        "<",
        commaSep1(
          choice(
            field("primary", $.contextual_member_expression),
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

    return_statement: ($) =>
      prec.right(seq("return", optional($._expression), optional(";"))),
    throw_statement: ($) => seq("throw", $._expression, optional(";")),
    break_statement: (_) => seq("break", optional(";")),
    continue_statement: (_) => seq("continue", optional(";")),
    defer_statement: ($) => seq("defer", optional("async"), $.block),
    guard_statement: ($) => seq("guard", choice($.optional_binding, $._expression), "else", choice($.block, $._statement)),
    region_statement: ($) =>
      seq(
        "region",
        field("name", $.identifier),
        optional(field("options", $.argument_list)),
        field("body", $.block),
      ),
    if_statement: ($) =>
      prec.right(seq("if", choice($.optional_binding, $._expression), $.block, optional(seq("else", choice($.if_statement, $.block))))),
    while_statement: ($) =>
      seq("while", choice($.optional_binding, $._expression), $.block),
    for_statement: ($) =>
      seq(
        "for",
        optional($._asynchronous_iteration_effects),
        optional(field("ownership", choice("ref", "inout", "copy"))),
        field("pattern", $.pattern),
        "in",
        field("value", $._expression),
        $.block,
      ),
    optional_binding: ($) =>
      seq(
        field("kind", choice("let", "var")),
        optional(field("ownership", choice("ref", "inout", "copy"))),
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
        optional(seq("if", field("guard", $._expression))),
        ":",
        repeat($._statement),
      ),

    do_statement: ($) => seq("do", $.block, repeat1($.catch_clause)),
    catch_clause: ($) =>
      seq(
        "catch",
        optional(
          seq(
            field("pattern", $.pattern),
            optional(seq("if", field("guard", $._expression))),
          ),
        ),
        $.block,
      ),

    expression_statement: ($) => seq($._expression, optional(";")),

    pattern: ($) =>
      choice(
        $.range_pattern,
        $.identifier,
        $.enum_pattern,
        $.struct_pattern,
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
    struct_pattern: ($) =>
      seq(
        field("type", $.type_name),
        "(",
        optional(
          seq(
            choice(
              seq(
                commaSep1($.struct_pattern_field),
                optional(seq(",", $.rest_pattern)),
              ),
              $.rest_pattern,
            ),
            optional(","),
          ),
        ),
        ")",
      ),
    struct_pattern_field: ($) =>
      choice(
        $.shorthand_struct_pattern_field,
        $.labeled_struct_pattern_field,
      ),
    shorthand_struct_pattern_field: ($) =>
      field("name", $.identifier),
    labeled_struct_pattern_field: ($) =>
      seq(
        field("field", $.identifier),
        ":",
        field("pattern", $.pattern),
      ),
    rest_pattern: (_) => "...",
    tuple_pattern: ($) => seq("(", commaSep1($.pattern), optional(","), ")"),

    _expression: ($) =>
      choice(
        $.assignment_expression,
        $.bounded_range_expression,
        $.binary_expression,
        $.unary_expression,
        $.optional_try_expression,
        $.optional_propagation_expression,
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
        $.unit_literal,
        $.parenthesized_expression,
        $.tuple_expression,
        $.repeat_array_literal,
        $.array_literal,
        $.map_literal,
        $.contextual_member_expression,
        $.quantity_literal,
        $.unit_suffix_literal,
        $.size_literal,
        $.number_literal,
        $.string_literal,
        $.multiline_string_literal,
        $.raw_string_literal,
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

    bounded_range_expression: ($) =>
      prec.dynamic(
        1,
        prec.left(
          9,
          seq(
            field("lower", $._expression),
            field("operator", choice("...", "..<", ">..", ">..<")),
            field("upper", $._expression),
          ),
        ),
      ),

    one_sided_range_expression: ($) =>
      choice(
        prec.left(
          9,
          seq(
            field("lower", $._expression),
            field("operator", choice("...", ">..")),
          ),
        ),
        prec.right(
          9,
          seq(
            field("operator", choice("...", "..<")),
            field("upper", $._expression),
          ),
        ),
      ),

    binary_expression: ($) =>
      choice(
        prec.right(
          14,
          seq(field("left", $._expression), field("operator", "**"), field("right", $._expression)),
        ),
        prec.right(
          1,
          seq(field("left", $._expression), field("operator", "??"), field("right", $._expression)),
        ),
        ...BINARY_OPERATORS.map(([operator, precedence]) =>
          prec.left(
            precedence,
            seq(field("left", $._expression), field("operator", operator), field("right", $._expression)),
          ),
        ),
      ),

    unary_expression: ($) =>
      prec.right(
        13,
        seq(
          field("operator", choice("!", "~", "-", "try", "await", "copy", "take", "pin", "inout", "ref")),
          field("operand", $._expression),
        ),
      ),

    optional_try_expression: ($) =>
      prec.right(
        13,
        seq("try", token.immediate("?"), field("operand", $._expression)),
      ),

    _asynchronous_iteration_effects: (_) => seq(optional("try"), "await"),

    optional_propagation_expression: ($) =>
      prec.left(17, seq(field("value", $._expression), token.immediate("?"))),

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
            $.contract_expression_argument,
            $.static_record_literal,
            $.static_array_literal,
            $.contextual_member_expression,
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
      seq(
        optional(seq(field("label", $.identifier), ":")),
        choice(
          seq(
            optional(field("expansion", $.argument_expansion)),
            field("value", $._expression),
          ),
          field("value", $.one_sided_range_expression),
        ),
      ),
    argument_expansion: (_) => "each",
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
          commaSep1(field("index", choice($._expression, $.one_sided_range_expression))),
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

    unit_literal: (_) => seq("(", ")"),
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
    repeat_array_literal: ($) =>
      seq(
        "[",
        field("value", $._expression),
        ";",
        field("count", $._expression),
        "]",
      ),
    array_literal: ($) => seq("[", commaSep($._expression), optional(","), "]"),
    map_literal: ($) => seq("[", commaSep1($.map_entry), optional(","), "]"),
    map_entry: ($) => seq(field("key", $._expression), ":", field("value", $._expression)),
    contextual_member_expression: ($) =>
      prec(16, seq(".", field("member", $.identifier))),

    quantity_literal: (_) =>
      token(
        prec(
          2,
          /[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?(?:[eE][+-]?[0-9](?:_?[0-9])*)?<[^>\r\n]+>/,
        ),
      ),
    unit_suffix_literal: (_) =>
      token(/[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?(?:[eE][+-]?[0-9](?:_?[0-9])*)?(?:C|F|km)/),
    size_literal: (_) =>
      token(/[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?(?:[eE][+-]?[0-9](?:_?[0-9])*)?(?:B|KiB|MiB|GiB)/),
    number_literal: (_) =>
      token(
        choice(
          /0[xX][0-9a-fA-F](?:_?[0-9a-fA-F])*(?:_[A-Za-z][A-Za-z0-9]*)?/,
          /0[bB][01](?:_?[01])*(?:_[A-Za-z][A-Za-z0-9]*)?/,
          /0[oO][0-7](?:_?[0-7])*(?:_[A-Za-z][A-Za-z0-9]*)?/,
          /[0-9](?:_?[0-9])*(?:\.[0-9](?:_?[0-9])*)?(?:[eE][+-]?[0-9](?:_?[0-9])*)?(?:_[A-Za-z][A-Za-z0-9]*)?/,
        ),
      ),
    // Tree-sitter keeps text and byte strings in one lexical node. Consumers
    // can inspect the `b` prefix. The compiler lexer emits distinct tokens.
    string_literal: (_) =>
      token(
        choice(
          /"([^"\\\r\n]|\\.)*"/,
          /b"(?:[\x20-\x21\x23-\x5b\x5d-\x7e]|\\(?:x[0-9A-Fa-f]{2}|[\\\"nrt0]))*"/,
        ),
      ),
    raw_string_literal: (_) =>
      token(
        choice(
          /#"(?:[^"\r\n]|"[^#\r\n])*"#/,
          /#"""([^"\r]|"[^"\r]|""[^"\r])*"""#/,
        ),
      ),
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
