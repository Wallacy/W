# Standard library do W

> **Status:** rascunho source. Não existe build da standard library.

[`DESIGN.md`](../DESIGN.md) define a policy plana por módulo e a semântica. Estes arquivos
testam se os contratos podem ser escritos em W.

## Módulos e contratos

Cada diretório abaixo é um módulo concreto. Ele declara target facts, required
capabilities, effects, provider status/digest e reachability; esses facts
determinam availability para o target selecionado. Todos os módulos bundled
usam a mesma policy de suporte e estabilidade. `SDK0`, `PYN` e outros nomes de
gate são etapas de design, não categorias de disponibilidade. O product inclui
somente o grafo alcançável e diagnostica um módulo indisponível no compile ou no
link.

## Estrutura inicial

```text
std/
  abort/
    contracts.w
  build/
    contracts.w
  cache/
    contracts.w
  database/
    contracts.w
  http/
    contracts.w
  io/
    contracts.w
  data/
    contracts.w
  csv/
    contracts.w
  parquet/
    contracts.w
  arrow/
    contracts.w
  presentation/
    contracts.w
  tensor/
    contracts.w
  dlpack/
    contracts.w
  net/
    contracts.w
  json/
    contracts.w
  runtime/
    task.w
    transaction.w
    work.w
    workflow.w
  stream/
    contracts.w
  url/
    contracts.w
```

`build/contracts.w` materializa bindings de transforms herméticas e o wrapper
nominal `Context`. SDK0 fecha quatro overloads para `String` e `Bytes`, com
Bytes identity e String UTF-8 estrito. O provider intrínseco `std.build@1`
continua missing. `Context` somente lê inputs e materializa candidatos em
staging. O host publica um action-result/manifest atômico após success, outputs
obrigatórios e budgets válidos. O arquivo descreve a interface e não alega
execução.
`runtime/task.w` materializa reasons, budget kinds, outcomes e timeout de tasks
lexicais. `Duration` é um intrinsic de execução signed e exato, com resolução de
nanosecond. `runtime/transaction.w` materializa o contrato de transação
estruturada. `runtime/work.w` materializa os tipos públicos usados por trabalho
supervisionado. `runtime/workflow.w` materializa effect policies, waits e event
delivery de workflows por steps. Uma suspensão pública contém duração restante,
não o alarm privado do adapter. `io/contracts.w` materializa byte I/O de host.
`json/contracts.w` materializa o codec JSON bounded de host. `Encodable`,
`Decodable` e `Codable` exigem conformance explícita. `Writer` e `Reader` são
cursors opacos; object e array cursors vivem somente em closures scoped.
`Limits` exige bounds positivos para bytes, depth, values, strings, number
tokens, object members e allocation, e `Limits(maximumBytes:)` escolhe defaults
finitos e fixos. `.interoperable` segue I-JSON/Web; `.rfc8259` aceita a grammar
numérica do RFC 8259 e verifica o range no target. `Number` é nominal,
validado e bounded; `Value` é o sum type explícito. Synthesis inclui Array,
fixed array, Option e Map<String,V>, com integers até i128/u128; tuples ficam
fora por shape ambígua. Synthesis fica limitada ao JSON fechado; não há
reflection, `Any`, annotation, macro ou serializer universal.
O provider `std.json@1` continua missing, e os oracles não alegam execução.
`stream/contracts.w` materializa o carrier readable do profile Web como owner
move-only que atende diretamente a `Stream` e, para bytes, a `ByteSource`. Tee
sempre recebe limite de lag. Item count é estrutural e usa o allocation budget.
O overload de bytes possui bound exato em bytes. O provider intrinsic interno
`std.readable-stream@1` continua missing. O arquivo fecha a interface, não cria
um runtime paralelo nem alega execução. `from` pode pagar box e indirection. O
handle tipado preserva `Item` e `Failure`. `cancel` consome o owner também em
Failure, deixa o handle inert e mantém o drain no root estruturado.
`abort/contracts.w` materializa o adapter Web de aborto. `AbortSignal` é um
handle de observação duplicável, `AbortController` é move-only e o primeiro
reason `Copy` bounded permanece estável. Timeout usa timer-resource monotônico
independente do creator/root. `any` limita primeiro os argumentos diretos e
depois as folhas pending únicas após flatten/dedup. Esse fan-in é por result; o
total vivo usa o allocation/admission budget do provider. Nenhum signal concede
authority ou vira `WireValue`. O provider intrinsic `std.abort-state@1`
continua missing.
`url/contracts.w` materializa os values portáteis de URL e parâmetros. Seu
provider intrinsic interno `std.url-record@1` segue o mecanismo da seção 19.3.1
e precisa implementar o URL Standard completo. A interface está em draft, mas
o provider executável continua missing; o arquivo não contém um parser
substituto. `URL` mantém backing canônico opaco, oferece views textuais O(1) e
materializa snapshots owned de `URLSearchParams` somente por call explícita.
`editSearchParams` mantém a mutação do URL scoped. Os outros arquivos
materializam values e protocols tipados.

`http/contracts.w` materializa o draft SDK0 de `Request`, `Response`,
`Context` e a declaration de `serve`. Um único provider intrinsic `std.http@1` possui handles de
mensagem, body, contexto e host. O provider continua missing até os gates
WHATWG Fetch, Streams integration, WPT Fetch/Headers, WinterTC/WinterCG,
workerd differential, ownership/tee fault injection, admission/cancellation,
ASan/TSan/leak e limits/fuzzing.

`BodySource` aceita somente String, Bytes, URLSearchParams e
`ReadableStream<Bytes, HttpBodyError>`. `Blob` e `FormData` permanecem
profile-final. Request e Response são owners move-only. Reads e clone são
consuming e bounded. `Request.json`/`Response.json` compõem `std.json` comum;
o valor de `Response.json` é borrowed. `http.Context` expõe wrappers tipados
para random, databases, caches, templates e signal. Registries usam bindings
const infallible resolvidos no link/startup e retornam `some` protocol owners.
As properties do `Context` são lazy: cada acesso recebe um owner retido
independentemente, o wrapper temporário cai no fim da full expression e um
wrapper explicitamente bound pode sobreviver ao valor `Context`, mas nunca ao
`request root`; `signal` devolve uma duplicata owned com a mesma regra.
`serve` exige limits, usa `net.ListenAddress`/`ref net.Network` e não transforma
cancellation em `ServerError`. A interface `std.net` está em draft, mas seu
provider `std.net@1` continua missing.

`net/contracts.w` materializa o carrier de rede SDK0 de host. `Network` é uma
capability host-provided move-only, sem initializer público, e as operações
públicas borrowam descriptors. Address values são data-only e bounded.
`HostName` usa UTS #46 nontransitional, STD3, validade IDNA2008 e forma A-label
ASCII lowercase; um único trailing dot é removido e nenhum OS search suffix é
aplicado. Parse/format de IP e socket não fazem DNS. `SocketAddress` exige
port, usa `192.0.2.1:443` ou `[2001:db8::1]:443`, e mantém scope numérico
IPv6 dentro dos brackets. TCP atende diretamente a `std.io.ByteSource` e
`ByteSink`, e `split` cria read/write halves owned. `finishWriting` faz FIN na
connection não dividida. Calls por borrow de `Network` têm state independente.
Calls `mut async` mantêm borrow exclusivo até completion ou cancellation drain.
UDP mantém datagram boundaries, informa truncation e serializa uma receive ou
send por socket. `ResolveLimits`, `ConnectOptions`,
`ListenerLimits` e `DatagramLimits` possuem defaults finitos. O provider
`std.net@1` continua missing até os gates de RFC 6724/8305/5952/8085,
differential targets, capability denial, SSRF, cancellation, partial I/O, fault
injection, sanitizers, leak, limits e fuzzing.

`std.process` é um módulo de host planejado. Ele fornece `Arguments`, `Context`,
`ExitCode`, `Signal` e o registry de signals. Named imports são recomendados.
Um namespace alias, como `process.Arguments`, continua válido. O módulo não
fornece um singleton ambiental geral chamado `process`; as duas projections
intrínsecas abaixo são a exceção estreita para roots `native-process`. Os SDKs
de target podem fornecer namespaces como `std.device`, `std.mobile` e
`std.audio`. Seus tipos
participam das assinaturas dos handlers que um product liga por `hostBindings`.
Os nomes dos slots pertencem ao host profile, não a esses módulos.

Somente um root `native-process` pode usar as projections intrínsecas
read-only `process.args` e `process.context`. O profile concede os bindings e o
compiler registra o effect/requirement; roots de teste, script e não-processo
usam fixtures explícitos ou recebem diagnostic.

O rascunho fixa nove fronteiras:

- build transforms recebem somente inputs e outputs declarados;
- workflows persistem points e outcomes, não task frames;
- I/O preserva short progress, borrows e cancellation até completion;
- streams readable mantêm um cursor e um pull em voo. Item lag é estrutural e
  byte lag é bounded em bytes. BYOB reutiliza `ByteSource`, sem reader object;
- abort signals espelham cancellation em boundaries Web, mas não substituem o
  control outcome de task. Controller drop não aborta e composição é bounded;
- HTTP valida tokens e fields antes de entregar uma mensagem a uma API safe;
  `Method.query` representa o método QUERY do RFC 10008;
- URL preserva serialização canônica, snapshots explícitos e edição live scoped
  de parâmetros, sem conceder network ou filesystem authority;
- database exige SQL const em parâmetros de chamada, usa binds nomeados, rows
  tipadas e transactions;
- cache local possui capacidade, devolve values owned e nunca vira rede por
  configuração.

`ByteSink.writeMany` usa segments borrowed e um fallback sem allocation.
Scatter read e transferência zero-copy permanecem em **Pesquisa**.

`StepEffect.atMostOnce` é o default seguro. Retry só ocorre quando o effect
contract permite. Timer e event wait não mantêm um worker ativo.

`io.SnapshotByteSource` é a fonte finita, posicional e estável durante todo o
owner. Ela publica `byteCount: u64` e `read(at:appendTo:maximum:)` com offset
`u64`, sem cursor; short reads e acessos posicionais paralelos são explícitos.

Os módulos `data`, `csv`, `parquet` e `arrow` são o rascunho TAB1 descrito em
[`DESIGN.md` §14.4.2](../DESIGN.md#1442-adapters-tabulares-tab1). Eles fecham
declarations, profiles, errors, limits, progress e ownership. Os providers
intrinsic `std.data@1`, `std.csv@1`, `std.parquet@1` e `std.arrow@1` continuam
missing. `std.io@1` também permanece missing para a implementação de
`SnapshotByteSource`; estes arquivos não implementam codec, reader, writer ou
bridge C.

`data.Batch<Row>` é o carrier columnar owned. `data.Schema` mantém a identidade
W em `SchemaIdentity` nominal, computada por provider validado. `Batch`,
`DynamicBatch` e columns são opacos move-only; descriptors carregam o Row owner.
`LogicalType` e os descritores decimal/extension usam handles nominais
indiretos com constructors validados e bounds de profundidade; date é calendar,
time é time-of-day, instant é UTC e localDateTime é civil sem conversão de
zone. `DynamicValue`
usa storage indireto para list/map/nested/extension.
`Column` suporta valor Copy. `batch.column` devolve uma loan `view` de
`Column`/`StringColumn`/`BytesColumn` ligada ao Batch; não há retain implícito,
e `copy` materializa explicitamente. Não há `Any`,
reflection, `view Value`, `XView` ou inferência de schema em runtime.
Constructors que armazenam LogicalType, SemanticExtension ou nested dynamic
storage usam `take`; identity e schema apenas observados usam `ref`.

CSV usa `decode` typed, `decodeDynamic` com `data.Schema`, `decodeAll` e dois
overloads de `encode`. O profile `portable` usa UTF-8, header obrigatório,
comma, double quote, CRLF/LF, whitespace preservado e null decode/encode
policies separadas. `DecodeDialect` e `EncodeDialect` validam seus delimiters e
colisões de tokens antes do
source. `.rfc4180` é estrito e o writer canonical é separado. Errors carregam
location e progress. Formula escaping é policy de apresentação separada.

Parquet usa `SnapshotByteSource` e valida footer, offsets, sizes, row groups,
pages, checksums, compression e logical types antes da publicação. Decode e
encode profiles são separados; binding default é `.exact`; `.project` exige
mapping. `KeyResolverCapability` chega por binding explícito de entry/context;
`KeyResolver.from` a consome sem plaintext fallback. WriterPlan
fixa codec/provider digests para bytes determinísticos. Dataset multi-file,
discovery, Hive e ambient directory ficam fora deste módulo.

Arrow separa `decodeIpcStream` e `decodeIpcFile`, além de variantes dynamic,
sem Profile de container. IPC untrusted valida metadata, buffers, dictionaries,
compression, schema e alignment; checksum só vem de envelope externo. C Data e
C Stream usam handles opaque move-only de producer trusted e options com
`BlockingQuota`/limits finitos; import C dynamic é explícito e export C Stream
está deferido. `copyPolicy:
.never` falha quando a cópia é necessária. Device handles não são tratados como
CPU pointers.

`presentation/contracts.w` materializa o protocol `Presentable`, o `Writer`
opaco, `Limits`, `MediaType` validada, `Error` e operações typed/streamed de
texto, imagem e JSON. O writer exige `text/plain` e rejeita media duplicada,
active content, collect implícito e device copy implícita. O provider
`std.presentation@1` continua missing até os gates de effect mask, bounds,
cancellation, ownership, sanitization e differential rendering. O arquivo não
cria kernel, frontend, display singleton ou capability ambiental.

`tensor/contracts.w` materializa Device provider-scoped, Queue capability opaca,
adapters ao Tensor core, transferência explícita e limites bounded. O provider
`std.tensor@1` continua missing. `dlpack/contracts.w` materializa DLPack 1.3
versioned, ManagedTensor move-only com drop síncrono somente para capsule
unconsumed comprovado, open/openDynamic, bind, materialize, export writable,
release exact-once e lifecycle de capsule. O provider `std.dlpack@1` continua
missing. Os dois arquivos não expõem raw pointer, não desserializam DLPack e não
afirmam execução de compiler, runtime, Python, CUDA ou ROCm.

Um cache remoto usa um `ServiceRef` async. Um adapter database ou HTTP pode
otimizar transporte, mas não pode mudar statements, results, ownership ou
failure semantics.

`Duration`, `Task`, `Deadline`, `Cancellation`, `CancellationId`, `WorkId`,
`WorkflowPointId`, `EffectId`, `EventId`, `WorkContext`, `StepContext`,
`ProcessContext` e handles de capability ainda são intrinsics do
compiler/runtime. `build.Context`, `Request`, `Response` e `http.Context` têm
wrappers W draft sobre providers intrinsics versionados. `std.build@1` e
`std.http@1` continuam missing.

Não serão criadas classes utilitárias quando uma operação pertence ao próprio
tipo. Exemplos:

- construção incremental pertence a `String`;
- projections read-only usam `view T`;
- IDs não concedem authority;
- handles de capability possuem os métodos que autorizam.

## Regra de promoção

Um módulo entra na std quando:

1. possui semântica no design;
2. possui ao menos um consumer no produto Última Luz;
3. possui oracle positivo e negativo;
4. tem custo, error, cancellation e target profile definidos;
5. não depende de uma implementação específica sem adapter.

O produto de referência pode usar uma API antes da promoção. Nesse caso, a API
permanece uma requirement de pesquisa e aparece no gate de build.
