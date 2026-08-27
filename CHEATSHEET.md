# Cheatsheet W

> **Rascunho de design · agosto de 2026**
>
> W ainda não tem compiler W completo, runtime, SDK, package manager ou
> providers de standard library. O target bootstrap `w` executa `w check` no
> perfil CHK9 de root efêmera local e imports alcançáveis, com o mapping
> bounded de diagnostics CHK10. Este arquivo é um
> mapa de leitura para a superfície proposta. Ele não promete que um snippet
> execute.

Este cheatsheet usa a forma integrada de DESIGN.md e os casos do produto de
referência [Última Luz](reference/last-light/README.md). A edição segue a
inspiração editorial do [QuickRef de Rust](https://quickref.me/rust), mas a
autoridade continua sendo DESIGN.md. A fonte .w, os oracles e o atlas são
evidência de design ou de parsing, não uma implementação.

Este arquivo é o cheatsheet editorial. Ele explica rotas de uso, contexto e
trocas. O arquivo [CHEATSHEET.md do atlas](reference/syntax-atlas/CHEATSHEET.md)
é uma projeção gerada dos snippets marcados e registra somente evidência
`tree-sitter-parse-only`. O atlas não substitui esta orientação e não deve ser
editado manualmente.

## Índice

- [Como ler este arquivo](#como-ler-este-arquivo)
- [Primeira rota](#primeira-rota)
- [Source, léxico e formatter](#source-léxico-e-formatter)
- [Módulos, imports e visibilidade](#módulos-imports-e-visibilidade)
- [Declarações, tipos e contratos](#declarações-tipos-e-contratos)
- [Bindings, callables e ownership](#bindings-callables-e-ownership)
- [Controle, patterns, generics e reflexão](#controle-patterns-generics-e-reflexão)
- [Errors, effects e cleanup](#errors-effects-e-cleanup)
- [Async, tasks, channels, streams e yield](#async-tasks-channels-streams-e-yield)
- [Shared, weak, lazy, atomic, locks e SnapshotCell](#shared-weak-lazy-atomic-locks-e-snapshotcell)
- [Services, recovery e capabilities](#services-recovery-e-capabilities)
- [I/O, texto, bytes e collections](#io-texto-bytes-e-collections)
- [Operadores, bits e política numérica](#operadores-bits-e-política-numérica)
- [Números, units, Quantity, dados e serialização](#números-units-quantity-dados-e-serialização)
- [Tensors, devices e custo](#tensors-devices-e-custo)
- [FFI, foreign bodies e segurança](#ffi-foreign-bodies-e-segurança)
- [Package, build, CLI, REPL e Jupyter](#package-build-cli-repl-e-jupyter)
- [Receitas de uso](#receitas-de-uso)
- [Mesmo objetivo, várias formas](#mesmo-objetivo-várias-formas)
- [Índices rápidos](#índices-rápidos)
- [Evidência, limites e validação](#evidência-limites-e-validação)

## Como ler este arquivo

### Maturidade

W está em fase de projeto. Os estados normativos de §0 são separados dos
qualificadores de evidência. **Forma vigente** significa que a forma está
integrada ao design e ao produto de referência. **Direção** é um princípio
estável que limita futuras soluções. **Pesquisa** é uma hipótese ou baseline.
**Rejeitado por enquanto** e **Rejeitado** não são formas atuais.

#### Estados normativos de §0

| Estado | Leitura segura |
| --- | --- |
| Forma vigente / current | Forma corrente de source ou contrato. Verifique a âncora de DESIGN.md. |
| Direção / direction | Princípio ou decisão de produto que limita soluções futuras. |
| Pesquisa / research | Hipótese ou baseline com escopo declarado; não copie como regra. |
| Rejeitado por enquanto | Alternativa não adotada sem evidência nova. |
| Rejeitado / rejected | Alternativa fora do W atual. Reabrir exige necessidade e evidência novas. |

A classificação ativa do design freeze tem uma entrada research-gated:
W-1486 (`RDX0-binary-registry-execution`). O fechamento histórico `Research=0`
permanece válido até W-1459. A direção candidate RDX0 e suas oito tasks não
são implementação; canonical signing payload, protocol/security/provider
evidence e stop conditions aguardam evidence e revisão. Pesquisa histórica
encerrada é um qualificador de proveniência em history, não um estado de §0.

#### Qualificadores de evidência, não estados

| Qualificador | Leitura segura |
| --- | --- |
| implementation-gap | O contrato pode estar vigente, mas compiler, runtime, CLI, std ou provider ainda faltam. |
| tree-sitter-parse-only | O snippet passou pelo parser de referência. Não houve type-check, lowering ou execução. |
| oracle-backed-current | Um oracle host registra uma decisão de design. Oracle não é runtime. |
| source-backed | O texto vem de um arquivo .w de Última Luz ou do atlas. |

Quando uma linha mostra uma alternativa, ela fica em célula própria e recebe
Não use ou Rejeitado. Um bloco com esse rótulo nunca é apresentado como W
válido.

### Rota de leitura

1. Leia [DESIGN.md §0](DESIGN.md#0-como-ler-este-documento) para a autoridade e os estados.
2. Use o [índice gerado](DESIGN-INDEX.md) para localizar uma decisão.
3. Compare o contrato com os [oracles e fontes da Última Luz](reference/last-light/README.md).
4. Trate o [atlas sintático](reference/syntax-atlas/CHEATSHEET.md) como uma
   projeção parse-only. Ele é gerado e não deve ser editado.

## Primeira rota

### Um arquivo de source

Esta é uma amostra curta do atlas. Ela é current / Forma vigente,
tree-sitter-parse-only, implementation-gap.

```w
module hello

import std.text

fn runHello() {
  let greeting = "hello"
}

entry Hello(runHello)
```

O nome do módulo e os imports pertencem ao source. Um package e um workspace
são records de manifesto no único root físico [build.w](reference/last-light/build.w),
não módulos W comuns.

### Comandos planejados

O target bootstrap `w` executa `w check` no perfil CHK9 de root efêmera local.
As demais rotas abaixo são uma interface prevista, não uma CLI disponível:

| Objetivo | Forma prevista | Estado |
| --- | --- | --- |
| Rodar arquivo único | `w run path/file.w` | Direção + implementation-gap |
| Construir package | `w build` | Direção + provider missing |
| Abrir sessão | `w repl` | Direção + implementation-gap |
| Verificar um source ou graph local | `w check path/file.w [--json]` | Forma vigente executável no perfil CHK9 bounded, com mapping CHK10 subset |
| Verificar um package | `w package check [package]` | Direção + implementation-gap |
| Verificar um workspace | `w workspace check` | Direção + implementation-gap |
| Exportar notebook | `w notebook export` | Direção + provider/implementation-gap |

`w check path/file.w` usa o source indicado como root da verificação e carrega
o module graph alcançável já definido pela resolution vigente. Ele não
transforma os outros products do owner em roots implícitos. O comando usa o
contexto de module-run do package, workspace ou contexto efêmero. Ele não exige
nem seleciona `entry`.

O comando lê a resolution vigente, mas não busca, resolve, atualiza ou instala
dependencies. Ele não executa build action, backend, link ou runtime. Ele não
gera artifact. `w package check [package]` verifica o package e seu module
graph. `w workspace check` verifica os members selecionados e sua resolution
compartilhada. Os três scopes são distintos.

Com `--json`, stdout contém somente JSONL D0. O renderer humano escreve em
stderr. CHK9 usa uma root explícita em contexto efêmero e imports locais
alcançáveis root-relative. A rota aceita até 64 sources, 4096 edges, depth 64,
16 MiB por source e agregado, CST de 32768 por source e 262144 agregados.
Source bytes, CST e JSON staging/final crescem adaptativamente. Linux exige
`openat2`; Windows exige `NtCreateFile`; outras capabilities falham fechadas.
Owner detection, resolução externa, provider `std`, package/workspace e o
frontend normativo completo continuam gaps. O comando não executa build,
backend, link ou runtime e não gera artifact.

O driver interno `w_seed_check_driver` fornece evidência bounded de um path
source → parser → frontend → D0. Ele aceita até 16 MiB e mapeia somente
`W-SEM-0001`. O gate cobre `platform.w`, sua inversão negativa e as barreiras
de source, parse e frontend. O target bootstrap `w` reutiliza o mesmo núcleo
privado. O driver não resolve package ou workspace.

CHK3 também fornece evidência interna caller-owned para origins e edges. O
scanner preserva spans exatos de `module` e imports diretos; o frontend separa
`logical_source_id`, `module_id` completo e `local_module_name`. Em modo de
resolution completa, cada import recebe exatamente um edge local ou external
fornecido pelo resolver, e lookup, receipt e output usam esse target exato.
Header não substitui a identidade completa, e o adapter D0 recebe o índice de
documento esperado. Isso não implementa provider, owner discovery, loader de
filesystem, package/workspace, reexport/service-import ou multi-file `w check`.

O [gate da Última Luz](reference/last-light/BUILD.md) separa parser,
checker, HIR, lowering, runtime, toolchain e provider. Não use um comando
planejado como evidência de que uma camada existe.

### Checklist de primeira leitura

- Comece com um `module` explícito. Use `entry Name(fn)` para um product
  nomeado. A forma de arquivo único com statements finais é uma direção de
  module run, não um segundo grammar.
- Prefira imports explícitos e nomes qualificados. A resolução de package,
  target e provider depende de manifestos e de um contexto de resolução.
- Faça ownership e effects visíveis no call site. take, ref, inout, await,
  spawn e unsafe não são decoração.
- Para qualquer snippet, confira a etiqueta de evidência e o arquivo Last
  Light ligado na mesma seção.

## Source, léxico e formatter

Contrato: [DESIGN.md §5](DESIGN.md#5-source-nomes-e-edição) e
[DESIGN.md §3](DESIGN.md#3-contratos-estáticos-e-orçamento-de-símbolos).

### Regras de source

| Item | Forma corrente |
| --- | --- |
| Codificação | UTF-8 com normalização NFC; identificadores e texto preservam Unicode. |
| Keywords | ASCII minúsculo. |
| Comentário de linha | // comentário |
| Comentário de bloco | /* comentário */ |
| Documentação | /// e /** ... */, ligada ao item seguinte. |
| Statements | ; é aceito na migração, mas o formatter escolhe a forma canônica. |
| Names | O nome exportado e o caminho de import são parte da interface. |
| Formatter | Deve preservar parse e significado; divergências são um gate de design. |

### Literais e collections

Este trecho é current / tree-sitter-parse-only / source-backed, reduzido de
[reference/syntax-atlas/CHEATSHEET.md](reference/syntax-atlas/CHEATSHEET.md).

```w
fn values(): () {
  let count = 1_000
  let ratio = 0.5e2
  let distance = 9.81<m/s^2>
  let speed = 12km
  let bytes = 64KiB
  let text = "city"
  let raw = #"raw city"#
  let multiline = """north
south"""
  let scalar = 'N'
  let byte = b'\x4e'
  let enabled = true
  let point = (north: 1, east: 2)
  let list = [1, 2, 3]
  let map = ["north": 1]
  let repeated = [0; 4]
  let selected = (point).north
}
```

Literal 12km, 64KiB e 9.81<m/s^2> exigem resolução de units e não provam um
provider numérico. A semântica de Quantity está em
[DESIGN.md §15](DESIGN.md#15-números-ranges-e-unidades) e no oracle
[quantity_oracle.w](reference/last-light/quantity_oracle.w).

## Operadores, bits e política numérica

### Operadores de consulta rápida

A tabela usa a ordem da menor para a maior força.

| Grupo | Formas | Associação | Semântica curta |
| --- | --- | --- | --- |
| assignment | `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `**=`, `<<=`, `>>=`, `&=`, `^=`, `\|=` | não encadeável | Escreve no place uma vez e resulta em Unit (`()`). |
| coalescing | `??` | direita | Seleciona o fallback somente para ausência. |
| logical OR | `\|\|` | esquerda, short-circuit | Avalia o lado direito somente quando necessário. |
| logical AND | `&&` | esquerda, short-circuit | Avalia o lado direito somente quando necessário. |
| bitwise OR | `\|` | esquerda | Opera nos bits do mesmo tipo integer. |
| bitwise XOR | `^` | esquerda | Opera nos bits do mesmo tipo integer. |
| bitwise AND | `&` | esquerda | Opera nos bits do mesmo tipo integer. |
| equality | `==`, `!=` | não encadeável | Compara valores conforme o contrato de tipo. |
| relation | `<`, `<=`, `>`, `>=`, `is`, `in` | não encadeável | Compara, testa tipo/case sem narrowing ou testa membership em Range ou tuple finita. |
| range | `...`, `..<`, `>..`, `>..<` | não encadeável | Cria um range com limites inclusivos ou exclusivos. |
| shift | `<<`, `>>` | esquerda | Move bits com contagem `UInt`. |
| additive | `+`, `-` | esquerda | Soma ou subtrai conforme a policy numérica. |
| multiplicative | `*`, `/`, `%`, `@` | esquerda | Multiplica, divide, calcula remainder ou faz matmul rank 1/2. |
| prefix | `!`, `~`, `-`, `try`, `try?`, `await`, `copy`, `take`, `pin`, `inout`, `ref` | direita | Opera no operand subtree e respeita effects/ownership. |
| power | `**` | direita | Potência. O lado direito aceita prefix. |
| postfix | call, member `.member`, index `[index]`, `?`, optional member `?.member` | esquerda | Encadeia call, projection, indexação e propagação de Option. |

`-2 ** 2` significa `-(2 ** 2)`. `2 ** -3` significa `2 ** (-3)`.
Assignment composta preserva a policy da operação e avalia o place uma vez.
`a = b = c` continua rejeitado pelo contrato. O seed Pratt e a grammar atual
formam uma árvore right-associative para este probe. O checker registra esse
frontend/parser conformance gap. A associação sintática não é prova semântica.

`>..` e `>..<` são formas current do contrato e estão nas tabelas seed
lexer/parser. O witness direto Tree-sitter dessas duas formas ainda falha. O
atlas registra o gap de parser. Esse texto não afirma parse-only para essas
duas formas.

### Política numérica e custo

| Superfície | Contrato |
| --- | --- |
| Operators integer | `+`, `-`, `*`, `/`, `%`, unary `-`, integer `**` usam arithmetic checked. Overflow e divisor inválido causam panic em runtime e diagnostic em const evaluation. |
| APIs named | `checked*` retorna `Result`; `wrapping*`, `saturating*` e `overflowing*` nomeiam a policy escolhida. |
| Multiprecision | `carryingAdd`, `borrowingSubtract` e `fullMultiply` expõem carry, borrow e produto completo. |
| Shifts e bits | `checkedShift*`, `wrappingShift*`, `maskedShift*`, `logicalShiftRight`, `rotatedLeft` e `rotatedRight` tornam a intenção explícita. |
| Representação | `toBits`/`fromBits` e APIs endian `toBytes`/`fromBytes` usam `.little`, `.big` ou `.native` sem alterar o valor. |
| `@` | Rank 1 e rank 2 usam a família fechada. Integer é checked e float usa mode `.strict`. Não há broadcast implícito. |
| SIMD e tensor | Use APIs nomeadas, como `tensor.matmul`, `tensor.contract` e `materialize`. Não há operator extra. |
| Otimização | Um operator não é hint de branchless, SIMD, unchecked ou fast-math. O optimizer só preserva semantics, panic, effects, ownership e numeric policy. |
| Compound assignment | O place é resolvido uma vez. A operação lê, calcula e escreve no mesmo place. |

#### Matriz fechada de policies integer

Cada nome é uma API associada. `overflowingX` retorna os low wrapped bits e a
flag de overflow.

| Operação | Checked | Wrapping | Saturating | Overflowing | Outra forma nomeada |
| --- | --- | --- | --- | --- | --- |
| add | `checkedAdd -> Result<T, ArithmeticError>` | `wrappingAdd -> T` | `saturatingAdd -> T` | `overflowingAdd -> (T, Bool)` | — |
| subtract | `checkedSubtract -> Result<T, ArithmeticError>` | `wrappingSubtract -> T` | `saturatingSubtract -> T` | `overflowingSubtract -> (T, Bool)` | — |
| multiply | `checkedMultiply -> Result<T, ArithmeticError>` | `wrappingMultiply -> T` | `saturatingMultiply -> T` | `overflowingMultiply -> (T, Bool)` | — |
| negate | `checkedNegate -> Result<T, ArithmeticError>` | `wrappingNegate -> T` | `saturatingNegate -> T` | `overflowingNegate -> (T, Bool)` | — |
| power | `checkedPower -> Result<T, ArithmeticError>` | `wrappingPower -> T` | `saturatingPower -> T` | `overflowingPower -> (T, Bool)` | — |
| divide | `checkedDivide -> Result<T, ArithmeticError>` | — | — | — | `euclideanDivide -> T` |
| remainder | `checkedRemainder -> Result<T, ArithmeticError>` | — | — | — | `euclideanRemainder -> T` |
| left shift | `checkedShiftLeft -> Result<T, ArithmeticError>` | `wrappingShiftLeft -> T` | — | — | `maskedShiftLeft -> T` |
| right shift | `checkedShiftRight -> Result<T, ArithmeticError>` | — | — | — | `maskedShiftRight -> T`, `logicalShiftRight -> T` |
| rotate left | — | — | — | — | `rotatedLeft -> T` |
| rotate right | — | — | — | — | `rotatedRight -> T` |

`maskedShiftLeft` e `maskedShiftRight` aplicam count módulo da largura.
`logicalShiftRight` explicita preenchimento zero. `rotatedLeft` e
`rotatedRight` reduzem `UInt` módulo da largura. `saturatingNegate` clampa o
resultado matemático, inclusive unsigned (`x > 0` produz zero). `checkedDivide`
rejeita divisor zero e `signed.min / -1`; `checkedRemainder` rejeita somente
divisor zero, e `signed.min % -1` produz `0`. W não publica
wrapping, saturating ou overflowing divide/remainder.

`euclideanDivide` e `euclideanRemainder` são associated functions integer que
retornam `T`. Para `b != 0`, satisfazem `a == b * q + r` e
`0 <= r < abs(b)`, com `abs(b)` matemático. Para signed integer, ambos aceitam
divisor positivo ou negativo. Divide rejeita divisor zero e `signed.min / -1`; remainder rejeita
somente divisor zero e retorna `0` em `signed.min % -1`. Em unsigned, coincidem
com `/` e `%`.

| Qual forma usar | Forma corrente | Limite |
| --- | --- | --- |
| Álgebra booleana de bits | `&`, `\|`, `^`, `~`, `<<`, `>>` | Operadores fixos. Não há operator definido pelo usuário. |
| Aritmética checked | `+`, `-`, `*`, `/`, `%`, `**` ou `checked*` | Overflow e divisor inválido permanecem explícitos. |
| Policy de overflow | `wrapping*`, `saturating*`, `overflowing*` | Nomeie a policy. Nenhum profile muda os operadores básicos. |
| Multiprecision | `carryingAdd`, `borrowingSubtract`, `fullMultiply` | Use `BigInt` ou `BigUInt` quando a largura fixa não basta. |
| Primitiva portátil de bits | `bitWidth`, `countOnes`, `countZeros`, `countLeadingZeros`, `countTrailingZeros`, `reversedBits`, `reversedBytes` | APIs puras, const-evaluable e sem allocation. Não prometem tempo constante. |
| FMA explícito | `math.fma(a, b, c)` | `a * b + c` não vira FMA em mode strict. |
| SIMD, tensor ou device explícito | `tensor.matmul`, `tensor.contract`, `materialize` e APIs de device | Transfer e shape ficam nomeados. Não há operator de performance. |
| Otimização ou PGO | profile, facts e `w explain performance` | PGO orienta otimização. Não altera value, panic, effects ou numeric policy. |

`%` mantém a semântica checked do operador: divisor zero causa panic; em signed,
`signed.min % -1` é `0`, enquanto somente `signed.min / -1` é erro.

Não existe **performance operator**. Intrinsics, instruções e fallback podem
implementar primitives portáteis. Crypto com exigência de side-channel usa seu
package, provider ou profile e publica evidence própria.

Não existem custom/user operators, unary `+`, `++`, `--`, postfix force unwrap,
`&&=`, `||=`, `??=` ou `@=`. `isSameInstance` é uma API nomeada de identidade.
Ela não é uma segunda forma de `is`.

Consulte a hierarquia normativa em
[DESIGN.md §3](DESIGN.md#3-contratos-estáticos-e-orçamento-de-símbolos) antes
de inferir precedência de uma forma nova.

## Módulos, imports e visibilidade

Contrato: [DESIGN.md §6](DESIGN.md#6-módulos-imports-e-visibilidade). O atlas
marca estas formas como current / tree-sitter-parse-only.

```w
module atlas_language<
  domains: [.serial],
>

import std.text
import { String as Text } from std.text
export * from atlas.foundation
export { FoundationPlace as BasePlace } from atlas.foundation
import domain { District } from atlas.domain
import service { RemoteCatalog<key: String> } from atlas.catalog
import service atlas.catalog as catalog
```

| Forma | Uso | Limite de evidência |
| --- | --- | --- |
| import std.text | Import ordinário de módulo | Parser e contrato; std ainda é rascunho. |
| import { Name as Alias } from path | Seleção e renomeação | A resolução depende do package context. |
| import domain { ... } | Carrega a interface de um domain | domain não implica process isolation disponível. |
| import service { ... } | Referência a service | Services são direção e têm provider gap. |
| export * from path | Reexportação explícita | Não transforma a unidade em um barrel sem contrato. |
| module name<domains: [...]> | Declara capabilities/domains do módulo | O domínio precisa ser compatível com o target. |

Visibilidade (private, internal, public/export) acompanha a interface do módulo.
Não use o [portal](portal/README.md) como fonte de regras de import: é um
protótipo congelado.

### Import local em invocation efêmera

Esta é uma forma de design de W-1485. Com uma root explícita fora de package ou
workspace, a origin local é root-relative: `import platform.native` procura
`platform/native.w`, e não um path relativo ao importer. `std` permanece no
provider de standard library e não é sombreado por `std.w` local.

```text
module app

import command
import platform.native
import std.io
```

O exemplo é oracle-backed e não uma promessa de execução. CHK3 fornece origins
e edges explícitos caller-owned; CHK4 fornece o graph builder caller-owned.
CHK5 fornece aquisição e revalidação somente para a root e os `SourceId`
explicitamente solicitados pelo caller, com evidência real do adapter Linux
quando `openat2` está disponível. CHK6 fornece um driver C11 interno
caller-owned de discovery local iterativo: ele compõe CHK5, parser/module scan
e CHK4 em waves bounded e entrega documentos em `document_order` e imports
resolvidos a um caller futuro. O driver não chama o frontend nem abre a CLI
pública `w check` multi-file. CHK9 usa essa composição na boundary pública
somente para a root efêmera local.

As waves CHK6 não formam uma transação única de snapshot. Candidates de waves
anteriores podem ser readquiridos, mas o CHK4 é a autoridade de reachability e
publica somente nodes alcançados; bytes, CST e facts da última wave estável
alimentam o graph. NFC completo, provider std, package/workspace e resolução
externa continuam gaps. CHK9 cobre somente imports locais alcançáveis. A
proveniência de capacity preservada pelo parser é evidência interna, sem novo
mapping D0 público. A API CHK5 isolada não faz discovery pelo raw import path.

CHK7 compõe internamente CHK6, frontend seed e D0 em uma API caller-owned
JSON-only. Todo o trabalho falível termina antes do commit: ela preflighta todos
os diagnostics por `document_index`, copia o JSONL uma vez para o buffer final e
então atualiza `jsonl_length`, sem novo ramo falível; qualquer falha deixa ambos
inalterados. A fixture prova
import/call de `root` para `child` e `W-SEM-0001` em `child.w`, de modo
determinístico. O corte mapeia somente `W-SEM-0001` e não abre CLI pública,
filesystem novo, provider `std`, package/workspace ou frontend completo.

CHK8 fornece o adapter Windows interno do provider efêmero. Ele usa
`NtCreateFile` relativo a um `HANDLE` de diretório, rejeita reparse points e
objetos que não sejam arquivos regulares, e confirma identidade por
`FILE_ID_INFO`. O perfil aceita root relativa e root absoluta drive-local.
UNC retorna `UNSUPPORTED`; namespaces, devices, ADS e formas rooted inválidas
retornam `INVALID`. O adapter é caller-owned, sem heap e bounded, e herda do
core a revalidação e a publicação all-or-nothing.

O teste Windows cobre nested child, hardlink alias, junction final e
intermediário, mutation, replacement, removal, UTF-8 físico e limites. O gate
separa os targets Linux e Windows, exige `windows-real=passed` em Windows,
prova Linux real via WSL no host Windows e executa os stubs fail-closed. CHK8
é adapter interno e não habilita `w check` público multi-file.

### CHK9 — `w check` público em root efêmera

`w check path/file.w [--json]` é a rota pública executável para uma root
explícita em contexto efêmero. Ela alcança somente imports locais
root-relative. Linux exige `openat2`; Windows exige `NtCreateFile`.
Outras plataformas ou capabilities ausentes falham fechadas.

A root usa basename ASCII `[A-Za-z_][A-Za-z0-9_]*.w` como `SourceId`.
O core/provider aceita diretório físico codificado em UTF-8, e o gate Windows
prova cwd Unicode; um path Unicode recebido por `argv` narrow não está provado
e permanece gap. Header override altera o module path da root, não o
`SourceId`. Sources filhos usam paths root-relative.

O bootstrap aceita até 64 sources, 4096 edges, depth 64, 16 MiB por source
e agregado, CST de 32768 por source e 262144 agregado, e JSON staging/final
até 64 MiB. Source, CST e JSON crescem por retry bounded que repete a
composição CHK6 → CHK7.

Exit `0` é clean. Exit `1` é diagnostic mapeável. Exit `2` é invocation,
source, parse, unsupported, barrier ou capacity. Exit `3` é allocation,
invariant, renderer ou falha de escrita. JSON usa `SourceId` lógico. Human usa
path físico somente para display.

O gate prova o witness single-source e o Restaurant multifile com child nested,
diagnostic determinístico, source inalcançado, missing/std/cycle, limites,
identidade, UTF-8, parse, frontend e symlink/junction escape. O witness público
de `W-MATCH-0001` usa `missingCases` set byte-sorted e label source-backed
`match-subject`, com JSON repetível e exit `1`. Package, workspace, provider
`std`, NFC completo e frontend normativo permanecem gaps.

### CHK10 — diagnostics frontend estruturados

O carrier frontend `w-seed-frontend-9` é caller-owned e append-only. Cada record
publica `code`, `primary`, `document_index` e ranges exatos de facts, items e
labels. STRING usa `text`; INTEGER usa `integer_value`; ARRAY/SET usam a faixa
de `diagnostic_items`. O adapter preflighta profile e comprimento, schema,
fact keys/types, UTF-8, sets únicos em ordem de bytes, grupos/ordem/cardinalidade
de labels, SourceIds válidos e únicos, documentos, spans e counts exatos antes
de medir ou escrever JSON.

O mapping fechado cobre exatamente estes 17 codes: `W-SEM-0001`,
`W-TYPE-0120`, `W-TYPE-0121`, `W-TYPE-0122`, `W-LABEL-0005`, `W-LABEL-0006`,
`W-MATCH-0001`, `W-MATCH-0002`, `W-MATCH-0003`, `W-CONST-0001`,
`W-CONTRACT-0001`, `W-CONTRACT-0002`, `W-CONTRACT-0003`, `W-CONTRACT-0004`,
`W-GENERIC-0001`, `W-GENERIC-0002` e `W-GENERIC-0003`; outros codes continuam
`UNSUPPORTED`. JSON é all-or-nothing no preflight e conserva SourceId lógico;
human compartilha a validação e usa paths físicos para primary e labels, com
summary específico. A evidência inclui matrix 17/17, caso cross-document,
sets determinísticos, três diagnostics em ordem e o witness público Restaurant
de `W-MATCH-0001`. Isto é uma fatia bounded de mapping, não frontend completo
nem `w check` completo; package/workspace, provider `std`, resolution externa,
owner detection e diagnostics fora dos 17 profiles continuam gaps.

## Declarações, tipos e contratos

Contrato: [DESIGN.md §8](DESIGN.md#8-tipos-e-conversões),
[DESIGN.md §7](DESIGN.md#7-bindings-funções-e-closures) e
[DESIGN.md §3](DESIGN.md#3-contratos-estáticos-e-orçamento-de-símbolos).

### Formas de declaração

```w
export struct Place<ID> : Hashable {
  id: ID
  var label: String = "square"

  init(id: ID, label: String) {
    self.id = id
    self.label = label
  }

  fn describe(): String {
    return label
  }
}

object Ward {
  var name: String
}

protocol Directory<Key> {
  type Value: Hashable
  fn lookup(key: Key): Value
}

enum Signal: Error {
  quiet
  alert(level: u8)
}
```

O bloco é uma amostra current / tree-sitter-parse-only. struct descreve valor
com layout; object descreve identidade/estado; protocol descreve contrato; enum
fecha cases. service, behavior, extension, type, alias, dimension e unit
aparecem em fontes Last Light e têm regras próprias. Não suponha que object
seja automaticamente shared.

### Tipos compostos e estáticos

```w
type PlaceId = String
alias MaybePlace = Place<String>?
type Location = (district: String, number: u16)
type Digest = [u8; 32]
type Callback = some fn(String): String
type SmallText = Array<u8><(.count <= 64)>
type Allowed = Signal<[.quiet, .alert]>
```

| Construção | Intenção |
| --- | --- |
| type Name = T | Nome de um tipo fechado ou de um refinement. |
| alias Name = T | Alias de leitura; não cria identidade nova. |
| T<(predicate)> | Refinement verificado no contrato estático. |
| Enum<[cases]> | Subconjunto estático de cases. |
| (label: T, ...) | Tuple nomeada. |
| [T; N] | Array de tamanho estático. |
| some fn(...) | Callable opaco com uma implementação concreta por valor. |
| any fn(...) | Existential callable com custo e mobilidade explícitos. |

Generics usam parâmetros de tipo, valor e associados. Heads como T: P & Q,
conformances condicionais e some/any não são equivalentes a um where textual.
A fonte [generics.w](reference/last-light/generics.w) concentra casos de
contrato.

### Inicialização e propriedades

Use Type(field: value) e init(...) com labels. Propriedades podem ter get,
set e modify, mas o acesso ainda obedece ownership. Um deinit não substitui
defer nem uma política de cleanup de async.

```w
object Cursor {
  var storedIndex: usize

  var index: usize {
    get => storedIndex
    set(value) => storedIndex = value
    modify { return inout storedIndex }
  }
}
```

Um behavior reutiliza o mesmo lifecycle. `modify` permite um hook local depois
do borrow sem copiar o valor anterior:

```w excerpt
// excerpt-source: reference/last-light/billing.w::export behavior Versioned
export behavior Versioned<Value> for Value {
  storage var current: Value
  storage var mutationEpoch: u64 = 0
  input initialValue: fn(): Value

  init {
    current = initialValue()
  }

  get {
    return current
  }

  mut set(newValue) {
    current = newValue
    mutationEpoch += 1
  }

  mut modify {
    defer { mutationEpoch += 1 }
    return inout current
  }
}
```

A baseline aceita somente `input initialValue: fn(): Value`. Todos os generic
parameters do behavior são inferidos pelo tipo depois de `for`. Configuração
estática pertence ao tipo lógico. Dependência runtime usa owner, método, service
ou channel nomeado. `Behavior(...)`, `Behavior<...>`, múltiplos inputs e acesso
ao backing não pertencem à baseline.

| Operação | Caminho da property |
| --- | --- |
| Inicialização do storage | Escreve o storage e não chama `set` ou `modify`. |
| Leitura | Chama `get`; um getter borrowed não move field move-only. |
| `property = value` | Chama `set` ou substitui o storage; nunca chama `modify`. |
| `property += value` ou call `mutating` | Abre `modify` exatamente uma vez; não usa get-copy-set oculto. |
| Fim do `return inout` | Retoma `defer` uma vez depois do borrow, inclusive quando a operação termina com error. |
| Substituição/drop | Destrói o valor antigo e o backing storage uma vez. |

Accessors são síncronos, não lançam error e não fazem I/O, service call,
blocking, task creation ou allocation geral oculta. Use método nomeado quando
o custo precisa de `try`, `await` ou outro efeito visível. `willSet`, `didSet`,
observer implícito e property `async`/`throws` não pertencem à baseline.

### Conversões, `is` e recuperação de tipo

W faz conversão implícita somente quando ela é total, exata e possui uma rota
única. Narrowing e parsing usam constructors ou APIs nomeadas:

```w
fn conversionExamples() {
  let wide: u16 = 120_u8
  let narrow = try u8(exactly: wide)
  let parsed = try i32.parse("42")
}
```

O checker não procura um terceiro tipo numérico comum. `u8 + i16` pode produzir
`i16`; `i8 + u8` exige que o source escolha o tipo.

`is` retorna somente `Bool`. Ele testa tag de enum ou o tipo nominal exato de
um existential que inclui `reflect.Reflectable`; não cria binding nem faz smart
cast. Para usar o valor concreto, recupere um borrow:

```w
fn inspectReservation(
  value: ref any Hashable & reflect.Reflectable,
) {
  if let ref key = reflect.downcast<ReservationKey>(value) {
    inspect(key.orderId)
  }
}
```

`reflect.downcast<T>` retorna `ref T?`, herda a origem do existential e não
copia, move, retém ou aloca. A baseline não possui downcast owned, `as`, `as?`,
`as!`, cast por string, type pattern ou narrowing flow-sensitive. `as` aparece
somente em import/reexport e `lock ... as binding`.

## Bindings, callables e ownership

Contrato: [DESIGN.md §7](DESIGN.md#7-bindings-funções-e-closures) e
[DESIGN.md §9](DESIGN.md#9-memória-layout-e-alocação).

### Bindings

| Forma | Significado de alto nível |
| --- | --- |
| let x = value | Binding imutável depois da inicialização. |
| var x = value | Binding reatribuível. Mutação ainda pode exigir exclusividade. |
| let ref x | Borrow de leitura com escopo controlado. |
| let inout x | Borrow mutável exclusivo durante a operação. |
| take x | Move explícito do valor. |
| copy x | Cópia explícita quando o tipo e o orçamento permitem. |
| pin x | Fixa uma relação de endereço/ownership no contexto restrito. |
| shared T, weak T, view T | Capacidades de acesso diferentes; não são sinônimos de ponteiros C. |
| var atomic x | Estado atômico com ordem de memória indicada na operação. |

```w
fn stage(allocator destination: ref Allocator, city: String): String {
  return city
}

fn moveCity(city: take String): String {
  return city
}
```

O primeiro snippet é current / tree-sitter-parse-only do atlas. O segundo
combina uma forma de parâmetro já usada em Last Light. Use
[memory.w](reference/last-light/memory.w),
[borrowed_values.w](reference/last-light/borrowed_values.w) e
[borrow_expressivity.w](reference/last-light/borrow_expressivity.w) para
distinguir borrow, move, copy, pin e allocation.
### Funções, labels e closures

Esta unidade completa é source-backed de
[execution.w](reference/syntax-atlas/execution.w). Ela mostra quatro modos de
capture e as chamadas que os consomem.

```w
fn captureModes(target: String, borrowed: ref String, moved: take String, sharedValue: shared String): String {
  let copyCapture = <[copy target]>() => target
  let refCapture = <[ref borrowed]>() => borrowed
  let takeCapture = <[take moved]>() => moved
  let weakCapture = <[weak sharedValue]>() => sharedValue
  let _ = copyCapture()
  let _ = refCapture()
  let _ = takeCapture()
  let _ = weakCapture()
  return target
}
```

Labels externos, labels obrigatórios, defaults, rest (each), static, const,
mut, async, throws, some fn e any fn compõem o callable type. Closure capture
é parte do contrato: copy, ref, take e weak dizem como a closure retém o valor.
O [oracle de callables](reference/last-light/callables.w) e o
[oracle de mobilidade](reference/last-light/mobility.w) são evidência, não uma
biblioteca executável.

### Allocators e orçamento

Esta unidade completa é source-backed de
[execution.w](reference/syntax-atlas/execution.w). Os três scopes vivem no
corpo de `prepare`.

```w
fn stage(allocator destination: ref Allocator, city: String): String {
  return city
}

fn prepare(city: String): String {
  var result = city
  allocator scratch: .fixed<capacity: 256> {
    let ref name = city
    var copyOfName = city
    let inout writableName = copyOfName
    var atomic count: usize = 0
    count += 1
    let moved = take writableName
    result = moved
    let staged = stage(city)
    let _ = staged
  }
  allocator .fixed<capacity: 128> {
    let _ = result.bytes.count
  }
  allocator .root {
    let rootName = result.bytes.count
    let _ = rootName
  }
  try allocator .none {
    let _ = result.bytes.count
  }
  return result
}
```

As formas .fixed, .root e .none são current. .bounded é uma
Pesquisa descrita, não um plano ativo em ASC0; não o trate como API corrente.
A política de propagação contextual está em
[DESIGN.md §9](DESIGN.md#9-memória-layout-e-alocação).

## Controle, patterns, generics e reflexão

### Controle de fluxo

if pode ser statement ou value block. guard encerra o caminho atual. switch
deve respeitar exhaustividade para enums e patterns fechados. for, while,
repeat, break, continue e return seguem os efeitos e ownership do corpo.

Esta unidade completa é source-backed de
[execution.w](reference/syntax-atlas/execution.w). O `guard` está dentro de
um corpo enclosing.

```w
fn walk(values: Array<i32>): i32 throws String {
  var total = 0
  rows: for ref value in values {
    for column in [value] {
      if column < 0 {
        continue rows
      } else {
        total += column
      }
    }
  }
  var index = 0
  while index < 3 {
    index += 1
  }
  repeat {
    total += 1
  } while total < 4
  do {
    if total > 8 { break }
  } catch {
    total = 0
  }
  guard total >= 0 else { throw "negative" }
  defer { total += 1 }
  return total
}
```

### Patterns

```w
fn classify(signal: Signal): String {
  return switch signal {
    case .quiet: "quiet"
    case .alert(let level) if level > 0: "alert"
    case .alert(let level): "alert"
  }
}
```

Patterns de enum, struct, tuple, range, wildcard e binding devem deixar claro
qual valor foi movido, emprestado ou copiado. A gramática e as regras de
exhaustividade estão em
[DESIGN.md §3](DESIGN.md#3-contratos-estáticos-e-orçamento-de-símbolos).

### Contracts, generics e reflection

| Recurso | Forma de consulta | Evidência |
| --- | --- | --- |
| Constraint | T: P & Q | DESIGN.md §8.7; [generics.w](reference/last-light/generics.w) |
| Associated type | protocol P { type Item: Hashable } | [enum_contracts.w](reference/last-light/enum_contracts.w) |
| Refinement | GuestCount = u16<(1...4096)> | [domain.w](reference/last-light/domain.w) |
| Static list/record | Signal<[.quiet, .alert]>, Config<{mode: .strict}> | [reflection.w](reference/last-light/reflection.w) |
| Reflection | reflect.Reflectable, TypeId.of<T>(), reflect.downcast<T>() | [reflection.w](reference/last-light/reflection.w) |
| Rest | T... e each values | [rest_arguments.w](reference/last-light/rest_arguments.w) |

Reflection e synthesis são contratos fechados. Não os trate como macros
universais, derive automático ou metadata de runtime.

## Errors, effects e cleanup

Contrato: [DESIGN.md §11](DESIGN.md#11-erros-panic-oom-e-cleanup) e
[DESIGN.md §3](DESIGN.md#3-contratos-estáticos-e-orçamento-de-símbolos).

### Ausência, erro e falha fatal

| Caso | Forma corrente | Não confundir com |
| --- | --- | --- |
| Ausência esperada | T?, .some, .none, try? | null universal ou exceção implícita |
| Erro recuperável | throws E, throw, try | panic ou retorno Any |
| Falha de programa | panic(...) | Erro de domínio que o caller deve tratar |
| Falta de memória | OutOfMemory/carrier definido pelo contrato | panic genérico |
| Invariante | guard ... else throw ou precondition do contrato | Conversão silenciosa |

```w
export fn optionalGuestName(input: ref String): GuestName? {
  return try? GuestName(input)
}

export fn requireGuest(
  guests: ref Map<GuestId, Guest>,
  id guestId: GuestId,
): ref Guest throws ServiceLookupError {
  return try guests[guestId].orThrow(.missingGuest(guestId))
}
```

Esses trechos vêm de [failure.w](reference/last-light/failure.w) e são
source-backed / oracle-backed-current. A implementação de Result, carriers e
diagnósticos ainda é um gap do frontend/runtime.

### Effects e expressões restritas

Esta unidade completa é source-backed de
[execution.w](reference/syntax-atlas/execution.w). Ela fecha os helpers para
que cada expressão restrita tenha contexto de parsing.

```w
struct AtlasLease {
  target: String
}

fn acquireLease(target: String): AtlasLease {
  return AtlasLease(target: target)
}

fn prepareLease(lease: AtlasLease): String {
  return lease.target
}

async fn restricted(target: String): String throws String {
  let captured = <[copy target]>(name) => name
  let value = if target == "north" { "day" } else { "night" }
  let range = 1..<4
  let (lease, ready) = try await pipeline {
    let lease = acquireLease(target)
    let ready = prepareLease(lease)
    return (lease, ready)
  }
  let guarded = lock target as city {
    city
  }
  let transactionValue = transaction<.serial> tx = target {
    commit tx
  }
  let unsafeValue = unsafe {
    target
  }
  let pinned = pin target
  let _ = captured
  let _ = range
  let _ = lease
  let _ = ready
  let _ = guarded
  let _ = transactionValue
  let _ = unsafeValue
  let _ = pinned
  return target
}
```

await, throws, unsafe, lock, transaction, pin e defer async formam efeitos
verificáveis. try!, conversão automática de cancelamento em erro ou errdefer
não são atalhos correntes sem uma decisão explícita.

### Cleanup

```w
export fn decodeWithCleanup(
  source: ref Bytes,
  trace cleanupTrace: inout Array<CleanupStep>,
): Course throws ServiceLookupError {
  cleanupTrace.append(.opened)
  defer { cleanupTrace.append(.closed) }

  guard source.count > 0 else throw .corruptRecord(0)
  cleanupTrace.append(.decoded)
  return .horizonCake
}
```

Use defer para cleanup lexical e defer async quando o close pode suspender. O
[oracle de failure](reference/last-light/failure.w) e
[DESIGN.md §11](DESIGN.md#11-erros-panic-oom-e-cleanup) definem a ordem.
Destrutor detached e errdefer ficam fora da forma corrente.

## Async, tasks, channels, streams e yield

Contrato: [DESIGN.md §12](DESIGN.md#12-concorrência-paralelismo-e-execução).
Consulte [execution.w](reference/last-light/execution.w),
[task_settlement.w](reference/last-light/task_settlement.w),
[streams.w](reference/last-light/streams.w) e
[synchronization.w](reference/last-light/synchronization.w).

### Call sites de execução

```w
export async fn mixPair(
  left: take MixingJob,
  right: take MixingJob,
): (MixingResult, MixingResult) throws BrigadeError {
  let leftResult = spawn<.compute> mixJob(take left)
  let rightResult = spawn<.compute> mixJob(take right)
  return try await (leftResult, rightResult)
}
```

`let x = async ...` cria uma child task lexical. `let x = spawn<.compute> ...` e
`let x = spawn<domain: .compute> ...` são duas formas correntes do mesmo slot
de placement; ambas continuam exigindo join. `await` é um ponto de suspensão;
o corpo de uma função pode inferir maySuspend quando a operação chamada o
exige. Não há
promessa de scheduler ou runtime disponível.

| Intent | Current form | Note |
| --- | --- | --- |
| call suspending now | `let x = await func()` | task atual; não cria child |
| direct entry sem potential suspension | `let y = sync func()` | `async fn` explícita com `directEntry: available`; compiler/lowering missing |
| bare may-suspend call | `let w = func()` | error, nunca warning |
| direct non-suspending call | `let z = func1()` | task atual |
| child initializer | `let q = async func1()` | child lexical no domain atual |

`sync` só é válido para declaration `async fn` explícita cujo body inteiro
prova `neverSuspend` e cujo function type preserva
`directEntry: available`. A call executa diretamente na mesma task, context e
domain; não cria child, não suspende, não bloqueia thread, não reentra o event
loop e não exige authority, quota ou provider. `try` continua tratando somente
a error edge. Qualquer caminho que alcança `await`, `Task.yield`, child/join,
service ou I/O suspending, `defer async`, call bare/`await` para `maySuspend` ou
`sync` para facet absent remove o facet, mesmo quando um cache hit parece
provável. `sync` pode chamar outra direct entry available: a async entry publica
`may`, mas a entry selecionada é `neverSuspend`. A prova compõe por ponto fixo
em SCCs sem executar recursão ou provar termination; perda de facet propaga aos
callers. Function ordinary, callable may-suspend apenas por inferência,
interface sem body e erasure sem o facet são errors, não warnings ou no-ops.
Uma forma `sync` inválida não vira call ordinary na prova do caller. Frontend
semântico, function type/HIR/interface, dual-entry lowering/ABI, diagnostics e
cross-module/erasure ainda estão missing.

### TaskGroup, cancellation e TaskLocal

Last Light usa TaskGroup.parallelMap, TaskGroup.parallelCollect,
Task.checkCancellation(), Task.yield() e TaskLocal. Essas formas são
oracle-backed-current / provider missing; veja
[execution.w](reference/last-light/execution.w). A regra é estrutural:
children pertencem ao parent, joins são observáveis e cancellation atravessa os
pontos definidos pelo contrato.

`TaskGroup` usa somente os labels `limit`, `ordering` e `using`. O limit é
positivo e obrigatório. `map` usa fail-fast; `collect` observa todos os
application errors e child cancellations sem perder o índice do input:

```w excerpt
// excerpt-source: reference/last-light/execution.w::export async fn inspectEveryFailure
export async fn inspectEveryFailure(
  jobs: take Array<MixingJob>,
  parallelism: usize,
  ordering: TaskGroupOrdering,
): Array<TaskSettlement<MixingResult, BrigadeError>> throws BrigadeError {
  guard parallelism > 0 && parallelism <= maximumParallelCooks else {
    throw .invalidParallelism(found: parallelism, maximum: maximumParallelCooks)
  }

  return await TaskGroup.parallelCollect<.compute>(
    take jobs,
    limit: parallelism,
    ordering: ordering,
    using: mixJob,
  )
}
```

Em `.input`, os settlements seguem o índice. Em `.completion`, seguem body
settlement, mas cada record ainda contém o índice original. `limit` limita
children vivos; não torna os arrays de input ou output sublineares. Parent
cancellation e fault não retornam array parcial e todo caminho drena.

`Task.firstSettled` faz uma escolha one-shot por completion order. Ele consome
handles já criados e não escolhe um domain:

```w excerpt
// excerpt-source: reference/last-light/task_settlement.w::export async fn firstMenuMirror
export async fn firstMenuMirror(
  primaryRequest: take MenuMirrorRequest,
  fallbackRequest: take MenuMirrorRequest,
): TaskSettlement<MirroredMenu, MenuMirrorError> {
  let primary = async readMenuMirror(take primaryRequest)
  let fallback = spawn<.network> readMenuMirror(take fallbackRequest)
  let settlement = await Task.firstSettled(take [primary, fallback])

  return switch take settlement {
    case .some(let winner): take winner
    case .none: panic("two menu mirrors cannot form an empty selection")
  }
}
```

O retorno é `TaskSettlement<Value, Failure>?`, com `index` e `outcome`. Array
vazio devolve `none`. O winner pode ser success, application error ou child
cancellation. A operação cancela e drena todos os losers antes de devolver.
Parent cancellation observada antes da publicação suprime o settlement, drena
todos e continua control outcome. Effects já committed não sofrem rollback. Não
há statement `select`, first-success implícito ou task escondida.

### Pipeline de service e promise pipelining

Uma call dependente usa `pipeline`. O caso `OvenLease` mostra a ordem concreta:
`acquire` devolve uma capability, `preheat` usa essa capability, e `bake` e
`close` continuam usando o lease. O bloco abaixo é um excerpt exato de
`prepareDish` em [restaurant.w](reference/last-light/restaurant.w), incluindo
o `spawn` da mistura, o pipeline, o cleanup e o `await` da mistura.

```w excerpt
// excerpt-source: reference/last-light/restaurant.w::prepareDish
  let mixture = spawn<.compute> mix(stock.ingredients, recipe: schedule.recipe)

  let (lease, ready) = try await pipeline {
    let lease = ovens.acquire(schedule.recipe.target, duration: schedule.duration)
    let ready = lease.preheat()
    return (lease, ready)
  }

  defer async {
    do {
      try await lease.close()
    } catch error {
      Trace.current.recordCleanupError(error)
    }
  }

  let mixture = try await mixture
  return try await lease.bake(take mixture, readiness: take ready)
```

O pipeline descreve um DAG estático de calls. Em uma rota remota, o caller
pode enviar `preheat()` antes de receber a capability de `acquire()`. O
resultado só fica observável depois do `await`. Error, cancelamento e
`unknownOutcome` seguem o contrato de service/effect; o pipeline não presume
rollback nem liberação automática de uma capability intermediária. O
`defer async` de `close` só existe depois de o pipeline publicar o lease,
como no excerpt acima.

Contrafactual explicativa (não é excerpt source-backed): o mesmo trabalho com
awaits sequenciais é mais simples, mas cria uma barreira entre cada call:

```w excerpt
// excerpt-kind: contrafactual
let lease = try await ovens.acquire(recipe.target, duration: recipe.duration)
let ready = try await lease.preheat()
let dish = try await lease.bake(take mixture, readiness: take ready)
try await lease.close()
return dish
```

A forma sequencial mantém a semântica de ownership e errors, mas pode pagar
round trips adicionais. Ela não faz promise pipelining.

Um initializer `async` expressa children independentes que o parent deve aguardar. Ele não
expressa a dependência `lease → preheat` sem primeiro aguardar o lease:

```w excerpt
// excerpt-kind: composed
let leaseTask = async ovens.acquire(recipe.target, duration: recipe.duration)
let lease = try await leaseTask
let ready = try await lease.preheat()
```

Use initializer `async` para siblings independentes. Use `pipeline` para dependências
de service. Nenhuma forma implica runtime, rede ou provider disponível neste
checkout.

### Channels e streams

```w
export async fn inspectMenuLines<E: Error>(
  source: take some Stream<view String, E>,
): usize throws E {
  var lines = take source
  var nonempty = 0_usize

  for try await line in lines {
    if !line.isEmpty { nonempty += 1 }
  }

  return nonempty
}
```

Stream<view Element, Failure> é pull-oriented. Channel<T><.send> e
Channel<T><.receive> tornam a autoridade direcional explícita.

O bloco compiler-owned de GEN2 é a forma vigente para um producer pull curto:

```w
export fn yieldOrders(source: take some Stream<Order, OrderFailure>): some Stream<Order, OrderFailure> {
  return stream <[take source]> {
    var cursor = take source
    while let order = try await cursor.next() {
      yield take order
    }
  }
}
```

stream <[...]> { yield take/copy ... } é uma forma vigente e estreita. Generic
generator, yield from, buffer oculto, channel bidirecional implícito, MPMC sem
domínio e buffer infinito estão fora da forma vigente.

No corpus de referência, `TaskGroup` e os initializers `async`/`spawn` mostram o lifecycle
lexical de tasks; `Stream` e `Channel` continuam tipos explícitos. O exemplo de
channel abre capacidade e endpoints explícitos com `Channel<Order>.open(capacity: 1)`,
envia/recebe, encerra o sender por drop e fecha ou drena o receiver conforme o
contrato.

Service streaming usa o mesmo `Stream`, com direção definida pela posição:

```w excerpt
// excerpt-source: reference/last-light/service_streaming.w::export protocol MenuExchangeApi
export protocol MenuExchangeApi {
  async fn summarize(
    signals: take some Stream<MenuSignal, MenuStreamError>,
  ): MenuSignalSummary throws MenuStreamError

  async fn exchange(
    signals: take some Stream<MenuSignal, MenuStreamError>,
  ): some Stream<MenuSignal, MenuStreamError>
}
```

`take some Stream` em parâmetro é client-streaming. `some Stream` no resultado
é server-streaming. As duas posições juntas são bidirectional. A call ainda usa
`try await` para admission/open, e cada stream mantém terminal, backpressure e
drain próprios. `Channel<T><.receive>` atende a `Stream<T, Never>`; portanto um
producer push pode abrir um channel com capacity explícita e transferir somente
o receiver. A service não cria Channel, fila ou capacity implicitamente. Não há
`stream fn`, `RpcStream` ou `any Stream` em interface publicada.

## Shared, weak, lazy, atomic, locks e SnapshotCell

Contrato: [DESIGN.md §9](DESIGN.md#9-memória-layout-e-alocação),
[DESIGN.md §12](DESIGN.md#12-concorrência-paralelismo-e-execução) e
[DESIGN.md §12.10.8](DESIGN.md#12108-snapshotcell).

### Capacidades de referência

| Forma | Capacidade | Custo e limite |
| --- | --- | --- |
| shared T | Compartilhamento explícito com política de liveness | Não é Arc<T> automático; domínio e mutação importam. |
| weak T | Observação que não mantém o owner vivo | Upgrade e ausência precisam de tratamento. |
| view T | Projeção de leitura/borrow | Não cria uma cópia nem uma lifetime annotation pública. |
| lazy T | Materialização sob demanda com política definida | O oracle de lazy ainda é provider gap. |
| var atomic x | Operações atômicas em estado permitido | Ordem (relaxed, acquire, release) deve ser indicada. |

### `ref`, `view` e interface projection

```w
fn inspectOwner(values: ref Array<Order>) {
  print(values.capacity) // owner completo e metadata
}

fn inspectWindow(values: view Array<Order>) {
  print(values.count)    // janela lógica
  // values.capacity       // Erro: view não possui capacity
}

fn inspectText(value: view String) { print(value) } // UTF-8 válido
fn inspectTensor(value: view Tensor<f32, shape: [rows, cols]>) {
  print(value.strides) // pode ser strided
}
```

`ref Array<T>` observa o owner completo e sua metadata. `view Array<T>` observa
uma janela lógica sem capacity. `view String` só representa substring UTF-8 com
boundaries válidas. `view Tensor` pode ser strided. Um tipo nominal pode
publicar uma view de uma família core por método. Um aggregate owned pode
guardar fields `ref`/`view` e carregar origins, como `BorrowedMenu`. Properties
suprimidas formam uma interface projection, não uma storage view. W não possui
`Viewable`, protocol universal de view ou `view Object` automático.

Para suprimir properties sem copiar o conteúdo, declare uma projeção nominal
borrowed. Use `ref` para um field completo e `view` somente para uma janela de
um carrier que define view:

```w excerpt
// excerpt-source: reference/last-light/views.w::export struct MenuCourse
export struct MenuCourse {
  title: String
  allergens: Array<String>
  supplierContract: String
}

// A nominal borrowed projection selects data. It is not `view MenuCourse`.
export struct PublicCourse {
  title: ref String
  allergens: view Array<String>
}

export fn publicCourse(course: ref MenuCourse): PublicCourse {
  return PublicCourse(
    title: ref course.title,
    allergens: course.allergens[0..<course.allergens.count],
  )
}
```

`PublicCourse` não possui `supplierContract` e carrega as origins de `course`.
Ele não mantém o owner vivo. Use `ref any Protocol` para limitar methods, esse
aggregate nominal para dados borrowed e um DTO owned para um snapshot que pode
escapar. Não há `view MenuCourse`, field mask ou derivação automática.

### Atomic e locks

Esta unidade completa é source-backed de
[synchronization.w](reference/last-light/synchronization.w). O objeto mantém
estado compartilhado e publica snapshots por lock.

```w
export struct ApologyLedgerState: Duplicable {
  revision: u64
  messages: Array<String>
}

export object ThreadApologyLedger {
  state: shared ApologyLedgerState

  export init() {
    self.state = ApologyLedgerState(revision: 0, messages: [])
  }

  fn record(message: take String): u64 {
    return lock state as ledger {
      ledger.messages.append(take message)
      ledger.revision += 1
      ledger.revision
    }
  }

  fn snapshot(): ApologyLedgerState {
    return lock state as ledger { copy ledger }
  }

  fn trySnapshot(): LockAttempt<ApologyLedgerState> {
    return try lock state as ledger { copy ledger }
  }
}
```

O [oracle de sincronização](reference/last-light/synchronization.w) também
mostra CAS, Atomic.wait/notify, try lock, domain .serial e SnapshotCell. Lock
é último recurso para estado compartilhado. Não use um mutex global,
Atomic<shared T> ou RCU implícito como se fossem formas correntes.

### SnapshotCell

SnapshotCell<T> separa leitura de snapshot e publicação de uma nova versão.
Ele não é um universal mutable cell nem substitui ownership. A política de
atomicidade, liveness e descarte está em
[DESIGN.md §12.10.8](DESIGN.md#12108-snapshotcell) e no oracle de sincronização.
O provider ainda é missing.

## Services, recovery e capabilities

Contrato: [DESIGN.md §13](DESIGN.md#13-módulos-de-execução-services-e-entries) e
[DESIGN.md §13.9.3](DESIGN.md#1393-recovery-de-service-e-deduplicação).

### Service e entry

Um service é uma fronteira de execução com interface e estado controlados. Um
entry escolhe o root do product. domain expressa placement e budget.
SupervisorRef, WorkKeyRef, WorkSnapshot, ServiceFailure, effectId e
deduplication pertencem ao contrato de recovery. Eles não significam que um
process supervisor ou rede esteja funcionando neste checkout.

Fontes de leitura: [service_oracle.w](reference/last-light/service_oracle.w),
[service_streaming.w](reference/last-light/service_streaming.w),
[service_recovery_oracle.w](reference/last-light/service_recovery_oracle.w),
[supervision.w](reference/last-light/supervision.w) e
[workflow.w](reference/last-light/workflow.w).

### Recovery seguro

| Peça | Papel |
| --- | --- |
| Closed turn | Um turno tem entradas, efeitos, outputs e budget observáveis. |
| WorkKeyRef | Chave limitada para deduplicar ou retomar trabalho. |
| WorkSnapshot | Snapshot versionado para recovery, não ponteiro mutável. |
| ServiceFailure | Falha tipada com causa e policy de retry explícitas. |
| effectId | Identidade para deduplicação; não autoriza repetir efeitos arbitrários. |
| Supervisor | Policy declarativa de restart, backoff e limite. |

### Direções de service stream

| Forma da operation | Direção |
| --- | --- |
| sem `Stream` | unary |
| resultado `some Stream<Item, Failure>` | server-streaming |
| parâmetro `take some Stream<Item, Failure>` | client-streaming |
| parâmetro e resultado | bidirectional |

Items são owned, transferable e `WireValue`. `Failure` inclui
`ServiceFailure`. Stream nested em outro carrier, item borrowed, input sem
`take`, Channel implícito e abertura sem `await` são errors. Retorno antecipado,
failure e cancellation resetam e drenam as edges ainda abertas antes de liberar
owners.

Reentrada livre, retry implícito, detached Promise e transação distribuída
genérica estão fora da forma vigente nesta baseline. Veja as trocas e a
evidência da decisão na tabela de comparações abaixo.

### Capabilities e security

Capability, target, sandbox e host lifecycle devem ser declarados. O código não
recebe acesso a filesystem, rede, device ou process apenas por importar um
nome. Veja [capability_security_oracle.w](reference/last-light/capability_security_oracle.w),
[session_security_oracle.w](reference/last-light/session_security_oracle.w) e
[DESIGN.md §19](DESIGN.md#19-ffi-unsafe-e-ilhas-de-linguagem).

## I/O, texto, bytes e collections

Contrato: [DESIGN.md §16](DESIGN.md#16-texto-bytes-e-collections) e
[DESIGN.md §14](DESIGN.md#14-prelude-e-standard-library).

### Texto e bytes

String opera sobre texto válido. Bytes opera sobre dados opacos. Conversão
entre ambos precisa de encoding e erro visíveis. view String, slices e
projections preservam ownership do storage. Consulte [text.w](reference/last-light/text.w),
[string_storage.w](reference/last-light/string_storage.w) e
[collections.w](reference/last-light/collections.w).

### I/O async-first

Os carriers correntes são ByteSource e ByteSink. Uma leitura pode retornar
ReadStep.data(bytes) ou ReadStep.end; EOF não é um byte mágico. Escrita usa
append de Bytes; leitura posicional recebe offset e tamanho. Os oracles
[io.w](reference/last-light/io.w), [fs_oracle.w](reference/last-light/fs_oracle.w),
[net_oracle.w](reference/last-light/net_oracle.w) e
[http_documents.w](reference/last-light/http_documents.w) registram o contrato.
Os providers std.fs, std.net e std.http ainda são gaps.

Scatter read usa um único owner para que memória ainda não inicializada nunca
vaze como `view`:

```w
fn makeEnvelopeBatch(): io.ReadBatch throws memory.AllocationError {
  return try io.ReadBatch(64, 4_096, 32)
}

async fn readEnvelope<E: Error, Source: io.ByteSource<E>>(
  input: inout Source,
  envelope: inout io.ReadBatch,
): io.ScatterReadStep throws E {
  return try await io.readMany(
    from: inout input,
    scatterInto: inout envelope,
  )
}
```

Os segments são preenchidos em ordem. `segment(at:)` devolve somente o prefixo
inicializado como `view Bytes`; `reset()` retém as reservas. O fallback usa uma
única leitura no primeiro segment incompleto. Não existe `IoSliceMut`,
`inout view Bytes...` ou probe `isReadVectored`.

Transferência de um snapshot posicional usa um plan bounded. A mesma chamada
pode baixar para `sendfile`/`TransmitFile` ou usar o scratch reservado; isso não
muda o resultado:

```w
fn makeTransferPlan(
  fileOffset: u64,
  byteCount: u64,
): io.TransferPlan throws io.TransferPlanError {
  return try io.TransferPlan(
    at: fileOffset,
    maximumBytes: byteCount,
    chunkBytes: 64 * 1_024,
  )
}

async fn transferStep<
  ReadFailure: Error,
  WriteFailure: Error,
  Source: io.SnapshotByteSource<ReadFailure>,
  Sink: io.ByteSink<WriteFailure>,
>(
  archive: ref Source,
  response: inout Sink,
  plan: inout io.TransferPlan,
): io.TransferStep throws io.TransferError<ReadFailure, WriteFailure> {
  return try await io.transfer(
    from: ref archive,
    to: inout response,
    using: inout plan,
  )
}
```

`TransferStep.data(count)` confirma bytes no sink; `.sourceEnd` e
`.limitReached` são distintos. `TransferError.committed` registra progresso e o
plan conserva o sufixo pendente. Zero-copy não é promessa portátil; consulte
`w explain io` para estratégia, fallback, scratch e motivo de perda do fast
path.

### HTTP, web e processo

HTTP, web bodies, process, time e filesystem aparecem como interfaces tipadas e
capabilities. Um snippet de http.Request não prova servidor, socket, TLS,
browser ou process launch. Veja [http_oracle.w](reference/last-light/http_oracle.w),
[web_bodies.w](reference/last-light/web_bodies.w),
[process_oracle.w](reference/last-light/process_oracle.w) e
[time_oracle.w](reference/last-light/time_oracle.w).
## Números, units, Quantity, dados e serialização

### Números, ranges e units

Contrato: [DESIGN.md §15](DESIGN.md#15-números-ranges-e-unidades).

| Forma | Uso |
| --- | --- |
| u8, u16, u32, u64, usize | Inteiros sem sinal com largura explícita. |
| i8, i16, i32, i64, isize | Inteiros com sinal. |
| f32, f64 | Ponto flutuante explícito. |
| T<(predicate)> | Limite/refinement, como u16<(0...10_000)>. |
| checkedAdd, wrappingAdd, saturatingAdd, overflowingAdd | Política de overflow no call site. |
| 9.81<m/s^2> | Literal com unidade resolvida no contexto. |
| Quantity(value, unit: ...) | Carrier de magnitude e unidade. |

O oracle [numerics.w](reference/last-light/numerics.w) cobre overflow e
largura. [units.w](reference/last-light/units.w) e
[quantity_oracle.w](reference/last-light/quantity_oracle.w) cobrem unidades e
adapters. Narrowing implícito, fast int dependente da máquina e terceiro tipo
numérico comum ficam fora da forma vigente.

### Tabular e formatos

data.Row, data.Batch<Row>, DynamicBatch, adapters tabulares e carriers de
CSV/Parquet/Arrow mantêm schema e ownership explícitos. JSON exige uma
conformance explícita. wWire é a Direção escolhida; profiles e contracts estão
fechados, enquanto decoder, provider e custo continuam em implementation-gap.
Fontes:
[data_formats.w](reference/last-light/data_formats.w),
[json.w](reference/last-light/json.w) e
[wire_oracle.w](reference/last-light/wire_oracle.w).

```w
struct TabularTelemetryRow: data.Row {
  sequence: u64
  hawkingFlux: f64
  warning: String?
}
```

O snippet é uma forma de oracle de design. Não há codec ou provider tabular
executável no repositório.

### Quantities e serialization

Não use Any ou duck typing para esconder unidade ou schema. Prefira carrier
tipado, adapter explícito e budget de bytes. O formato de wire deve declarar
versão, bounds, tags e erro de decode. Consulte
[DESIGN.md §14.4.1](DESIGN.md#1441-carrier-tabular),
[DESIGN.md §15.5.3](DESIGN.md#1553-wwire-para-quantity) e
[DESIGN.md §15.5.4](DESIGN.md#1554-json-de-domínio-para-quantity).

## Tensors, devices e custo

Contrato: [DESIGN.md §17](DESIGN.md#17-matrizes-tensors-e-ml) e
[DESIGN.md §18](DESIGN.md#18-performance-e-custo).

### Matriz e tensor

Arrays fixos e matrizes usam shape e element type visíveis. Tensor interop usa
carriers explícitos, DLPack e cópia/borrow declarados. O oracle
[tensor_interop.w](reference/last-light/tensor_interop.w) registra
tensor.transfer, tensor.materialize e export. Não trate um tensor como
Array<Any> nem infira device por uma operação.

### Device e kernel

```w
export const lastLightKernels = accelerator.module<{
  forecast: forecastKernel,
  normalize: normalizeKernel,
}>()
```

Este bloco é forma de design para um descriptor fechado. Launch, Queue, Device e kernel scope estão em
[device_execution_oracle.w](reference/last-light/device_execution_oracle.w)
e [DESIGN.md §12.7.2](DESIGN.md#1272-device-scopes-e-kernels). Provider de GPU,
JIT e device runtime são gaps. Transferência implícita, raw stream e kernel sem
target declarado são rejeitados.

### Performance e custo

Toda alternativa nesta página deve ser comparada por ownership, effects,
authority, allocation, cópia, suspensão, largura de dados e budget. O
[oracle de performance](reference/last-light/performance.w) mede contratos
de design. Ele não é benchmark de um compiler existente. O gate pré-implementação
fica em [DESIGN.md §18.9](DESIGN.md#189-gate-pré-implementação-de-pesquisa-sota)
e a matriz seed extensível fica em
[RATIONALE.md §1.37](RATIONALE.md#137-gate-sota-de-performance-e-matriz-de-responsabilidade).
A linguagem define semântica, tipos, ownership, effects e numeric modes; o
compiler prova, transforma e faz lowering; runtime, provider e library medem e
escolhem packing, microkernels, dispatch e device. Trabalho pequeno e estático
pode receber lowering para código inline; trabalho grande ou irregular vai para
provider/library, e sparse/graph mantém rota separada. `.strict`, `.fast` e
`.reproducible` ficam explícitos.

### SIMD portátil

`std.simd` publica os heads compiler-owned `Simd<Element, lanes: usize>` e
`SimdMask<_ lanes: usize>`. `lanes` é compile-time em `1...64`, sem
power-of-two requirement. O label `lanes:` de `Simd` é required porque há
`Element` e value parameter. O label de `SimdMask` é optional porque a mask só
tem a largura. A aplicação curta corrente é `SimdMask<16>`, sem uma segunda
identity. Omissão de `lanes:` é `W-GENERIC-0003`, lane fora de `1...64` é
`W-CONST-0004`; Element fora de `i8`/`i16`/`i32`/`i64`/`i128`, `u8`/`u16`/
`u32`/`u64`/`u128`, `Int`/`UInt`/`isize`/`usize`/`f32`/`f64` não está no
domínio e produz `W-CONTRACT-0002`. Somente a lista fechada é Element:
`Bool` como Element produz `W-CONTRACT-0002`; vetores de Bool usam
`SimdMask`. A sequência de lanes é fixa e igual em todo target. O backend
escolhe native, split ou scalarize. Layout, ABI, FFI, wire, persistência e
`transmute` não são implícitos.

Use `splat`, `fromArray`, `toArray`, `load`, `store`, `loadPartial` e
`storePartial`. `SimdMask.splat(Bool) -> SimdMask<N>`,
`fromArray([Bool; N]) -> SimdMask<N>` e `toArray() -> [Bool; N]` não alocam.
Partial load borrow a source, preenche
lanes inativas e devolve `SimdMask`; `at == count` é toda inactive e `at > count`
falha antes de qualquer read. Store recebe destination `inout`; partial store
faz preflight de todas as lanes ativas e falha antes de qualquer write. Lanes
inactive OOB são permitidas e não são acessadas. Arithmetic, bitwise, shifts,
compound e policy só existem quando o scalar Element admite a operação; floats
não ganham bitwise, shifts ou overflow APIs. Integer `overflowingX` retorna
`(Simd<T, lanes: N>, SimdMask<N>)` com flag por lane. `==`/`!=` retornam `Bool`;
comparações nomeadas retornam mask. `SimdMask` usa `&`, `|`, `^`, `~`,
`all() -> Bool`, `any() -> Bool`, `none() -> Bool` e `countTrue() -> UInt`, sem
`&&`/`||`. `select(whenTrue: Simd<T, lanes: N>, otherwise: Simd<T, lanes: N>)`
`-> Simd<T, lanes: N>` exige lanes iguais e não é short-circuit. Swizzle é static, aceita duplicatas e
rejeita índice inválido. Swizzle valida count em `1...64` antes dos elementos e
reporta o primeiro OOB em source order. Count vazio, 65 ou índice OOB usa
`W-CONST-0004`. Integer reductions têm `reduceAdd`,
`wrappingReduceAdd`, `saturatingReduceAdd`, `reduceMultiply`,
`wrappingReduceMultiply`, `saturatingReduceMultiply`, `reduceBitAnd`,
`reduceBitOr` e `reduceBitXor`, sempre em ordem de lanes. Float reductions usam
`reduceAdd(mode:)`/`reduceMultiply(mode:)` com `ReductionMode` nominal
obrigatório `.strict`, `.fast` ou `.reproducible`. Omissão, forma posicional,
label desconhecido ou aridade errada de `mode:` usa `W-LABEL-0005`; repetir o
mesmo `mode:` usa `W-LABEL-0006`. Strict é left fold com identidade `0`/`1`; reproducible v1
é árvore binária balanceada target-independent; fast segue o float contract e
não exige igualdade de bits entre backends. A policy versionada faz parte da
edition, não da identidade do tipo.

O scalar fallback é requisito de disponibilidade. Native acceleration não é
uma promessa de API ou de performance. `w explain performance` informa
native/split/scalar, physical width, loads, tails, reduction mode e missed
reason. Gather, scatter, raw pointer, alignment assertion, intrinsics e
`nativeLanes` ficam fora do core portable.

### Address e bitwise

`Address` expõe os bits do mesmo address space. Faça alignment, tagging ou
masking nos bits, não no pointer:

```w
unsafe fn maskedPointer<T>(
  pointer: c.ptr<T>,
  mask: Address.Bits,
): c.ptr<T> {
  let location = pointer.address
  let alignment = location.bits & mask
  let masked = location.withBits(location.bits & mask)
  return unsafe { pointer.withAddress(masked) }
}
```

O pointer original preserva provenance. `withAddress` não cria bounds,
lifetime ou authority. Bitwise direto sobre pointer é rejeitado.

## FFI, foreign bodies e segurança

Contrato: [DESIGN.md §19](DESIGN.md#19-ffi-unsafe-e-ilhas-de-linguagem).

### Foreign e ABI

```w
export foreign c {
  const LL_HORIZON_OK_V1: c.int = 0
}

export unsafe fn<abi: .c> ll_horizon_checksum_v1(
  data: c.ptr<c.uchar>,
  size: c.size,
): c.uint {
  var hash: c.uint = 2_166_136_261
  var index: c.size = 0
  while index < size {
    hash = (hash ^ data[index]) * 16_777_619
    index += 1
  }
  return hash
}
```

Essas formas são current / parse-only / provider missing e devem permanecer
em ilhas unsafe. ABI, carrier físico e ownership C precisam de uma prova de
fronteira. Consulte [abi.w](reference/last-light/abi.w),
[abi_oracle.w](reference/last-light/abi_oracle.w),
[hardware.w](reference/last-light/hardware.w) e
[system_escapes.w](reference/last-light/system_escapes.w).

fn<C> é uma forma vigente de body inline C; ela não é um generic W. A forma
unsafe fn<abi: .c> acima é uma exportação W com ABI C. Ficam fora da forma
vigente: tratar fn<C> como generic/sandbox, passar objetos W ricos por C, usar
dynamic library sem capability ou fazer um arquivo foreign comportar-se como
módulo W normal. A especificação deve distinguir WInterface, ABI, runtime
requirements e carriers.

## Package, build, CLI, REPL e Jupyter

Contrato: [DESIGN.md §21](DESIGN.md#21-packages-builds-e-releases),
[DESIGN.md §22](DESIGN.md#22-tooling-e-interface-para-máquinas) e
[DESIGN.md §23](DESIGN.md#23-protocolos-e-pesquisas-de-ecossistema).

### Package e workspace

Manifestos separam identidade, dependências, targets, capabilities, artifacts
e resolução. Eles são records data-only no root físico único [build.w](reference/last-light/build.w).
Consulte
[BUILD.md](reference/last-light/BUILD.md).

Package e workspace são unidades de manifesto diferentes do `module` W. Um
package standalone não precisa de workspace: ele próprio é owner de `resolution`
e `deployments`. Use um workspace opcional quando vários packages devem
compartilhar members, resolução e operações de desenvolvimento; nesse caso, um
package member conserva sua identity e products, e delega `resolution` e
`deployments` de modo único ao workspace.
Um workspace com um único member só faz sentido para policy, resolução ou
coordenação de desenvolvimento. O workspace coordena packages. Ele não
substitui o package nem vira um módulo importável.

| Unidade | Use quando | Owner principal |
| --- | --- | --- |
| `module` | declarar source, imports e symbols de um módulo W | symbol graph do source |
| `package` | standalone/member | owns resolution/deployments; member keeps identity/products |
| `workspace` | coordenar packages quando necessário | members, `resolution` e `deployments` |

Um workspace pode conter um package, mas os manifestos conservam as duas
responsabilidades: o package member declara identity e products; o workspace é
owner de `resolution` e `deployments` de forma única. Não copie campos de
`package` para um `module`.

| Dado | Por que existe |
| --- | --- |
| Package name/version | Identidade e release. |
| Module set | Conjunto de modules permitido no target. |
| Dependency/source pin | Reprodutibilidade; não use PATH ambiente. |
| Target spec | OS, ABI, capabilities e execution platform. |
| Artifact digest | CAS/release e verificação do output. |
| Toolchain provider | Provider por digest e contrato, não SDK invisível. |

### CLI e tooling

`w package check [package]`, `w workspace check`, `w build`, `w run`, `w repl`,
`w test` e comandos de export são direções de interface. O target bootstrap `w`
implementa `w check path/file.w [--json]` no perfil CHK9 de root efêmera local,
com imports alcançáveis root-relative e os limites bounded registrados acima.
Owner detection, resolution externa, provider `std`, package/workspace, package
manager e as demais rotas da CLI continuam gaps. O tooling existente neste
checkout inclui o target bootstrap, Tree-sitter, atlas e checks de design.

### Distribuição binary-first e execução remota

O bundle [`RDX0`](tooling/studies/rdx0-binary-registry-execution/) registra a
direção do registry e as pesquisas de publicação, cápsula, evidence, runner,
sandbox e entitlement. O bundle não anuncia implementação.

| Eixo | Forma registrada | Limite |
| --- | --- | --- |
| Registry | `w.registry-http/1`, static-first | HTTP/1.1, HTTP/2 e HTTP/3 equivalentes; HTTPS fora de local explícito |
| Discovery | `/.well-known/w-registry.json` | metadata não concede authority |
| Package | `/v1/packages/<encoded-package-id>/index.json` | signed, bounded, monotonic, versions/channels e release digests |
| Release | `/v1/releases/<algorithm>/<digest>.json` | immutable por digest |
| Object | `/v1/objects/<algorithm>/<digest>` | GET/HEAD; Range opcional |
| Catalog checkpoint | `/v1/catalog/checkpoint.json` | trusted checkpoint assinado |
| Catalog pages | `/v1/catalog/pages/<first>-<last>.jsonl` | immutable append-only; mirror/search |
| Search | projection do catálogo, `/v1/search` opcional | nunca resolve known identity ou entra no lock |
| Evidence | `/v1/evidence/<algorithm>/<subject-digest>/index.json` | attestation objects imutáveis |
| Channel | `/v1/channels/<encoded-package-id>/<encoded-channel>/<encoded-target-profile>.json` | signed convenience, não substitui lock |

Release e objects não são reescritos. Deprecation recomenda replacement. Yank
impede nova resolution por default. Revocation bloqueia install ou execution no
scope. JSON é UTF-8 estrito e duplicate keys são rejeitadas. Package index
rollback compara com trusted checkpoint, não somente com contador do servidor.
Read capability ou signed URL privada concede acesso scoped por object/package,
audience e expiry, mas digest continua identity dos bytes. Channel JSON é
convenience e não substitui lock ou verification. Search só é reconstruído de
checkpoint e pages. Privacy mode escolhe 401, 403 ou 404 sem ampliar mirror ou
token authority.

`PCB0` usa release intent assinado e assertion OIDC curta. O serviço W valida
issuer, audience, subject, workflow e ref e emite capability W de publicação
one-use, curta e scoped. CI busca source da authority escolhida e publica
artifact e attestations. Builder, registry e maintainer têm authorities
distintas. Provider/tools podem observar source. Claim de descarte não prova
descarte físico. Pinned builder/toolchain/actions, egress mínimo, redaction de
logs/artifacts, secret lifecycle e provider identity entram no threat model.
Plano e billing de CI são externos. Não há claim de que GitHub gratuito atende
source fechado.
O ledger separa `oidc-assertion-replay` de `publication-capability-reuse`.

`WEC0` mantém HIR/MLIR/LLVM bitcode privado da recipe e exige exact toolchain
key para IR. `ExecutionDescriptor` registra entrypoints, requirements, sandbox
profile e payload refs. Fingerprints de section/chunk e runtime measurements
tratam relocation/ASLR sem raw in-memory hash. Benchmark source rebuild versus
exact capsule reuse/link mede compile time, cache, storage e network. Não existe
promessa de universal binary.

`TEV0` usa `TestDescriptor` e `TestPlan` para `@example`, `w test`, testes
co-localizados e `*.test.w`. Cada descriptor exige stable ID, owner, origin
carrier, source map, kind, fixtures/effects, oracle ou expected
diagnostic/outcome, target/profile, seed/limits e body/plan digest. Async,
cancellation e snapshot/golden identity têm cases próprios. Unit, compile-fail,
property/fuzz, simulation, provider, multi-process/hardware fault e performance
são lanes separadas. `SEV0` mantém security/advisory evidence, SBOM,
RuntimeClosure, reachability, matches, snapshots append-only e analyzer
conflicts por eixo/freshness, sem safe badge agregado.

`SBX0` pesquisa provider/profile enforcement antes do user code. Learn mode não
é receipt. `RSX0` exige resolução exata, authorization, admission, digest,
freshness, revocation e consumer policy para `w run package@version`; execution
remoto é sandboxed por default e native code usa child process ou compartment.
`ENT0` pesquisa lease opaco com expiry sem raw token na API. O witness adversarial
usa o fluxo `compile-final-menu / menu-compiler` do
[`reference/last-light/README.md`](reference/last-light/README.md) e percorre os
oito tasks, sem alegar execução. Nenhuma pesquisa promete DRM inviolável.

### REPL, module run e Jupyter

REPL e notebook devem usar o parser/checker/HIR normal, gerações transacionais e
receipts tipados. [repl_session_oracle.w](reference/last-light/repl_session_oracle.w),
[presentation.w](reference/last-light/presentation.w) e
[pyn3_oracle.w](reference/last-light/pyn3_oracle.w) mostram a direção. Não use
eval dinâmico, monkey patch, widget HTML oculto, replay sem receipt ou um
segundo parser de notebook.

As seções [DESIGN.md §24.1.2](DESIGN.md#2412-module-run-arquivo-único),
[DESIGN.md §24.1.3](DESIGN.md#2413-sessão-e-repl-transacionais) e
[DESIGN.md §24.1.4](DESIGN.md#2414-apresentação-jupyter-e-export-de-notebooks)
fecham os limites conhecidos, mas não anunciam um produto disponível.

## Receitas de uso

Os blocos seguintes são excerpts de call sites da Última Luz. Eles mostram
entrada, operação e resultado; não são source units novos e não prometem
execução.

### Ownership, erro e cleanup

Excerpt de `recoverGuest`: `guests` e `guestId` entram na consulta e o
resultado é um `ref Guest` ou `ServiceLookupError`.

```w excerpt
// excerpt-source: reference/last-light/failure.w::recoverGuest
    return try requireGuest(guests, id: guestId)
```

Excerpt de `decodeWithCleanup`: o scope registra o cleanup antes de operar e o
fecha na saída normal ou de erro.

```w excerpt
// excerpt-source: reference/last-light/failure.w::decodeWithCleanup
  defer { cleanupTrace.append(.closed) }
```

As duas linhas são de [failure.w](reference/last-light/failure.w), mas vêm de
funções diferentes; elas não formam uma sequência executável nova.

### Stream e channel

```w excerpt
// excerpt-kind: composed
// entrada: dois valores Order -> channel bounded
let (output, input) = Channel<Order>.open(capacity: 1)
let firstSend = async submitOrder(copy output, take first)
let secondSend = async submitOrder(copy output, take second)
let _ = take output

// resultado: envios do channel -> Array<Order> aceito
let accepted = await acceptOrders(take input)
let _ = try await firstSend
let _ = try await secondSend
return accepted
```

Este excerpt de [streams.w](reference/last-light/streams.w) transforma dois
`Order` em um `Array<Order>`. O initializer `async` cria siblings; os `await` finais
consomem os outcomes de envio e mantêm o erro de channel explícito.

### Quantity e matriz

```w excerpt
// excerpt-kind: composed
// entrada: unidades de duração equivalentes
let fromSeconds: PhysicalDuration = 30<si.s>
let fromMinutes: PhysicalDuration = 0.5<si.min>

// resultado: um valor canônico e um bit pattern
expect fromSeconds.canonicalValue == fromMinutes.canonicalValue
```

Em [quantity_oracle.w](reference/last-light/quantity_oracle.w), duas unidades
entram e produzem um valor canônico. Para uma matriz, o excerpt de
[horizon.w](reference/last-light/horizon.w) mantém shape e modo numérico no
call site:

```w excerpt
// excerpt-kind: composed
// entrada: features de window + matriz de calibration
let calibrated = window.features @ calibration
let means = calibrated.mean(axis: 0, mode: .reproducible)
// resultado: matriz centrada com shape declarado
let centered = calibrated - means.broadcast(to: [samples, 6])
```

Aqui a entrada é `window` mais `calibration`; o resultado é a matriz centrada.

### Package, module e invocation

```w excerpt
// excerpt-kind: manifest-fragment
module: "app"
entry: "LastLightTui"
```

```w excerpt
// excerpt-source: reference/last-light/app.w::entry LastLightTui
entry LastLightTui(runTuiEntry)
```

```text
w run last-light-native --deployment local -- --tui
```

O primeiro excerpt vem do product em
[build.w](reference/last-light/build.w); o segundo vem de
[app.w](reference/last-light/app.w). O package resolve o product para o module
e entry declarados; o module fornece o symbol callable.
O comando é a forma planejada em [BUILD.md](reference/last-light/BUILD.md),
mas `w run` e o package manager ainda não estão implementados neste checkout.
O resultado é uma seleção de product, não uma invocação disponível.

## Mesmo objetivo, várias formas

Esta é a matriz central para escolher uma forma. Cada linha tem pelo menos uma
forma corrente, uma condição ou uma lacuna quando necessário, e uma alternativa
rejeitada. As trocas devem ser avaliadas em cinco eixos: **ownership** (quem
possui e pode mover), **effects** (suspensão, erro, unsafe), **authority** (qual
domínio pode agir), **custo** (allocation, cópia, sync, ABI) e **evidência**.

| Objetivo | Forma vigente (current) | Outra forma/condição | Não use (rejected) | Troca principal | Design + Última Luz |
| --- | --- | --- | --- | --- | --- |
| Escolher root do product | entry Name(run) ou entry {} | Descriptor anônimo e module-run wrapper | entry(args, ctx) {} e process.main = run sem contrato | Ownership do root e lifecycle ficam explícitos; wrapper de arquivo único custa source-map | [DESIGN.md §13](DESIGN.md#13-módulos-de-execução-services-e-entries) · [app.w](reference/last-light/app.w) |
| Importar HTTP/std | import http from std, import std.http ou import { Name } from path | Resolução de capability/provider pelo manifest | Export default implícito e as que esconde a origem | Resolver manifest e capability custa verbosidade, mas fixa authority | [DESIGN.md §6](DESIGN.md#6-módulos-imports-e-visibilidade) · [http_documents.w](reference/last-light/http_documents.w) |
| Construir valor | Type(field: value) | Memberwise init fechado por type | new Type ou Type {} sem init | Labels tornam ownership e defaults auditáveis; synthesis exige schema | [DESIGN.md §8](DESIGN.md#8-tipos-e-conversões) · [domain.w](reference/last-light/domain.w) |
| Delegar initializer | self = Type(...) | Init helpers com contract head | self.init(...) como mutação escondida | Reassignment preserva estado observável e impede partial init implícito | [DESIGN.md §8](DESIGN.md#8-tipos-e-conversões) · [state_transitions.w](reference/last-light/state_transitions.w) |
| Resolver overload | Labels + arity + tipos | Constraint heads mais ricos | Ranking global por tipo e nomes únicos obrigatórios | Call site paga labels, mas evita escolha oculta e effect surpresa | [DESIGN.md §7](DESIGN.md#7-bindings-funções-e-closures) · [callables.w](reference/last-light/callables.w) |
| Representar ausência | T?, .none, .some, try? | Option com refinements | null, sentinel ou try! como padrão | Option torna branch/ownership visíveis; chaining pode custar checks | [DESIGN.md §8](DESIGN.md#8-tipos-e-conversões) · [failure.w](reference/last-light/failure.w) |
| Propagar erro | throws E, throw, try | Carrier Result em adapters | Exceção implícita ou conversão cancelamento→erro | Effects aparecem no callable; carrier explícito custa tipos, mas permite recovery | [DESIGN.md §11](DESIGN.md#11-erros-panic-oom-e-cleanup) · [failure.w](reference/last-light/failure.w) |
| Falhar invariantes | guard ... else throw ou panic explícito | Contract/refinement estático | debugAssert que some e validação silenciosa | Compile-time reduz custo de runtime; panic não é erro de domínio | [DESIGN.md §11](DESIGN.md#11-erros-panic-oom-e-cleanup) · [domain.w](reference/last-light/domain.w) |
| Passar ownership | ref, inout, take, copy, pin | view e projection borrow | Lifetimes públicas, partial move implícito e copy automático | Call site mostra autoridade; anotação custa caracteres e evita retenção oculta | [DESIGN.md §7](DESIGN.md#7-bindings-funções-e-closures) · [borrow_expressivity.w](reference/last-light/borrow_expressivity.w) |
| Capturar closure | <[copy x]>, <[ref x]>, <[take x]>, <[weak x]> | some fn/any fn conforme erase | Fn/FnMut/FnOnce ou capture inferido sem diagnóstico | Capture explícito reduz ciclos e custo de liveness; existential pode alocar | [DESIGN.md §9.4.1](DESIGN.md#941-captures-e-ciclos-fortes) · [callables.w](reference/last-light/callables.w) |
| Declarar contrato estático | type, refinement, enum subset, T: P & Q | Associated types e conformances condicionais | where textual, protocol list aberta ou guard runtime para invariantes estáticas | Schema fecha HIR e ABI; composição exige mais símbolos | [DESIGN.md §8](DESIGN.md#8-tipos-e-conversões) · [generics.w](reference/last-light/generics.w) |
| Refletir/sintetizar | Reflectable, TypeId.of<T>(), downcast borrowed e conformance head | Metadata limitada e declarada | Type<T> universal, downcast owned, derive mágico, metadata livre | Reflection tipada preserva custo e authority; synthesis universal seria difícil de auditar | [DESIGN.md §8](DESIGN.md#8-tipos-e-conversões) · [reflection.w](reference/last-light/reflection.w) |
| Aceitar rest arguments | T... + each values | Rest homogêneo com bound explícito | Pack heterogêneo obrigatório ou Array<Any> | Pack homogêneo preserva schema e ownership; materialização só ocorre quando pedida | [DESIGN.md §3](DESIGN.md#3-contratos-estáticos-e-orçamento-de-símbolos) · [rest_arguments.w](reference/last-light/rest_arguments.w) |
| Escrever matriz | [[1, 2], [3, 4]] | Carrier shape-checked | [1 2; 3 4] como grammar separada | Array literal é familiar; shape estático exige type/contract | [DESIGN.md §17](DESIGN.md#17-matrizes-tensors-e-ml) · [numerics.w](reference/last-light/numerics.w) |
| Controlar allocation | allocator scratch, .fixed, .root, .none | .bounded é Pesquisa descrita, não plano ASC0 | Arena API universal, propagação implícita ou using obrigatório | Budget explícito limita efeitos; annotations aumentam superfície | [DESIGN.md §9](DESIGN.md#9-memória-layout-e-alocação) · [allocation.w](reference/last-light/allocation.w) |
| Projetar borrow | ref T, view T, inout T | Projection física e borrow oracle | StringView/Slice públicos como segunda hierarquia | Menos tipos públicos, mas checker precisa acompanhar projection e liveness | [DESIGN.md §9](DESIGN.md#9-memória-layout-e-alocação) · [views.w](reference/last-light/views.w) |
| Executar async | direct call, await, `let x = async ...`, `let x = spawn<.compute> ...`, `let x = spawn<domain: .compute> ...`, TaskGroup map/collect | `limit`, `ordering` e `using` explícitos; collect devolve `TaskSettlement` | Promise/Future, detached task, launcher fora de `let`, spawn sem domain e collect que perde o input | Structured join preserva ownership; domain e bounds explícitos custam call-site | [DESIGN.md §12](DESIGN.md#12-concorrência-paralelismo-e-execução) · [execution.w](reference/last-light/execution.w) |
| Expressar urgência | deadline + service isolada + admission/reserva/budget; domain só para placement | política física aparece somente em `w explain execution` e provider receipt | `priority`/`qos`, domain como safety, `.background`, `.userInteractive`, `Task.currentPriority` ou `Task.withPriority` | Ordem garantida pelo contrato não muda; ordem unspecified e deadline/admission/winner podem variar entre traces permitidos | [DESIGN.md §12.6.2](DESIGN.md#1262-domain-placement-e-política-física-de-scheduling) · [BUILD.md](reference/last-light/BUILD.md#32-execution-profiles) |
| Consumir stream | for try await ref item in source ou stream <[take source]> { yield take/copy ... } | Stream pull e capacity declarados | Generator genérico, yield from e buffer oculto | Pull mantém backpressure e borrow; collect aloca e perde incrementalidade | [DESIGN.md §12](DESIGN.md#12-concorrência-paralelismo-e-execução) · [streams.w](reference/last-light/streams.w) |
| Enviar por channel | Channel<T><.send> / <.receive> (MPSC) | Capacity e close explícitos | Channel bidirecional implícito, MPMC infinito | Endpoints expressam authority; bounded buffer pode suspender | [DESIGN.md §12](DESIGN.md#12-concorrência-paralelismo-e-execução) · [streams.w](reference/last-light/streams.w) |
| Compartilhar estado | owner por domain, shared, atomic, channel | SnapshotCell e adapters especializados | Atomic<shared T>, mutex global ou RCU implícito | Serialização e snapshot reduzem races; cópia/sync têm custo visível | [DESIGN.md §12.10.7](DESIGN.md#12107-exclusão-mútua-como-último-recurso) · [synchronization.w](reference/last-light/synchronization.w) |
| Fazer I/O | ByteSource/ByteSink, ReadBatch/readMany e TransferPlan/transfer | Adapters async-first, leitura posicional, scatter e file-to-sink com estratégia explicável | IoSlice/IoSliceMut públicos, `inout view Bytes...`, syscall/probe no source, zero-copy universal, Reader/Writer síncronos e EOF sentinel | Owners escondem memória não inicializada e preservam retry; plans tornam reserva/progresso visíveis | [DESIGN.md §14.2.11](DESIGN.md#14211-backend-io-vetorizado-e-transferências-especializadas) · [io.w](reference/last-light/io.w) |
| Modelar service | closed turn + SupervisorRef + WorkKeyRef | Recovery com snapshot/dedup | Reentrant service, detached Promise e retry implícito | Effect identity permite replay seguro; metadata e storage têm custo | [DESIGN.md §13.9.3](DESIGN.md#1393-recovery-de-service-e-deduplicação) · [service_recovery_oracle.w](reference/last-light/service_recovery_oracle.w) |
| Selecionar build | package/workspace records em build.w + target spec + digest | Provider por capability e CAS | PATH/SDK ambiente, target string e config invisível | Reprodutibilidade exige manifest e receipts; setup fica mais explícito | [DESIGN.md §21](DESIGN.md#21-packages-builds-e-releases) · [build.w](reference/last-light/build.w) |
| Cruzar FFI | foreign c, carrier typed, unsafe fn<abi: .c> com nome | ABI adapters gerados sob prova; fn<C> é body inline C vigente | Tratar fn<C> como generic/sandbox, W object por C, foreign module W ou lib dinâmica sem capability | Unsafe ilha limita blast radius; marshaling custa cópia/validations | [DESIGN.md §19](DESIGN.md#19-ffi-unsafe-e-ilhas-de-linguagem) · [abi.w](reference/last-light/abi.w) |
| Representar unidade | 9.81<m/s^2>, Quantity(value, unit:) | wWire/JSON de Quantity como contrato de design, decoder/provider em gap | Bracket syntax, narrowing implícito e número fast | Units no tipo evitam erro dimensional; adapters custam schema | [DESIGN.md §15.5.3](DESIGN.md#1553-wwire-para-quantity) · [quantity_oracle.w](reference/last-light/quantity_oracle.w) |
| Serializar tabela | data.Batch<Row>, adapters CSV/Parquet/Arrow | Carrier tabular v1 e wWire | DataFrame universal, duck typing e Any | Schema fechado permite bounds e zero-copy futuro; adapters são verbosos | [DESIGN.md §14.4.1](DESIGN.md#1441-carrier-tabular) · [data_formats.w](reference/last-light/data_formats.w) |
| Mover tensor/device | tensor.transfer, Launch, Queue, Device | DLPack open/materialize/export | Runtime JIT mágico, raw stream, transfer implícito | Device é authority e custo explícitos; transfer pode copiar e suspender | [DESIGN.md §12.7.2](DESIGN.md#1272-device-scopes-e-kernels) · [tensor_interop.w](reference/last-light/tensor_interop.w) |
| Limpar recurso | defer / defer async + close explícito | Capability de lifecycle | errdefer, destructor detached e finalizer global | Ordem lexical é auditável; close async publica effect | [DESIGN.md §11](DESIGN.md#11-erros-panic-oom-e-cleanup) · [abort.w](reference/last-light/abort.w) |
| Abrir notebook | parser/checker/HIR normal + geração transacional | Presentable, Jupyter e export receipts | eval dinâmico, monkey patch e HTML oculto | Mesmo contrato reduz divergência; receipts e snapshots custam armazenamento | [DESIGN.md §24.1.4](DESIGN.md#2414-apresentação-jupyter-e-export-de-notebooks) · [pyn3_oracle.w](reference/last-light/pyn3_oracle.w) |
| Transformar collection | Pipeline lazy sem side-effect: `tickets.lazy.filter(...).map(...).take(...).collect()` (Forma vigente) | Loop explícito com `for`, `append` e `break` (Forma vigente para controle e side-effect) | Comprehension (Rejeitado por enquanto) | Pipeline adia custo até `collect`; loop expõe controle, effects e ownership; ambos preservam ordem e limite | [DESIGN.md §16](DESIGN.md#16-texto-bytes-e-collections) · [collections.w](reference/last-light/collections.w) |
| Acessar índice ou fim | `get(index)` e `.last`; `suffix` somente quando o carrier o publica (Forma vigente por carrier) | `count - 1` após guard (alternativa explícita) | `[-1]` ou syntax relativa especial (Rejeitado por enquanto) | APIs nominais deixam bounds e `Option` visíveis; arithmetic exige guard; suffix pode preservar view e borrow do carrier | [DESIGN.md §16.2](DESIGN.md#162-views-índices-e-slices) · [collections.w](reference/last-light/collections.w) · [billing.w](reference/last-light/billing.w) |
| Ajustar shapes tensor | `means.broadcast(to: [samples, 6])` com shape checked (Forma vigente) | Scalar expansion conforme o contrato do carrier (Forma vigente, sem shape inferido) | Broadcast implícito entre shapes diferentes ou dotted broadcast (Rejeitado por enquanto) | Shape explícito compra diagnóstico e autoridade de device; a operação custa tokens, mas evita mismatch e eixos ocultos | [DESIGN.md §17](DESIGN.md#17-matrizes-tensors-e-ml) · [horizon.w](reference/last-light/horizon.w) · [ai_harness.w](reference/last-light/ai_harness.w) |
| Preservar ordem de call labels | Ordem de declaration: `Money(majorUnits: 42, currency: .cr)` (Forma vigente) | Defaults e overloads criam sequências ordenadas distintas | Labels unordered ou reordered (Rejeitado por enquanto) | A ordem torna resolver e diagnostics determinísticos; labels custam source, mas evitam ranking e effects ocultos | [DESIGN.md §7.2.2](DESIGN.md#722-overloads-por-forma-de-call) · [billing.w](reference/last-light/billing.w) |
| Escolher ownership de callable | `fn`, `some fn`, `any fn`, `mut fn` e `take fn` separados (Forma vigente) | Capture `<[copy ...]>`, `<[ref ...]>`, `<[take ...]>` ou `<[weak ...]>`; erase só quando pedido | `fn` unificado que apaga modo e custo (Rejeitado por enquanto) | Modos mantêm ownership, mutação, erasure e allocation observáveis; a separação aumenta a assinatura e reduz inferência oculta | [DESIGN.md §7.5](DESIGN.md#75-valores-callable-e-closures) · [callables.w](reference/last-light/callables.w) |
| Esperar siblings com fail-fast | Tuple `try await (left, right)` em join lexical (Forma vigente) | `try await left` e depois `try await right` (Forma vigente, mas não equivalente) | Gather detached, fire-and-forget ou task sem owner (Rejeitado) | Tuple cancela siblings no primeiro erro settled e drena cleanup; awaits sequenciais mudam observação, timing e cancel; escolha altera effects e custo | [DESIGN.md §12.4](DESIGN.md#124-join-erro-e-outcome) · [execution.w](reference/last-light/execution.w) |
| Escolher primeiro settlement | `await Task.firstSettled(take tasks)` com `TaskSettlement?` (Forma vigente) | Tuple join ou `Task.withTimeout` quando a intenção é fail-fast ou timeout | `select` statement, first-success implícito, drop de future ou retorno antes do drain (Rejeitado) | Completion order vira resultado; losers cancelam e drenam, mas effects committed permanecem | [DESIGN.md §12.4.1](DESIGN.md#1241-first-settled-estruturado) · [task_settlement.w](reference/last-light/task_settlement.w) |
| Encerrar receiver consuming | `(take cursor).finish()` explicita a transferência antes do lookup (Forma vigente) | `take fn finish()` declara o member consuming e torna o contrato visível | `cursor.finish()` com inferência de receiver (Rejeitado; `W-OWNERSHIP-0011`) | O prefixo preserva a fronteira de ownership e o erro de uso; inferência esconderia move, cleanup e indisponibilidade posterior | [DESIGN.md §7.3](DESIGN.md#73-parâmetros-e-ownership) · [command.w](reference/last-light/command.w) · [state_transitions.w](reference/last-light/state_transitions.w) |
| Encadear envelopes de contrato | `StaticList<ServiceStage><(isValidStagePath(.member))>` sequencial (Forma vigente) | Typestate `StagePath` e transitions fechadas no mesmo domínio | `StaticList<[ServiceStage, (isValidStagePath(.member))]>` fused (`W-CONTRACT-0002`, Rejeitado) | Envelopes sequenciais preservam o kind de cada slot e a ordem de validação; fused economizaria tokens, mas perde schema e diagnóstico | [DESIGN.md §3.5.4](DESIGN.md#354-grammar-normativa-g2-tipos-e-contratos-angulares) · [domain.w](reference/last-light/domain.w) · [state_transitions.w](reference/last-light/state_transitions.w) |

### Como escolher uma linha

1. Comece pela forma current. Ela tem o menor risco de divergir do contrato.
2. Use direction apenas quando o target ou o produto exigir a capability.
3. Marque implementation-gap no issue ou no README do package; não esconda a
   lacuna sob um snippet que parece executável.
4. Se uma alternativa rejected parecer mais simples, escreva a necessidade e a
   evidência que poderiam reabrir a decisão em vez de adotá-la em source.

## Índices rápidos

### Literais

| Categoria | Exemplos |
| --- | --- |
| Inteiro | 0, 1_000, 0xff |
| Float | 0.5, 0.5e2 |
| Bool | true, false |
| Char/byte | 'N', b'\x4e' |
| String | "text", #"raw"#, """multi""" |
| Unit | 12km, 64KiB, 9.81<m/s^2> |
| Tuple | (north: 1, east: 2) |
| Array/map | [1, 2], ["north": 1] |
| Repeated | [0; 4] |

### Ownership e callable modes

| Mode | Pista de leitura |
| --- | --- |
| let / var | Mutabilidade do binding. |
| ref / view | Borrow de leitura/projection. |
| inout | Borrow mutável exclusivo. |
| take / copy | Move ou cópia explícita. |
| shared / weak | Liveness compartilhada ou não-owning. |
| fn / mut fn | Callable síncrono, com mutação quando declarada. |
| async fn | Callable que pode suspender. |
| throws E | Callable que publica error effect. |
| static / const | Avaliação e acesso sem estado de instance; confira o contrato. |
| foreign / unsafe | Fronteira externa ou operação fora das garantias normais. |

### Effects

| Effect | Aparece como | Pergunta antes de usar |
| --- | --- | --- |
| Suspension | await, async, maySuspend | Quem faz join e como cancellation chega? |
| Error | throws, throw, try | O caller trata o carrier ou propaga? |
| Ownership | take, ref, inout, copy, pin | Quem pode mover, mutar ou manter vivo? |
| Allocation | allocator, shared, carrier | Qual budget e qual allocator? |
| Synchronization | atomic, lock, transaction | Qual domain e qual ordem de memória? |
| Unsafe/FFI | unsafe, foreign | Qual prova de ABI, capability e layout? |
| Cleanup | defer, defer async, close | A saída normal e a falha fecham o recurso? |

### Status de implementação

| Camada | Estado neste checkout |
| --- | --- |
| Design e forma de source | Forma vigente para avaliação, não release |
| Atlas/Tree-sitter | Protótipo de parse e corpus; não checker/runtime |
| Oracles host | Evidência lógica/física de design; não runtime |
| Formatter/frontend/HIR/MLIR | Planejados; implementation gap |
| Runtime/scheduler/allocator | Planejados; implementation gap |
| std/providers | Contratos e oracles; provider missing |
| CLI além de `w check` / package manager | Direção; implementation gap |
| Portal | Protótipo visual congelado; não autoridade |

## Evidência, limites e validação

### Fontes

- [DESIGN.md](DESIGN.md) é normativo. As âncoras desta página apontam para
  contratos correntes, pendências e ordem de implementação.
- [DESIGN-INDEX.md](DESIGN-INDEX.md) é uma projeção gerada para navegação.
- [reference/last-light/README.md](reference/last-light/README.md) define os
  oracles e os limites do produto de referência.
- [reference/syntax-atlas/CHEATSHEET.md](reference/syntax-atlas/CHEATSHEET.md)
  é gerado. Seus snippets são parse-only, salvo indicação contrária.
- [std/README.md](std/README.md) descreve contratos de std. Não há std build ou
  provider implícito neste documento.

### O que um snippet prova

Um snippet tree-sitter-parse-only prova apenas que a gramática do corpus o
aceitou no momento da geração. Um snippet source-backed prova que a forma está
escrita em um fixture ou oracle. Um link para um oracle host prova que há uma
decisão ou teste de design. Nenhum desses rótulos prova compiler, type-checker,
runtime, scheduler, ABI, CLI, codec, serviço ou provider pronto.

### Checks recomendados

Na raiz do repositório, depois de alterar esta página e o link do README:

```text
bun run check:links
bun run --cwd tooling/tree-sitter-w check:syntax-atlas
bun run --cwd tooling/tree-sitter-w parse:reference
bun run --cwd tooling/tree-sitter-w parse:std
bun run design:index:check
git diff --check
bun run --cwd tooling/tree-sitter-w check:docs
```

Os quatro primeiros checks mantêm links, atlas, corpus de referência, corpus de
std e índice. check:docs é o gate integral e pode demorar mais. Antes de
propor uma mudança, revise headings/anchors e procure claims proibidos:

```text
rg -n -i "implemented|compiler.*works|runtime.*works|provider.*available|CLI.*available|std.*available" CHEATSHEET.md
```

Se a busca encontrar uma frase afirmativa, reescreva-a como contrato, direção,
evidência ou lacuna. Não faça stage ou commit de um atlas gerado para corrigir
este arquivo.
