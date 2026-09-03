(() => {
  "use strict";

  // This is a deliberately small lexical fallback. It is not a W grammar and
  // must be replaced by the same Tree-sitter/WASM grammar used by editor tooling.
  const keywords = new Set([
    "alias", "any", "as", "async", "atomic", "await", "behavior", "break", "capture", "case", "catch",
    "const", "continue", "copy", "defer", "deinit", "dimension", "do", "else", "entry", "enum", "export", "extension",
    "false", "fn", "for", "foreign", "from", "get", "guard", "if", "import", "in", "inout", "is", "let",
    "mut", "object", "package", "panic", "pipeline", "protocol", "ref", "return", "service", "set", "shared", "some",
    "spawn", "struct", "switch", "take", "test", "throw", "throws", "true", "try", "type", "unit", "unsafe", "var",
    "weak", "while",
  ]);
  const pairedDelimiters = { "(": ")", "[": "]", "{": "}" };
  const operators = [">..<", ">..", "...", "..<", "|>", "=>", "??", "?.", "**", "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "%=", "&&", "||", "<<", ">>"];

  function scan(source) {
    const tokens = [];
    const notes = [];
    const delimiters = [];
    let index = 0;
    let line = 1;
    let column = 1;

    function advance(count = 1) {
      for (let offset = 0; offset < count && index < source.length; offset += 1) {
        if (source[index] === "\n") {
          line += 1;
          column = 1;
        } else {
          column += 1;
        }
        index += 1;
      }
    }

    function add(kind, start, startLine, startColumn) {
      tokens.push({ kind, value: source.slice(start, index), start, end: index, line: startLine, column: startColumn });
    }

    function startsQuerySubject(value) {
      return /^\s*(?:[\p{L}\p{N}_]|["'#(\[])/u.test(value);
    }

    function isContextualQueryWord(value, start, end) {
      if (value === "info") {
        const of = source.slice(end).match(/^\s+of\b([\s\S]*)/u);
        return of !== null && startsQuerySubject(of[1]);
      }
      if (value === "of") {
        return /\b(?:type|info)\s+$/u.test(source.slice(0, start)) && startsQuerySubject(source.slice(end));
      }
      return false;
    }

    function isContextualPipelineLabel(value, start, end) {
      if (value !== "transaction") return false;
      const before = source.slice(0, start);
      const after = source.slice(end);
      return /\bpipeline\s*<(?:(?!>)[\s\S])*$/u.test(before) && /^\s*:/u.test(after);
    }

    while (index < source.length) {
      const char = source[index];
      if (/\s/u.test(char)) {
        advance();
        continue;
      }

      const start = index;
      const startLine = line;
      const startColumn = column;

      if (char === "/" && source[index + 1] === "/") {
        const kind = source[index + 2] === "/" ? "doc-comment" : "comment";
        while (index < source.length && source[index] !== "\n") advance();
        add(kind, start, startLine, startColumn);
        continue;
      }

      if (char === "/" && source[index + 1] === "*") {
        const kind = source[index + 2] === "*" ? "doc-comment" : "comment";
        advance(2);
        let closed = false;
        while (index < source.length) {
          if (source[index] === "*" && source[index + 1] === "/") {
            advance(2);
            closed = true;
            break;
          }
          advance();
        }
        add(closed ? kind : "comment-incomplete", start, startLine, startColumn);
        if (!closed) notes.push({ tone: "warning", text: `Linha ${startLine}: comentário de bloco sem fechamento.` });
        continue;
      }

      if (char === "b" && source[index + 1] === "'") {
        const kind = "byte";
        advance();
        advance();
        let closed = false;
        while (index < source.length && source[index] !== "\n") {
          if (source[index] === "\\") {
            advance();
            if (index < source.length) advance();
          } else if (source[index] === "'") {
            advance();
            closed = true;
            break;
          } else {
            advance();
          }
        }
        add(closed ? kind : "string-incomplete", start, startLine, startColumn);
        if (!closed) notes.push({ tone: "warning", text: `Linha ${startLine}: literal ${kind} sem fechamento.` });
        continue;
      }

      const tripleQuote = source.startsWith('\"\"\"', index)
        ? '\"\"\"'
        : source.startsWith("'''", index)
          ? "'''"
          : null;
      if (tripleQuote) {
        advance(3);
        let closed = false;
        while (index < source.length) {
          if (source.startsWith(tripleQuote, index)) {
            advance(3);
            closed = true;
            break;
          }
          advance();
        }
        add(closed ? "string" : "string-incomplete", start, startLine, startColumn);
        if (!closed) notes.push({ tone: "warning", text: `Linha ${startLine}: string multiline sem fechamento.` });
        continue;
      }

      if (char === "#") {
        let hashCount = 0;
        while (source[index + hashCount] === "#") hashCount += 1;
        const quote = source[index + hashCount];
        if (quote !== '"' && quote !== "'") {
          advance();
          add("operator", start, startLine, startColumn);
          continue;
        }
        const quoteCount = source.slice(index + hashCount, index + hashCount + 3) === quote.repeat(3)
          ? 3
          : 1;
        const delimiter = quote.repeat(quoteCount);
        advance(hashCount + quoteCount);
        let closed = false;
        while (index < source.length) {
          if (source.startsWith(delimiter, index)
            && source.slice(index + quoteCount, index + quoteCount + hashCount) === "#".repeat(hashCount)) {
            advance(hashCount + quoteCount);
            closed = true;
            break;
          }
          advance();
        }
        add(closed ? "string" : "string-incomplete", start, startLine, startColumn);
        if (!closed) notes.push({ tone: "warning", text: `Linha ${startLine}: string raw sem fechamento.` });
        continue;
      }

      if (char === '"' || char === "'") {
        const quote = char;
        advance();
        let closed = false;
        while (index < source.length) {
          if (source[index] === "\\") {
            advance();
            if (index < source.length) advance();
          } else if (source[index] === quote) {
            advance();
            closed = true;
            break;
          } else {
            advance();
          }
        }
        add(closed ? "string" : "string-incomplete", start, startLine, startColumn);
        if (!closed) notes.push({ tone: "warning", text: `Linha ${startLine}: string sem fechamento.` });
        continue;
      }

      const rest = source.slice(index);
      const number = rest.match(/^(?:0[xX][\dA-Fa-f](?:_?[\dA-Fa-f])*|0[bB][01](?:_?[01])*|\d(?:[\d_]*\d)?(?:\.\d(?:[\d_]*\d)?)?(?:[eE][+-]?\d(?:[\d_]*\d)?)?)/u);
      if (number) {
        advance(number[0].length);
        const unit = source.slice(index).match(/^<[\p{L}\p{N}_°./*^()+-]+>/u);
        if (unit) advance(unit[0].length);
        add(unit ? "quantity" : "number", start, startLine, startColumn);
        continue;
      }

      const identifier = rest.match(/^[\p{L}_][\p{L}\p{N}_]*/u);
      if (identifier) {
        advance(identifier[0].length);
        const isKeyword = keywords.has(identifier[0])
          || isContextualQueryWord(identifier[0], start, index)
          || isContextualPipelineLabel(identifier[0], start, index);
        add(isKeyword ? "keyword" : "identifier", start, startLine, startColumn);
        continue;
      }

      const operator = operators.find((candidate) => source.startsWith(candidate, index));
      if (operator) {
        advance(operator.length);
        add("operator", start, startLine, startColumn);
        continue;
      }

      if ("+-*/%=!<>~&^|?@".includes(char)) {
        advance();
        add("operator", start, startLine, startColumn);
        continue;
      }

      if (Object.hasOwn(pairedDelimiters, char)) {
        delimiters.push({ char, line });
        advance();
        add("delimiter", start, startLine, startColumn);
        continue;
      }
      if (Object.values(pairedDelimiters).includes(char)) {
        const open = delimiters.at(-1);
        if (!open || pairedDelimiters[open.char] !== char) {
          notes.push({ tone: "warning", text: `Linha ${line}: delimitador ${char} sem par correspondente.` });
        } else {
          delimiters.pop();
        }
        advance();
        add("delimiter", start, startLine, startColumn);
        continue;
      }

      advance();
      add("punctuation", start, startLine, startColumn);
    }

    delimiters.forEach((item) => notes.push({ tone: "warning", text: `Linha ${item.line}: delimitador ${item.char} sem fechamento.` }));
    return { tokens, notes };
  }

  function appendHighlightedCode(target, source) {
    const { tokens } = scan(source);
    const fragment = document.createDocumentFragment();
    let cursor = 0;

    tokens.forEach((token) => {
      if (cursor < token.start) fragment.append(document.createTextNode(source.slice(cursor, token.start)));
      const span = document.createElement("span");
      span.className = `w-token w-token-${token.kind}`;
      span.textContent = token.value;
      fragment.append(span);
      cursor = token.end;
    });
    if (cursor < source.length) fragment.append(document.createTextNode(source.slice(cursor)));
    target.replaceChildren(fragment);
  }

  function highlightCode(target) {
    appendHighlightedCode(target, target.textContent ?? "");
  }

  function highlightAll(root = document) {
    root.querySelectorAll(".code-shell code").forEach(highlightCode);
  }

  window.WSyntax = Object.freeze({ appendHighlightedCode, highlightAll, highlightCode, keywords, scan });
})();
