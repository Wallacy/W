((function_declaration
  (language_tag
    language: (identifier) @_language)
  body: (foreign_body
    (foreign_body_content) @injection.content))
  (#eq? @_language "C")
  (#set! injection.language "c"))

((function_declaration
  (language_tag
    language: (contextual_member_expression
      (identifier) @_language))
  body: (foreign_body
    (foreign_body_content) @injection.content))
  (#eq? @_language "c")
  (#set! injection.language "c"))
