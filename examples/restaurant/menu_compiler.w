// A small compiler for kitchen cards.
// This file intentionally uses only the bootstrap.w0 source profile.

import std.text

export type MenuSymbol = u32

export enum MenuToken {
  word(String, line: usize, column: usize)
  newline(line: usize, column: usize)
  end(line: usize, column: usize)
}

export enum MenuInstruction {
  ingredient(MenuSymbol)
  heat(u32)
  wait(u32)
  serve
}

export struct MenuProgram {
  instructions: Array<MenuInstruction>
  symbols: Array<String>
}

export struct MenuBytecode {
  bytes: Bytes
  symbols: Array<String>
}

export enum MenuCompileError: Error {
  unexpectedEnd
  expectedWord(line: usize, column: usize)
  expectedLineEnd(line: usize, column: usize)
  unknownInstruction(name: String, line: usize)
  invalidNumber(value: String, line: usize)
  instructionAfterServe(line: usize)
  missingServe
  overflow
}

fn lexMenu(source: ref String): Array<MenuToken> {
  var tokens: Array<MenuToken> = []
  var lineNumber: usize = 1

  for line in source.scalars.lines() {
    var column: usize = 1

    for word in line.words() {
      tokens.append(.word(
        word.toString(),
        line: lineNumber,
        column: column,
      ))
      column += word.scalars.count + 1
    }

    tokens.append(.newline(line: lineNumber, column: column))
    lineNumber += 1
  }

  tokens.append(.end(line: lineNumber, column: 1))
  return tokens
}

fn checkedMenuSymbol(value: usize): MenuSymbol throws MenuCompileError {
  do {
    return try MenuSymbol(value)
  } catch {
    throw .overflow
  }
}

fn parseUnsigned(text: ref String, line: usize): u32 throws MenuCompileError {
  do {
    return try u32.parse(text)
  } catch {
    throw .invalidNumber(value: copy text, line: line)
  }
}

object MenuSymbols {
  var ids = Map<String, MenuSymbol>()
  var names: Array<String> = []

  mut fn intern(name: take String): MenuSymbol throws MenuCompileError {
    if let existing = ids[name] {
      return existing
    }

    let id = try checkedMenuSymbol(names.count)
    ids[copy name] = id
    names.append(take name)
    return id
  }

  fn snapshot(): Array<String> {
    return copy names
  }
}

object MenuParser {
  tokens: Array<MenuToken>
  var cursor: usize = 0
  var symbols = MenuSymbols()

  mut fn advance(): MenuToken throws MenuCompileError {
    guard cursor < tokens.count else throw .unexpectedEnd
    let token = copy tokens[cursor]
    cursor += 1
    return token
  }

  mut fn word(): (String, usize) throws MenuCompileError {
    let token = try advance()

    return switch token {
      case .word(let value, let line, _): (value, line)
      case .newline(let line, let column):
        throw .expectedWord(line: line, column: column)
      case .end(let line, let column):
        throw .expectedWord(line: line, column: column)
    }
  }

  mut fn lineEnd() throws MenuCompileError {
    let token = try advance()

    switch token {
      case .newline(_, _): return
      case .end(_, _):
        cursor -= 1
        return
      case .word(_, let line, let column):
        throw .expectedLineEnd(line: line, column: column)
    }
  }

  mut fn unsigned(): u32 throws MenuCompileError {
    let (text, foundLine) = try word()
    return try parseUnsigned(text, line: foundLine)
  }

  mut fn instruction(name: take String, line: usize): MenuInstruction throws MenuCompileError {
    return switch name {
      case "ingredient":
        let (ingredient, _) = try word()
        .ingredient(try symbols.intern(take ingredient))
      case "heat": .heat(try unsigned())
      case "wait": .wait(try unsigned())
      case "serve": .serve
      case let unknown: throw .unknownInstruction(name: unknown, line: line)
    }
  }

  mut fn parse(): MenuProgram throws MenuCompileError {
    var instructions: Array<MenuInstruction> = []
    var served = false

    while cursor < tokens.count {
      let token = try advance()

      switch token {
        case .newline(_, _): continue
        case .end(_, _):
          guard served else throw .missingServe
          return MenuProgram(
            instructions: take instructions,
            symbols: symbols.snapshot(),
          )
        case .word(let name, let line, _):
          guard !served else throw .instructionAfterServe(line: line)
          let operation = try instruction(take name, line: line)
          served = operation == .serve
          instructions.append(take operation)
          try lineEnd()
      }
    }

    throw .unexpectedEnd
  }
}

fn emit(program: ref MenuProgram): MenuBytecode throws MenuCompileError {
  var bytes = Bytes()

  for instruction in program.instructions {
    switch instruction {
      case .ingredient(let symbol):
        bytes.append(0x01_u8)
        bytes.appendLittleEndian(symbol)
      case .heat(let kelvin):
        bytes.append(0x02_u8)
        bytes.appendLittleEndian(kelvin)
      case .wait(let seconds):
        bytes.append(0x03_u8)
        bytes.appendLittleEndian(seconds)
      case .serve:
        bytes.append(0xff_u8)
    }
  }

  return MenuBytecode(bytes: take bytes, symbols: copy program.symbols)
}

export fn compileMenu(source: ref String): MenuBytecode throws MenuCompileError {
  let tokens = lexMenu(source)
  var parser = MenuParser(tokens: take tokens)
  let program = try parser.parse()
  return try emit(program)
}

test "bootstrap compiler emits deterministic kitchen bytecode" for compileMenu {
  let source = """ingredient horizon-fruit
heat 450
wait 30
serve"""

  let first = try compileMenu(source)
  let second = try compileMenu(source)
  expect first == second
  expect first.bytes.last == 0xff_u8
}
