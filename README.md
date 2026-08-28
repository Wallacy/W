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
Owner detection, resolução externa, provider `std`, package/workspace e o
frontend normativo completo continuam gaps.

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
8. [Build do Última Luz](reference/last-light/BUILD.md) — products, target
   specs, toolchain plans, ABIs, artifacts e gates;
9. [Rascunho da std](std/README.md) — contratos da standard library em W;
10. [Tooling](tooling/README.md) — Tree-sitter, TextMate e extensão local.

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

`docs:check` é o gate focal para as projeções documentais e o índice.
`check:docs` permanece o gate completo, com BMD e a cadeia do Tree-sitter.

Para validar somente documentação e índice:

```powershell
bun run docs:check
```

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
| [Seed C: source reader, lexer, scanner C, parser, formatter, frontend seed, HIR0/HLO0/HLO1 e target bootstrap w](compiler/seed-c/README.md) | implementação caller-owned/incremental; Unicode 17 pinado, scanner C source-validation-only, parser/formatter CST, frontend seed e adapter D0; `w check` executa CHK9 bounded para root efêmera local, com até 64 sources e 16 MiB por source/agregado; HIR0 verifica o subset e HLO1 emite/executa C11 somente para Hello World verified-HIR-backed; não é frontend normativo completo, compiler ou typechecker completo |
| Formatter normativo, frontend normativo completo, HIR geral e MLIR | planejados, não implementados; o formatter, o frontend seed e a HIR0 verificada são fatias fechadas e não substituem essas camadas |
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

Um script de arquivo único pode usar statements finais sem `entry {}`.
O comando `w run path/file.w` cria um wrapper `.default` privado para esse body
e preserva o source map. Use `fn` mais `entry(fnName)` quando o handler precisar
de `args`, `Context`, return customizado ou errors públicos.

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
o estado atual. A lista dos arquivos históricos preservados está em
[`history/README.md`](history/README.md). Esses arquivos preservam proveniência.
Eles não definem o W atual.
