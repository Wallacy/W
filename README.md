# W

> **Working Draft · 29 de julho de 2026**
>
> **Prazer para humanos. Clareza para máquinas.**

W é uma proposta de linguagem nativa para aplicações, sistemas, concorrência,
paralelismo e computação científica. O compilador e o runtime ainda não existem.
A superfície atual é uma baseline experimental para revisão.

## Uma fonte de verdade

Leia estes artefatos nesta ordem:

1. [DESIGN.md](DESIGN.md) — contrato integral de linguagem, runtime, SDK,
   compilador, packages, distribuição, tooling, plano e alternativas;
2. [Restaurante Última Luz](examples/restaurant/README.md) — ensaio visual
   integrado e fontes `.w`;
3. [Tooling](tooling/README.md) — Tree-sitter, TextMate e extensão local.

O [portal](portal/README.md) é um protótipo visual congelado. Ele não precisa
acompanhar cada mudança antes do design freeze.

`DESIGN.md` é a única autoridade para o estado atual. O Book e os exemplos
mostram esse contrato, mas não criam regras próprias.

## Estado atual

| Camada | Estado |
|---|---|
| Visão e invariantes | **Direção** |
| Forma integrada da linguagem | **Líder DB2** para avaliação |
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

entry LastLight {
  process.main = run
  http.fetch = fetch
}
```

O mesmo `Command` e a mesma resposta tipada atendem CLI, TUI e HTTP. O runtime
pode co-localizar services sem mudar esses contratos.

O `place()` dessa rota permanece um oracle de closed turn longo. A rota alvo
resolve `ServiceFamily<OrderCoordinatorApi, OrderId>`. O descriptor injeta um
`WorkKeyRef` limitado ao pedido. Consulte
[`supervision.w`](examples/restaurant/supervision.w).

## Histórico

As notas originais e os documentos substituídos ficam em
[`Y/W/`](../Y/W/). A fotografia consolidada da DB1 está em
[`Y/W/archive/db1-2026-07-27/`](../Y/W/archive/db1-2026-07-27/). Esses arquivos
preservam proveniência. Eles não definem o W atual.
