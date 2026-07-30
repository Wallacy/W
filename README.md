# W

> **Working Draft · 29 de julho de 2026**
>
> **Prazer para humanos. Clareza para máquinas.**

W é uma proposta de linguagem nativa para aplicações, sistemas, concorrência,
paralelismo e computação científica. O compiler e o runtime ainda não existem.
A superfície atual é um design experimental para revisão.

## Uma fonte de verdade

Leia estes artefatos nesta ordem:

1. [DESIGN.md](DESIGN.md) — contrato integral de linguagem, runtime, SDK,
   compilador, packages, distribuição, tooling, plano e alternativas;
2. [Última Luz](reference/last-light/README.md) — produto de referência,
   oracles e fontes `.w`;
3. [Build do Última Luz](reference/last-light/BUILD.md) — products, targets,
   artifacts e gates;
4. [Rascunho da std](std/README.md) — contratos da standard library em W;
5. [Tooling](tooling/README.md) — Tree-sitter, TextMate e extensão local.

O [portal](portal/README.md) é um protótipo visual congelado. Ele não precisa
acompanhar cada mudança antes do design freeze.

`DESIGN.md` é a única autoridade para o estado atual. O Book e o produto de referência
mostram esse contrato, mas não criam regras próprias.

## Estado atual

| Camada | Estado |
|---|---|
| Visão e invariantes | **Direção** |
| Forma integrada da linguagem | **Forma vigente** para avaliação |
| Alternativas | preservadas por decisão em `DESIGN.md` |
| Tree-sitter e highlighting | protótipo funcional |
| Formatter, frontend, HIR e MLIR | planejados, não implementados |
| Runtime, SDK e package manager | planejados, não implementados |
| wQL, wRPC, V6, GPU e HDL | **Pesquisa** fora do caminho crítico |

## Amostra

```w
import std.http
import { Command } from restaurant.command
import { RestaurantApi } from restaurant.restaurant

const restaurantService = ServiceBinding<RestaurantApi>(name: "last-light")

async fn fetch(request: http.Request, ctx: http.Context): http.Response throws AppError {
  let restaurant = try await ctx.services.get(restaurantService)
  let command = try request.json.decode<Command>()
  let response = try await dispatch(take command, restaurant: restaurant)
  return try http.Response.json(response)
}

entry(runNative) {
  process.signal = shutdown
}

entry LastLightTui(runTui)
```

O descriptor anônimo é o default do product nativo. O mesmo binário atende CLI,
TUI e servidor local. Um worker usa outro product e outro host lifecycle.

O `place()` dessa rota permanece um oracle de closed turn longo. A rota alvo
resolve `ServiceFamily<OrderCoordinatorApi, OrderId>`. O descriptor injeta um
`WorkKeyRef` limitado ao pedido. Consulte
[`supervision.w`](reference/last-light/supervision.w).

## Histórico

As notas originais e os documentos substituídos ficam em
[`Y/W/`](../Y/W/). A tentativa de consolidação DB1 está em
[`Y/W/archive/db1-2026-07-27/`](../Y/W/archive/db1-2026-07-27/). Esses arquivos
preservam proveniência. Eles não definem o W atual.
