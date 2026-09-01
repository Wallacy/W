// Candidate executable grammar for W's surface syntax.
// DESIGN.md owns language decisions.

const DECLARATION_KEYWORDS = [
  "alias",
  "allocator",
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
  "commit",
  "continue",
  "defer",
  "deinit",
  "do",
  "else",
  "for",
  "guard",
  "if",
  "lock",
  "pipeline",
  "repeat",
  "return",
  "switch",
  "throw",
  "while",
  "yield",
];

const MODIFIER_KEYWORDS = [
  "async",
  "atomic",
  "await",
  "const",
  "copy",
  "export",
  "inout",
  "let",
  "mut",
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
  "module",
  "domain",
  "package",
  "modify",
  "set",
  "some",
  "stream",
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

const ASSIGNMENT_OPERATORS = [
  "=",
  "+=",
  "-=",
  "*=",
  "/=",
  "%=",
  "**=",
  "<<=",
  ">>=",
  "&=",
  "^=",
  "|=",
];

module.exports = grammar({
  name: "w",

  externals: ($) => [
    $.foreign_body_content,
    $._foreign_body_error_sentinel,
  ],

  extras: ($) => [/[\s\uFEFF\u2060\u200B]/, $.comment],

  word: ($) => $.identifier,

  conflicts: ($) => [
    [$._expression, $.closure_parameter],
    [$._type_identifier, $._expression],
    [$._type_identifier, $._expression, $.closure_parameter],
    [$._type_identifier, $.pattern],
    [$._type_identifier, $.enum_pattern],
    [$.type_name, $.enum_pattern],
    [$.if_statement, $.if_expression],
    [$.reexport_item, $.export_item],
    [$.labeled_tuple_type_element, $.closure_parameter],
    [$.tuple_type, $.unit_literal],
  ],

  rules: {
    source_file: ($) =>
      choice(
        seq(
          optional($.module_header),
          repeat(
            choice(
              $.service_import_statement,
              $.domain_import_statement,
              $.import_statement,
              $.reexport_declaration,
            ),
          ),
          repeat($._declaration),
        ),
        $.build_manifest,
      ),

    module_header: ($) =>
      seq(
        "module",
        field("name", $.identifier),
        optional(field("contract", $.module_contract)),
        optional(";"),
      ),
    module_contract: ($) =>
      seq(token.immediate("<"), commaSep1($.manifest_argument), optional(","), ">"),

    domain_import_statement: ($) =>
      seq(
        "import",
        "domain",
        field("items", $.named_imports),
        "from",
        field("module", $.module_path),
        optional(";"),
      ),

    service_import_statement: ($) =>
      prec(
        1,
        seq(
          "import",
          "service",
          choice(
            seq(
              field("items", $.named_service_imports),
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
      ),

    named_service_imports: ($) =>
      seq("{", commaSep1($.service_import_item), optional(","), "}"),
    service_import_item: ($) =>
      seq(
        field("name", $.identifier),
        optional(seq("as", field("alias", $.identifier))),
        optional($.service_key_contract),
      ),
    service_key_contract: ($) =>
      seq(
        token.immediate("<"),
        "key",
        ":",
        field("key_type", $.type),
        optional(","),
        ">",
      ),

    import_statement: ($) =>
      seq(
        "import",
        choice(
          seq(
            field("items", $.wildcard_import),
            "from",
            field("module", $.module_path),
          ),
          seq(
            field("items", $.named_imports),
            "from",
            field("module", $.module_path),
          ),
          seq(
            field("binding", $.identifier),
            "from",
            field("origin", $.module_path),
          ),
          field("module", $.module_path),
        ),
        optional(";"),
      ),

    reexport_declaration: ($) =>
      seq(
        "export",
        choice(
          seq("*", "from", field("module", $.module_path)),
          seq(
            "{",
            commaSep1($.reexport_item),
            optional(","),
            "}",
            "from",
            field("module", $.module_path),
          ),
        ),
        optional(";"),
      ),
    reexport_item: ($) =>
      seq(field("name", $.identifier), optional(seq("as", field("alias", $.identifier)))),

    wildcard_import: (_) => "*",
    named_imports: ($) => seq("{", commaSep1($.import_item), optional(","), "}"),
    import_item: ($) =>
      seq(field("name", $.identifier), optional(seq("as", field("alias", $.identifier)))),
    module_path: ($) => prec.right(seq($.identifier, repeat(seq(".", $.identifier)))),

    _declaration: ($) =>
      choice(
        $.export_list_declaration,
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

    export_list_declaration: ($) =>
      seq(
        "export",
        "{",
        commaSep1($.export_item),
        optional(","),
        "}",
        optional(";"),
      ),
    export_item: ($) => field("name", $.identifier),

    declaration_prefix: (_) => "export",

    _function_prefix: ($) =>
      seq(
        optional($.declaration_prefix),
        optional("static"),
        optional("const"),
        optional("unsafe"),
        optional(field("receiver_modifier", choice("mut", "take"))),
        optional("async"),
        "fn",
      ),
    _function_tail: ($) =>
      seq(
        field("name", $.identifier),
        optional($.generic_parameters),
        field("parameters", $.parameter_list),
        optional(seq(":", field("return_type", $.type))),
        optional(seq("throws", field("error_type", $.type))),
        optional($.borrow_clause),
      ),
    _function_header: ($) =>
      seq($._function_prefix, optional($.abi_contract), $._function_tail),
    _foreign_function_header: ($) =>
      seq($._function_prefix, $.language_tag, $._function_tail),
    function_declaration: ($) =>
      choice(
        seq($._function_header, field("body", $.block)),
        seq($._foreign_function_header, field("body", $.foreign_body)),
      ),
    function_signature: ($) =>
      seq($._function_header, optional(";")),

    foreign_body: ($) =>
      seq("{", optional($.foreign_body_content), "}"),

    language_tag: ($) =>
      seq(
        token.immediate("<"),
        choice(
          field("language", $.identifier),
          seq("lang", ":", field("language", $.contextual_member_expression)),
        ),
        repeat(seq(",", field("option", $.type_argument))),
        optional(","),
        ">",
      ),

    abi_contract: ($) =>
      seq(
        token.immediate("<"),
        "abi",
        ":",
        field("abi", $.contextual_member_expression),
        optional(","),
        ">",
      ),

    parameter_list: ($) => seq("(", commaSep($.parameter), optional(","), ")"),
    borrow_clause: ($) =>
      seq(
        "borrows",
        "(",
        commaSep1($.borrow_pair),
        optional(","),
        ")",
      ),
    borrow_pair: ($) =>
      seq(
        field("result", $.slot_ref),
        ":",
        "[",
        commaSep1(field("source", $.slot_ref)),
        optional(","),
        "]",
      ),
    slot_ref: ($) => choice($.identifier, $.number_literal),
    parameter: ($) =>
      choice(
        seq(
          field("name", alias("allocator", $.identifier)),
          ":",
          optional(
            choice(
              field("ownership", choice("ref", "inout", "take")),
              field("const_requirement", "const"),
            ),
          ),
          field("type", alias($.non_borrowed_type, $.type)),
        ),
        seq(
          "allocator",
          field("name", $.identifier),
          ":",
          optional(
            choice(
              field("ownership", choice("ref", "inout", "take")),
              field("const_requirement", "const"),
            ),
          ),
          field("type", alias($.non_borrowed_type, $.type)),
        ),
      seq(
        choice(
          field("name", $.identifier),
          seq(field("label", choice($.identifier, "_")), field("name", $.identifier)),
        ),
        ":",
        optional(
          choice(
            field("ownership", choice("ref", "inout", "take")),
            field("const_requirement", "const"),
          ),
        ),
        field("type", alias($.non_borrowed_type, $.type)),
        optional(field("rest", $.rest_marker)),
        optional(seq("=", field("default", $._expression))),
        ),
      ),
    rest_marker: (_) => "...",

    generic_parameters: ($) =>
      seq(token.immediate("<"), commaSep1($.generic_parameter), optional(","), ">"),
    generic_parameter: ($) =>
      choice(
        seq(
          field("label_omission", "_"),
          field("name", $.identifier),
          ":",
          field("domain", $.type),
        ),
        seq(
          field("name", $.identifier),
          optional(seq(":", field("domain", $.type))),
        ),
      ),

    struct_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "struct",
        field("name", $._type_identifier),
        optional($.generic_parameters),
        optional($.conformance_clause),
        $.type_body,
      ),
    object_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "object",
        field("name", $._type_identifier),
        optional($.generic_parameters),
        optional($.conformance_clause),
        $.type_body,
      ),
    service_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "service",
        field("name", $.identifier),
        optional($.service_key_contract),
        ":",
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
        optional($.generic_parameters),
        optional(seq(":", field("conformance", $.type))),
        $.enum_body,
      ),

    primary_associated_types: ($) =>
      seq(
        token.immediate("<"),
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
            alias($.function_signature, $.function_declaration),
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
        optional($.generic_parameters),
        "=",
        field("value", $.type),
        optional(";"),
      ),

    alias_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "alias",
        field("name", $._type_identifier),
        optional($.generic_parameters),
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
        "extension",
        optional($.generic_parameters),
        field("extended_type", $.type),
        optional($.conformance_clause),
        $.type_body,
      ),

    behavior_declaration: ($) =>
      seq(
        optional($.declaration_prefix),
        "behavior",
        field("name", $._type_identifier),
        optional($.generic_parameters),
        "for",
        field("logical_type", $.type),
        choice(
          field("body", $.behavior_body),
          seq("=", field("composition", $.behavior_composition)),
        ),
        optional(";"),
      ),

    // A composition is nominal and closed. Each component has a stable alias
    // so inherited facets remain qualified at every use site.
    behavior_composition: ($) =>
      seq("(", commaSep1($.behavior_component), optional(","), ")"),
    behavior_component: ($) =>
      seq(field("alias", $.identifier), ":", field("behavior", $.type)),

    behavior_body: ($) =>
      seq(
        "{",
        repeat(
          choice(
            $.behavior_field_declaration,
            $.behavior_accessor,
            $.behavior_facet_property,
            $.behavior_hook,
            $.function_declaration,
          ),
        ),
        "}",
      ),
    behavior_field_declaration: ($) =>
      seq(
        "var",
        field("name", $.identifier),
        ":",
        field("type", $.type),
        optional(seq("=", field("value", $._expression))),
        optional(";"),
      ),
    behavior_accessor: ($) =>
      choice(
        $.behavior_initializer,
        seq(
          optional("mut"),
          field("kind", choice("get", "set", "modify")),
          optional($.behavior_parameter_list),
          field("body", $.block),
        ),
      ),
    behavior_initializer: ($) =>
      seq(
        "init",
        field("parameters", $.behavior_initializer_parameters),
        field("body", $.block),
      ),
    behavior_initializer_parameters: ($) =>
      seq(
        "(",
        optional(
          seq(
            "initialValue",
            ":",
            "fn",
            "(",
            ")",
            ":",
            field("type", $.type),
          ),
        ),
        ")",
      ),
    behavior_parameter_list: ($) =>
      seq("(", commaSep($.behavior_parameter), optional(","), ")"),
    behavior_parameter: ($) =>
      seq(
        field("name", $.identifier),
        optional(seq(":", field("type", $.type))),
      ),
    behavior_facet_property: ($) =>
      seq(
        "export",
        optional("var"),
        field("name", $.identifier),
        ":",
        field("type", $.type),
        field("accessors", $.property_accessor_body),
      ),
    behavior_hook: ($) =>
      seq(
        optional(field("receiver_modifier", "mut")),
        field("kind", choice("willSet", "didSet", "willModify", "didModify")),
        field("parameters", $.behavior_hook_parameters),
        field("body", $.block),
      ),
    behavior_hook_parameters: ($) =>
      choice(
        seq(
          "(",
          field("current", $.behavior_ref_parameter),
          ",",
          field("proposed", $.behavior_ref_parameter),
          ")",
        ),
        seq("(", field("current", $.behavior_ref_parameter), ")"),
        seq("(", ")"),
      ),
    behavior_ref_parameter: ($) =>
      seq(
        field("name", $.identifier),
        ":",
        "ref",
        field("type", $.type),
      ),

    entry_declaration: ($) =>
      seq(
        "entry",
        choice(
          field("body", $.block),
          $._entry_default_handler,
          seq(
            field("name", $._type_identifier),
            choice(field("body", $.block), $._entry_default_handler),
          ),
        ),
      ),
    _entry_default_handler: ($) =>
      seq("(", field("default_handler", $.identifier), ")"),

    build_manifest: ($) =>
      choice(
        seq($.package_manifest, optional($.workspace_manifest)),
        seq($.workspace_manifest, optional($.package_manifest)),
      ),
    package_manifest: ($) =>
      prec(
        1,
        seq(
        "package",
        field("body", $.manifest_record),
        ),
      ),
    workspace_manifest: ($) =>
      prec(
        1,
        seq(
        "workspace",
        field("body", $.manifest_record),
        ),
      ),
    manifest_record: ($) =>
      seq("{", repeat($.manifest_field), "}"),
    manifest_field: ($) =>
      seq(
        field("name", $.identifier),
        ":",
        field("value", $.manifest_value),
        optional(","),
      ),
    manifest_value: ($) =>
      choice(
        $.manifest_record,
        $.manifest_list,
        $.manifest_constructor,
        $.contextual_member_expression,
        $.string_literal,
        $.size_literal,
        $.quantity_literal,
        $.number_literal,
        $.boolean_literal,
      ),
    manifest_list: ($) =>
      seq(
        "[",
        repeat(seq($.manifest_value, optional(","))),
        "]",
      ),
    manifest_constructor: ($) =>
      prec(
        17,
        seq(
          field("constructor", $.contextual_member_expression),
          "(",
          commaSep($.manifest_argument),
          optional(","),
          ")",
        ),
      ),
    manifest_argument: ($) =>
      seq(
        optional(seq(field("label", $.identifier), ":")),
        field("value", $.manifest_value),
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
            $.const_declaration,
            alias($.function_signature, $.function_declaration),
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
              choice(
                "any",
                "some",
                "shared",
                "weak",
                "ref",
                "inout",
                "view",
                "const",
                seq("inout", "view"),
              ),
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
      seq(token.immediate("<"), commaSep1($.type_argument), optional(","), ">"),
    type_argument: ($) =>
      choice(
        $.contract_expression_argument,
        $.static_record_literal,
        $.static_array_literal,
        $.type,
        $.number_literal,
        $.boolean_literal,
        $.string_literal,
        $.quantity_literal,
        $.size_literal,
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
        $.quantity_literal,
        $.size_literal,
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
    tuple_type: ($) =>
      choice(
        seq("(", ")"),
        seq(
          "(",
          $.type,
          ",",
          optional(seq(commaSep1($.type), optional(","))),
          ")",
        ),
        seq(
          "(",
          $.labeled_tuple_type_element,
          ",",
          optional(seq(commaSep1($.labeled_tuple_type_element), optional(","))),
          ")",
        ),
      ),
    labeled_tuple_type_element: ($) =>
      seq(
        field("label", $.identifier),
        ":",
        field("type", $.type),
      ),
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
          optional($.borrow_clause),
        ),
      ),
    function_type_parameter: ($) =>
      choice(
        seq(
        "allocator",
        field("name", $.identifier),
        ":",
        optional(
          choice(
            field("ownership", choice("ref", "inout", "take")),
            field("const_requirement", "const"),
          ),
        ),
        field("type", alias($.non_borrowed_type, $.type)),
        ),
        seq(
        optional(
          choice(
            field("ownership", choice("ref", "inout", "take")),
            field("const_requirement", "const"),
          ),
        ),
        field("type", alias($.non_borrowed_type, $.type)),
        optional(field("rest", $.rest_marker)),
        ),
      ),

    block: ($) => seq("{", repeat($._statement), "}"),
    _statement: ($) =>
      choice(
        $.allocator_statement,
        $.binding_declaration,
        $.commit_statement,
        $.return_statement,
        $.throw_statement,
        $.defer_statement,
        $.guard_statement,
        $.if_statement,
        $.labeled_statement,
        $.while_statement,
        $.for_statement,
        $.repeat_statement,
        $.do_statement,
        $.break_statement,
        $.continue_statement,
        $.expression_statement,
        $.yield_statement,
      ),

    yield_statement: ($) =>
      seq(
        "yield",
        field("ownership", choice("take", "copy")),
        field("value", $._expression),
        optional(";"),
      ),

    allocator_statement: ($) =>
      seq(
        optional("try"),
        "allocator",
        optional(seq(field("name", $.identifier), ":")),
        field("plan", choice($.allocator_builtin_plan, $._expression)),
        field("body", $.block),
      ),
    allocator_builtin_plan: ($) =>
      seq(field("kind", $.contextual_member_expression), field("contract", $.type_arguments)),

    binding_declaration: ($) =>
      seq(
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
        token.immediate("<"),
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
    commit_statement: ($) =>
      prec.right(seq("commit", optional($._expression), optional(";"))),
    throw_statement: ($) => seq("throw", $._expression, optional(";")),
    break_statement: ($) =>
      prec.right(seq("break", optional(field("label", $.identifier)), optional(";"))),
    continue_statement: ($) =>
      prec.right(seq("continue", optional(field("label", $.identifier)), optional(";"))),
    defer_statement: ($) => seq("defer", optional("async"), $.block),
    guard_statement: ($) => seq("guard", choice($.optional_binding, $._expression), "else", choice($.block, $._statement)),
    if_statement: ($) =>
      prec.right(seq("if", choice($.optional_binding, $._expression), $.block, optional(seq("else", choice($.if_statement, $.block))))),
    labeled_statement: ($) =>
      seq(
        field("label", $.identifier),
        ":",
        field("statement", choice($.while_statement, $.for_statement, $.repeat_statement, $.block)),
      ),
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
    repeat_statement: ($) =>
      seq("repeat", field("body", $.block), "while", field("condition", $._expression), optional(";")),
    optional_binding: ($) =>
      seq(
        field("kind", choice("let", "var")),
        optional(field("ownership", choice("ref", "inout", "copy"))),
        field("pattern", $.pattern),
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
        $.quantity_literal,
        $.size_literal,
        seq("let", $.identifier),
        "_",
      ),
    range_pattern: ($) =>
      choice(
        prec(
          1,
          seq(
            field("lower", $.pattern_bound),
            field("operator", choice("...", "..<", ">..", ">..<")),
            field("upper", $.pattern_bound),
          ),
        ),
        seq(field("operator", choice("...", "..<")), field("upper", $.pattern_bound)),
        seq(field("lower", $.pattern_bound), field("operator", choice("...", ">.."))),
      ),
    pattern_bound: ($) =>
      choice($.number_literal, $.quantity_literal, $.size_literal),
    enum_pattern: ($) =>
      prec.right(
        seq(
          choice(
            seq(".", field("case", $.identifier)),
            seq(
              field("enum", $._type_identifier),
              repeat(
                seq(
                  ".",
                  field("enum_member", $._type_identifier),
                ),
              ),
              ".",
              field("case", $.identifier),
            ),
          ),
          optional($.enum_payload_pattern),
        ),
      ),
    enum_payload_pattern: ($) =>
      choice(
        seq("(", commaSep1($.pattern), optional(","), ")"),
        seq(
          "(",
          commaSep1($.labeled_pattern_field),
          optional(seq(",", $.rest_pattern)),
          optional(","),
          ")",
        ),
      ),
    labeled_pattern_field: ($) =>
      seq(
        field("label", $.identifier),
        ":",
        field("pattern", $.pattern),
      ),
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
    tuple_pattern: ($) =>
      choice(
        seq("(", $.pattern, ",", ")"),
        seq(
          "(",
          $.pattern,
          ",",
          $.pattern,
          repeat(seq(",", $.pattern)),
          optional(","),
          ")",
        ),
      ),

    _expression: ($) =>
      choice(
        $.assignment_expression,
        $.pipe_forward_expression,
        $.task_expression,
        $.bounded_range_expression,
        $.type_query_expression,
        $.conditional_cast_expression,
        $.binary_expression,
        $.unary_expression,
        $.optional_try_expression,
        $.optional_propagation_expression,
        $.panic_expression,
        $.call_expression,
        $.generic_application_expression,
        $.member_expression,
        $.facet_expression,
        $.optional_member_expression,
        $.index_expression,
        $.closure_expression,
        $.capture_expression,
        $.pipeline_expression,
        $.stream_expression,
        $.lock_expression,
        $.unsafe_expression,
        $.if_expression,
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
        -1,
        seq(field("left", $._expression), field("operator", choice(...ASSIGNMENT_OPERATORS)), field("right", $._expression)),
      ),

    // `|>` has a fixed slot: the right side is always a free-function call
    // template. The semantic checker owns labels, ownership, and modifiers.
    pipe_forward_expression: ($) =>
      prec.left(
        0,
        seq(
          field("left", $._expression),
          "|>",
          field("right", $.pipe_call_template),
        ),
      ),
    pipe_call_template: ($) =>
      seq(
        repeat($.pipe_call_modifier),
        field("function", $.pipe_callable_path),
        optional(field("generic_arguments", $.generic_call_arguments)),
        field("arguments", $.argument_list),
      ),
    // The path is syntactically qualified here, but semantic resolution must
    // prove a free/static namespace. It must never become a value receiver or
    // an extension/UFCS fallback.
    pipe_callable_path: ($) =>
      prec.right(seq($.identifier, repeat(seq(".", $.identifier)))),
    pipe_call_modifier: ($) =>
      choice(
        "try",
        seq("try", token.immediate("?")),
        "await",
        "sync",
        "async",
        seq("spawn", optional(field("task_contract", $.task_contract))),
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

    // Type queries are prefix forms. The resolver, not this grammar, applies
    // the type-namespace-first rule for an unparenthesized subject.
    type_query_expression: ($) =>
      prec.right(
        13,
        seq(
          field("query", choice("type", "info")),
          "of",
          field("subject", choice($.type, $._expression)),
        ),
      ),

    // `as?` recovers a borrowed nominal target. The checker owns its dynamic
    // identity, composition, and lifetime rules.
    conditional_cast_expression: ($) =>
      prec.left(
        8,
        seq(
          field("source", $._expression),
          field("operator", seq("as", token.immediate("?"))),
          field("target", $.type),
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

    task_expression: ($) =>
      prec.right(
        13,
        seq(
          field("task_kind", choice("async", "spawn")),
          optional(field("task_contract", $.task_contract)),
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
        commaSep1($.type_argument),
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
      prec.left(
        15,
        seq(
          field("object", $._expression),
          ".",
          field("property", choice($.identifier, $.tuple_index)),
        ),
      ),
    optional_member_expression: ($) =>
      prec.left(
        15,
        seq(
          field("object", $._expression),
          "?.",
          field("property", choice($.identifier, $.tuple_index)),
        ),
      ),
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
    // `<[...]>` is a contextual closure/stream-capture contract. It is not a
    // StaticList runtime value or a generic argument.
    capture_expression: ($) =>
      prec.right(
        seq(
          "<",
          "[",
          commaSep1($.capture_item),
          optional(","),
          "]",
          ">",
          $.closure_expression,
        ),
      ),
    stream_capture_list: ($) =>
      seq(
        "<",
        "[",
        commaSep($.capture_item),
        optional(","),
        "]",
        ">",
      ),
    capture_item: ($) =>
      seq(
        field("mode", choice("copy", "ref", "take", "weak")),
        field("name", $.identifier),
      ),
    pipeline_expression: ($) =>
      prec.right(
        20,
        seq(
          "pipeline",
          optional(field("contract", $.pipeline_contract)),
          choice(
            field("body", $.block),
            seq(
              "each",
              field("item", $.identifier),
              "in",
              field("source", $._expression),
              field("body", $.block),
            ),
            seq(
              field("binding", $.identifier),
              "=",
              field("source", $._expression),
              field("body", $.block),
            ),
            field("chain", $.pipeline_chain),
          ),
        ),
      ),
    pipeline_chain: ($) =>
      prec.left(
        15,
        seq(
          field("source", choice($.identifier, $.parenthesized_expression)),
          field("first", $.pipeline_call_step),
          repeat1(field("step", $.pipeline_call_step)),
        ),
      ),
    pipeline_call_step: ($) =>
      seq(".", field("function", $.identifier), field("arguments", $.argument_list)),
    pipeline_contract: ($) =>
      seq(
        token.immediate("<"),
        commaSep1($.pipeline_contract_item),
        optional(","),
        ">",
      ),
    pipeline_contract_item: ($) =>
      choice(
        seq(
          "transaction",
          ":",
          field(
            "transaction",
            choice($.pipeline_transaction_contract, $.contextual_member_expression),
          ),
        ),
        seq("tasks", ":", field("tasks", $.pipeline_task_mode)),
        seq("limit", ":", field("limit", $._expression)),
        seq("ordering", ":", field("ordering", $.contextual_member_expression)),
        seq("errors", ":", field("errors", $.contextual_member_expression)),
      ),
    pipeline_transaction_contract: ($) =>
      seq(
        "{",
        "isolation",
        ":",
        field("isolation", $._expression),
        ",",
        "access",
        ":",
        field("access", $._expression),
        optional(","),
        "}",
      ),
    pipeline_task_mode: ($) =>
      seq(
        ".",
        field("mode", $.identifier),
        optional(seq("<", field("domain", $.contextual_member_expression), ">")),
      ),
    facet_expression: ($) =>
      prec.left(
        15,
        seq(
          field("object", $._expression),
          "#",
          field("path", $.facet_path),
        ),
      ),
    facet_path: ($) =>
      prec.right(
        seq(
          field("alias", $.identifier),
          repeat(seq(".", field("facet", $.identifier))),
        ),
      ),
    stream_expression: ($) =>
      prec.right(
        -1,
        seq(
          field("keyword", "stream"),
          field("captures", $.stream_capture_list),
          field("body", $.block),
        ),
      ),
    lock_expression: ($) =>
      prec.right(
        seq(
          "lock",
          field("target", $._expression),
          "as",
          field("binding", $.identifier),
          field("body", $.block),
        ),
      ),
    unsafe_expression: ($) => prec.right(seq("unsafe", $.block)),
    if_expression: ($) =>
      prec.right(
        seq(
          "if",
          choice($.optional_binding, $._expression),
          field("consequence", $.block),
          "else",
          field("alternative", choice($.if_expression, $.block)),
        ),
      ),
    closure_parameters: ($) => seq("(", commaSep($.closure_parameter), optional(","), ")"),
    closure_parameter: ($) => seq(field("name", $.identifier), optional(seq(":", field("type", $.type)))),

    unit_literal: (_) => seq("(", ")"),
    parenthesized_expression: ($) => seq("(", $._expression, ")"),
    tuple_expression: ($) =>
      choice(
        seq(
          "(",
          $.tuple_element,
          ",",
          optional(seq(commaSep1($.tuple_element), optional(","))),
          ")",
        ),
        seq(
          "(",
          $.labeled_tuple_element,
          ",",
          optional(
            seq(commaSep1($.labeled_tuple_element), optional(",")),
          ),
          ")",
        ),
      ),
    tuple_element: ($) => field("value", $._expression),
    labeled_tuple_element: ($) =>
      seq(
        field("label", $.identifier),
        ":",
        field("value", $._expression),
      ),
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
    tuple_index: (_) => /[0-9]+/,
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
