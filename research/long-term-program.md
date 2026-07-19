# Programa de pesquisa de longo prazo

> **Status:** **Pesquisa**; não amplia a promessa de v0 · 19 de julho de 2026

Este programa transforma ideias de longo prazo em hipóteses falsificáveis. A
[arquitetura de longo prazo](../ARCHITECTURE.md) define as fronteiras estáveis; o
[roadmap](../ROADMAP.md) continua definindo o caminho crítico. Uma linha abaixo
só entra no roadmap depois de cumprir seu gate e ser promovida em
[STATUS.md](../STATUS.md).

## Regra experimental comum

Todo experimento declara antes de executar:

1. problema e comportamento observável;
2. baseline mais simples;
3. targets, workload, versões e limites;
4. positivos, negativos, oracle e failure modes;
5. métricas de tempo, memória, tamanho, complexidade e portabilidade;
6. fallback correto e condição de remoção;
7. quais contratos públicos mudariam se a hipótese fosse aceita.

Resultados brutos são artefatos; a conclusão curta e reproduzível entra neste
catálogo. Um benchmark do fornecedor é ponto de partida, não decisão W.

## Trilhas

| ID | Trilha | Baseline que já funciona sem a pesquisa | Hipótese principal | Primeiro gate |
|---|---|---|---|---|
| LT-01 | especificação e conformidade | spec + corpus por comportamento | uma semântica operacional pequena reduz divergência entre implementações | segunda implementação de um subset passa o mesmo corpus sem compartilhar AST/HIR |
| LT-02 | frontend e edição | parser normativo separado + TextMate/Tree-sitter IDE | CST compartilhada reduz drift sem piorar diagnostics | parser batch e incremental concordam em corpus válido/inválido e recovery edit-by-edit |
| LT-03 | representação e memória | layouts convencionais, owner único e allocator do host | niches, tags, arenas ou ARC seletivo reduzem custo sem aparecer na semântica | implementação dual em x86_64/arm64, FFI e sanitizers, com fallback bit-for-bit observável |
| LT-04 | texto e identificadores | `String` UTF-8 válido e identifiers ASCII canônicos | Unicode mais amplo pode ser seguro com profile, normalization e lints | UAX #15/#31/#39, confusable corpus, version pin e migração entre bundles |
| LT-05 | tasks e scheduler | executor simples, scopes e cancelamento cooperativo | work stealing, I/O nativo e executors especializados melhoram throughput sem mudar ordering | scheduler determinístico de teste + três workloads + cleanup/cancel invariants |
| LT-06 | services e durability | object/instance serial in-process e storage adapter explícito | fine-grained logical units podem ser co-located ou remotas com o mesmo contrato | local/IPC equivalentes em ordering, failure, backpressure e cancellation |
| LT-07 | isolamento e capabilities | authority explícita + boundary de processo quando necessário | WASI/component boundaries ampliam portabilidade sandboxed | filesystem/rede negados por default, limits e escape threat model em dois hosts |
| LT-08 | compiler, IR e bootstrap | seed C + HIR própria + LLVM nativo | MLIR acelera lowerings múltiplos sem dominar interfaces W | pin LLVM reproduzível, adapter isolado e upgrade ensaiado em duas revisões |
| LT-09 | ABI e evolução | source-first e rebuild por toolchain exata | interfaces resilientes justificam distribuição binária seletiva | library v1/v2 e client antigo/novo em matriz de layout, generics, enums e errors |
| LT-10 | SDK e portabilidade | T0 pequeno + T1 por capabilities | tiers explícitos preservam ergonomia sem mentir sobre targets | mesma API contract testada em Windows/POSIX e um profile limitado |
| LT-11 | ciência e devices | numéricos escalares, quantities e CPU fallback | shapes/units/refinements orientam SIMD, linalg e GPU com diagnóstico melhor | kernel térmico CPU/SIMD/device, erro numérico e transfer cost publicados |
| LT-12 | build e supply chain | lock, CAS, recipe e rebuild local | transparência e rebuilds independentes dão evidência útil sem score agregado | dois builders reproduzem payload; divergência, rollback e revogação são exercitados |
| LT-13 | observabilidade e tooling | locations, diagnostics estruturados e debug symbols sidecar | task stacks, cost lens e explanations tornam abstrações auditáveis | source→HIR→machine mapping e uma falha async explicáveis em IDE/CLI |
| LT-14 | IA e automação | formatter, schemas, docs/tests executáveis | agents produzem mudanças melhores consumindo fatos estruturados | benchmark versionado de tarefas com revisão humana, sem enviar source por default |
| LT-15 | governança e continuidade | decisões no repo e releases imutáveis | editions, target policy e conformance evitam dependência de uma pessoa/implementação | processo público ensaiado em uma breaking proposal e uma retirada de target fictícia |

## Decisões derivadas da pesquisa atual

### Dependable C e o seed

[Dependable C](https://dependablec.org/) é uma lista valiosa de atritos de
implementação, portabilidade e legibilidade. Ele explicitamente se considera o
oposto de um dialeto e favorece um subset muito conservador. Para W isso produz
três decisões:

- o nome técnico é profile `w-seed-c`, não “a semântica C do W”;
- o seed continua baseado em C11 para inteiros, atomics e ferramentas que o
  bootstrap realmente exigir, mas cada uso recebe fallback ou support matrix;
- as exceções chamadas de “dependable UB” não entram: o seed deve passar em
  sanitizers, warnings fixados e múltiplos compilers sem depender de UB conhecido.

O gate compara Clang, GCC e MSVC quando disponíveis, em Windows e ao menos um
host POSIX. Builds fixam flags; warnings novos são triados, não transformados em
quebra não reproduzível por uma opção global cega.

### MLIR sem lock-in semântico

A documentação oficial informa que a C API é pequena, feita para wrappers e
ainda sem estabilidade garantida. O próprio projeto também não promete
compatibilidade da API C++. Portanto:

- o pin de LLVM/MLIR faz parte da recipe;
- HIR, diagnostics e module interface W não expõem objetos MLIR;
- adapters de dialect/pass podem usar C++/TableGen internamente;
- upgrade de LLVM é uma mudança testada por corpus, IR verifier, performance e
  reprodutibilidade;
- bytecode MLIR é cache/interchange do toolchain; a versão do dialeto W e seus
  upgrades continuam responsabilidade W.

### Tagged addresses e allocator

Arm MTE usa tags para verificar correspondência entre pointer e allocation; isso
não oferece automaticamente bits livres para uma representação de linguagem. Da
mesma forma, pointer features dependem de ABI, kernel, sanitizers e hardware.
Tagged values só avançam por target e nunca carregam a única cópia de informação
necessária a ownership ou safety.

O mimalloc permanece um profile de allocator promissor, não default sem dados. A
implementação oferece first-class heaps e modos secure/guarded, mas também muda
entre versões e nenhum allocator vence todo workload. O gate inclui allocator do
host, mimalloc e região em CLI, servidor, compiler e serviço keyed; mede peak RSS,
fragmentação, tail latency, startup, FFI e sanitizer support.

### Unicode e source longevo

Texto de aplicação e identifier não precisam da mesma policy. `String` preserva
UTF-8 válido sem normalizar silenciosamente conteúdo. Identifiers começam ASCII;
um profile Unicode futuro precisa fixar versão de dados, forma de normalization,
XID, scripts permitidos e warnings de confusables. Skeleton de UTS #39 serve para
detecção, não para renomear ou armazenar o identificador.

### WASM/WASI como profile

Component Model/WASI são candidatos para playground, plugins ou deployment
sandboxed. Eles não substituem o runtime nativo nem tornam capability enforcement
automático. O experimento publica imports concedidos, limits, engine/toolchain,
startup, cópias na boundary e equivalência com o corpus nativo suportado.

### SQLite e estado durável

SQLite oferece atomic commit e WAL úteis, mas storage continua adapter explícito.
O número de performance para blobs pequenos não generaliza a serviço, compiler
cache ou database workload. Crash tests, fsync policy, backup/migration, lock
contention e target support fazem parte do gate. Um adapter in-memory fornece o
oracle; filesystem/CAS e SQLite são comparados separadamente.

### Supply chain sem selo mágico

Reprodutibilidade, provenance, assinatura, transparência e review são eixos
independentes. TUF oferece um modelo testado de roles, thresholds, expiração e
proteção contra rollback/freeze; SLSA descreve provenance; Sigstore/Rekor pode
fornecer transparência. W deve interoperar com esses modelos antes de inventar um
JWT ou score universal.

O primeiro protótipo usa `SOURCE_DATE_EPOCH` como input explícito compatível com
o ecossistema, ordenação estável, path remapping e randomness declarada. A recipe
W pode ser mais estrita, mas não deve criar nomes incompatíveis sem necessidade.

## Ondas de pesquisa

| Onda | Quando | Trilhas permitidas | Motivo |
|---|---|---|---|
| A | antes/durante frontend | LT-01, LT-02, LT-04, LT-08 | evitam congelar grammar, source e bootstrap errados |
| B | primeiro executável | LT-03, LT-05, LT-09, LT-10, LT-13 | exigem código nativo e métricas reais |
| C | package/runtime útil | LT-06, LT-07, LT-12 | exigem boundaries, artifacts e failure injection |
| D | depois da baseline CPU | LT-11, LT-14, LT-15 | especialização e governança precisam de sistema utilizável |

Uma onda posterior pode receber notas antes da hora; não pode mudar o caminho
crítico usando resultados que ainda não existem.

## Registro de resultados

Cada experimento aprovado cria um diretório autocontido:

```text
research/experiments/LT-XX-name/
  README.md       # hipótese, baseline, oracle e conclusão
  recipe.lock     # inputs/toolchain fixados
  cases/          # workloads mínimos e negativos
  results/        # summaries pequenos; bruto pode ser artefato externo por digest
```

Somente conclusões reproduzidas e revisadas alteram um documento canônico. Um
resultado negativo é preservado: evita repetir uma ideia atraente sem aprender
com o custo anterior.

## Fontes primárias iniciais

- [Dependable C](https://dependablec.org/)
- [MLIR C API](https://mlir.llvm.org/docs/CAPI/)
- [MLIR bytecode](https://mlir.llvm.org/docs/BytecodeFormat/)
- [Arm Memory Tagging Extension](https://developer.arm.com/Architectures/Memory%20Tagging%20Extension)
- [mimalloc](https://github.com/microsoft/mimalloc)
- [Unicode normalization](https://www.unicode.org/reports/tr15/)
- [Unicode identifiers](https://www.unicode.org/reports/tr31/)
- [Unicode security](https://www.unicode.org/reports/tr39/)
- [WebAssembly Component Model](https://component-model.bytecodealliance.org/)
- [WASI](https://wasi.dev/)
- [SQLite WAL](https://sqlite.org/wal.html)
- [Rust editions](https://doc.rust-lang.org/edition-guide/editions/)
- [Swift library evolution](https://www.swift.org/blog/library-evolution/)
- [Reproducible Builds](https://reproducible-builds.org/docs/)
- [SLSA provenance](https://slsa.dev/spec/v1.2/provenance)
- [The Update Framework](https://theupdateframework.github.io/specification/latest/)
- [Sigstore transparency log](https://docs.sigstore.dev/logging/overview/)
