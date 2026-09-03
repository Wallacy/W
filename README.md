# W

> **Working Draft · 10 de agosto de 2026**
>
> **Joy for humans. Clarity for machines.**
>
> Prazer para humanos. Clareza para máquinas.

W é uma proposta de linguagem nativa para aplicações, sistemas, concorrência,
paralelismo e computação científica. O compiler W completo, o runtime e
o package manager continuam gaps. O seed C possui source reader, lexer lossless,
scanner C de validação de fonte, parser seed caller-owned e incremental,
formatter seed CST-driven e adapter D0 caller-owned, com suporte sintático
coberto por 28 IDs F0. O target bootstrap `w` executa a rota pública `w check`
no perfil CHK9 de root efêmera explícita e imports locais alcançáveis.
O seed também executa o subset print-literal input-driven verified-HLO0 por
HLO1/RUN0 em gates internos bounded e test-only. W-1520 define a primeira rota
nativa real: source → parser/frontend → HIR0 verificada → MLIR0 → LLVM dialect →
LLVM IR → clang/native, sem passar por C source ou depender de HLO0. Hello é o
witness canônico, mas o mesmo caminho aceita payloads String variáveis e
compara stdout exato. A evidência MLIR0 é Linux x86_64 sob WSL no checkout
Windows, não suporte Windows nativo.
[`PLATFORM-SUPPORT.md`](PLATFORM-SUPPORT.md) publica a matriz operacional de
targets, compiler hosts e cross-compilation. O baseline primário tem nove
edges host→target, incluindo self edges. Nenhum edge é supported. A edge WSL
de desenvolvimento fica fora da matriz nativa e não promove Windows. A
evidência factual MLIR0 usa 20.1.2 e está marcada `update-required`; planos
futuros pinam `llvmorg-23.1.0`, mas permanecem bloqueados por build, aquisição,
proveniência, outputs, CI e smoke evidence. O pin sucessor não promove a
evidência corrente.
[`DEPENDENCIES.md`](DEPENDENCIES.md) publica o catálogo gerado de currency,
compatibility floors, evidence snapshots e external evaluations.
HIR0/W-1494 continua uma representação intermediária bounded mais ampla; é o
seletor HLO0 W-1505, sobre HIR0 verificada, que aplica a forma direta de uma
função/entry/block/call/argument. W-1519 adds a bounded verified binding shape.
Os nomes target/handler são byte strings
derivadas da HIR0, iguais e zero-tail; o verifier de plano isolado não prova
source provenance nem identifier válido.
Essa evidência não publica `w run` nem a leitura segura dos arquivos de entrada.
Owner detection, resolução externa, provider `std`, package/workspace e o
frontend normativo completo continuam gaps.

W-1519 is `source-backed-current` for a strictly bounded immutable local
String path. Frontend schema `w-seed-frontend-11` publishes an indexed lexical
binding relation. HIR0 schema `w-seed-hir0-2` copies the binding name and bytes,
then verifies its owners, order, types, spans, dense ranges, alias barriers,
digests, and receipt. HLO0 schema `w-seed-hlo0-2` accepts direct `CONST_STRING`
or exactly one `BINDING → CALL` chain with `BINDING_READ` by binding index.
HLO0 proves that binding plan independently; MLIR0 consumes the same verified
HIR directly. The Restaurant witness reaches MLIR0 and native execution with
exact stdout `Table 42 remains open\n`.
W-1520 replaces MLIR0 schema v1 in place with `w-seed-mlir0-2`: its internal
API accepts only `{ program, hir_result }`, re-verifies HIR through the private
shared native-subset selector, and performs no textual lookup. HLO0/HLO1/RUN0
remain bootstrap, audit, and recovery paths, not native prerequisites.
This decision does not claim general locals, `var`, assignment, nested scopes,
general SSA, ownership, additional targets, public `w run`, or performance.

MAN0 é o reader C controlado, compilado em modo C23, guarded, caller-owned e
bounded de manifests estruturais. C11 é somente a lane explícita de
recovery/compatibilidade do seed.
Ele lê todos os candidates OWN0 em duas waves e publica somente depois de
igualdade de bytes, bindings e digests. O gate Linux real usa a sessão retida;
em host Windows, WSL Ubuntu é obrigatório e o adapter Windows permanece um
stub direto `UNSUPPORTED` fail-closed. A evidência é estreita e não fecha
Windows operacional, vínculo ACQ0, schema decoder, WSP0 ou produto público.

BND0 compõe ACQ0, OWN0 e MAN0 em uma binding caller-owned e bounded. A
publication é all-or-nothing e fica presa à generation do guard. O adapter
Linux exige o token `linux-openat2-v2` e a identidade
`STATX_MNT_ID_UNIQUE` + device major/minor + inode. Adapters ausentes falham
fechados. BND0 não abre `w run`, package/workspace geral, registry, backend ou
runtime, e sua evidência Linux permanece limitada ao gate bounded.

Dois objetivos centrais orientam o design: gerência automática de memória sem
anotações de lifetime no caminho comum e execução estruturada que mantém
concorrência, paralelismo e placement explícitos no call site. Esses contratos
estão especificados e possuem oracles de design. Eles ainda não são claims de
uma implementação pronta.

Contribuições humanas, assistidas por IA e automatizadas são bem-vindas. W
avalia o resultado, a evidência e a responsabilidade. A ferramenta usada não
define a qualidade da contribuição.

Leia [Sobre o W](ABOUT.md) para uma visão curta da origem, dos princípios e do
estado atual do projeto.
Leia [Mapa do repositório](REPOSITORY.md) para a organização da infraestrutura
e [Catálogo de estudos](STUDIES.md) para a projeção humana dos estudos.

## Fontes canônicas

Leia estes artefatos nesta ordem:

1. [DESIGN-INDEX.md](DESIGN-INDEX.md) — índice gerado com intervalos, métricas e
   pesquisas abertas; não define semântica;
2. [DESIGN.md](DESIGN.md) — autoridade normativa para contratos correntes,
   estado, pesquisas que alteram o contrato e ordem de implementação;
3. [RATIONALE.md](RATIONALE.md) — justificativas, evidência, alternativas e
   proveniência; é complementar e não normativa;
4. [Última Luz](reference/last-light/README.md) — produto de referência,
   oracles e fontes `.w`;
5. [Atlas sintático](reference/syntax-atlas/README.md) — projeção gerada dos
   snippets marcados, com evidência Tree-sitter parse-only; não é um tutorial;
6. [Cheatsheet W](CHEATSHEET.md) — guia editorial de uso, alternativas,
   trade-offs e limites de evidência; ele não é gerado pelo atlas;
7. [Catálogo de diagnostics](DIAGNOSTICS.md) — índice humano gerado para
   códigos, fatos, papéis, fixes e referências normativas;
8. [Catálogo de estudos](STUDIES.md) — status, função, path, gate e entrypoint
   dos estudos registrados;
9. [Mapa do repositório](REPOSITORY.md) — diretórios, autoridade, dependências
   e comandos públicos;
10. [Catálogo de dependency currency](DEPENDENCIES.md) — versões gerenciadas,
    pisos de compatibilidade, snapshots e avaliações externas;
11. [Build do Última Luz](reference/last-light/BUILD.md) — products, target
   specs, toolchain plans, ABIs, artifacts e gates;
12. [Rascunho da std](std/README.md) — contratos da standard library em W;
13. [Tooling](tooling/README.md) — infraestrutura, Tree-sitter, TextMate e
    extensão local.

O [portal](portal/README.md) é um protótipo visual congelado. Ele não precisa
acompanhar cada mudança antes do design freeze.

`DESIGN.md` é a autoridade normativa para o estado atual. `RATIONALE.md` explica
por que o contrato existe, sem definir comportamento. O Book e o produto de
referência mostram esse contrato, mas não criam regras próprias.

## Superfícies humanas e de máquina

Use o [Cheatsheet W](CHEATSHEET.md) para rotas de uso e trade-offs. Use o
[catálogo humano de diagnostics](DIAGNOSTICS.md) para buscar códigos e abrir a
referência normativa correspondente. Use o
[SYNTAX-COVERAGE.md](reference/syntax-atlas/SYNTAX-COVERAGE.md) para cobertura
técnica de snippets parse-only.
Use [STUDIES.md](STUDIES.md) para consultar estudos por status sem percorrer os
JSONs. Os READMEs locais explicam cada estudo quando disponíveis.
Use [DEPENDENCIES.md](DEPENDENCIES.md) para consultar currency, pisos,
snapshots e avaliações sem percorrer os manifests.

As superfícies de máquina ficam em `tooling/*.json`, nos checkers e no manifest
do atlas. Elas sustentam geração e gates. Nenhuma projeção substitui
`DESIGN.md` como autoridade semântica.

O arquivo [`reference/syntax-atlas/SYNTAX-COVERAGE.md`](reference/syntax-atlas/SYNTAX-COVERAGE.md)
é gerado por `bun tooling/syntax-atlas.mjs --write` e prova somente o parse dos
snippets marcados. O [Cheatsheet W](CHEATSHEET.md) é mantido como texto
editorial e explica quando usar cada forma. O [catálogo de diagnostics](DIAGNOSTICS.md)
é outra projeção gerada para consulta humana. Não confunda essas superfícies.

Use `DESIGN-INDEX.md` para localizar uma seção sem carregar o documento
integral. O check do tooling falha quando o índice fica desatualizado.

Para extrair somente uma seção, um heading ou uma decisão com contexto, use o
leitor sem escrita:

```powershell
bun tooling/design-slice.mjs --heading 12.13
bun tooling/design-slice.mjs --id W-711 --context 2
bun tooling/design-slice.mjs --rationale-heading 1.1
```

O leitor recorta `DESIGN.md` para contratos e `RATIONALE.md` para evidência e
ledger; ele não cria uma segunda fonte de autoridade.

`docs:check` é o gate focal para as projeções documentais, dependency currency e
o índice.
`check:docs` permanece o gate completo, com BMD e a cadeia do Tree-sitter.

Para validar somente documentação e índice:

```powershell
bun run docs:check
```

Para a manutenção diária, use `bun run check:quick`; para os gates do
compilador seed use `bun run check:compiler`.

Para regenerar somente as projeções de documentação:

```powershell
bun run docs:write
```

Para validar também BMD e toda a cadeia documental do Tree-sitter:

```powershell
bun run tooling:install
bun run check:docs
```

Use `bun run check` quando grammar, corpus, std ou sources `.w` mudarem.

## Estado atual

| Camada | Estado |
|---|---|
| Visão e invariantes | **Direção** |
| Forma integrada da linguagem | **Forma vigente** para avaliação |
| Alternativas | justificadas em `RATIONALE.md`; o contrato escolhido fica em `DESIGN.md` |
| Tree-sitter e highlighting | protótipo funcional |
| Oracles host de memória | M1 lógico e A0 físico congelados como evidência de design; não são runtime |
| [Seed C: source reader, lexer, scanner C, parser, formatter, frontend seed, HIR0/HLO0/HLO1/MLIR0/RUN0 e target bootstrap w](compiler/seed-c/README.md) | seed mínimo caller-owned: `w check` CHK9, HIR0/HLO0 bounded, HLO1 C23 bootstrap/recovery, MLIR0 LLVM-dialect terminal para um target e RUN0 interno test-only. Limites, witnesses e gaps ficam no README do componente; o checkout não publica `w run` |
| [Matriz de platform support](PLATFORM-SUPPORT.md) | catálogo gerado de targets, compiler hosts e baseline cross-compilation 3x3; evidence WSL é dev-only e não é Windows nativo; os planos nativos pinam LLVM 23.1.0, mas aguardam build/proveniência |
| Formatter normativo, frontend normativo completo, HIR geral e W/MLIR geral | planejados, não implementados; formatter, frontend seed, HIR0 verificada e ponte MLIR0 são fatias fechadas e não substituem essas camadas |
| Runtime, SDK e package manager | planejados, não implementados |
| Governança | liderança inicial; contribuição aberta e revisão baseada em evidência |
| services, `ServiceLink`, `pipeline` e wRPC | **Direção**; implementação na fase 6 |
| wWire | **Pesquisa**; layout, registro core v0 e seed vectors vigentes; decoder e custo exigem protótipo |
| wQL, V6, GPU e HDL | **Pesquisa** ou módulos separados fora do bootstrap |

## Amostra

```w
import http from std
import { Command } from command
import { lastLight } from restaurant

async fn fetch(request: http.Request, ctx: http.Context): http.Response throws AppError {
  let command = try await (take request).json<Command>(maximumBytes: 64<KiB>)
  let response = try await dispatch(take command, restaurant: lastLight)
  return try http.Response.json(value: ref response, maximumBytes: 64<KiB>)
}

entry(runNative)
entry LastLightTui(runTuiEntry)
```

O descriptor anônimo é o default do product nativo. O mesmo binário atende CLI,
TUI e servidor local. Um segundo product escolhe `LastLightTui` para gerar uma
TUI dedicada. Linux, Darwin e Windows mantêm o mesmo import; o manifest escolhe
o module set nativo. O entry registra os process signals no runtime. Um worker
usa outro product e outro host lifecycle.

Um arquivo único é um módulo normal. Ele exige uma declaração `fn` e um
`entry`. A direção futura `w run path/file.w` seleciona `.default` ou usa
`--entry` para selecionar outro entry. O checkout atual não publica esse
comando. Statements finais sem `entry` são rejeitados.

O Última Luz também é um workspace. O package `last-light/menu-compiler`
descreve uma build transform tipada para o package principal. O contrato recebe
somente o cardápio declarado e prepara candidatos para um action-result/manifest
no CAS. O host publica esse record somente após success, outputs obrigatórios e
budgets válidos. O provider `std.build@1` continua missing.

O mesmo package publica uma static library `.wExact` e uma façade C dinâmica do
horizon monitor. Esse laboratório separa `WInterface`, ABI W, runtime
requirements e carriers C.

O `place()` dessa rota permanece um oracle de closed turn longo. A rota alvo
resolve `ServiceFamilyRef<OrderCoordinatorApi, OrderId>`. O runtime graph
passa um `WorkKeyRef` limitado ao pedido para o initializer. Consulte
[`supervision.w`](reference/last-light/supervision.w).

## Participar

Comece pelo [guia de contribuição](CONTRIBUTING.md). Ele define o fluxo comum
para pessoas, equipes, automação e agentes de IA.

- [Governança](GOVERNANCE.md) define autoridade, decisão, recurso e transição.
- [Guia para maintainers](MAINTAINERS.md) define revisão, merge e manutenção.
- [Código de Conduta](CODE_OF_CONDUCT.md) protege colaboração respeitosa.
- [Política de segurança](SECURITY.md) define relato privado e escopo atual.

Somente pessoas podem ser maintainers, aprovar merges e assinar releases.
Ferramentas podem ajudar em qualquer etapa verificável. A pessoa que envia ou
aprova uma mudança continua responsável pelo resultado.

## Sobre e proveniência

[Sobre o W](ABOUT.md) resume a origem, o método do restaurante, os princípios e
o estado atual. A proveniência removida do checkout permanece no histórico do
Git. `ABOUT.md` resume a narrativa e `RATIONALE.md` registra as decisões e a
evidência sem definir o W atual.
