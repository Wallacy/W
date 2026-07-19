# Revisão integral da Baseline de Design 1

> **Status:** **Em aberto** · proposta de fechamento em lote · 19 de julho de 2026

Este documento responde de uma vez ao inventário de questões abertas do W. Ele
não altera sozinho o estado canônico de nenhuma decisão: [STATUS.md](STATUS.md)
continua sendo a autoridade. Depois da ratificação humana, as escolhas aceitas
serão promovidas em lote e distribuídas pelas especificações correspondentes.

## Como responder

A resposta curta possível é:

```text
Aceito os defaults H01–H14.
Exceções: H03 ..., H05 ...
```

Cada bundle abaixo apresenta a recomendação da máquina. A matriz posterior cobre
cada ID individualmente e mostra qual bundle o ratifica. Alternativas que não
entram na baseline continuam preservadas como **Pesquisa** ou **Rejeitado por
enquanto**; “completo” não significa fazer toda otimização depender da primeira
implementação.

O passe propõe 90 questões como **Candidato**, cinco como **Candidato** com um
gate de **Pesquisa**, uma como **Pesquisa** e uma como **Rejeitado por enquanto**.
Essas contagens descrevem o resultado sugerido, não uma promoção já realizada.

## Quatro correções de rumo

### Range não é property behavior

`Range<T>` deve representar um domínio/intervalo e oferecer operações gerais:

```w
let ovenRange = 30[°C]...300[°C]

if requested in ovenRange {
  apply(requested)
}

let safe = ovenRange.clamp(requested)
let overlap = ovenRange.intersection(calibrationRange)
```

`clamp` transforma um valor fora do intervalo; `contains` apenas testa; construir
um refined type valida/rejeita. São semânticas diferentes. `clamp` é total apenas
num range que contém os dois extremos; bounds abertos não inventam o próximo
valor representável. Intersection retorna range opcional e uma union descontígua
produz `RangeSet`. Um behavior de propriedade poderia reutilizar `range.clamp`,
mas `Clamped` não precisa ser feature do core nem o exemplo principal de property
behaviors.

### Layout W e layout C continuam separados

Fazer toda `struct` W usar layout C parece reduzir conceitos, mas C não possui um
único layout portátil: ABI, `long`, enums, bitfields, alinhamento e calling
convention variam por target. Tornar esse contrato o default também impediria
reordenação, niches e evolução de fields sem tornar os bytes portáveis.

A separação recomendada é:

```w
struct Point { x: f64, y: f64 } // layout W nativo

foreign c {
  struct Header { kind: c.uint, size: c.size } // ABI C do target
}
```

Uma fronteira usa o tipo C diretamente ou um wrapper W; não presume que duas
declarações parecidas tenham o mesmo layout.

### Newtype não precisa de `transparent`

A alternativa preferida é dar papéis distintos a `type` e `alias`:

```w
type UserId = u64                         // nominal, representação preservada
type Port = u16 where value in 1...65535 // nominal + refinement
alias NativeSize = c.size                 // apenas outro nome
```

`type` cria identidade e conversões controladas, mas não adiciona storage ao
tipo base. O adapter FFI pode baixar `UserId` para `u64` sem custo. Isso cobre o
caso comum de newtype de forma mais direta que uma struct de um field. Uma
`struct` de um field continua sendo aggregate W normal, sem promessa implícita de
ABI.

### T0/T1/T2 são tiers do SDK, não do compilador

Os cinco níveis internos já descritos — intrinsics, prelude, std portátil,
adapters e packages — respondem a dependência e portabilidade. T0/T1/T2 respondem
a distribuição e expectativa de estabilidade; as duas classificações são
ortogonais:

| Tier do SDK | Contrato proposto | Exemplos |
|---|---|---|
| T0 · Foundation | necessário ao programa W comum; prelude pequena e extremamente estável | `Option`, errors, ranges, `String`, `Bytes`, `Array`, `Slice`, `print` quando o host oferece console |
| T1 · Systems | APIs comuns, portáteis quando possível e capability-gated | tasks/sync, time, filesystem, process, TCP/UDP/DNS, codecs fundamentais |
| T2 · Domains | módulos oficiais explícitos, completos e atualizáveis fora do ritmo do core | HTTP/TLS, SI, decimal/Money, análise numérica, tensors, JSON, regex, SQLite e TUI |

Todo o source pode acompanhar o SDK e continuar sendo reachability-linked. T2
usa o package/lock normal e pode receber correções de segurança sem exigir nova
edição da linguagem. Tier não significa qualidade menor nem import implícito.

## Questionário único de ratificação

### H01 · Superfície e forma canônica

Recomendação: `async fn f(): T throws E`; apenas `mut`, `async`, `throws` e
`unsafe` aparecem como efeitos de linguagem. `switch` é também expressão
exaustiva e não existe sinônimo `match`. `**` é exponenciação, associativa à
direita e mais forte que unary minus. Membership finito usa `value in (a, b)`
sem alocação. Formatter omite `;`, mas o parser o aceita para desambiguação ou
dois statements na mesma linha. Strings usam `"..."`, raw `r#"..."#`, multiline
`"""..."""` com dedent e interpolation `${expr}`.

### H02 · Ownership, regiões, OOM e panic

Recomendação: move automático no último uso inequívoco; `take` força/explica uma
transferência e partial move exige destructuring. `shared T` é a semântica de
owners independentes, com ARC/RC como fallback otimizável e `weak T?` para
ciclos. Regiões usam bloco lexical nomeável e API de allocator por baixo. Budget
excedido é erro/cancelamento recuperável da operação; OOM ambiental comum aborta
a isolation boundary, salvo uso de allocator explicitamente fallible. `panic`
não faz unwind na baseline e nunca cruza FFI.

### H03 · Tipos, layout e ABI

Recomendação: `type X = T` é newtype nominal com o mesmo storage e `alias` é
sinônimo. Struct W comum possui layout físico opaco entre builds; tipos
declarados em `foreign c` seguem a ABI C do target. A v0 inclui uma facility
segura de `packed`/`aligned`: acesso a field desalinhado ocorre por valor, nunca
produz `ref`/`inout`, e endianness de wire é explícito. Não congela uma ABI W
universal: módulos nativos usam fingerprint exato de toolchain/target/profile;
fronteira durável usa C ou schema. Generics são híbridos — specialization local
e witnesses/interfaces nas fronteiras.

### H04 · Unicode e strings

Recomendação: identifiers seguem Unicode XID + NFC, rejeitam controles bidi e
default-ignorables e diagnosticam confusables/mixed scripts; nomes de package
permanecem ASCII. `String` é UTF-8 contíguo owned, sem COW obrigatório; SSO é
invisível. `StringView` empresta. Não há `string[i]`: `.bytes`, `.scalars` e
`.graphemes` possuem índices/views próprios. Unicode data, locale e timezone são
bundles versionados do SDK, não estado do host.

### H05 · Ranges, SI e computação científica

Recomendação: `Range<T>` é intervalo; tipos discretos podem fazê-lo conformar a
iteração, enquanto `stride` produz progressão lazy. Range fechado oferece
`clamp`; intersection retorna optional e union pode produzir `RangeSet`.
Quantities usam dimensions com expoentes racionais normalizados em compile time,
unidades como escala/apresentação e representação numérica genérica.
Literais usam uma subgramática delimitada, por exemplo `9.81[m/s**2]` e
`180[°C]`; temperatura absoluta e delta são tipos distintos. `std.si` T2 inclui
as sete dimensões base, unidades derivadas, prefixos e constantes oficiais
versionadas. `std.math.analysis` cobre integração/diferenciação/raízes numéricas
com tolerância/erro explícitos; álgebra simbólica permanece package T2
first-party separado e experimental. As constantes definidoras do SI são exatas;
constantes físicas medidas, incertezas e correlações vêm de bundle CODATA
versionado separado.
IEEE estrito é default, `reproducible` fixa também ordem/dependências e `fast`
exige scope explícito.

### H06 · Property behaviors

Recomendação: declaração dedicada `behavior` e uso
`var Behavior(...) value: T = initial`, sem annotations. Em
`var Lazy heatProfile = deriveHeatProfile(model)`, `var` ancora o parse e tudo
entre ele e o nome pertence à lista de behaviors; nenhum lookup semântico é
necessário para reconhecer a declaração. `Lazy var heatProfile` permanece
alternativa humana, mas exigiria ordenar behaviors junto de `export`, ownership
e outros modifiers. O tipo lógico não muda; HIR vê storage,
init/get/set/modify, efeitos e cleanup. Accessors comuns são síncronos e
non-throwing; variantes com efeitos exigem `try`/`await` no uso e nunca escondem
blocking/rede. `self` só pode ser usado por accessors após definite
initialization. Composição v0 exige nesting ou behavior composto nomeado.
`Lazy`/memoization pura, `Observed`, `Once` e COW são casos; clamp fica na API de
range.

### H07 · Concorrência e paralelismo

Recomendação: cancellation é um control signal separado de `throws`, com reason
diagnóstico e propagação estrutural. `Task<T,E>` é handle linear nomeável; await
consome o resultado, e multi-await exige `SharedTask`. Falhas simultâneas escolhem
primary lexical determinístico e preservam as demais como suppressed/aggregate.
Task groups são lexicais e bounded; completion order é default, source order é
opção. `Send` e `Sync` são protocols públicos derivados. `Atomic<T>` oferece
default seq-cst e orders avançadas explícitas. Async streams são pull; channels
bounded são tipo separado. Blocking exige adapter/pool explícito. O dialeto W
preserva semântica e pode baixar para LLVM coroutines/MLIR Async sem adotá-los
como runtime contract.

### H08 · Módulos, services e durability

Recomendação: `service` merece keyword e baixa para object + descriptor. Handler
tem turn fechado/non-reentrant por default; reentrância é opt-in. Nenhum
singleton é implícito: processo/key/request/deployment são policies de host, com
keyed services como construção explícita. Módulos são DAGs definidos no manifest,
privados por default, `export` por declaração, `export import` para re-export e
`package` para visibility intermediária. `ServiceRef` sempre usa `await`, mesmo
local. Mailbox é bounded, send aguarda espaço e `trySend` falha imediatamente.
Turn durable usa transação/output gate por default no adapter. Unidade lógica é
service; processo/isolate é policy de trust/deployment. Capabilities são handles
tipados. Promise pipelining e “nanoservice” como keyword ficam fora do core.

### H09 · SDK T0/T1/T2 e capabilities

Recomendação: adotar os tiers descritos acima e uma prelude T0 curada, congelada
por edição; não importar todo nome único da stdlib. `print` permanece livre e
explicável pelo tooling. T1 inclui sockets/DNS, filesystem, process, time/random e
primitivas de runtime; T2 inclui HTTP/TLS e domínios. I/O é async-first, com API
sync separada/blocking. Authority entra por handles/context injetável. `Path`
preserva a representação nativa; `Utf8Path` é distinto. `Map` preserva insertion
order com hashing randomizado; `HashMap` pode ter ordem não observável.

### H10 · Frontend, MLIR e bootstrap

Recomendação: parser normativo handwritten recursive-descent + Pratt, acompanhado
por EBNF e corpus; Tree-sitter permanece projeção de IDE. Core MLIR em
C++/TableGen; frontend/tooling pode usar Bun durante bootstrap e migrar para W.
CMake/Ninja é build canônico por integração LLVM; xmake pode ser facade, nunca
segunda fonte. EmitC é backend de inspeção para subset síncrono, não backend
universal. Interface W versionada própria + MLIR bytecode descartável. Cache é
item-level; módulo é unidade semântica. Toolchain publicado é bundle fixado e
reconstruível em estágios.

### H11 · C e `fn<lang>`

Recomendação: importer Clang + adapter declarations/overrides geram wrappers C;
manual wrapper é escape hatch. Ilhas da aplicação aceitam body inline e `from`
como duas projeções do mesmo compilation unit, com delimitador inequívoco e
source maps. C é a única linguagem obrigatória inicial. Outro frontend entra por
allowlist hermética com toolchain/runtime/ABI fixados; compartilhar LLVM não é
suficiente, e Rust/Swift normalmente cruzam por ABI C estável.

### H12 · Packages, reprodução e supply chain

Recomendação: manifest humano compila para modelo canônico; lock/recipe/metadata
usam um profile determinístico de CBOR sem floats livres, duplicate keys ou
indefinite lengths. Digest inicial é `sha256`, sempre algorithm-tagged e migrável.
Nota W usa sections nativas + sidecar, não trailer universal. “Reproduced” exige
dois builders independentes; um builder apenas atesta “built by”. Closed source
nunca recebe alegação de auditoria pública. Comptime/build scripts rodam em worker
hermético capability-gated. Múltiplas versões ficam isoladas por package instance.
Registry usa namespaces/delegações por chave; bytes são imutáveis. License é SPDX,
policy é externa e exceção assinada. Registry publica fatos; `verified` é decisão
da policy do consumidor. Editions são opt-in com `w migrate`.

### H13 · Recursos, testes e observabilidade

Recomendação: lens inicial mede delta por import e reachable symbol, tamanho do
payload, baseline de instância e peak por operação separadamente. Primeiro budget
é peak por pedido do restaurante. Contratos duros vivem no manifest/profile;
estimates inferidos permanecem rotulados. Segundo workload é um parser. Profiles
de CI publicam receita e aggregates redigidos; claims usam corpus reproduzível.
`w test` orquestra unit, doc e compile-fail na baseline, com property/fuzz como
engines oficiais. Tooling só chama de guarantee um fato exato ou bound provado;
intervalos estimados e medições ficam separados.

### H14 · Ratificação global de escopo

Recomendação: a DB1 pode definir uma linguagem pública completa sem colocar cada
otimização ou integração no core. “Fora da v0” significa que a baseline possui
uma rota correta e uma fronteira de extensão definida — não uma promessa vaga de
resolver depois. Aceitar H14 promove em lote os defaults técnicos não alterados
explicitamente em H01–H13 e mantém apenas gates empíricos como **Pesquisa**.

## Matriz exaustiva das questões

### W-O001–W-O030

| ID | Destino sugerido | Fechamento proposto | Bundle |
|---|---|---|---|
| W-O001 | **Candidato** | `async fn`; `throws E` após o retorno | H01 |
| W-O002 | **Candidato** | last-use move; `take` para transferência forçada/relevante; partial move por destructuring | H02 |
| W-O003 | **Candidato** | `shared T` explícito, ARC/RC fallback e regiões/service ownership como otimizações/alternativas locais | H02 |
| W-O004 | **Candidato** | bloco `region` lexical nomeável sobre API de allocator; inference somente quando equivalente | H02 |
| W-O005 | **Candidato** | tagged result na ABI W, otimizado por target; wrapper C usa status + out quando necessário | H03 |
| W-O006 | **Candidato** | source: `mut`, `async`, `throws`, `unsafe`; I/O/alloc/blocking em capabilities e metadata/lens | H01 |
| W-O007 | **Candidato** | recursive-descent/Pratt normativo, EBNF e corpus diferencial | H10 |
| W-O008 | **Candidato** | Tree-sitter para IDE; frontend separado compartilhando tokens/corpus, não CST semântica | H10 |
| W-O009 | **Candidato** | core MLIR C++/TableGen com fronteira estreita para tooling/frontend | H10 |
| W-O010 | **Candidato** | CMake/Ninja canônico; xmake apenas facade opcional | H10 |
| W-O011 | **Candidato** + **Pesquisa** | EmitC para inspeção/subset síncrono; backend universal rejeitado enquanto não houver coverage | H10 |
| W-O012 | **Candidato** | interface/metadata W versionada + MLIR bytecode apenas como cache interno | H10 |
| W-O013 | **Candidato** | worker hermético, sem rede, inputs/capabilities/budgets declarados e hasheados | H12 |
| W-O014 | **Candidato** | Unicode XID/NFC seguro; package names ASCII | H04 |
| W-O015 | **Candidato** | um único `switch`, exaustivo e utilizável como expressão; sem `match` sinônimo | H01 |
| W-O016 | **Candidato** | budget recuperável cancela/falha a operação com cleanup; pressão fatal respeita isolation profile | H02 |
| W-O017 | **Candidato** + **Pesquisa** | system allocator é fallback; mimalloc acompanha SDK como profile condicionado a benchmark | H02 |
| W-O018 | **Candidato** + **Pesquisa** | niches convencionais na baseline; low/high address bits somente após gate por target | H02 |
| W-O019 | **Candidato** | profile determinístico de CBOR + digest algorithm-tagged `sha256` inicial | H12 |
| W-O020 | **Candidato** | nota mínima em ELF/PE/Mach-O/Wasm e sidecar canônico; sem trailer obrigatório | H12 |
| W-O021 | **Candidato** | fatos N-of-M; badge público reproduced começa em 2 builders independentes; policy pode exigir mais | H12 |
| W-O022 | **Candidato** | closed source só recebe attestation do builder/confidential workflow, nunca auditabilidade pública simulada | H12 |
| W-O023 | **Candidato** | keyword `service`, lowering para object + descriptor | H08 |
| W-O024 | **Candidato** | closed turn default, reentrância explícita | H08 |
| W-O025 | **Candidato** | nenhum singleton default; scopes e keyed identity declarados pelo host/service | H08 |
| W-O026 | **Candidato** | prelude T0 curada/frozen; namespaces continuam disponíveis e tooling mostra origem | H09 |
| W-O027 | **Rejeitado por enquanto** | “nanoservice” permanece lente; somente `service` tem semântica pública | H08 |
| W-O028 | **Candidato** | lens por import e symbol reachability, com instância/operação separadas | H13 |
| W-O029 | **Candidato** | primeiro budget: peak live bytes por pedido; payload e baseline coletados em paralelo | H13 |
| W-O030 | **Candidato** | budgets no manifest/profile; estimates inferidos; source declaration apenas se houver contrato algorítmico real | H13 |

### W-O031–W-O060

| ID | Destino sugerido | Fechamento proposto | Bundle |
|---|---|---|---|
| W-O031 | **Candidato** | parser incremental/diagnóstico como segundo workload | H13 |
| W-O032 | **Candidato** | raw local, aggregate redigido em CI e corpus público para claims | H13 |
| W-O033 | **Candidato** | `from` declarado e inserção implícita somente quando total, única e não ambígua; `catch` fora disso | H02 |
| W-O035 | **Candidato** | tipo opaco + factories por default; fields ganham `export` individual quando são parte da API | H03 |
| W-O036 | **Candidato** | dimension/unit declarations + quantity literal delimitado `[unit expression]`; símbolos SI usam subgramática própria | H05 |
| W-O037 | **Candidato** | IEEE strict default; scopes `reproducible` e `fast` explícitos | H05 |
| W-O038 | **Candidato** | `value in (a, b)` é membership finito intrinsic; flags usam `hasAny`/`hasAll` | H01 |
| W-O039 | **Candidato** | `**` para exponenciação; `pow` permanece API para famílias/casos especiais | H05 |
| W-O040 | **Candidato** | promover W-C025: primeiro posicional, seguintes nomeados, labels customizáveis | H01 |
| W-O041 | **Candidato** | intervalo primeiro, `clamp` apenas para closed range, `RangeSet` para union descontígua e `stride` separado | H05 |
| W-O042 | **Candidato** | inline e `from` como forms equivalentes; compilation unit nomeada pelo adapter/manifest | H11 |
| W-O044 | **Candidato** | layout W opaco, layout C na fronteira, `type` newtype, packed/aligned restritos na v0 | H03 |
| W-O045 | **Candidato** | profile compacto somente em artefato/fingerprint compatível; mismatch rejeita ou faz marshal | H03 |
| W-O046 | **Candidato** | UTF-8 owned contíguo, move-first, view borrowed e SSO invisível; COW não é contrato | H04 |
| W-O047 | **Candidato** | views bytes/scalars/graphemes com índices próprios; sem indexação direta de `String` | H04 |
| W-O048 | **Candidato** | uma forma normal, raw hash-delimited, multiline dedent e `${}` | H04 |
| W-O049 | **Candidato** | lattice total/value-preserving de W-C031; literals contextuais, casts numéricos perigosos explícitos | H05 |
| W-O050 | **Candidato** | generics híbridos: monomorphization/specialization local e witnesses nas interfaces | H03 |
| W-O051 | **Candidato** | inference bidirecional local e solver decidível/budgeted; predicates restantes checam em runtime | H05 |
| W-O052 | **Candidato** | captures inferidos; lista `copy/ref/take/weak` só para override; env de closure explícito na HIR | H02 |
| W-O053 | **Candidato** | allocator fallible explícito; OOM geral encerra isolation boundary | H02 |
| W-O054 | **Candidato** | abort da isolation boundary, sem unwind default e nunca através de FFI | H02 |
| W-O055 | **Candidato** | `Atomic<T>`/locks na std; seq-cst conveniente e memory orders avançadas explícitas | H07 |
| W-O056 | **Candidato** | `task.cancel(reason:)`; cancellation é signal estrutural separado de error set | H07 |
| W-O057 | **Candidato** | `Task<T,E>` linear e one-await; `SharedTask` explícita para múltiplos observers | H07 |
| W-O058 | **Candidato** | primary lexical determinístico, siblings cancelados e outras falhas preservadas | H07 |
| W-O059 | **Candidato** | protocols públicos `Send`/`Sync`, derivados e verificáveis; conformance manual somente unsafe | H07 |
| W-O060 | **Candidato** | task group lexical bounded, backpressure por await e ordering selecionável | H07 |

### W-O061–W-O099

| ID | Destino sugerido | Fechamento proposto | Bundle |
|---|---|---|---|
| W-O061 | **Candidato** | linguagem não promete prioridade/latência; runtime expõe progress, policy e métricas; executor determinístico em testes | H07 |
| W-O062 | **Candidato** | async pull sequence; channel bounded separado; generator respeita demand | H07 |
| W-O063 | **Candidato** | blocking proibido no executor cooperativo sem adapter/pool bounded explícito | H07 |
| W-O064 | **Candidato** | W async dialect/state machine define semântica; LLVM coro/MLIR Async são lowerings substituíveis | H10 |
| W-O065 | **Candidato** | manifest é source of truth dos files; módulo é unidade semântica multi-arquivo sem runtime init implícito | H08 |
| W-O066 | **Candidato** | privado default, `export`, `export import`, visibility `package`, sem friend | H08 |
| W-O067 | **Candidato** | DAG entre módulos; ciclos internos pertencem ao mesmo módulo | H08 |
| W-O068 | **Candidato** | toda `ServiceRef` call é async/fallible; fast path é otimização | H08 |
| W-O069 | **Candidato** | mailbox bounded; send espera, `trySend` falha; FIFO por sender e policies explícitas | H08 |
| W-O070 | **Candidato** | durable turn transacional + output gate no adapter; escape avançado explícito | H08 |
| W-O071 | **Candidato** | service é restart unit lógica; process/isolate é failure/security policy do deployment | H08 |
| W-O072 | **Candidato** | root grants no manifest e capability handles tipados/delegáveis no runtime | H08 |
| W-O073 | **Pesquisa** | promise pipelining/capability RPC em wRPC T2; não requisito do core v0 | H08 |
| W-O074 | **Candidato** | I/O async-first, sync/blocking em API/context separado | H09 |
| W-O075 | **Candidato** | Unicode/locale/timezone como bundles providers versionados e lockados | H09 |
| W-O076 | **Candidato** | erro semântico estável + cause nativa opaca e detalhe do adapter consultável | H09 |
| W-O077 | **Candidato** | clocks/random/environment são capabilities injetáveis; conveniência do host é substituível em testes | H09 |
| W-O078 | **Candidato** | filesystem/process T1 capability-based; `Path` nativo lossless e `Utf8Path` separado | H09 |
| W-O079 | **Candidato** | sockets/DNS T1; TLS/HTTP T2 oficial, bundled e atualizável independentemente | H09 |
| W-O080 | **Candidato** | `Map` insertion-ordered + hash seed random; `HashMap` oferece variante unordered | H09 |
| W-O081 | **Candidato** | fixed-scale decimal primeiro; `Money<Currency,Rep>` T2 com rounding explícito | H05 |
| W-O082 | **Candidato** | Array/Slice no T0; Tensor/views ranked no T2 com lowering protocolar MLIR | H05 |
| W-O083 | **Candidato** | Clang importer + adapter declarations/overrides + wrapper manual | H11 |
| W-O084 | **Candidato** + **Pesquisa** | C obrigatório; outros frontends entram por matriz hermética e ABI/runtime comprovados | H11 |
| W-O085 | **Candidato** | ABI W keyed por toolchain/target/profile; source-first; ABI C/schema para longevity | H03 |
| W-O086 | **Candidato** | newline contextual, `;` aceito mas nunca emitido canonicamente salvo necessidade | H01 |
| W-O087 | **Candidato** | profiles/features no manifest; `when target(...)` limitado e registrado na interface | H12 |
| W-O088 | **Candidato** | package instances isolam versões; type identity inclui digest/instância; feature unification local | H12 |
| W-O089 | **Candidato** | dependency graph/cache por item; módulo é unidade de type checking; generic instance keyed no CAS | H10 |
| W-O090 | **Candidato** | bundle de toolchain fixado + rebuild em estágios e diversidade posterior | H10 |
| W-O091 | **Candidato** | namespaces por owner/org com delegação/rotação de keys; conteúdo imutável por digest | H12 |
| W-O092 | **Candidato** | SPDX/SBOM no artefato; policy externa e exceção assinada | H12 |
| W-O093 | **Candidato** | registry publica facts; policy do consumidor calcula verified; yank/revoke possuem autoridades separadas | H12 |
| W-O094 | **Candidato** | editions opt-in, migrations automatizadas e janela de suporte explícita | H12 |
| W-O095 | **Candidato** | runner único; unit/doc/compile-fail baseline; property/fuzz engines oficiais | H13 |
| W-O096 | **Candidato** | facts/bounds provados são guarantees; estimates intervalares e profiles medidos ficam rotulados | H13 |
| W-O097 | **Candidato** | `var Behavior value = initial`; `Behavior var` preservado; expansão e composição explícitas | H06 |
| W-O098 | **Candidato** | tiers T0/T1/T2 do SDK, ortogonais a intrinsics/portabilidade/adapters | H09 |
| W-O099 | **Candidato** + **Pesquisa** | SI e análise numérica oficiais T2; álgebra simbólica fica num package first-party experimental | H05 |

W-O034 continua reservado e W-O043 já foi promovida. Assim, cada questão ativa
aparece exatamente uma vez nesta matriz.

## Fontes primárias de comparação

- [BIPM — SI Brochure, 9th edition/current revision](https://www.bipm.org/en/publications/si-brochure/)
- [NIST — CODATA fundamental physical constants](https://physics.nist.gov/cuu/Constants/)
- [F# — Units of Measure](https://learn.microsoft.com/en-us/dotnet/fsharp/language-reference/units-of-measure)
- [Unicode UAX #31 — Identifiers](https://www.unicode.org/reports/tr31/)
- [Unicode UTS #39 — Security mechanisms](https://www.unicode.org/reports/tr39/)
- [Rust Reference — type layout](https://doc.rust-lang.org/stable/reference/type-layout.html)
- [Zig — `extern` and `packed struct`](https://ziglang.org/documentation/master/)
- [Swift SE-0030 — Property Behaviors](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0030-property-behavior-decls.md)
- [Swift SE-0258 — Property Wrappers](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0258-property-wrappers.md)
- [MLIR — Async dialect](https://mlir.llvm.org/docs/Dialects/AsyncDialect/)
- [MLIR — EmitC dialect](https://mlir.llvm.org/docs/Dialects/EmitC/)
- [LLVM — coroutines](https://llvm.org/docs/Coroutines.html)
- [LLVM — atomics](https://llvm.org/docs/Atomics.html)
- [RFC 8949 — deterministic CBOR](https://www.rfc-editor.org/rfc/rfc8949.html#section-4.2)
- [SLSA provenance](https://slsa.dev/spec/v1.2/provenance)
- [The Update Framework](https://theupdateframework.github.io/specification/latest/)
- [Reproducible Builds — definition](https://reproducible-builds.org/docs/definition/)
