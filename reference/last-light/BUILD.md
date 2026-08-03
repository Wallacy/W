# Build e products do Última Luz

> **Status:** projeção do design vigente. W ainda não possui compiler, runtime
> ou package manager.

Este documento aplica o contrato canônico de
[`DESIGN.md`](../../DESIGN.md) ao produto de referência. Ele não cria regras
novas.

## 1. O que o build seleciona

O build seleciona um product. Todo product seleciona module roots, target,
profile e toolchain plan. O kind fecha os outros fields:

| Product iniciado por host | Library |
|---|---|
| entry descriptor | exports exatos |
| host profile | ABI `.wExact` ou `.c` |
| runtime graph e packing | runtime e panic policy |
| capabilities e limits | header e ABI sidecars |

Uma library não recebe entry ou host por default omitido. Esses fields são
inválidos para seu kind.

O source usa:

```w
entry(runNative)
entry LastLightTui(runTuiEntry)
```

O descriptor anônimo é `.default`. `LastLightTui` é outro descriptor. Um não
herda dados do outro.

`entry(runNative)` não escreve `process.main`. O host profile declara esse slot
como default. O compiler faz o binding e grava descriptor e handler na
interface.

O manifest escolhe a forma anônima:

```w
{
  name: "last-light-native"
  module: "restaurant.app"
  host: "w.host/native-process@1"
  executionProfile: "native-bounded"
}
```

`entry` não é escolhido por argumento de runtime. O product resolve o
descriptor durante o link. A omissão seleciona `.default`. Escrever
`entry: ".default"` é válido, mas redundante.

Outro product pode escolher o descriptor nomeado:

```w
{
  name: "last-light-tui"
  module: "restaurant.app"
  entry: "LastLightTui"
  host: "w.host/native-process@1"
  executionProfile: "native-bounded"
}
```

`runNative` e `runTuiEntry` registram os signals no runtime. A registration pode
mudar conforme o estado da aplicação. Ela não faz parte de `hostBindings`.

O manifest omite `entry` somente para `.default`. Um descriptor nomeado precisa
ser explícito.

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
| `last-light-horizon-w` | static library | none | API W exata do horizon monitor |
| `last-light-horizon-c` | dynamic library | none | façade C versionada do horizon monitor |
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

Um plano operacional não é um artifact. O build produz uma recipe por product,
target spec, profile e toolchain-plan row.

Somente o source e a grammar existem. Todos os products dependem de runtime e
SDK futuros. A tabela define gates, não suporte entregue.

### 3.1 Runtime graphs

`package.w` contém cinco grafos:

| Graph | Uso | Imports abertos |
|---|---|---|
| `restaurant-core` | processo nativo e providers do restaurante | `pantry`, `ovens`, `paymentGateway`, `audience` e `aromaDevice` |
| `restaurant-client` | worker e mobile | `lastLight` |
| `wifi-edge` | captive portal | `wifiSessions` |
| `observatory-client` | observatório | `satellites` e `horizonMonitor` |
| `benchmark-host` | sete workloads HTTP | database PostgreSQL e cache local |

Uma declaração `export service name: P { ... }` contém boundary e provider
default. `import service` adapta um protocol ou módulo comum quando o caller
escolhe a boundary.

`services` seleciona scope, limits e argumentos. `servicePolicy` fecha os links
permitidos. `.local` usa mailbox e thunk. `.component` usa a component ABI.
`.wrpc` declara seus transports internos. O resolver cria os slots durante o
startup. Um nome textual não cria authority.

Um graph que alcança uma service stream também declara `streamLimits`. O
`observatory-client` limita streams abertos, bytes por item, janelas em voo,
fila decoded, traversal, capability slots e taxa. O deployment pode reduzir
esses limites. Ele não pode ampliá-los.

`restaurant-core` liga `lastLight` ao provider local por default. Uma launch
config pode selecionar um provider assinado por component, IPC ou network. O
artifact mantém os mesmos bytes e grava a configuração no startup audit record.
Um import comum não aceita esse override.

`ServiceLink` materializa a boundary inteira. `ServiceTransport` existe somente
dentro do link wRPC. Cap'n Proto, Cap'n Web e gRPC entram como foreign links
fixados por adapter digest. Eles não aparecem como transports.

O futuro `interface.lock` registra identities estáveis de protocols, operations,
fields e enum cases. O source snapshot e a release recipe incluem seu digest.
`w build` não modifica esse arquivo.

O compiler deriva um `WireSchemaDigest` para cada input, output e application
error. `.wrpc` exige `WireValue` para todo tipo alcançável. Por isso,
`RestaurantSnapshot.activeOrders` usa `u32`. `OvenReady` usa um token owned e
não transporta `Instant`.

O startup negocia `exact` por raiz quando os wire schema digests são iguais. Um
compatibility map seleciona `compatible` nos outros casos aceitos. O package não
escolhe o profile por conveniência ou por target.

A expressão `pipeline` entra no `ServiceIR` como um DAG de calls dependentes. O
linker divide o grafo em ilhas de route. O runtime preserva effect IDs, cleanup
e incerteza quando uma barreira impede o fast path. O build não transforma o
pipeline em uma transaction nem em uma closure remota.

Uma edge `some Stream<Item, Failure>` também entra no `ServiceIR`. O linker usa
ligação direta dentro da mesma route. Outra route recebe um relay bounded com
créditos de items e bytes. `Failure` precisa aceitar `ServiceFailure`.

`transaction<...> tx = provider { ...; commit value }` usa um único provider
nominal. O product fixa seus limits e capabilities. Um binding local pode usar
uma conexão direta. Um binding wRPC usa um scope remoto com lease. Nenhum
placement compõe duas capabilities numa transação distribuída implícita.

As capabilities padrão, como clock e random, entram no contexto tipado do host
pelo envelope do product. Recursos nomeados, como database e cache, entram como
imports do runtime graph. Essa diferença impede lookup livre por string.

### 3.2 Execution profiles

Build profile e execution profile respondem a perguntas diferentes:

| Seleção | Pergunta |
|---|---|
| `--profile release` | como compiler, checks e representação produzem bytes? |
| `executionProfile: "native-bounded"` | quais tasks, pools, domains e cleanups o artifact permite? |
| `--execution-platform linux-x64` | em qual plataforma hermética o build executa? |

`package.w` contém três execution profiles:

| Profile | Products | Contrato |
|---|---|---|
| `native-bounded` | native, TUI, simulation e observatory | CPU e blocking pools bounded; domain térmico compartilha o CPU pool |
| `edge-bounded` | worker, Wi-Fi e mobile | tasks de I/O bounded; sem blocking domain alcançável |
| `benchmark-bounded` | benchmark | envelope maior, ainda com queue, frame e timer limits |

Cada unit criada pelo packing recebe seu próprio envelope. O packing
`single-process` cria um runtime compartilhado. O packing `split-services`
produz um runtime por unit. `deployments/distributed.w` reduz cada unit
separadamente.

O deployment não cria domain nem muda fallback. Ele reduz somente números
dentro do envelope. `execution.thermal` e `.compute` usam o mesmo pool `cpu`.
Portanto, dois nomes lógicos não duplicam workers.

```text
w explain execution last-light-native \
  --deployment deployments/local.w
```

O relatório deve mostrar requirements alcançáveis, pool compartilhado, budgets
do artifact, redução por unit e digest do profile.

### 3.3 Workflow de pedidos

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

### 3.4 Libraries e fronteiras ABI

O laboratório do horizonte produz duas libraries:

| Product | Boundary | Export |
|---|---|---|
| `last-light-horizon-w` | `.wExact` | `restaurant.horizon::classifyHorizon` |
| `last-light-horizon-c` | `.c` | `ll_horizon_classify_v1` |

A library W publica `WInterface`, `WAbiKey`, `RepresentationMap`, ABI note e
symbol manifest. Um consumer reutiliza o artifact somente quando a key e os
layouts compartilhados conferem. Se houver source, uma diferença exige rebuild.
Se não houver source, o build falha.

Requirements de runtime permanecem separados. O product final registra o
`RuntimeClosureKey` após resolver os offers do provider.

A export W devolve `HorizonStatus`. A `RepresentationMap` registra o enum e seus
payloads. A façade C converte o enum para `error`, `kind` e `score`. Ela não
expõe o layout W ao caller C.

A façade C está em [`abi.w`](abi.w). O body usa W. A signature usa carriers C e
o calling convention C do target. O builder gera o header e a export list. O
nome exportado é exato. A call não transfere um owner W ou a responsabilidade
de usar o allocator W.

Essas libraries não possuem `entry` ou `host`. O container não inicia runtime e
não executa module constructors. O product C usa `panic: .forbid`. Seu call
graph exportado não pode alcançar panic. Outra façade pode escolher
`.abortProcess`. Nenhuma dynamic library nativa isola a falha. Um plugin isolado
usa uma component ou um process.

O product C usa `runtime: .none`. Seus exports não recebem contexto oculto. Uma
façade com estado usaria um context handle C e exports explícitos de create e
destroy.

O contrato normativo está em [DESIGN.md](../../DESIGN.md#204-abi-e-runtime).
Este laboratório valida o contrato sem duplicá-lo.

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
import { nativeTerminalBackend } from platform.native
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

### 4.7 Target specs

As strings de `targetSets` são formas abreviadas. A distribuição W expande cada
string para um `TargetSpec` com CPU, features e platform contract:

```text
x86_64-unknown-linux-gnu
  -> CPU .portable
  -> features []
  -> platform contract fixado pela distribuição
```

Uma release registra o record expandido na toolchain plan e na recipe. O target
triple não escolhe um SDK.

O profile `benchmark` usa `cpuPolicy: .explicit`. Por isso, ele exige uma CPU e
uma lista de features:

```text
w build last-light-benchmark \
  --target x86_64-unknown-linux-gnu \
  --cpu x86-64-v3 \
  --features +avx2,+fma \
  --profile benchmark \
  --locked
```

O comando não consulta a CPU do runner. Outra CPU ou outra lista de features
produz outra recipe.

### 4.8 Profiles de memória

Cada build profile fixa o allocator geral e a policy de representação:

| Profile | Allocator geral | Representação | Uso |
|---|---|---|---|
| `debug` | `.system` | `.portable` | fallback e diagnóstico |
| `release` | `.system` | `.optimized` | payload normal |
| `benchmark` | `.system` | `.optimized` | baseline de medição |
| `benchmark-mimalloc` | runtime contract mimalloc@3 | `.optimized` | comparação do provider |

O último profile adiciona uma runtime requirement. A toolchain plan seleciona e
fixa o provider que oferece o contrato. Ele não usa `PATH`, preload ou override
global de `malloc`.

```text
w toolchain resolve \
  --product last-light-benchmark \
  --target x86_64-unknown-linux-gnu \
  --execution-platform linux-x64 \
  --cpu x86-64-v3 \
  --features +avx2,+fma \
  --profile benchmark-mimalloc \
  --output build/benchmark-mimalloc.wplan

w build last-light-benchmark \
  --target x86_64-unknown-linux-gnu \
  --cpu x86-64-v3 \
  --features +avx2,+fma \
  --profile benchmark-mimalloc \
  --toolchains build/benchmark-mimalloc.wplan \
  --locked
```

System e mimalloc podem produzir o mesmo `RepresentationMap`. A recipe,
`RuntimeClosureKey` e measurements continuam diferentes.

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

### 5.2 Toolchain plan

```text
w toolchain resolve \
  --product last-light-native \
  --target x86_64-unknown-linux-gnu \
  --execution-platform linux-x64 \
  --profile release \
  --output build/linux-x64.wplan

w toolchain explain last-light-native \
  --target x86_64-unknown-linux-gnu \
  --execution-platform linux-x64

w build last-light-native \
  --target x86_64-unknown-linux-gnu \
  --profile release \
  --toolchains build/linux-x64.wplan \
  --locked
```

O primeiro comando não compila o product. Ele resolve roles e grava provider
digests. O segundo explica as seleções e rejeições. O terceiro usa a plan sem
consultar `PATH` ou procurar outro SDK.

### 5.3 Matriz

```text
w toolchain inventory --output build/release-providers.winventory

w toolchain resolve \
  --product last-light-native \
  --matrix desktop \
  --profile release \
  --providers build/release-providers.winventory \
  --output build/desktop.wplan

w build --matrix desktop \
  --product last-light-native \
  --packing single-process \
  --profile release \
  --toolchains build/desktop.wplan \
  --locked
```

O index da matriz aponta para um payload por target. Ele não afirma que bytes de
architectures diferentes possuem o mesmo hash. Cada phase seleciona uma das
execution platforms ordenadas no workspace. O inventory informa quais provider
records estão disponíveis nos pools. A plan grava somente as escolhas.

### 5.4 Execução

```text
w run last-light-native --deployment deployments/local.w -- --cli
w run last-light-native --deployment deployments/local.w -- --tui
w run last-light-native --deployment deployments/local.w -- --serve
```

### 5.5 Explicação

```text
w explain product last-light-native
w explain product last-light-tui
w explain target-variant last-light/restaurant::native-terminal \
  --target x86_64-pc-windows-msvc
w explain artifact sha256:...
w explain runtime restaurant-core
w explain execution last-light-native \
  --deployment deployments/local.w
w explain workflow fulfillment --key order:42
w explain performance restaurant.horizon::forecast
w explain memory restaurant.allocation::countStagedMenuInParallel
w explain layout restaurant.memory::BellTarget
w explain resources restaurant.audio::renderFinalSong
w audit effects last-light-native
```

### 5.6 Benchmark

```text
w build last-light-benchmark \
  --target x86_64-unknown-linux-gnu \
  --cpu x86-64-v3 \
  --features +avx2,+fma \
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

### 5.7 Interface e ABI

```text
w build last-light-horizon-w \
  --target x86_64-unknown-linux-gnu \
  --profile release \
  --locked

w build last-light-horizon-c \
  --target x86_64-pc-windows-msvc \
  --profile release \
  --locked

w interface show last-light-horizon-w
w abi show last-light-horizon-w
w abi key last-light-horizon-w
w runtime explain last-light-horizon-w
w symbols show last-light-horizon-c
w c header last-light-horizon-c --output build/last_light_horizon.h
w interface diff artifact:old artifact:new
w abi diff artifact:old artifact:new
```

O oracle de ABI cobre estes casos:

- mudar somente documentation ou spans preserva `SemanticInterfaceKey`;
- metadata truncada, oversized ou com ciclo proibido falha sob limites;
- uma `WAbiKey` igual permite reuse;
- os fingerprints dos tipos compartilhados também precisam conferir;
- `HorizonStatus` não atravessa a façade C como bytes W;
- uma key diferente causa rebuild por source ou error para binary-only;
- funções W homônimas de módulos distintos não colidem;
- dois exports C com o mesmo nome falham antes do linker;
- lookup W usa handle e manifest, não o primeiro symbol global;
- Clang, GCC e MSVC aceitam o header nos targets correspondentes;
- header e library de target slices diferentes não podem ser combinados;
- layout assertions conferem size, alignment e offsets do result carrier;
- um caller C não libera memória com o allocator errado;
- error tipado não finito ou negativo vira status C distinto;
- `panic: .forbid` fecha o call graph; typed error continua no result carrier;
- a library não executa código antes de validar a ABI note;
- `runtime: .none` não cria contexto global ou lazy;
- um runtime requirement ausente causa error antes da primeira call;
- release do handle `.wExact` fecha novas calls, sem prometer unmap físico;
- ThinLTO ligado ou desligado preserva interface, exports e comportamento;
- version skew isolado usa component adapter e drain de instances.

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

O workspace também limita a resolução de toolchain:

```w
toolchainPolicy: {
  catalogs: [.distribution]
  systemImports: .explicit
  providerOrder: [.distribution, .system]
  foreignLanguages: [.c]
  executionPlatforms: [
    {
      name: "linux-x64"
      target: "x86_64-unknown-linux-gnu"
      sandbox: "w.build-sandbox/1"
    },
    {
      name: "windows-x64"
      target: "x86_64-pc-windows-msvc"
      sandbox: "w.build-sandbox/1"
    },
    {
      name: "macos-arm64"
      target: "aarch64-apple-darwin"
      sandbox: "w.build-sandbox/1"
    },
  ]
}
```

`.distribution` usa o catalog snapshot que acompanha o executable W.
`.explicit` permite somente providers de sistema importados antes da análise.
`providerOrder` e `executionPlatforms` dão uma ordem explícita.
`foreignLanguages` autoriza somente o adapter C usado pelo produto. Essa policy
não escolhe um executable, endpoint ou runner por path.

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

A recipe por product, target spec, profile e toolchain-plan row fixa:

- content tree digest de cada source local;
- lock digest;
- entry, host, runtime graph e packing;
- target spec, profile e toolchain-plan row;
- compiler, runtime, adapters, sysroot, SDK, linker e packager selecionados;
- execution platform da action;
- target e artifact de cada build tool;
- action recipes, input digests, output schemas e budgets;
- generated output digests usados como product inputs;
- environment permitido e valores usados.

O artifact record liga o recipe digest aos payloads, resources, sidecars e
provenance. Um action result liga a action recipe aos generated output digests.
Nenhum result digest entra na recipe que o produz.

`w publish check` resolve o package sem substituição por workspace. Assim, uma
release local ausente não fica escondida por um member.

### 6.2 Toolchains do produto

O contrato normativo de provider, inventory, plan e recipe está em
[`DESIGN.md` 21.2.1](../../DESIGN.md#2121-resolução-de-toolchain-e-sdk). Esta
seção aplica o contrato ao Última Luz.

O Última Luz usa requirements por role. O package não fixa `clang.exe`,
`ld.lld`, `xcrun`, `link.exe` ou um diretório de SDK.

#### 6.2.1 Matriz candidata

Esta tabela projeta a primeira infraestrutura de CI. Ela não declara suporte já
entregue:

| Product target | Execution platform candidata | Providers necessários | Payload |
|---|---|---|---|
| Linux x86-64 | Linux x86-64 | W frontend, MLIR/LLVM, W runtime, glibc sysroot e LLD | ELF |
| Linux AArch64 | Linux x86-64 | mesmos roles com sysroot AArch64 | ELF |
| Windows x86-64 | Windows x86-64 | W frontend, MLIR/LLVM, W runtime, Windows SDK/UCRT e COFF linker | PE/COFF + PDB |
| macOS AArch64 | macOS AArch64 | W frontend, MLIR/LLVM, W runtime, Apple SDK e Mach-O tools | Mach-O + dSYM |
| Android AArch64/x86-64 | Linux x86-64 | W frontend, MLIR/LLVM, Android NDK, runtime e Android packager | ELF + AAB |
| iOS device/simulator | macOS AArch64 | W frontend, MLIR/LLVM, Apple SDK, runtime e bundle tools | Mach-O + app bundle |
| WASI 0.3 | Linux x86-64 | W frontend, Wasm backend, W component runtime e component linker | Wasm component |
| Cortex-M/RISC-V | Linux x86-64 | W frontend, LLVM/LLD, freestanding runtime, device pack e image tool | firmware image |
| NVIDIA/AMD/SPIR-V | Linux x86-64 | W frontend, MLIR device lowering e vendor/device provider | device bundle |

O mesmo W frontend pode acompanhar vários providers. A tabela não presume que
um único bundle possui licença ou suporte para todas as linhas.

LLD é o linker inicial preferido para ELF, COFF, Wasm e targets freestanding.
Ele não é uma obrigação da linguagem. Apple platform tools e um vendor tool
podem continuar necessários quando o gate do target exigir.

#### 6.2.2 Requirements por product

`last-light-native` em Linux exige:

```text
.wFrontend
.backend(.elf)
.wRuntime("native-process@1")
.sysroot(.glibc)
.linker([.elf, .gcSections])
.packager(.directory)
```

`last-light-controller` exige:

```text
.wFrontend
.backend(.elf)
.wRuntime("firmware@1")
.platformSdk(.devicePack)
.linker([.elf, .linkerScript, .gcSections])
.deviceTools([.image, .map, .symbols])
```

`last-light-accelerators` exige um provider por target device. O
`last-light-ai-lab` é um host executable separado. Ele consome device bundles
por digest. O native product não incorpora CUDA, ROCm e Vulkan por inferência.

O oracle de cross-build do `menu-compiler` adiciona outra row:

```text
product target:   x86_64-unknown-linux-gnu
build-tool target: x86_64-pc-windows-msvc
```

O tool executa em Windows. Seu output entra como resource do executable Linux.
Ele não recebe o target ou o sysroot do executable.

#### 6.2.3 Providers de sistema

Providers redistribuíveis podem vir do catalog da distribuição. SDKs de sistema
exigem import explícito:

```text
w toolchain import windows-msvc --instance <id> --sdk <version>
w toolchain import apple-xcode --developer-dir <path>
w toolchain import android-ndk --root <path>
w toolchain import nvidia-cuda --root <path>
```

O import cria uma closure por digest. O build posterior não executa
`vcvarsall`, `xcrun` ou discovery do NDK. Caminhos locais ficam no provider
store. O importer usa CAS ou snapshot local selado quando a licença permite. Um
provider system-backed passa por verificação forte antes da action.

Um release control plane agrega provider records dos pools Linux, Windows e
macOS em um inventory sem paths:

```text
w toolchain inventory --output build/release-providers.winventory
```

O inventory é input da resolução. Somente providers selecionados entram na row
usada pela recipe. Um SDK não selecionado continua disponível para explicação,
mas não muda o artifact.

Uma plan Linux candidata possui esta forma reduzida:

```text
last-light-native / x86_64-unknown-linux-gnu / release
  target spec
    CPU                 .portable
    features            []
    platform contract   .linux(
                          kernel: "<baseline>",
                          libc: .glibc("<baseline>"),
                        )
  phase compile-and-link
    execution platform  linux-x64
    execution target    x86_64-unknown-linux-gnu
    sandbox             w.build-sandbox/1
    W distribution      sha256:...
    frontend/backend    sha256:...
    runtime             sha256:...
    sysroot             sha256:...
    linker              sha256:...
    environment         []
```

Uma plan Apple separa build e release:

```text
compile-and-package -> unsigned app bundle -> artifact record
sign-and-notarize   -> signed delivery      -> delivery record
```

O certificado, o timestamp e a resposta de notarization não entram na payload
recipe.

#### 6.2.4 Determinismo e disponibilidade

O oracle de toolchain executa estes casos:

- troca a ordem dos providers no filesystem;
- altera `PATH`, `SDKROOT`, `INCLUDE`, locale e timezone;
- instala um SDK mais novo ao lado do SDK importado;
- executa a mesma plan em dois runners compatíveis;
- muda um header dentro de um provider system-backed;
- tenta usar a CPU do runner no profile `benchmark`;
- fornece duas provider lineages na mesma prioridade, inclusive sob a mesma
  authority;
- remove uma role necessária ao target;
- alcança uma API acima do platform contract;
- adiciona um provider que nenhuma row seleciona;
- compõe duas slices na ordem inversa;
- assina novamente o mesmo envelope sem assinatura.

Os onze primeiros casos precisam falhar ou manter a mesma recipe conforme o
contrato. O último pode produzir outro delivery digest. Ele não pode mudar o
payload digest.

O resultado publica facts separados:

| Fact | Pergunta |
|---|---|
| `bit-reproducible` | a mesma recipe produziu os mesmos bytes? |
| `publicly-rebuildable` | um terceiro possui acesso legal a todos os inputs? |
| `platform-signed` | uma authority da plataforma assinou o envelope? |
| `independently-reproduced` | outro builder publicou evidence equivalente? |
| `metadata-fresh` | timestamp, snapshot e targets passam expiry e rollback policy? |
| `transparency-recorded` | assinatura e provenance possuem evidence no log exigido? |

Um SDK fechado pode permitir `bit-reproducible` sem permitir
`publicly-rebuildable`.

O plano distribuído exige `maintainer-authorized`, `independently-reproduced`,
`metadata-fresh` e `transparency-recorded`. Ele rejeita artifacts `revoked` ou
`yanked`. A assinatura do OS não substitui esses facts.

#### 6.2.5 Gates de desempenho do build

O builder publica um record para estes workloads:

| Workload | Invalidation esperada |
|---|---|
| clean build | todo o grafo alcançável |
| no-op build | análise de digests, sem compile ou link |
| body edit privado | item, dependentes de inline e link necessário |
| interface edit | consumers da interface alterada |
| target matrix | rows independentes em paralelo |
| provider import | closure calculada uma vez |
| provider `.systemBacked` | verificação forte antes da action |

Cada record contém wall time, CPU time, peak memory, bytes lidos/escritos,
actions executadas, cache hits, cache misses e motivo de cada invalidation. O
primeiro protótipo estabelece a baseline. A documentação não inventa um limite
antes da medição.

No-op e body edit são gates de produto. Um ganho de clean build não compensa
invalidar todo o module graph. LLD é um candidato porque oferece cross-link e
link paralelo. O benchmark decide se ele permanece em cada target.

### 6.3 Build transform do cardápio

`compile-final-menu` usa o `.tool` product `menu-compiler`:

```text
menus/final.menu
  -> build.Input<String>("menu")
  -> last-light/menu-compiler::menu-compiler
  -> build.Output<Bytes>("bytecode")
  -> CAS resource
  -> last-light-native
```

O tool artifact é compilado para o target da execution platform. A action
executa nessa platform. O executable final continua no product target. Um
cross-build em Windows para Linux não tenta executar um tool Linux.

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

### 6.4 Publicação

```text
w package list last-light/restaurant
w package check --matrix last-light/restaurant
w toolchain resolve \
  --product last-light-native \
  --target x86_64-unknown-linux-gnu \
  --execution-platform linux-x64 \
  --profile release \
  --output build/release-linux-x64.wplan
w build last-light-native \
  --target x86_64-unknown-linux-gnu \
  --profile release \
  --toolchains build/release-linux-x64.wplan \
  --output-index dist/release.windex \
  --locked
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

### 6.5 Envelopes de plataforma

O payload interno e o envelope de entrega são records distintos:

| Target family | Payload | Envelope candidato |
|---|---|---|
| Linux | ELF | archive, package de sistema ou OCI image |
| Windows | PE/COFF + PDB sidecar | directory ou MSIX sem assinatura |
| macOS | Mach-O + dSYM sidecar | app bundle ou archive sem assinatura |
| Android | ELF libraries e resources | APK ou Android App Bundle sem assinatura |
| iOS | Mach-O e resources | app bundle sem assinatura |
| bare metal | image, map e symbols | firmware bundle |
| accelerator | kernels, objects e launch metadata | device bundle |

O packager selecionado produz o envelope determinístico. Signing, notarization
e timestamp produzem um delivery record. Eles não mudam o digest do payload ou
do envelope sem assinatura.

Uma instalação pode conter vários products. Por exemplo, o observatory instala
um control plane nativo, firmware de satélite e device kernels. O deployment
liga esses artifacts por digest. O build não tenta convertê-los em um único
executável.

## 7. Packing e deployment de nanoservices

O mesmo grafo lógico pode usar vários layouts físicos.

### 7.1 Packing `single-process`

```text
main
  ├─ lastLight
  ├─ orderCoordinators
  ├─ fulfillment supervisor
  ├─ oracle
  ├─ aromaProbe
  ├─ billing
  └─ diningRoom
```

Calls locais mantêm a semântica de service e usam o `.local` link. Esse link
pode remover encode, decode e framing. Ele não remove `await`, admission,
failure boundary ou trace.

### 7.2 Packing `split-services`

```text
gateway  -> lastLight, orderCoordinators, fulfillment
planning -> oracle, aromaProbe
finance  -> billing
dining   -> diningRoom
```

O build gera uma unit por grupo. Cada crossing usa um `ServiceLink`. O artifact
index fixa estas edges privadas:

```text
gateway  -> planning : oracle, aromaProbe
gateway  -> finance  : billing
gateway  -> dining   : diningRoom
```

O deployment roteia essas edges. O plano distribuído seleciona wRPC para native
units em hosts distintos. Uma Wasm edge pode selecionar `.component`. O
deployment não pode substituir os providers.

### 7.3 Deployment

Os planos estão em [`deployments/`](deployments/):

- `local.w` usa `single-process` e adapters locais;
- `distributed.w` usa `split-services`, WASI 0.3, swarm e sensores externos;
- `benchmark.w` fixa PostgreSQL, cache local, admission e logs em disco
  desativados.

O deployment muda placement. Ele não reagrupa providers, não religa edges
privadas e não remove `await`. O futuro `deployment.lock` grava artifacts,
units, link kind, protocol, wire schema, codec profile, transport, peers e
adapters por digest.

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
| task runtime | budgets, fail-fast, deadline, drain e shared pools passam scheduler adversarial |
| Wi-Fi | authority, rate limit e session binding permanecem explícitos |
| observatory | satélites e horizonte fazem join sem task solta ou dado stale |
| fulfillment workflow | crash em todo commit preserva effect ID, outcome e ownership |
| mobile | suspend drena trabalho e resume não duplica efeitos |
| controller | nenhuma interrupt faz allocation ou blocking |
| audio | callback não aloca, bloqueia ou perde deadline |
| accelerators | CPU e device concordam dentro do numeric mode |
| AI lab | treino no host e kernel de device preservam shapes e numeric mode |
| benchmark | workload oficial sem bypass e com configuração registrada |
| ABI laboratory | interface, key, symbols, header e runtime requirements concordam |

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
9. codecs JSON explícitos ou derivados por synthesis autorizada, mais encoder,
   decoder, oracle e fuzzer dos profiles wWire;
10. pool, adapters de protocolo e validação de schema do database;
11. implementação de cache local, eviction e single-flight;
12. journal de workflow, replay checker, timer, event inbox e adapter SQLite;
13. validator do runtime graph, execution profile e interface de cada unit;
14. schemas de provider, inventory, catalog, system importer e toolchain plan;
15. executor local/remoto, sandbox, build-performance harness e corpus de
    parity entre execution platforms;
16. platform packaging, composition recipes e signing records;
17. device memory e kernel ABI;
18. harness TechEmpower versionado e seus validadores;
19. deployment resolver, lock e validator;
20. schema e serializer canônico de `WInterface`;
21. `WAbiKey`, ABI note, symbol manifest e diagnostics de compatibilidade;
22. gerador de header C, export list e harness para C callers;
23. loader por digest para artifacts W exatos;
24. task runtime, clocks virtuais, scheduler replay e blocking adapters;
25. verifier e lowering de `call_pipeline`, route islands e fault injection;
26. wRPC channel profiles, workload identity, transcript e threat corpus;
27. capability grants, attenuation, revocation e table quotas;
28. release envelope, independent reproduction, registry metadata e mirrors;
29. compiler e backends.

Essas lacunas são resultados do ensaio. Elas não são falhas escondidas.
