# W para VS Code (local)

Extensão declarativa mínima para abrir arquivos `.w` como **W** e destacar a
sintaxe candidata atual. Ela não contém JavaScript, runtime, telemetria, LSP,
formatter ou compilador. Um `package.lock` canônico também é detectado pela
linha inicial `lock {`.

O ícone local é um `W` azul transparente em `icons/w.png`. Ele é registrado
como ícone da extensão e ícone default da linguagem. Sua exibição no Explorer
depende do file icon theme ativo: o VS Code só usa o ícone default quando o tema
permite language mode icons e não possui associação mais específica para `.w`.

> **Status: Direção.** A gramática é uma ajuda visual alinhada a
> `DESIGN.md` e aos exemplos do restaurante; não
> define nem valida a linguagem.

## O que cobre

- comentários `//`, `/* */` e `///`;
- strings normais, raw, multiline, byte strings e interpolação `${...}`;
- números decimais, binários, octais, hexadecimais, expoentes e sufixos;
- declarations, `init`, computed properties, keywords e tipos;
- patterns de struct com `...`;
- efeitos `async`, `await`, `spawn`, `try` e `throws`, inclusive
  `for try await`;
- storage modifier `atomic` separado dos efeitos;
- ilhas `fn<Language>` e funções W com `fn<abi: .c>`;
- operações de ownership e acesso `ref`, `view`, `inout`, `take`, `copy` e
  `pin`;
- contratos de capture `<[copy name, take other]>` em closures;
- API avançada `Arena` para storage fixed/bounded;
- blocos estruturados `pipeline` e `transaction`, com `commit`;
- pares, comentário e indentação básicos do editor.

O destaque é tolerante a código incompleto, mas não substitui parser/CST. Em
especial, comentários de bloco aninhados, escopo exato de interpolação,
identificadores Unicode e toda semântica de `service` continuam fora deste
artefato.

## Usar localmente

1. Abra esta pasta (`tooling/vscode-w`) no VS Code.
2. Pressione `F5` ou escolha **Run Extension**. Uma janela **Extension
   Development Host** será aberta.
3. Nessa janela, abra qualquer arquivo `.w`, por exemplo
   `../../reference/last-light/kitchen.w`.

Para instalação local persistente, gere um `.vsix` em uma cópia ou diretório
temporário e instale-o pelo VS Code:

```powershell
$wVsix = Join-Path $env:TEMP "w-language-0.0.21.vsix"
bunx @vscode/vsce package --allow-missing-repository --out $wVsix
code --install-extension $wVsix
```

O `.vsix` é artefato de distribuição: não o versionar aqui. O comando acima o
mantém no diretório temporário. `bunx` evita uma
instalação global; a operação requer acesso ao registro npm. Como a extensão é
local e sem código executável, a execução pelo Development Host costuma ser a
forma mais rápida de iterar.

## Evolução

Tree-sitter é a fonte estrutural candidata para highlighting, folds e navegação;
`wls`/HIR deverá produzir semantic tokens, diagnósticos e completions. TextMate
permanece como baseline lexical nativa do VS Code, mas sua lista de tokens deve
ser uma projeção pequena do inventário compartilhado — não uma segunda
gramática. A fixture negativa `fixtures/script-context.w` cobre `let`/`fn`/
`module`/property names. O token contextual `script` usa anchor de documento (`\\A`) e é uma
aproximação conservadora: comments antes do header não são reclassificados com
segurança. Tree-sitter continua autoritativo e não colore `script` em binding,
function, member ou property contexts. A fixture `fixtures/restaurant-syntax.w`
é o começo lexical desse corpus compartilhado.

Referência: [ícones default de linguagem no VS Code](https://code.visualstudio.com/api/extension-guides/file-icon-theme#language-default-icons).
