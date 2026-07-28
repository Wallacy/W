(source_file) @local.scope
(function_declaration body: (block) @local.scope)
(behavior_accessor body: (block) @local.scope)
(closure_expression (block) @local.scope)
(switch_case) @local.scope
(catch_clause) @local.scope

(parameter name: (identifier) @local.definition)
(behavior_parameter name: (identifier) @local.definition)
(closure_parameter name: (identifier) @local.definition)
(binding_declaration
  pattern: (pattern (identifier) @local.definition))
(optional_binding name: (identifier) @local.definition)

(identifier) @local.reference
