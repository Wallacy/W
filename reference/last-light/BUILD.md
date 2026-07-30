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

`last-light-worker` é diferente. Seu host chama `http.fetch` diretamente. Ele
usa outro lifecycle e gera outro artifact.

Regra:

```text
modo dentro do mesmo lifecycle -> um entry pode selecionar no runtime
lifecycle diferente             -> outro product e, normalmente, outro artifact
```

## 3. Products

O manifest vigente está em [`package.w`](package.w).

| Product | Kind | Host | Finalidade |
|---|---|---|---|
| `last-light-native` | executable | native process | CLI, TUI e servidor local |
| `last-light-worker` | component | HTTP worker | API do restaurante |
| `last-light-wifi` | component | HTTP worker | captive portal e sessões |
| `last-light-simulation` | executable | native process | oracle determinístico |
| `last-light-observatory` | executable | native process | swarm e horizon telemetry |
| `last-light-mobile` | executable | mobile app | lifecycle Android/iOS |
| `last-light-controller` | firmware | device | sensores e rádio |
| `last-light-audio` | firmware | audio device | callback sem allocation |
| `last-light-accelerators` | device bundle | accelerator | kernels de tensor |
| `last-light-benchmark` | benchmark | HTTP host | corpus TechEmpower |

Somente o source e a grammar existem. Todos os products dependem de runtime e
SDK futuros. A tabela define gates, não suporte entregue.

### 3.1 Runtime graphs

`package.w` contém cinco grafos:

| Graph | Uso | Imports abertos |
|---|---|---|
| `restaurant-core` | processo nativo e providers do restaurante | pantry, ovens, payment gateway, audience e aroma device |
| `restaurant-client` | worker e mobile | `last-light` |
| `wifi-edge` | captive portal | `wifi-sessions` |
| `observatory-client` | observatório | `satellites` |
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

## 5. Comandos previstos

### 5.1 Build único

```text
w resolve
w build last-light-native \
  --target x86_64-unknown-linux-gnu \
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

### 6.1 Resolução

```text
w resolve
w diff-lock
w fetch --locked
w build last-light-native --locked
```

`package.lock` fixa:

- versões e origins;
- source e artifact digests;
- adapters e toolchains;
- target e profile;
- features;
- grafo de módulos;
- expansão de `moduleSets`;
- recipe e provenance.

### 6.2 Publicação

```text
w package last-light-native --matrix desktop --locked
w publish --release 0.1.0 --artifacts dist/release.windex
w verify registry:last-light/restaurant@0.1.0
w reproduce registry:last-light/restaurant@0.1.0
```

O maintainer autoriza a release. Um builder produz provenance. Um segundo
builder pode publicar evidência de reprodução. Um auditor publica análise
separada.

Nenhum selo combina essas propriedades em uma afirmação vaga de “seguro”.

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
- `distributed.w` usa `split-services`, WASI 0.3 e controladoras externas;
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
| simulation | mesmos eventos e totais para a mesma recipe |
| native | CLI, TUI e HTTP produzem a mesma resposta tipada |
| worker | request limits, cancellation e status são equivalentes |
| fulfillment workflow | crash em todo commit preserva effect ID, outcome e ownership |
| mobile | suspend drena trabalho e resume não duplica efeitos |
| controller | nenhuma interrupt faz allocation ou blocking |
| audio | callback não aloca, bloqueia ou perde deadline |
| accelerators | CPU e device concordam dentro do numeric mode |
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

1. parser data-only e schemas de package e deployment;
2. expansão determinística de `moduleSets`;
3. host profiles e slot registry;
4. std de process, mobile, device, audio e accelerator;
5. implementação de `Request`, `Response`, `http.Context` e adapters HTTP;
6. codecs JSON explícitos ou derivados por synthesis autorizada;
7. pool, adapters de protocolo e validação de schema do database;
8. implementação de cache local, eviction e single-flight;
9. journal de workflow, replay checker, timer, event inbox e adapter SQLite;
10. validator do runtime graph e da interface de cada unit;
11. platform packaging e signing;
12. device memory e kernel ABI;
13. harness TechEmpower versionado e seus validadores;
14. deployment resolver, lock e validator;
15. compiler e backends.

Essas lacunas são resultados do ensaio. Elas não são falhas escondidas.
