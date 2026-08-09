# W

> **Working Draft · 3 de agosto de 2026**
>
> **Joy for humans. Clarity for machines.**
>
> Prazer para humanos. Clareza para máquinas.

W é uma proposta de linguagem nativa para aplicações, sistemas, concorrência,
paralelismo e computação científica. O compiler e o runtime ainda não existem.
A superfície atual é um design experimental para revisão.

Contribuições humanas, assistidas por IA e automatizadas são bem-vindas. W
avalia o resultado, a evidência e a responsabilidade. A ferramenta usada não
define a qualidade da contribuição.

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
5. [Build do Última Luz](reference/last-light/BUILD.md) — products, target
   specs, toolchain plans, ABIs, artifacts e gates;
6. [Deployments](reference/last-light/deployments/README.md) — planos local e
   distribuído;
7. [Rascunho da std](std/README.md) — contratos da standard library em W;
8. [Tooling](tooling/README.md) — Tree-sitter, TextMate e extensão local.

O [portal](portal/README.md) é um protótipo visual congelado. Ele não precisa
acompanhar cada mudança antes do design freeze.

`DESIGN.md` é a autoridade normativa para o estado atual. `RATIONALE.md` explica
por que o contrato existe, sem definir comportamento. O Book e o produto de
referência mostram esse contrato, mas não criam regras próprias.

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

Para validar somente documentação e índice:

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
| Formatter, frontend, HIR e MLIR | planejados, não implementados |
| Runtime, SDK e package manager | planejados, não implementados |
| Governança | liderança inicial; contribuição aberta e revisão baseada em evidência |
| services, `ServiceLink`, `pipeline` e wRPC | **Direção**; implementação na fase 6 |
| wWire | **Pesquisa**; layout, registro core v0 e seed vectors vigentes; decoder e custo exigem protótipo |
| wQL, V6, GPU e HDL | **Pesquisa** ou T2 fora do bootstrap |

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

## Histórico

As notas originais e os documentos substituídos ficam em
[`history/`](history). A consolidação substituída de 27 de julho de 2026 está no
[arquivo histórico](history/archive/db1-2026-07-27).
Esses arquivos preservam proveniência. Eles não definem o W atual.
