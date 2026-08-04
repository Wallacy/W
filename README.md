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

## Uma fonte de verdade

Leia estes artefatos nesta ordem:

1. [DESIGN-INDEX.md](DESIGN-INDEX.md) — índice gerado com intervalos, métricas e
   pesquisas abertas; não define semântica;
2. [DESIGN.md](DESIGN.md) — contrato integral de linguagem, runtime, SDK,
   compilador, packages, distribuição, tooling, plano e alternativas;
3. [Última Luz](reference/last-light/README.md) — produto de referência,
   oracles e fontes `.w`;
4. [Build do Última Luz](reference/last-light/BUILD.md) — products, target
   specs, toolchain plans, ABIs, artifacts e gates;
5. [Deployments](reference/last-light/deployments/README.md) — planos local e
   distribuído;
6. [Rascunho da std](std/README.md) — contratos da standard library em W;
7. [Tooling](tooling/README.md) — Tree-sitter, TextMate e extensão local.

O [portal](portal/README.md) é um protótipo visual congelado. Ele não precisa
acompanhar cada mudança antes do design freeze.

`DESIGN.md` é a única autoridade para o estado atual. O Book e o produto de
referência mostram esse contrato, mas não criam regras próprias.

Use `DESIGN-INDEX.md` para localizar uma seção sem carregar o documento
integral. O check do tooling falha quando o índice fica desatualizado.

Para extrair somente uma seção, um heading ou uma decisão com contexto, use o
leitor sem escrita:

```powershell
bun tooling/design-slice.mjs --heading 12.13
bun tooling/design-slice.mjs --id W-711 --context 2
```

O leitor apenas recorta `DESIGN.md`; ele não cria uma segunda fonte de verdade.

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
| Alternativas | preservadas por decisão em `DESIGN.md` |
| Tree-sitter e highlighting | protótipo funcional |
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
  let command = try request.json.decode<Command>()
  let response = try await dispatch(take command, restaurant: lastLight)
  return try http.Response.json(response)
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
fornece uma build transform tipada ao package principal. O tool recebe somente
o cardápio declarado e produz um resource no CAS.

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
