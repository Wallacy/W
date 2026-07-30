# Build e products do Última Luz

> **Status:** projeção do design vigente. W ainda não possui compiler, runtime
> ou package manager.

Este documento aplica o contrato canônico de
[`W/DESIGN.md`](../../DESIGN.md) ao produto de referência. Ele não cria regras
novas.

## 1. O que o build seleciona

O build seleciona um product. O product seleciona:

1. um módulo de entry;
2. um descriptor;
3. um host profile;
4. um target;
5. um build profile;
6. um runtime graph;
7. um packing.

O source usa:

```w
entry(runNative) {
  process.signal = shutdown
}

entry LastLightTui(runTui)
```

O descriptor anônimo é `.default`. `LastLightTui` recebe os bindings do
descriptor anônimo e substitui o slot default.

`entry(runNative)` não escreve `process.main`. O host profile declara esse slot
como default. O compiler faz o binding e grava a forma expandida na interface.

O manifest escolhe a forma anônima:

```w
{
  name: "last-light-native"
  module: "restaurant.app"
  entry: ".default"
  host: "w.host/native-process@1"
}
```

`entry` não é escolhido por argumento de runtime. O product resolve o
descriptor durante o link.

Outro product pode escolher o descriptor nomeado:

```w
{
  name: "last-light-tui"
  module: "restaurant.app"
  entry: "LastLightTui"
  host: "w.host/native-process@1"
}
```

Esse product herda o binding de signal. Ele não repete `process.signal`. O
manifest pode omitir `entry` somente quando o módulo possui um único descriptor
resolvível.

## 2. Um binário com vários modos

`last-light-native` é um executável multimodo:

```text
last-light-native --cli
last-light-native --tui
last-light-native --serve
```

O único `process.main` executa `runNative`. Essa função escolhe o modo.

O modo `--serve` abre um servidor com uma capability do processo. O sistema
operacional não chama `http.fetch`.

O mesmo `process.main` também pode iniciar uma shell GTK ou .NET, um tray
service e um servidor. Esses adapters ficam no grafo alcançável do product.
Eles não exigem outro entry quando o sistema operacional controla um único
process lifecycle.

`last-light-worker` é diferente. Seu host chama `http.fetch` diretamente. Ele
usa outro lifecycle e gera outro artifact.

Regra:

```text
modo dentro do mesmo lifecycle -> um entry pode selecionar no runtime
lifecycle diferente             -> outro product e, normalmente, outro artifact
```

## 3. Products

O workspace vigente está em [`workspace.w`](workspace.w). O package principal
está em [`package.w`](package.w).

| Product | Kind | Host | Finalidade |
|---|---|---|---|
| `last-light-native` | executable | native process | CLI, TUI e servidor local |
| `last-light-tui` | executable | native process | TUI dedicada e artifact menor |
| `last-light-worker` | component | HTTP worker | API do restaurante |
| `last-light-wifi` | component | HTTP worker | captive portal e sessões |
| `last-light-simulation` | executable | native process | oracle determinístico |
| `last-light-observatory` | executable | native process | swarm e horizon telemetry |
| `last-light-mobile` | executable | mobile app | lifecycle Android/iOS |
| `last-light-controller` | firmware | device | sensores e rádio |
| `last-light-audio` | firmware | audio device | callback sem allocation |
| `last-light-accelerators` | device bundle | accelerator | kernels de tensor |
| `last-light-ai-lab` | executable | native process | treino e oracle CPU/device |
| `last-light-benchmark` | benchmark | HTTP host | corpus TechEmpower |

Os products formam cinco planos:

```text
venue       -> native, TUI, mobile, Wi-Fi e áudio
edge        -> worker HTTP e sessions
observatory -> swarm, horizonte e control plane
device      -> controllers, rádio, MMIO e callbacks
compute     -> simulação, treino, kernels e benchmark
```

Um plano não é um artifact. Ele é uma visão operacional. O build continua a
produzir uma recipe por product, target e profile.

Somente o source e a grammar existem. Todos os products dependem de runtime e
SDK futuros. A tabela define gates, não suporte entregue.

### 3.1 Runtime graphs

`package.w` contém cinco grafos:

| Graph | Uso | Imports abertos |
|---|---|---|
| `restaurant-core` | processo nativo e providers do restaurante | pantry, ovens, payment gateway, audience e aroma device |
| `restaurant-client` | worker e mobile | `last-light` |
| `wifi-edge` | captive portal | `wifi-sessions` |
| `observatory-client` | observatório | `satellites` e `horizon-monitor` |
| `benchmark-host` | sete workloads HTTP | database PostgreSQL e cache local |

O compiler deve derivar requirements do source. O manifest escolhe providers ou
declara imports. Um nome textual não cria authority.

As capabilities padrão, como clock e random, entram no contexto tipado do host
pelo envelope do product. Recursos nomeados, como database e cache, entram como
imports do runtime graph. Essa diferença impede lookup livre por string.

### 3.2 Workflow de pedidos

O supervisor `fulfillment` liga `fulfillOrderDurably`. O artifact fixa:

- `FulfillmentPoint` e `fulfillmentSignals`;
- schemas de input, progress, output e failure;
- `recovery: .required`;
- `confidentiality: .hostEncrypted`;
- quotas separadas para roots, running e admission queue;
- budgets de history, step e inbox;
- adapters compatíveis.

O deployment seleciona o adapter por role:

```w
adapters: [
  {
    artifact: "restaurant"
    supervisor: "fulfillment"
    role: .workflowJournal
    provider: .adapter(
      "w.std/sqlite-workflow@1",
      storage: .capability("last-light/workflow-store"),
    )
  },
]
```

O lock grava o digest do adapter. Um adapter em memória pode executar o oracle
volátil, mas não satisfaz `recovery: .required`. Storage sem encryption at rest
não satisfaz `.hostEncrypted`. Essas regras impedem o deployment de reduzir as
garantias do product.

## 4. Targets

### 4.1 Desktop e servidor

| Target | Envelope esperado | Gate mínimo |
|---|---|---|
| `x86_64-unknown-linux-gnu` | ELF | process, files, TCP, TLS e debugger |
| `aarch64-unknown-linux-gnu` | ELF | mesmo corpus em AArch64 |
| `x86_64-pc-windows-msvc` | PE/COFF | process, IOCP adapter e PDB sidecar |
| `aarch64-apple-darwin` | Mach-O | process, Network adapter e dSYM sidecar |

O target informa emissão e ABI. O host profile informa lifecycle e
capabilities.

### 4.2 Mobile

| Target | Package externo | Gate mínimo |
|---|---|---|
| `aarch64-unknown-linux-android` | APK/AAB | lifecycle, JNI boundary e signing |
| `x86_64-unknown-linux-android` | APK/AAB | emulator CI |
| `aarch64-apple-ios` | app bundle | lifecycle, Swift/C bridge e signing |
| `aarch64-apple-ios-simulator` | app bundle | simulator CI |

W não precisa definir uma UI toolkit para entrar nesses targets. O product pode
expor domain logic e lifecycle para uma shell nativa.

### 4.3 WebAssembly

`wasm32-wasip3` gera um component com native async. O host concede HTTP, clocks,
storage e outgoing network por capabilities.

`wasm32-wasip2` permanece como target de compatibilidade. Ele não define a
interface async do W.

Esse target não concede DOM. Ele também não transforma W em substituto de
JavaScript no browser.

### 4.4 Embedded

| Target | Primeiro profile |
|---|---|
| `thumbv7em-none-eabihf` | Cortex-M com floating point |
| `riscv32-unknown-none-elf` | RISC-V bare metal |

O primeiro gate embedded exige:

- startup e linker script reproduzíveis;
- interrupts tipados;
- MMIO somente em `unsafe` ou adapter verificado;
- monotonic clock;
- fixed allocator ou no-heap;
- panic handler e watchdog;
- artifact map e budget por section.

### 4.5 Accelerators

| Target | Lowering candidato |
|---|---|
| `nvptx64-nvidia-cuda` | MLIR GPU → NVVM |
| `amdgcn-amd-amdhsa` | MLIR GPU → ROCDL |
| `spirv64-unknown-vulkan` | MLIR GPU → SPIR-V |

O device bundle contém kernels e metadata. Um host product mantém device
selection, transfer, launch, synchronization e errors.

ASIC e FPGA permanecem em **Pesquisa**. Eles exigem um modelo de tempo, memória
e synthesis que LLVM IR geral não fornece.

### 4.6 Variantes de module graph

O source nativo importa sempre:

```w
import { nativeTerminalBackend } from restaurant.platform.native
```

`package.w` oferece dois module sets com a mesma module identity:

```text
Linux, Darwin -> native-terminal/posix
Windows       -> native-terminal/windows
outros        -> fallback vazio
```

Os cases são disjuntos. A ordem não altera o resultado. Os dois cases concretos
precisam exportar a mesma interface porque o grupo declara
`interface: .uniform`.

O fallback vazio permite que products mobile, Wasm, firmware e device ignorem
o adapter. Se um módulo alcançável importar `restaurant.platform.native` em um
desses targets, o build falha por implementação ausente.

O lock grava o case escolhido. A recipe inclui o module set expandido. Nem a
máquina que executa o build nem um filename suffix decide o resultado.

## 5. Comandos previstos

### 5.1 Build único

```text
w resolve
w build last-light-native \
  --target x86_64-unknown-linux-gnu \
  --packing single-process \
  --profile release \
  --locked

w build last-light-tui \
  --target x86_64-pc-windows-msvc \
  --packing single-process \
  --profile release \
  --locked
```

### 5.2 Matriz

```text
w build --matrix desktop \
  --product last-light-native \
  --packing single-process \
  --profile release \
  --locked
```

O index da matriz aponta para um payload por target. Ele não afirma que bytes de
architectures diferentes possuem o mesmo hash.

### 5.3 Execução

```text
w run last-light-native --deployment deployments/local.w -- --cli
w run last-light-native --deployment deployments/local.w -- --tui
w run last-light-native --deployment deployments/local.w -- --serve
```

### 5.4 Explicação

```text
w explain product last-light-native
w explain product last-light-tui
w explain target-variant last-light/restaurant::native-terminal \
  --target x86_64-pc-windows-msvc
w explain artifact sha256:...
w explain runtime restaurant-core
w explain workflow fulfillment --key order:42
w explain performance restaurant.horizon::forecast
w explain resources restaurant.audio::renderFinalSong
w audit effects last-light-native
```

### 5.5 Benchmark

```text
w build last-light-benchmark \
  --target x86_64-unknown-linux-gnu \
  --packing entry-only \
  --profile benchmark \
  --locked

w benchmark validate last-light-benchmark \
  --deployment deployments/benchmark.w \
  --harness github:TechEmpower/FrameworkBenchmarks@57d92fbec6f8fd7431bc77326dd0484e60c96e20

w benchmark run last-light-benchmark \
  --deployment deployments/benchmark.w \
  --harness github:TechEmpower/FrameworkBenchmarks@57d92fbec6f8fd7431bc77326dd0484e60c96e20 \
  --evidence results/last-light.wbench
```

`validate` verifica semântica e configuração. `run` mede somente a combinação
validada e grava a evidence com os digests do artifact, deployment e harness.

## 6. Packages e releases

### 6.1 Workspace e resolução

[`workspace.w`](workspace.w) contém dois members:

```text
.                       -> last-light/restaurant
packages/menu-compiler  -> last-light/menu-compiler
```

O segundo member satisfaz uma `.build` dependency do primeiro. Identity e
version precisam conferir com a dependency publicada. Os dois manifests
declaram a registry authority `w`. O workspace não altera os imports W e não
vira uma release conjunta.

```text
w context
w workspace check
w resolve
w add w/telemetry@^1.0 --as telemetry --use product --dry-run
w tree last-light-native
w diff-lock
w fetch --locked
w build last-light-native --locked
w package check --matrix last-light/restaurant
w publish check --matrix last-light/restaurant
```

`w add` altera o manifest e o lock na mesma transação. O `--dry-run` acima
mostra a authority, o alias, o usage, as versions candidatas e os novos edges.
Ele não executa o package. `w remove` aplica a mesma regra e falha quando um
product, feature, action ou target variant ainda referencia o alias.

`package.lock` fixa:

- digest do workspace manifest e dos package manifests;
- roots e dependency usages;
- target roles e identities dos resolution contexts;
- versões e origins;
- external source tree digests;
- member paths, manifests e source-inventory digests;
- active source-set digest de cada context;
- features;
- case e digest de cada target variant;
- build-tool packages;
- expansão de `moduleSets`;
- metadata snapshots e razões da resolução.

A recipe por product, target e profile fixa:

- content tree digest de cada source local;
- lock digest;
- entry, host, runtime graph e packing;
- target, profile, compiler, runtime, adapters, sysroot e SDK;
- execution target e artifact de cada build tool;
- action recipes, input digests, output schemas e budgets;
- generated output digests usados como product inputs;
- environment permitido e valores usados.

O artifact record liga o recipe digest aos payloads, resources, sidecars e
provenance. Um action result liga a action recipe aos generated output digests.
Nenhum result digest entra na recipe que o produz.

`w publish check` resolve o package sem substituição por workspace. Assim, uma
release local ausente não fica escondida por um member.

### 6.2 Build transform do cardápio

`compile-final-menu` usa o `.tool` product `menu-compiler`:

```text
menus/final.menu
  -> build.Input<String>("menu")
  -> last-light/menu-compiler::menu-compiler
  -> build.Output<Bytes>("bytecode")
  -> CAS resource
  -> last-light-native
```

O tool executa para o execution target. O executable continua no product
target. Um build Windows para Cortex-M não tenta executar um tool Cortex-M.

```text
w explain dependency last-light/menu-compiler
w explain action compile-final-menu
w build last-light-native \
  --target x86_64-unknown-linux-gnu \
  --locked
```

A action recebe um input de até 64 KiB e produz um output de até 1 MiB. Ela não
recebe network, environment, clock, random, secret ou filesystem geral. Error,
panic, cancellation ou output ausente não confirmam objeto no CAS.

### 6.3 Publicação

```text
w package list last-light/restaurant
w package check --matrix last-light/restaurant
w build --matrix desktop --product last-light-native --locked
w publish last-light/restaurant --artifacts dist/release.windex --locked
w verify registry:last-light/restaurant@0.1.0
w reproduce registry:last-light/restaurant@0.1.0
```

O maintainer autoriza a release. Um builder produz provenance. Um segundo
builder pode publicar evidência de reprodução. Um auditor publica análise
separada.

O source snapshot usa a allowlist de `package.w`. Ele não consulta
`.gitignore`. Cada member inclui seu próprio `LICENSE`, e `package check`
reconstrói usando somente o snapshot.

Nenhum selo combina essas propriedades em uma afirmação vaga de “seguro”.

### 6.4 Envelopes de plataforma

O payload interno e o envelope de entrega são records distintos:

| Target family | Payload | Envelope candidato |
|---|---|---|
| Linux | ELF | archive, package de sistema ou OCI image |
| Windows | PE/COFF + PDB sidecar | directory ou MSIX |
| macOS | Mach-O + dSYM sidecar | app bundle ou signed archive |
| Android | ELF libraries e resources | APK ou Android App Bundle |
| iOS | Mach-O e resources | app bundle assinado |
| bare metal | image, map e symbols | firmware bundle |
| accelerator | kernels, objects e launch metadata | device bundle |

O target adapter produz o envelope. Signing, notarization e timestamp não
mudam o digest do payload interno. Eles recebem records próprios.

Uma instalação pode conter vários products. Por exemplo, o observatory instala
um control plane nativo, firmware de satélite e device kernels. O deployment
liga esses artifacts por digest. O build não tenta convertê-los em um único
executável.

## 7. Packing e deployment de nanoservices

O mesmo grafo lógico pode usar vários layouts físicos.

### 7.1 Packing `single-process`

```text
main
  ├─ LastLightRestaurant
  ├─ OrderCoordinator
  ├─ fulfillment supervisor
  ├─ TableOracle
  ├─ AromaProbeService
  ├─ BillingLedger
  └─ PrismDiningRoom
```

Calls locais mantêm `ServiceRef` e podem usar fast path.

### 7.2 Packing `split-services`

```text
gateway  -> LastLightRestaurant, OrderCoordinator, fulfillment
planning -> TableOracle, AromaProbeService
finance  -> BillingLedger
dining   -> PrismDiningRoom
```

O build gera uma unit por grupo. Cada crossing usa a service ABI. O artifact
index fixa estas edges privadas:

```text
gateway  -> planning : oracle, aroma-probe
gateway  -> finance  : billing
gateway  -> dining   : dining-room
```

O deployment roteia essas edges. Ele não pode substituir os providers.

### 7.3 Deployment

Os planos estão em [`deployments/`](deployments/):

- `local.w` usa `single-process` e adapters locais;
- `distributed.w` usa `split-services`, WASI 0.3, swarm e sensores externos;
- `benchmark.w` fixa PostgreSQL, cache local, admission e logs em disco
  desativados.

O deployment muda placement. Ele não reagrupa providers, não religa edges
privadas e não remove `await`. O futuro `deployment.lock` grava artifacts,
units e adapters por digest.

```text
w deploy resolve deployments/local.w
w deploy check deployments/local.w --locked
w deploy apply deployments/local.w --locked
```

### 7.4 Distribuição fina

Uma instance keyed pode ser colocada perto do estado que possui. O runtime
continua obrigado a preservar:

- admission;
- ordering;
- cancellation;
- deadline;
- failure boundary;
- `unknownOutcome`;
- trace causal.

## 8. Gates do produto

Cada product precisa de um oracle observável.

| Product | Oracle |
|---|---|
| menu compiler | execution/target separados e mesmo bytecode por input |
| simulation | mesmos eventos e totais para a mesma recipe |
| native | CLI, TUI e HTTP produzem a mesma resposta tipada |
| TUI dedicada | backend do target é único e modes não alcançáveis saem do artifact |
| worker | request limits, cancellation e status são equivalentes |
| Wi-Fi | authority, rate limit e session binding permanecem explícitos |
| observatory | satélites e horizonte fazem join sem task solta ou dado stale |
| fulfillment workflow | crash em todo commit preserva effect ID, outcome e ownership |
| mobile | suspend drena trabalho e resume não duplica efeitos |
| controller | nenhuma interrupt faz allocation ou blocking |
| audio | callback não aloca, bloqueia ou perde deadline |
| accelerators | CPU e device concordam dentro do numeric mode |
| AI lab | treino no host e kernel de device preservam shapes e numeric mode |
| benchmark | workload oficial sem bypass e com configuração registrada |

O produto de referência se torna parte da suíte de:

- parser e formatter;
- type checking;
- ownership e cleanup;
- runtime e scheduler;
- packages e reproducibility;
- cross-target conformance;
- performance regression;
- documentação e treinamento.

## 9. Lacunas reveladas

Os arquivos atuais exigem contratos ainda não implementados:

1. schemas semânticos de package, workspace e deployment;
2. resolver, lock contexts e standalone publish check;
3. expansão determinística de `moduleSets`;
4. selector de target, prova de disjointness e interface matrix;
5. `build-transform@1`, sandbox e action CAS;
6. host profiles e slot registry;
7. std de process, mobile, device, audio e accelerator;
8. implementação de `Request`, `Response`, `http.Context` e adapters HTTP;
9. codecs JSON explícitos ou derivados por synthesis autorizada;
10. pool, adapters de protocolo e validação de schema do database;
11. implementação de cache local, eviction e single-flight;
12. journal de workflow, replay checker, timer, event inbox e adapter SQLite;
13. validator do runtime graph e da interface de cada unit;
14. platform packaging e signing;
15. device memory e kernel ABI;
16. harness TechEmpower versionado e seus validadores;
17. deployment resolver, lock e validator;
18. compiler e backends.

Essas lacunas são resultados do ensaio. Elas não são falhas escondidas.
