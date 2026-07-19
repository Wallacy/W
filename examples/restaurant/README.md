# O restaurante W

> **Working Draft · pseudocódigo de design · não executável**

Este exemplo top-down usa um restaurante para mostrar como W pretende tornar
custos e ownership visíveis. Não existe compilador capaz de executar estes
arquivos. As APIs `ServiceHost`, `ServiceRef`, `Mailbox` e os nomes de métodos
são ilustrações candidatas; `service`, `worker`, `assistant` e `nanoservice` não
são keywords adotadas.

O mote continua sendo: **Prazer para humanos. Clareza para máquinas.** Uma
operação pequena pode virar uma unidade fine-grained quando precisa de lifetime,
estado, authority, isolamento lógico ou escala próprios. A boundary física
continua sendo escolha explícita do profile/trust domain. Isso não transforma
toda função em RPC e não esconde uma call remota atrás de uma chamada local.

## A história de cima para baixo

```text
cliente
  → salão abre pedidos keyed sem compartilhar `inout` entre children
  → menu escolhe um fluxo com switch
  → OrderApi encontra/cria uma instância keyed por OrderId
  → a instância serializa o estado do pedido
  → KitchenApi recebe uma call tipada, suspensível e falível
  → assistants são child tasks estruturadas
  → cozinhas independentes recebem trabalho paralelo com spawn
  → conclusão ou erro atualiza o pedido; cancelamento fecha a árvore
```

Uma seta entre instâncias representa custo observável: `try await`, typed error,
deadline/cancelamento e backpressure. O exemplo nunca pressupõe que os serviços
estão no mesmo processo.

## “Make a Cake” como pipeline

O bolo percorre estágios deliberadamente legíveis:

```text
validar → admitir pedido → reservar ingredientes → preparar
        → assar → decorar → embalar → concluir
```

- validar e admitir são sequenciais: o próximo estágio depende do anterior;
- forno e despensa podem progredir concorrentemente durante espera com
  `async let`;
- lotes independentes podem assar em cozinhas/cozinheiros diferentes com
  `spawn let`;
- assistants continuam filhos do handler: retorno antecipado não deixa tarefas
  destacadas;
- mailbox limitada aplica backpressure; overload não vira drop silencioso;
- cancelamento é cooperativo e `defer`/destruição executam cleanup;
- errors permanecem tipados e calls entre instâncias usam `try await`.
- boundaries convertem `OrderError`/`KitchenError` explicitamente no error set do
  menu; a ergonomia final dessa composição continua em
  [W-O033](../../STATUS.md).

## Uma mesa inteira

`dining_room.w` amplia o bolo para uma mesa de aniversário. As três instâncias
de pedido são abertas sequencialmente porque `ServiceHost` entra como `inout` e
não pode ser capturado por três children simultâneos. Depois de obter handles
independentes, bolo, sopa e salada progridem concorrentemente:

```w
let cakeOrder = try await openMenuOrder(request.cake.orderId, on: inout orders)
let soupOrder = try await openMenuOrder(request.soup.orderId, on: inout orders)
let saladOrder = try await openMenuOrder(request.salad.orderId, on: inout orders)

async let cake = prepareMenuCake(request.cake, for: cakeOrder, in: kitchen)
async let soup = prepareMenuSoup(request.soup, for: soupOrder, in: kitchen)
async let salad = prepareMenuSalad(request.salad, for: saladOrder, in: kitchen)
let (cake, soup, salad) = try await (cake, soup, salad)
```

Se um child falha, a estrutura lexical cancela e reúne os irmãos. Isso não
reverte automaticamente `OrderState`, pois essas instâncias têm lifecycle
externo ao scope. `cancelBirthdayTable` deixa a compensação explícita; como
integrá-la automaticamente a cleanup assíncrono continua uma decisão de runtime,
não uma promessa escondida pelo exemplo.

## Arquivos

- [`domain.w`](domain.w): requests, pratos, IDs e recibos como values;
- [`resources.w`](resources.w): capabilities tipadas de despensa, forno e lanes;
- [`menu.w`](menu.w): menu exaustivo, conversão de errors e roteamento top-down;
- [`order_service.w`](order_service.w): instância keyed por pedido e estado
  serial candidato;
- [`kitchen.w`](kitchen.w): pipelines dos três pratos, child tasks e paralelismo;
- [`dining_room.w`](dining_room.w): fan-out estruturado de uma mesa completa.

Os arquivos repetem o aviso de Working Draft para não parecerem corpus
executável quando abertos isoladamente.

## O que este exemplo ensina — e o que não decide

| Ideia | Leitura neste exemplo |
| --- | --- |
| módulo | unidade estática; import não cria instância nem concede autoridade |
| instância por pedido | API candidata com `.key(order.id)` e policy serial |
| `ServiceRef<Api>` | handle conceitual; toda call usa `try await` |
| error sets compostos | helpers com `do`/`catch`; `try` não faz injeção implícita |
| `async let` | child concorrente, útil enquanto forno/despensa esperam |
| `spawn let` | intenção paralela para lotes independentes e dados transferidos |
| `take` de fields | dry/wet/icing exercitam partial moves; a regra final segue em [W-O002](../../STATUS.md) |
| `inout` | impede abrir pedidos concorrentes sobre a mesma authority mutável |
| mailbox | limitada; aguarda vaga ou falha com error tipado |
| cleanup | pertence ao scope e ocorre em sucesso, error ou cancelamento |
| compensação | estado de serviço externo não é desfeito implicitamente com a task |

O exemplo não escolhe sintaxe própria para `service`, política final de
reentrância, escopo de singleton ou API definitiva de backpressure. Essas
questões continuam abertas.

## Contratos canônicos

- [sintaxe de trabalho](../../spec/syntax.md);
- [concorrência estruturada](../../spec/concurrency.md);
- [módulos, imports e instâncias](../../spec/modules.md);
- [arquitetura candidata de módulos/runtime](../../design/modules-and-runtime.md);
- [estimativa experimental de recursos](../../design/resource-estimation.md);
- [status e questões abertas](../../STATUS.md).

O portal oferece uma leitura visual e um lexer local. Ele não substitui esses
documentos e não compila W.
