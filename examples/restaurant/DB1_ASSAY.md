# Ensaio integrado da DB1 no restaurante

> **Status:** candidato para double-check humano · pseudocódigo não executável

Este ensaio força as decisões H01–H14 do inventário original a conviverem num
único programa. Ele não substitui os módulos do restaurante: mostra as formas
canônicas, os açúcares e as alternativas que precisam chegar ao primeiro corpus.
A auditoria posterior recuperou W-O100–W-O103; os sketches da seção 9 são
deliberadamente não normativos e precisam ser ratificados antes do frontend.

## 1. Módulo, visibilidade e documentação

```w
import { CakeFlavor } from restaurant.domain
import { Temperature } from restaurant.units

/// Retorna o ponto de operação recomendado para o bolo.
///
/// ```w test
/// expect cakeSetpoint(.vanilla) == 180C
/// ```
export fn cakeSetpoint(flavor: CakeFlavor): Temperature {
  switch flavor {
    case .chocolate: return 178[degC]
    case .vanilla: return 180C
    case .carrot: return 347F
  }
}

fn calibrationSecret(): f64 {
  return 0.997
}

test "formas de temperatura convergem" for cakeSetpoint {
  expect cakeSetpoint(.vanilla) == 180[°C]
}
```

O módulo e `calibrationSecret` são privados por default; somente a declaração com
`export` atravessa a interface. Não há `public`/`private` redundantes. `///` se
anexa à declaração; o doctest e `test` entram no grafo de `w test`, não no payload
release e nunca executam durante import.

Alternativas preservadas: `*.test.w` para casos grandes; tags JSDoc apenas se uma
directive futura carregar informação que a assinatura não contém. A DSL histórica
`@`/`@@` não entra na DB1.

## 2. Literais, unidades e fórmulas

```w
let path = #"C:\restaurant\${notInterpolation}\recipes"#
let gravity = 9.80665[m/s^2]
let ovenRange = 30[degC]...300[degC]
let setpoint = ovenRange.clamp(180C)
let networkLimit = 10[MB/s]
let mailboxLimit = 64KiB

let wallLoss = surface * transmittance * (inside - ambient)
let cavityEnergy = (heaterPower * duty - wallLoss) * elapsed
let nextCavity = cavity + cavityEnergy / cavityCapacity
```

Forma estável: `[unit expression]`, com `^` somente nessa subgramática. Fora dela,
`^` é XOR e `**` é exponenciação W. `degC`/`degF` evitam depender de teclado;
`90C`, `90F`, `90[°C]`, `5km` e `64KiB` são sugars congelados pela edição. `MB`
é decimal e `MiB` binário; case folding nunca corrige unidade silenciosamente.

Alternativas preservadas: `pow(x, 2)` quando método/policy precisam ser nomeados;
tipo nominal longo na FFI/reflection; símbolo Unicode quando melhora a leitura.

## 3. Range, membership e intenção

```w
fn canMove(from current: OrderStage, to next: OrderStage): Bool {
  switch current {
    case .accepted: return next in (.preparing, .cancelled)
    case .preparing: return next in (.baking, .finishing, .cancelled)
    case .baking: return next in (.finishing, .cancelled)
    case .finishing: return next in (.completed, .cancelled)
    case .completed, .cancelled: return false
  }
}

fn shouldAccumulate(rawDuty: f64, error: f64): Bool {
  switch rawDuty {
    case 0.0...1.0: return true
    case ..<0.0 where error > 0.0: return true
    case 1.0>.. where error < 0.0: return true
    case _: return false
  }
}
```

`value in (a, b)` é membership finito OR sem coleção runtime. Para sets/flags,
`hasAny` e `hasAll` nomeiam semânticas diferentes. `where` relaciona uma guarda ao
pattern; não é uma segunda grafia geral para `&&`.

Alternativas equivalentes continuam documentadas: comparações `==`/`||`, expressão
booleana direta e tuple-pattern. `.isOneOf(...)` perde para `in (...)` na forma
canônica, mas pode sobreviver como API comum se não alocar.

## 4. Storage previsível

```w
export object OvenAnalytics {
  model: ThermalModel
  var Lazy heatProfile = deriveHeatProfile(model)
  var atomic completedBatches: u64 = 0

  fn recordCompletion(): Void {
    completedBatches += 1
  }
}
```

`var Behavior name` é o único slot de property behavior; `Lazy var`, `with` e
`by` não são formas canônicas. `Lazy` mantém `HeatProfile` como tipo lógico e
expande storage/accessors em HIR. `atomic` usa o mesmo lugar visual, mas é modifier
intrínseco: o storage inferido é `Atomic<u64>`, o acesso comum é seq-cst e não há
borrow ordinário do payload. APIs nomeadas selecionam ordering avançado.

Ponto a provar: um método não-`mut` pode realizar apenas mutação atômica interna,
pois não exige exclusividade do object; o verifier deve rejeitar qualquer outra
mutação. Isso será um negativo do corpus, não uma suposição do runtime.

## 5. Concorrência, paralelismo e cancelamento

```w
export async fn makeCake(
  plan: CakePlan,
  pantry: ServiceRef<PantryApi>,
  ovens: ServiceRef<OvenPoolApi>,
): Cake throws KitchenError {
  let lease = try await ovens.reserve(plan.temperature)
  defer { lease.release() }

  async let ovenTask = lease.preheat()
  async let ingredientsTask = pantry.fetchCake(plan)
  let (oven, ingredients) = try await (ovenTask, ingredientsTask)

  spawn let left = oven.leftKitchen.bake(take ingredients.left)
  spawn let right = oven.rightKitchen.bake(take ingredients.right)
  let (leftLayer, rightLayer) = try await (left, right)

  return finish(leftLayer, rightLayer)
}

export async fn runInterfaces(restaurant: ServiceRef<RestaurantApi>): Void throws AppError {
  async let terminal = runTerminal(restaurant)
  async let web = serveWeb(restaurant)
  defer {
    cancel terminal, reason: .shutdown
    cancel web, reason: .shutdown
  }
  let (_, _) = try await (terminal, web)
}
```

`async let` sobrepõe espera sem prometer outra thread. `spawn let` expressa
trabalho CPU/paralelo e exige captures transferíveis (`Send`); ambos criam filhos
lexicais, reunidos antes do scope fechar. `cancel` é statement contextual somente
para um handle `Cancellable`; a solicitação é cooperativa, e cleanup/join ocorre
antes de propagar o resultado. Método `.cancel()` permanece uma API possível de
tipos comuns, mas não é a forma canônica de structured tasks.

## 6. Tiers e nomes curtos

```w
let label = order.label.uppercased() // T0: puro, independente do ambiente
print("pedido ${order.id}")          // T1: console capability e I/O visíveis
await http.serve(address, handler)   // T2: domínio first-party explícito
```

T0 contém somente semântica independente do ambiente. A edição pode tornar
`print` curto, mas a resolução registra seu símbolo T1, effect, capability e custo.
Colisão exige qualificação ou import explícito; namespaces std permanecem sempre
disponíveis. Tiers não controlam linking: reachability controla o payload.

## 7. Ownership, layout e fronteira C

```w
let snapshot = copy jobs
submit(take batch)
normalize(inout sample)
let checksum = inspect(buffer)

foreign c equipment {
  struct Reading {
    c.double value
    c.int status
  }

  fn read_sensor(out: c.ptr<Reading>): c.int
}
```

Values usam layout opaco entre builds W por default; `foreign c` e tipos de
representação explícita seguem ABI/layout C. `copy`, `take`, `inout` e borrow
inferido tornam o lifecycle reconstruível sem annotations de lifetime. Debug
symbols ficam em sidecar removível; metadata privada de ownership não entra no
payload release.

`fn<C>` inline continua a ilha de migração da própria aplicação, depois que a
mesma assinatura passar pela fronteira C segura. Compartilhar LLVM não resolve
ABI, ownership, exceptions ou runtime automaticamente.

## 8. Bootstrap e entrega

O restaurante deve ser compilável pelo primeiro compiler W self-hosted. O seed
`w-seed-c` é W-owned, baseado em C11 portátil, construído por CMake/Ninja e retido
como rota de auditoria. Dependable C informa testes de compatibilidade; não muda o
profile para C89 nem permite UB. O backend MLIR/LLVM fica atrás de ABI C estreita.

Builds usam source, edition, compiler, SDK, target, flags, env declarada, manifest
e lock como inputs. A mesma receita precisa produzir o mesmo payload; debug e
attestation são sidecars vinculados por digest. Binário reproduzido, assinatura,
análise de segurança e policy do consumidor são fatos separados.

## 9. Ensaio do adendo recuperado

> **Em aberto:** os quatro fragmentos abaixo são instrumentos de comparação, não
> sintaxe aceita pelo parser, formatter ou highlighter.

### Domínio de execução sem thread física prometida

```w
// Alternativa A: preferência explícita na criação do filho.
async on network let menu = fetchMenu()
spawn on compute let forecast = forecastDemand(history)

// Um service isolado faz o hop na call; `spawn` não vira sinônimo de fila serial.
await renderer.show(snapshot)
```

`network` e `compute` são nomes lógicos resolvidos pelo produto. A primeira linha
pede progresso concorrente num executor adequado a I/O; a segunda pede trabalho
paralelo num pool limitado. `renderer` possui isolamento serial, não uma promessa
de mesma thread do sistema. Descriptor no manifest e binding no service continuam
alternativas à cláusula `on`.

### Handlers comuns ligados a um profile de host

```w
async fn run(args: Array<String>, ctx: ProcessContext): ExitCode { ... }
async fn fetch(request: Request, ctx: HttpContext): Response { ... }
async fn readCommand(line: String, ctx: CliContext): Void { ... }
async fn pointer(event: PointerEvent, ctx: UiContext): Void { ... }

entry Restaurant {
  process.main = run
  http.fetch = fetch
  process.stdinLine = readCommand
  ui.pointer = pointer
}
```

O bloco é um descriptor de bindings, não um record literal; por isso não usa
vírgulas. Os handlers continuam funções W ordinárias e testáveis. `entry` como
keyword contextual, `export { fetch ... }` e bindings apenas no manifest são as
três superfícies preservadas. Slots são namespaced e versionados pelo profile;
`Context` é uma família de capabilities tipadas, não uma sacola dinâmica.

### Matriz operacional e tensor de previsão

```w
// Baseline explícita: arrays aninhados + API nomeada.
let transition = Matrix([[0.82, 0.18], [0.24, 0.76]])
let nextDemand = matmul(transition, demand)

// Açúcares a comparar, não gramática aceita.
let compact = [0.82, 0.18; 0.24, 0.76]
let batchPrediction = features @ weights + bias.broadcast(to: .rows)
```

O corpus só promove o açúcar se também conseguir expressar shape estático e
dinâmico, dtype/promotion, views e aliasing, device, broadcasting, reductions,
random reproduzível e interoperabilidade. Transferência CPU/GPU deve ser
explícita. Autodiff pertence primeiro a um módulo T2 experimental.

### Newtype, refinement e parâmetro compile-time separados

```w
type BoundedString<const min: usize, const max: usize> =
  String where value.scalars.count in min...max

type RestaurantName = BoundedString<min: 1, max: 100>
type TicketLabel = InlineString<capacity: 32>
```

`RestaurantName` tem identidade nominal e invariante; `BoundedString` declara
parâmetros de valor; `InlineString` escolhe representação. Isso evita fazer
`String<size; 1000>` significar simultaneamente tamanho, capacity, unidade de
contagem e layout. Argumentos rotulados por vírgula, posicionais e refinamento
sem generic wrapper continuam no teste; ponto e vírgula fica rejeitado por
enquanto.

## Matriz do double-check

| Bundle | Evidência integrada | O que ainda precisa de protótipo |
|---|---|---|
| H01 | raw `#""#`, `in`, ranges, `**`/unit `^` | recovery e formatter |
| H02 | cópias/conversões apenas seguras e únicas | algoritmo de implicit conversion |
| H03 | módulo opaco + `foreign c` | fingerprints/layout/ABI |
| H04 | `String`/raw UTF-8; bytes continuam distintos | graphemes, normalization e bundles |
| H05 | SI, IEC, affine temperature e fórmula térmica | generics, diagnostics e lowering zero-cost |
| H06 | `var Lazy` | init order, exclusivity, effects e composição |
| H07 | async/spawn/cancel/atomic | executor, atomics targets e verifier Send/Sync |
| H08 | `ServiceRef`, keyed state e closed turn | reentrância, durability e boundary remota |
| H09 | T0 puro, `print` T1, HTTP/SI T2 | inventário e disponibilidade por target |
| H10 | seed C → self-host W → MLIR | bootstrap reproduzível em três toolchains C |
| H11 | `foreign c` e rota `fn<C>` | importer, source map e ownership adapters |
| H12 | receita, digests, attestations e policy separadas | builders independentes e registry |
| H13 | `///`, doctest, `test ... for` e lens de custo | runner, compile-fail, fuzz e measurements |
| H14 | todas as features têm baseline/boundary | nenhum protótipo pode reabrir sem evidência |

O adendo adiciona outra matriz de revisão, ainda sem ratificação:

| Questão | Evidência top-down | O que decide |
|---|---|---|
| W-O100 | `async/spawn on` e call a renderer isolado | isolamento, herança, starvation, afinidade e portabilidade |
| W-O101 | descriptor de `process.main`/`http.fetch`/eventos | grammar, profiles versionados, capabilities e adapters de host |
| W-O102 | forecast por matriz/tensor | shapes, promotion, aliasing, device, autodiff e lowerings interoperáveis |
| W-O103 | `BoundedString`/`InlineString` | kind system, labels, runtime checks, layout e diagnostics |

## Resultado esperado da revisão

A DB1 não exige que cada otimização esteja pronta, mas exige que não haja um vazio
semântico escondido. H01–H14 estão ratificadas; W-O100–W-O103 não estão. Uma
objeção ao ensaio deve apontar: exemplo que não pode ser baixado, runtime
surpreendente, regra impossível de verificar, ambiguidade de parser/tooling ou
ergonomia humana pior. Esse feedback reabre somente a decisão afetada e ganha
corpus positivo/negativo correspondente. A análise completa está no
[adendo da DB1](../../DB1_ADDENDUM.md).
