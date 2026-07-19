# Biblioteca padrão do W

**Working Draft.** Este documento fecha o gap arquitetural entre a semântica da
linguagem, o runtime mínimo e os pacotes. Ele não descreve uma implementação
existente. A autoridade sobre tipos, ownership, erros e tasks continua em
[tipos e memória](../spec/types-and-memory.md), [sintaxe](../spec/syntax.md) e
[concorrência](../spec/concurrency.md); resolução e distribuição pertencem ao
[sistema de pacotes](packages.md).

## Direção

### Fronteiras

A superfície se divide em cinco níveis, com dependências apenas para baixo:

1. **Core conhecido pelo compilador e intrinsics:** tipos e operações necessários
   para type checking e lowering, como primitivos, funções, `T?`, ownership,
   borrows, `throws E`, operações atômicas fundamentais e pontos de suspensão.
   Intrinsics são uma interface interna versionada, não uma coleção de APIs
   privilegiadas para aplicações.
2. **Superfície implícita da edição:** core, nomes livres e namespaces da stdlib
   registrados num mapa versionado. Visibilidade não executa trabalho nem concede
   capability; `print` pode ser curto e ainda registrar I/O, terminal e
   reachability no tooling. `Option<T>` dá a semântica de `T?`; isso não adiciona
   estados além de `.some(T)` e `.none`. Erros recuperáveis no source usam
   `throws E`; a stdlib não impõe `Result` como assinatura pública.
3. **Stdlib portátil:** valores, algoritmos e contratos que preservam a mesma
   semântica em todos os targets suportados. Uma implementação pode usar
   intrinsics, mas precisa de fallback conforme [W-D010](../STATUS.md).
4. **Adapters por target:** relógio do sistema, filesystem, sockets, processo,
   ambiente, console, entropia e backends de I/O/scheduler. Cada adapter declara
   capabilities, blocking, cancelamento e restrições do target.
5. **Pacotes versionados:** módulos first-party T2 ou da comunidade — protocolos,
   bancos, codecs, UI, frameworks e integrações — que não são necessários para
   compilar ou executar o programa mínimo. São resolvidos por manifesto e
   lockfile; acompanhar o SDK não os transforma em intrinsics nem em nomes
   implícitos.

Essa taxonomia descreve dependência/portabilidade. [W-O098](../STATUS.md) adiciona
uma taxonomia ortogonal para aquilo que acompanha o SDK público:

| Tier | Expectativa | Conteúdo recomendado |
|---|---|---|
| T0 · Foundation | prelude/core comum, pequeno e estável | options/errors, ranges, strings/bytes, collections fundamentais e `print` capability-gated |
| T1 · Systems | uso frequente e abstração de plataforma | tasks/sync, clocks, random, filesystem, process, TCP/UDP/DNS e codecs fundamentais |
| T2 · Domains | first-party explícito, bundled e atualizável | HTTP/TLS, SI, decimal/Money, análise numérica, tensors, JSON, regex, SQLite e TUI |

T0/T1/T2 não são níveis de privilégio, qualidade ou linking. Todo source pode
acompanhar o SDK, mas somente reachability entra no artefato. T2 usa o mesmo
manifest/lock dos packages para poder corrigir TLS, HTTP, timezone ou dados
científicos sem amarrar sua cadence à edição da linguagem.

O compiler pode conhecer a semântica de uma operação sem congelar sua
representação. `Option`, errors, `String`, collections e task frames podem receber
lowerings especializados, mas layouts compactos, tree strings e tagged values
não são observáveis pela API.

### Princípios de API

- Ownership aparece no contrato: a API recebe borrow com `ref`/`inout`, transfere
  com `take` e torna cópias potencialmente caras deliberadas.
- Falha recuperável tem error set tipado com `throws E`; ausência esperada usa
  `T?`. Um tipo agregador de resultados pode existir para collect-all, mas não
  substitui a semântica pública de `throws`.
- Operações suspensíveis são `async`, pertencem à concorrência estruturada e
  documentam pontos de cancelamento. Deadline é cancelamento com metadata.
- Custos relevantes são visíveis: alocação, crescimento, normalização Unicode,
  cópia, blocking, criação de task, paralelismo e transição FFI não são disfarçados
  por propriedades ou conversões aparentemente baratas.
- Nenhuma API comum cria thread, acessa I/O, storage, rede, ambiente ou relógio
  global silenciosamente. Esses recursos entram por handle, contexto ou
  capability explícita.
- APIs seguras não expõem layout interno nem aceitam memória cujo allocator,
  owner ou lifetime seja desconhecido.

### Camadas funcionais

| Camada | Contrato portátil | Dependência de target |
|---|---|---|
| strings e bytes | `String` UTF-8 válido, `Bytes`, views de bytes/scalars/graphemes e conversões fallible | tabelas/engine Unicode podem vir de bundle versionado |
| collections | `Array`, `Slice`, `Map`, `Set`, iteradores e algoritmos com ownership/custo definidos | hashing/entropia process-wide exige policy explícita |
| allocators | protocolo/handle de allocator, ownership do buffer, falha e budgets explícitos | allocator do sistema é um adapter, não uma origem universal implícita |
| sync | atomics tipados, `Mutex<T>`, `RwLock<T>` e primitivas de condição | disponibilidade e garantias são capabilities do target |
| tasks | task groups, join, cancelamento cooperativo, deadlines e teste determinístico | executor, wakeups e blocking pool pertencem ao runtime/adapter |
| time | `Duration` e `Instant` monotônico como valores; conversões explícitas | wall clock, timezone e sleep/timer exigem clock/timer capability |
| io | contratos de reader/writer, buffers, EOF, partial progress e errors tipados | filesystem, terminal e backend assíncrono são adapters |
| os | nenhum contrato universal além de tipos de fronteira | processo, signals, paths nativos, ambiente e permissões são módulos de target |
| network | endereços e contratos de transporte sem protocolo de aplicação embutido | DNS, sockets, TLS e interfaces de rede exigem capabilities/adapters próprios |

I/O cancelável precisa preservar cleanup e declarar quando uma operação é
realmente interrompível. Uma chamada bloqueante pode ser encaminhada a um
executor apropriado, desde que blocking, consumo de thread e limitação apareçam
em metadata/profile; uma declaração sem adapter não a torna cancel-safe.

### Capabilities e portabilidade

Capabilities descrevem autoridade e disponibilidade, não apenas detecção de
features. Um módulo que abre arquivo ou socket recebe um handle capaz de fazê-lo;
o import, por si só, não concede acesso. O build registra target, features e
capabilities autorizadas, e a API falha de forma tipada quando uma condição
dinâmica prevista não é satisfeita. Ausência estrutural de uma API no target deve
ser diagnosticada durante resolução/build quando possível.

O perfil portátil não promete threads, filesystem, wall clock, DNS, sockets ou
processos. Targets podem fornecer adapters diferentes sem alterar ownership,
errors ou cancelamento observáveis. Otimizações como `io_uring`, pointer tagging,
SIMD ou uma implementação Unicode específica exigem fallback e testes de
equivalência.

### Fronteira C

Bindings brutos vivem em `foreign c`; wrappers de stdlib ou de pacote convertem
nullable pointer em `T?`, `(ptr, len)` em borrow ou valor owned, status/`errno` em
`throws E`, callbacks em closures com lifetime e allocator estrangeiro em owner
com deallocator correto. O wrapper também declara thread safety, blocking,
executor de callback e suporte real a cancelamento.

Headers e wrappers exportados para C não expõem layouts internos instáveis.
Exceptions não cruzam a fronteira. A calling convention usada no lowering de
`throws E` continua independente da API source e deve ser validada por harness C
compilado separadamente, como exige a [arquitetura do compilador](compiler.md).

### Escopo do primeiro slice implementável

O primeiro recorte do compilador deve ser suficiente para as fatias síncronas, ownership/errors
e tasks estruturadas do compilador:

- prelude mínima com primitivos, `Option`/`T?` e contratos fundamentais de
  igualdade, ordenação e conversão explicitamente comprovados pelo protótipo;
- `String`, `Bytes`, `Array`, `Slice` e uma coleção associativa, com iteradores,
  bounds checking, UTF-8 e comportamento de alocação documentados;
- owner de buffer e uma interface mínima de allocator/budget que permita testar
  allocator padrão, estrangeiro e região explícita sem escolher um universal;
- errors tipados para allocation, encoding e I/O; APIs source continuam
  `throws E`;
- atomics essenciais, mutex e task group/cancellation suficientes para provar o
  runtime mínimo, sem prometer uma abstração lock-free universal;
- `Duration`, `Instant`, reader/writer em memória e contratos básicos de I/O;
- um adapter pequeno de host para console/filesystem/timer e uma fronteira C
  exercitada end to end em pelo menos dois targets.

Não são objetivos desse **slice de bootstrap**: framework web, TLS, HTTP, banco
de dados, serialização universal, locale completo, regex, UI, package registry
client, streams com `yield`, actor framework, GPU ou API estável de plugins. Isso
não exclui tais módulos do SDK público completo: T2 pode implementá-los depois
que T0/T1 e o package system estiverem conformes, antes do lançamento público.
Compatibilidade durável da ABI W também não é presumida antes de ownership,
errors e tasks estarem provados.

### Testes, provenance e versões

Cada módulo da stdlib deve ter testes positivos e negativos de tipos/ownership,
property/fuzz para collections e codecs, falhas de allocation, testes
determinísticos de tasks/cancelamento, e matriz de targets, debug/release e
sanitizers disponíveis. Otimizações de representação são comparadas ao fallback;
goldens de IR não substituem execução. Adapters de C incluem harness ABI separado
para buffers, callbacks, errors e allocators.

A stdlib distribuída com o toolchain registra versão do source, interface,
intrinsics/runtime, compiler, Unicode data e adapters de target. Esses inputs
participam da chave de build. Releases publicáveis entram no lock/SBOM e emitem
provenance conforme [packages.md](packages.md); caches de interface são
descartáveis e nunca autoridade. Mudanças observáveis seguem versionamento de API;
mudanças apenas de layout ainda exigem testes de equivalência e compatibilidade
na fronteira estável declarada.

## Candidato

- Manter o namespace portátil pequeno e organizar adapters sob namespaces de
  target/capability, evitando que disponibilidade acidental vire contrato comum.
- Fazer allocators e budgets parâmetros explícitos nas APIs que precisam de
  controle, com conveniência limitada a um contexto configurado e consultável.
- Tratar Unicode data, timezone data e certificados como bundles versionados e
  registráveis na receita de build/deploy, não como estado ambiental invisível.
- Publicar wrappers C reutilizáveis como módulos normais quando não forem parte
  do bootstrap; apenas o mecanismo `foreign c` pertence ao core.
- Gerar com cada edição uma tabela canônica `nome livre → export std`, rejeitando
  entradas ambíguas e preservando namespace qualificado para todas elas. O LSP e
  `w explain name` devem mostrar origem, effects, capability e custo alcançável.

## Em aberto

- O contrato T0/T1/T2 e o escopo científico T2 aguardam ratificação conjunta em
  [W-O098/W-O099](../DB1_REVIEW.md#h09--sdk-t0t1t2-e-capabilities).
- Quais exports entram no mapa implícito sem aumentar compile time, autocomplete
  e compromisso de compatibilidade? Comparar todos os nomes únicos, uma prelude
  curada e namespaces implícitos com poucos nomes livres em W-O026.
- Qual é a API exata para allocator/região/budget e como ela interage com
  inferência de moves, shared ownership e cancelamento?
- Capabilities aparecem em parâmetros, metadata inferida ou effects adicionais
  no source? A resposta depende de [W-O006](../STATUS.md).
- Quais partes de graphemes, normalização, locale e timezone são oficiais no v0,
  e como seus dados são versionados entre targets?
- Qual contrato mínimo une I/O síncrono e assíncrono sem esconder blocking nem
  duplicar toda API?
- Quais guarantees de fairness, prioridade e backpressure pertencem a task
  groups, executors e streams futuros?
- Como estabilizar nomes e error sets de adapters sem congelar errno, códigos de
  plataforma ou a ABI interna de typed errors?

## Pesquisa

- Tree strings permanecem estrutura especializada para interning/índices; não
  são representação pública de `String`.
- Tagged values e niches permanecem otimizações não observáveis, com fallback
  por target.
- SQLite permanece adapter explícito ou storage interno de tooling, nunca
  storage implícito da stdlib.
- Arenas/heaps por módulo, streams, TLS/HTTP oficial e adapters multilíngues só
  avançam com problema mensurável, fallback, cancelamento/FFI definidos e testes
  em ao menos dois targets, conforme o [catálogo de pesquisa](../research/README.md).

## Rejeitado por enquanto

- Uma stdlib monolítica que importa serviços de OS/rede/storage no programa
  mínimo.
- `Result<T, E>` obrigatório nas assinaturas públicas em lugar de `throws E`.
- Alocação, deep copy, normalização, blocking ou criação de thread ocultos em
  operações aparentemente triviais.
- SQLite, tree strings, tagged pointers ou um allocator específico como contrato
  universal.
- Escolher um único backend de I/O, scheduler ou formato de linking como
  semântica da linguagem.
