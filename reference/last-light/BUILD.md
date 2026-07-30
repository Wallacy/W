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
6. um runtime envelope.

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

`wasm32-wasi-preview2` gera um component. O host concede HTTP, clocks, storage e
outgoing network por capabilities.

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
  --profile release \
  --locked
```

### 5.2 Matriz

```text
w build --matrix desktop \
  --product last-light-native \
  --profile release \
  --locked
```

O index da matriz aponta para um payload por target. Ele não afirma que bytes de
architectures diferentes possuem o mesmo hash.

### 5.3 Execução

```text
w run last-light-native -- --cli
w run last-light-native -- --tui
w run last-light-native -- --serve
```

### 5.4 Explicação

```text
w explain product last-light-native
w explain artifact sha256:...
w explain performance restaurant.horizon::forecast
w explain resources restaurant.audio::renderFinalSong
```

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

## 7. Packing de nanoservices

O mesmo grafo lógico pode usar vários layouts físicos.

### 7.1 Processo único

```text
last-light-native
  ├─ Restaurant
  ├─ Billing
  ├─ Observatory
  ├─ SatelliteCoordinator
  └─ WifiSession
```

Calls locais mantêm `ServiceRef` e podem usar fast path.

### 7.2 Processos por domínio

```text
edge        -> HTTP gateway, WifiSession
kitchen     -> Restaurant, Pantry, Oven
observatory -> Horizon sensors, SatelliteCoordinator
compute     -> Oracle host, accelerator driver
```

O deployment muda placement. Ele não troca a interface ou remove `await`.

### 7.3 Distribuição fina

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

1. parser data-only e schema de `package.w`;
2. expansão determinística de `moduleSets`;
3. host profiles e slot registry;
4. std de process, HTTP, mobile, device, audio e accelerator;
5. runtime graph completo para os services;
6. platform packaging e signing;
7. device memory e kernel ABI;
8. benchmark harness versionado;
9. deployment schema e validator;
10. compiler e backends.

Essas lacunas são resultados do ensaio. Elas não são falhas escondidas.
