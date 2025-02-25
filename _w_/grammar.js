module.exports = grammar({
    name: 'W',
  
    rules: {
      source_file: $ => repeat($._declaration),
  
      _declaration: $ => choice(
        $.function_declaration,
        $.variable_declaration,
        $.struct_declaration
      ),
  
      function_declaration: $ => seq(
        'func',
        field('name', $.identifier),
        field('parameters', $.parameter_list),
        optional(seq('->', field('return_type', $.type))),
        field('body', $.block)
      ),
  
      variable_declaration: $ => seq(
        choice('let', 'var'),
        field('name', $.identifier),
        ':',
        field('type', $.type),
        optional(seq('=', field('value', $._expression)))
      ),
  
      struct_declaration: $ => seq(
        'struct',
        field('name', $.identifier),
        field('body', $.struct_body)
      ),
  
      parameter_list: $ => seq(
        '(',
        optional(seq(
          $.parameter,
          repeat(seq(',', $.parameter))
        )),
        ')'
      ),
  
      parameter: $ => seq(
        field('name', $.identifier),
        ':',
        field('type', $.type)
      ),
  
      block: $ => seq(
        '{',
        repeat($._statement),
        '}'
      ),
  
      struct_body: $ => seq(
        '{',
        repeat($.variable_declaration),
        '}'
      ),
  
      _statement: $ => choice(
        $.variable_declaration,
        $.return_statement,
        $.if_statement,
        $.expression_statement
      ),
  
      return_statement: $ => seq('return', optional($._expression)),
  
      if_statement: $ => seq(
        'if',
        field('condition', $._expression),
        field('consequence', $.block),
        optional(seq('else', field('alternative', choice($.block, $.if_statement))))
      ),
  
      expression_statement: $ => $._expression,
  
      _expression: $ => choice(
        $.identifier,
        $.number,
        $.string,
        $.binary_expression,
        $.call_expression
      ),
  
      binary_expression: $ => prec.left(1, seq(
        field('left', $._expression),
        field('operator', choice('+', '-', '*', '/', '==', '!=', '<', '>', '<=', '>=')),
        field('right', $._expression)
      )),
  
      call_expression: $ => seq(
        field('function', $.identifier),
        field('arguments', $.argument_list)
      ),
  
      argument_list: $ => seq(
        '(',
        optional(seq(
          $._expression,
          repeat(seq(',', $._expression))
        )),
        ')'
      ),
  
      type: $ => choice('Int', 'String', 'Bool', 'Void'),
  
      identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,
      number: $ => /\d+/,
      string: $ => /"[^"]*"/
    }
  });