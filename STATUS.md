# Status e registro de decisões

> **Working Draft · 19 de julho de 2026**

Este documento impede que uma hipótese exploratória seja lida como promessa. Ele
é o índice de maturidade do design, não uma especificação formal.
O [mapa de fechamento](DESIGN_CLOSURE.md) organiza as mesmas decisões por
dependência; este arquivo continua sendo o registro canônico do estado.

## Vocabulário

| Estado | Significado |
|---|---|
| **Direção** | intenção estável do projeto; mudar exige revisar a visão |
| **Candidato** | escolha coerente usada nos exemplos; precisa de protótipo antes de ser normativa |
| **Em aberto** | problema delimitado com mais de uma alternativa legítima |
| **Pesquisa** | ideia preservada, mas fora do caminho crítico da versão zero |
| **Rejeitado por enquanto** | não cabe no núcleo atual; pode ser reavaliado com nova evidência |

Nenhum item neste arquivo é uma garantia de compatibilidade, pois ainda não existe uma versão publicada da linguagem.

## Direções

| ID | Decisão de direção | Consequência |
|---|---|---|
| W-D001 | W prioriza prazer de uso e previsibilidade nativa. | sintaxe, diagnósticos e custos observáveis importam tanto quanto performance |
| W-D002 | C é ABI e ecossistema de primeira classe, não a definição semântica de W. | W pode usar MLIR sem perder interop C nem herdar todo undefined behavior de C |
| W-D003 | Concorrência e paralelismo são conceitos distintos. | `async`/`await` e `spawn` têm contratos diferentes |
| W-D004 | Concorrência é estruturada por padrão. | filhos pertencem ao escopo, cancelamento e join são previsíveis |
| W-D005 | Safe W não permite dangling pointers nem data races. | FFI/manual memory forma uma fronteira explícita |
| W-D006 | Semântica observável não muda entre debug e release. | overflow, checks e cancelamento têm políticas uniformes |
| W-D007 | Toolchain e package security são first-party. | formatter, lock, provenance e verificação entram cedo no roadmap |
| W-D008 | Uma forma canônica é preferida a múltiplos sinônimos. | `fn`, uma sintaxe de retorno e uma família de strings no núcleo candidato |
| W-D009 | Features para IA devem também melhorar a compreensão humana. | metadata e diagnósticos estruturados, sem um dialeto opaco “para LLM” |
| W-D010 | Otimizações não podem ser requisito sem fallback portável. | tagged pointers, io_uring, SIMD e GPU ficam atrás de lowering/target capabilities |
| W-D011 | Módulo estático, instância de execução e package são conceitos distintos. | import não cria lifecycle, heap, thread, autoridade nem boundary de segurança |
| W-D012 | Mesmos inputs declarados e ambiente fixado produzem os mesmos bytes do payload. | tempo, commit, paths, seed, target e toolchain são inputs ou são eliminados |
| W-D013 | W não pretende substituir JavaScript no browser. | WASM pode ser target de playground/deployment, sem DOM ou runtime web implícito |
| W-D014 | Unidades lógicas podem ser fine-grained sem impor uma fronteira física por unidade. | compiler/runtime podem co-localizar ou agrupar mantendo contrato e observabilidade |
| W-D015 | Estimativas de recursos expõem escopo, proveniência, confiança e partes desconhecidas. | tooling não transforma medição, hipótese ou ausência de prova em garantia de runtime |

## Modelo candidato usado nos exemplos

| ID | Escolha candidata | Por que esta é a baseline |
|---|---|---|
| W-C001 | `fn name(arg: Type): Return` | combina leitura familiar com retorno por `:`; evita `fn`/`func` e `:`/`->` simultâneos |
| W-C002 | `const`, `let`, `var` | separa compile time, binding imutável e binding mutável |
| W-C003 | `struct`, `object`, `enum`, `protocol` | torna valor, identidade, soma e contrato visíveis; “tudo é enum” vira implementação possível, não semântica |
| W-C004 | `T?` tem exatamente `.some(T)` ou `.none` | elimina a fusão de null/undefined/uninitialized/empty |
| W-C005 | `throws E` + `try` para erro recuperável tipado | permite lowering explícito sem exceptions ocultas; `panic` fica para invariantes quebradas |
| W-C006 | inteiros fixos `i8…i128`, `u8…u128` e `Int`/`UInt` nativos | layout público usa larguras fixas; o tipo nativo serve ao código comum |
| W-C007 | overflow é verificado por padrão; wrapping é explícito | comportamento idêntico entre modos e source que anuncia a intenção |
| W-C008 | `String` é UTF-8 válido; bytes/scalars/graphemes são views explícitas | evita uma unidade ambígua chamada apenas `length` |
| W-C009 | owner único por padrão; `ref`, `inout`, `take`, `copy` nos pontos relevantes | caminho comum leve sem esconder transferências importantes |
| W-C010 | `async let` inicia filho concorrente; `spawn let` inicia filho paralelo | preserva a distinção central com pouca sintaxe |
| W-C011 | módulo é namespace/unidade de build, não heap singleton obrigatório | lifecycle e recursos ficam em objetos/serviços explícitos; arenas de módulo continuam pesquisáveis |
| W-C012 | imports usam nomes lógicos resolvidos pelo manifest/lock | código não depende de URL mutável; resolução continua reproduzível |
| W-C013 | `foreign c` declara a fronteira C; ilhas da aplicação `fn<lang>` ficam para depois | primeiro resolve ABI, ownership e erros de uma linguagem bem suportada antes de embutir seu frontend no build W |
| W-C014 | frontend preserva semântica num dialeto W/MLIR antes do lowering | ownership, tasks e efeitos não somem cedo num C intermediário |
| W-C015 | source + artefatos reproduzíveis/content-addressed | static libs são preferíveis quando compatíveis, não a única forma de distribuição |
| W-C016 | experimento do primeiro protótipo: mapa de exports implícitos da stdlib congelado por edição, com fallback somente para nome único | `print` e outras APIs comuns podem ser curtas sem lookup ambiental; origem, efeito, capability e dependência continuam visíveis no tooling e na receita; o conjunto exato segue em W-O026 |
| W-C017 | baseline do primeiro protótipo: instância `service` explícita com handler serial e turn fechado | estado e eventos têm lifecycle; o default final de reentrância segue em W-O024 |
| W-C018 | registry governa metadata assinada; hosts/CDNs são mirrors de bytes por digest | trocar o mirror não troca identidade nem trust root |
| W-C019 | payload inclui nota W mínima; attestations e envelopes permanecem externos | evita assinatura autorreferente e separa reprodução, autorização e platform signing |
| W-C020 | lens de import mede delta de artefato/reachability e separa custos de instância e operação | mantém `import` estático, evita dupla contagem e coloca o custo perto da decisão que o introduziu |
| W-C021 | `enum E: Error` forma um error set fechado; `try` só propaga o mesmo `E` sem conversão | composição de errors diferentes permanece explícita enquanto W-O033 estiver aberta |
| W-C022 | tooling mantém Tree-sitter + queries como projeção estrutural; TextMate é compatibilidade do VS Code e o scanner do portal é temporário | evita três grammars permanentes sem encerrar W-O007/W-O008 nem confundir CST com tradução semântica |
| W-C023 | declarations são privadas ao módulo por default; `export` forma a interface W | evita `public`/`private` redundantes; valores transparentes, objetos encapsulados e operações remotas continuam semanticamente distintos |
| W-C024 | formatter candidato usa largura preferida 120, mantém a construção inteira em uma linha quando cabe e usa forma vertical determinística quando quebra | reduz ruído e tokens sem criar duas formas canônicas; fórmulas e comentários longos seguem mensuráveis no corpus |
| W-C025 | primeiro argumento é posicional por default; seguintes usam o nome como label, com labels customizáveis | economiza repetição no receiver/valor principal e preserva intenção nos argumentos seguintes; defaults alternativos seguem em W-O040 |
| W-C026 | `mut fn` marca mutação do receiver implícito; free function usa `inout` sem repetir `mut` | mantém mutação visível no contrato e evita dois marcadores para a mesma authority explícita |
| W-C027 | grammar experimenta quatro closures de range: `...`, `..<`, `>..`, `>..<` | a posição de `>`/`<` mostra qual bound é excluído; semântica de intervalo/progressão segue em W-O041 |
| W-C028 | switch pattern experimenta ranges bounded e one-sided com guard `where` | reutiliza pattern refinement em vez de tornar `where` um sinônimo geral de `&&`; first-class unbounded ranges seguem abertos |
| W-C029 | compactação de valores é invisível no source, sempre tem fallback e fica explicável pelo tooling | existentials/erasure interna podem fazer box com custo reportado; `Option<ref T>` promete zero alocação, não tamanho universal; W-O018 ainda seleciona profiles |
| W-C030 | a v0 não expõe um tipo universal `Any`; erasure irrestrita permanece interna ou em biblioteca futura | enums, generics e `any P` cobrem código tipado; JSON, FFI e linguagens dinâmicas usam tipos/adapters próprios sem metadata universal no programa comum |
| W-C031 | conversão implícita só é aceita quando total, value-preserving e com um único caminho canônico | widening seguro, refined→base e `T→any P` podem ser implícitos; narrowing, parsing, rounding, reinterpretation e conversões ambíguas continuam explícitos; W-O049 fecha a lattice exata |
| W-C032 | refinement preserva tipo/aritmética base e autoriza niches, range optimization e storage mais estreito apenas onde layout/endereço não são observáveis | structs materializados, ABI, FFI, persistência e borrows usam layout canônico; SSA, storage interno não escapante, SIMD e GPU podem especializar com reextensão/checks corretos |
| W-C033 | existential é escrito `any P`; `T: P` é generic e `some P` preserva uma identidade concreta opaca | um token distingue type erasure em assinaturas sem poluir call sites; `P` sozinho não ganha um segundo significado e não existe forma sinônima |
| W-C034 | `any P` carrega somente value witnesses e witnesses exigidos por `P`; reflection nominal/estrutural é opt-in e alcançável | `ref any P` pode ser fat borrow sem alocação; owned existential pode inline/box; descriptor identity serve de `TypeId` local sem nomes/fields universais |
| W-C035 | declarar conformance a `Reflectable` sintetiza metadata alcançável, sem annotation; debug symbols são separados e removíveis | reflection runtime não atravessa encapsulamento nem força nomes/fields em todo binário; customização futura usa requisitos do protocol, não decorators |
| W-C036 | a v0 não possui sistema genérico de `@annotations` | comportamento semântico usa keywords/blocos próprios; build/deployment usa manifest; macros, derives e metadata arbitrária não criam uma segunda linguagem escondida |

## Questões abertas prioritárias

Os IDs são estáveis e não são renumerados; W-O034 ficou sem atribuição durante a
consolidação inicial e permanece reservado para evitar reutilização acidental.

| ID | Questão | Alternativas a prototipar | Teste de decisão |
|---|---|---|---|
| W-O001 | `async` e `throws` ficam após o retorno ou antes de `fn`? | `fn f(): T async throws E` · `async fn f(): T throws E` | leitura, parser, autocomplete e tipos de função |
| W-O002 | qual é o algoritmo exato de move implícito? | last-use move · move sempre explícito para `object` · partial field move vs destructuring prévio | mensagens em código real, número de marcadores `take`/`copy` e preparo concorrente dos ingredientes do restaurante |
| W-O003 | compartilhamento é ARC, região ou tipo `shared`? | `shared T` com ARC · arenas · ownership de serviço | ciclos, FFI, concorrência e custo medido |
| W-O004 | como expressar regiões opcionais? | API de stdlib · bloco `region` · inference-only | request lifecycle, retorno e cancelamento |
| W-O005 | como representar typed throws no ABI? | result struct · status + out parameter · tagged union | zero-copy, C wrappers e otimização MLIR |
| W-O006 | quais efeitos além de `mut`, `async` e `throws` são source-level? | `io`/`alloc` explícitos · capabilities em parâmetros · metadata inferida | composição de APIs sem poluição visual |
| W-O007 | qual parser é normativo? | recursive descent + gramática EBNF · parser gerado | qualidade de erro, manutenção e tooling |
| W-O008 | Tree-sitter participa do compilador ou apenas da IDE? | CST compartilhada · frontend separado | recuperação de erro e divergência de gramática |
| W-O009 | como implementar o core MLIR inicial? | C++/TableGen · C API + wrapper · frontend Bun + core C++ | estabilidade, velocidade de bootstrap e acesso a dialect APIs |
| W-O010 | sistema de build do compilador | CMake nativo do LLVM · xmake como facade · ambos | Windows/Linux/macOS e pin de LLVM |
| W-O011 | qual subset de C pode sair via EmitC? | backend de inspeção · backend portável oficial | coverage de tipos, errors e corrotinas |
| W-O012 | formato de módulo/ABI metadata | MLIR bytecode · formato próprio versionado · combinação | leitura sem toolchain completa e evolução de versão |
| W-O013 | como restringir compile-time execution? | sandbox sem rede · capability grants · hermetic worker | segurança, reprodutibilidade e ergonomia |
| W-O014 | limites de Unicode em identificadores | ASCII canônico · Unicode normalizado · Unicode com lint confusables | segurança, tooling e experiência internacional |
| W-O015 | sintaxe de match avançado | `switch` estilo Swift · `match` expression | exhaustividade, retorno como expressão e multi-value cases |
| W-O016 | como falha um budget de memória? | `throws` · cancelamento do serviço · policy do profile | recovery em dois hosts sem corrupção, leak ou cleanup perdido |
| W-O017 | quais perfis de allocator entram na v0? | allocator do sistema · mimalloc opcional · escolha do host | benchmark reproduzível, FFI, sanitizers e fallback por target |
| W-O018 | quais representações tagged passam o primeiro gate? | niches convencionais · bits baixos · top-byte quando suportado | implementação dual, FFI e hardening em x86_64 e arm64 |
| W-O019 | qual serialização e digest iniciam receitas, manifests e attestations? | formato canônico existente · subset W data-only · formato próprio | implementações independentes, migração criptográfica e bytes canônicos |
| W-O020 | como materializar a nota W e separar envelopes de plataforma? | sections ELF/PE/Mach-O/Wasm · trailer comum · ambos | strip, LTO, notarização, inspeção e extração do payload |
| W-O021 | qual quorum e independência de rebuild para cada tier? | 1-of-N · 2-of-3 · policy organizacional | custo, diversidade real, divergência preservada e recovery |
| W-O022 | que alegação é permitida para código fechado? | rebuilt-by-builder · confidential builder · nenhuma verificação pública | wording que não confunda trust no builder com reprodução/auditoria independente |
| W-O023 | `service` começa como keyword, declaração gerada ou API? | sintaxe própria · object + metadata · IDL/codegen | leitura, tooling, lifecycle e possibilidade de remover o açúcar |
| W-O024 | qual modelo de turn/reentrância é o default? | closed turn · reentrância opt-in · actor estrito | invariantes, deadlock, latency e três workloads com I/O |
| W-O025 | quais escopos de singleton existem no primeiro corte? | process · key · deployment · request · nenhum default | routing, restart, testes e isolamento observáveis |
| W-O026 | quais exports da stdlib entram no mapa implícito da edição? | todos os nomes únicos · somente uma prelude curada · namespaces implícitos + poucos nomes livres | clareza de authority, tokens, autocomplete, estabilidade entre edições e colisões no restaurante |
| W-O027 | “nanoservice” é nome público e qual é sua unidade mínima? | apenas lente interna · nome de `service` · handler stateless também | state/lifecycle/capability que justifique boundary sem feature soup |
| W-O028 | qual é o primeiro corte do lens de recursos? | somente import/artefato · import + calls do restaurante | precisão, latência incremental e utilidade antes de existir runtime |
| W-O029 | qual budget valida primeiro o modelo? | payload final · baseline por instância · peak por request | dado reproduzível, ação de recovery e valor em CI |
| W-O030 | onde declarar contratos de recursos? | declaração source própria · manifest/profile · metadata inferida/gerada | legibilidade, ABI, verificação independente e custo sintático |
| W-O031 | qual workload complementa o restaurante? | parser · servidor HTTP · imagens | recursão, I/O, FFI, fanout e alocação com formas diferentes |
| W-O032 | como perfis medidos entram em revisão e CI? | apenas local · anexo redigido · corpus público reproduzível | privacidade, portabilidade e não confundir p95 com teto |
| W-O033 | como compor error sets diferentes sem esconder controle? | `catch` explícito · conversão `from` declarada · injeção única inferida | ambiguidade, ergonomia, ABI e diagnóstico no restaurante |
| W-O035 | como exportar values com invariantes sem expor toda a representação? | `export struct` transparente · fields exportados individualmente · tipo opaco + factories | evolução de API, construção cross-module, pattern matching e ABI |
| W-O036 | qual sintaxe representa quantidades e unidades físicas? | sufixo de literal · `Number<Unit>` · declaração de unit dedicada | dimensional analysis, legibilidade de fórmulas, generics, FFI e zero overhead |
| W-O037 | quais modos numéricos são observáveis? | IEEE estrito default · modo reproduzível · fast-math explícito por scope | resultados entre targets, vetorização, redução paralela e diagnóstico |
| W-O038 | qual forma compacta testa um valor contra várias alternativas? | `value.isOneOf(a, b)` · `value in (a, b)` · pattern alternativo | tokens, ausência de alocação/varargs, narrowing, diagnostics e leitura de `canMove` |
| W-O039 | qual forma representa exponenciação? | `base ** exponent` · `pow(base, exponent)` · APIs distintas por família numérica | precedência de unary minus, dimensions, inteiros negativos, overflow e lowering math |
| W-O040 | qual default de labels produz melhor API? | primeiro posicional + restantes nomeados · todos nomeados · todos posicionais com lint | tokens, autocomplete, refactors e leitura de calls com vários argumentos do restaurante |
| W-O041 | `Range<T>` é intervalo, progressão iterável ou tipos separados? | intervalo + `stride` explícito · producer lazy com step · `Interval<T>` separado de `Range<T>` | membership float, `for` inteiro, count/last, alocação, infinitos e mensagem matemática correta |
| W-O042 | qual forma delimita e agrupa ilhas `fn<lang>` da própria aplicação? | body inline opaco · `from` para source separado · namespace de compilation unit · adapter declarado | migração gradual, language injection, chamadas intra-unit, ABI, source maps, cache, provenance e targets |
| W-O044 | quais layouts são observáveis e quais fronteiras explícitas entram na v0? | W nativo opaco · declaração dentro de `foreign c` · `transparent struct` · layout W fixo futuro | `sizeOf`, evolução de fields/enums, FFI, módulos de versões distintas e otimização cross-module |
| W-O045 | como módulos negociam um profile de representação interna? | somente dentro do mesmo artefato · metadata versionada · marshal sempre entre módulos | LTO, dynamic linking, cache, sanitizers, fallback e rejeição segura de incompatibilidade |
| W-O046 | qual é storage, ownership e mutabilidade de `String`? | UTF-8 contíguo value · COW · buffer owned com views · rope especializado separado | copy/move/drop, SSO, concatenação, FFI, tasks e custo previsível |
| W-O047 | quais unidades podem indexar/slicear `String` e como boundaries inválidos falham? | bytes/scalars/graphemes por views nomeadas · índices tipados · apenas iteradores para graphemes | complexidade, alocação, normalização, invalid UTF-8 e diagnostics |
| W-O048 | qual é a sintaxe canônica de string literal, raw, multiline e interpolation? | um delimitador + escapes · raw com marcador · multiline dedent explícito | gramática sem sinônimos, Unicode, source maps, formatter e injection segura |
| W-O049 | quais conversions, promotions e casts numéricos são implícitos? | somente widening comprovado · nenhum cast implícito cross-family · regras contextuais limitadas | overflow, generics, units, literals, SIMD e portabilidade |
| W-O050 | qual modelo de generics e conformances entra na v0? | monomorphization · dictionaries · híbrido; `protocol`/traits explícitos | compile time, code size, dynamic linking, diagnostics e specialization |
| W-O051 | qual é o limite de inference, refinements e avaliação compile-time? | subset decidível · solver limitado por budget · predicates somente runtime fora do subset | termination, mensagens locais, build hermético e expressão matemática útil |
| W-O052 | qual sintaxe e representação governam captures e closures que escapam? | lista explícita `copy/ref/take/weak` · inferência com diagnostics · closure object uniforme | lifetime, alocação, callbacks C, `async`/`spawn` e cycles |
| W-O053 | qual é a política para falha de alocação fora de um budget declarado? | `throws AllocError` em APIs alocantes · panic/abort por profile · allocator fallible explícito | cleanup, containers, FFI, overcommit e código que não pode recuperar |
| W-O054 | o que `panic` faz e W suporta unwind entre frames? | abort por default · unwind W-only · policy por deployment sem cruzar FFI | destructors, locks, tasks, binary size, C++ exceptions e isolamento de serviço |
| W-O055 | qual modelo de memória e superfície de atomics W expõe? | atomics na stdlib com orders explícitas · subset seguro + `unsafe` avançado · apenas primitives de runtime na v0 | data races, LLVM mapping, target support, reclamation e diagnostics |
| W-O056 | qual sintaxe solicita cancelamento e como o motivo é representado? | `cancel task` · `task.cancel(reason:)` · cancellation token/capability | typed errors, cleanup, idempotência, deadlines e propagação estrutural |
| W-O057 | `Task<T,E>` é nomeável e qual é a semântica de `await` do resultado? | handle lexical one-shot · task pública multi-await para `Copy` · shared result explícito | ownership do resultado, retenção de frame, cancelamento e API de groups |
| W-O058 | qual falha tem primazia quando filhos concorrentes falham? | ordem lexical · primeira observada · aggregate tipado · policy do group | determinismo, cancelamento dos siblings, logs e reproducibilidade de testes |
| W-O059 | como `Send`, `Sync` e cancellation safety são nomeados e derivados? | traits públicas · propriedades inferidas consultáveis · capabilities estruturais sem nomes públicos | generics, unsafe opt-out, FFI, diagnostics e evolução compatível |
| W-O060 | qual API mínima de task group, ordering e backpressure? | group lexical · producer/consumer bounded · combinators na stdlib | fan-out grande, memória limitada, erro/cancelamento e ordem de resultados |
| W-O061 | quais garantias pertencem ao executor/scheduler? | fairness mínima sem prioridade · priorities explícitas · policy de host | starvation, latency, determinismo, oversubscription, blocking e observabilidade |
| W-O062 | qual é o modelo de streams assíncronos? | `AsyncSequence` pull · channel bounded · generator `yield` com demand | ownership por elemento, buffering, erro, cancelamento e fusão de pipelines |
| W-O063 | qual é o contrato entre execução bloqueante e executor async? | pool bloqueante explícito · adapters por capability · proibir blocking em executor cooperativo | deadlock, oversubscription, FFI síncrona, filesystem e diagnostics |
| W-O064 | qual lowering e ABI de runtime implementam `async`/tasks? | state machines próprias · LLVM coroutines · MLIR Async como etapa | cancelamento, destruction de frames, debug, portability e custo de call |
| W-O065 | como declarar módulos multi-arquivo, init e módulos internos? | manifest determina files · declaração no source · convenção de diretório | ordem de init, rebuild incremental, partial modules e tooling |
| W-O066 | qual sistema de visibility, re-export e acesso de package existe? | export por declaração · blocos de export · package/internal sem `friend` | API review, refactor, tests, cycles e código gerado |
| W-O067 | imports entre módulos precisam formar DAG? | DAG estrito · SCC com interfaces separadas · cycles somente de tipos | init, resolução, compile time, cache e diagnostics |
| W-O068 | uma call de `ServiceRef` exige `await` mesmo no fast path local? | sempre async · overload local sync separado · efeito inferido por placement | refactor local→IPC, failure, latency visível e otimização sem mudança semântica |
| W-O069 | qual policy de mailbox define overload, fairness e batching? | aguardar espaço · erro tipado imediato · policy explícita por instância | memory bound, starvation, cancelamento, priority inversion e métricas |
| W-O070 | qual contrato une turn, transação durable e output gate? | turn automático transacional · transação explícita · adapter neutro com commit causal | crash recovery, retries, external effects, logs e SQLite/alternativas |
| W-O071 | qual é a failure/isolation/restart boundary default? | processo da app · processo por trust domain · isolate por serviço · deployment obrigatório | panic, seccomp, custo, state recovery e observabilidade de geração |
| W-O072 | como authority/capability é representada e transportada? | parâmetro tipado · referência de capability · grants no manifest + handle runtime | ambient authority, delegation, revocation, IPC e testabilidade |
| W-O073 | promise pipelining e capability RPC entram na v0? | protocolo stdlib posterior · runtime local/remote uniforme · apenas wRPC em Pesquisa | partial failure, cycles, lifetime remoto, backpressure e complexidade do core |
| W-O074 | qual contrato mínimo une I/O sync e async sem esconder blocking? | APIs distintas · interface comum com efeito observável · async-first + adapter explícito | ergonomia, targets sem event loop, FFI, cancelamento e custo |
| W-O075 | quais dados Unicode, locale e timezone acompanham a v0? | bundle mínimo versionado · provider registrável · pacote first-party separado | resultados reproduzíveis, tamanho, atualizações de segurança e targets embedded |
| W-O076 | como error sets de adapters preservam portabilidade sem congelar códigos do OS? | erro semântico + cause opaca · enum por adapter · wrapper de código nativo | matching, logs, ABI, evolução e diagnóstico cross-platform |
| W-O077 | quais clocks, deadlines, timers e fontes de aleatoriedade são padrão? | capabilities explícitas · context do host · globais std com substituição em teste | determinismo, segurança, virtual time, suspensão e builds reproduzíveis |
| W-O078 | qual contrato portátil de paths, filesystem e processos entra na stdlib? | tipos por capability · paths como bytes/Unicode por target · módulos first-party fora do core | Windows/POSIX, encoding, sandbox, cancelamento e efeitos |
| W-O079 | HTTP, TLS e networking pertencem à stdlib ou a pacotes first-party? | sockets portáteis no std · stack HTTP/TLS oficial · interfaces + adapters versionados | segurança, cadence de atualização, binary size, embedded e server workloads |
| W-O080 | quais collections, hashing e regras de iteração são observáveis? | ordem estável por tipo · hash randomizado por default · variantes determinísticas explícitas | DoS, reproducibilidade, serialization, paralelismo e generics |
| W-O081 | qual modelo de decimal e `Money` entra no primeiro corte? | decimal fixed-scale · decimal floating · `Money<Currency,Scale>` com rounding explícito | impostos, ABI, serialization, overflow e locale |
| W-O082 | qual modelo de arrays/tensors/views preserva shape e aliases? | `Array` core + pacote tensor · ranked types · memref-like views públicas | bounds, strides, mutation, SIMD/linalg, devices e FFI científica |
| W-O083 | quais mappings e overrides permitem gerar wrappers C seguros? | import de headers + adapter declarations · IDL W explícita · wrappers manuais primeiro | macros C, ownership, nullability, callbacks, variadics e diagnostics |
| W-O084 | quais linguagens e artefatos podem existir em `fn<lang>`? | somente C primeiro · frontends LLVM allowlist · source/objeto externo por adapter | parser/lowering disponível, ABI, exceptions, runtime, cache e provenance |
| W-O085 | como generics, layouts e symbols evoluem na ABI W? | ABI instável até 1.0 · interfaces resilientes · distribuição source-first | inlining, specialization, plugins, dynamic libs e cache binário |
| W-O086 | qual é a regra exata de newline, `;`, comments e forma canônica? | newline contextual · `;` sempre opcional/permitido · terminador explícito em casos ambíguos | parser recovery, formatter, diffs, formula layout e codegen por IA |
| W-O087 | quais profiles, features e conditional compilation são source-level? | manifest/profile somente · `when target(...)` limitado · feature flags tipadas | reprodutibilidade, dead code, API divergente e matriz de testes |
| W-O088 | como múltiplas versões e features de packages coexistem no grafo? | uma versão global · isolamento por package · unificação controlada de features | type identity, ABI, diamond dependencies, lockfile e binary size |
| W-O089 | qual é a unidade de compilação incremental e monomorphization? | módulo · arquivo · item; instâncias no consumidor ou pacote produtor | invalidation, cache CAS, parallel build, debug e distribuição binária |
| W-O090 | quais partes do toolchain pertencem ao bootstrap confiável? | bundle fixado completo · seed mínimo reconstruível · estágios diversos verificados | trusting-trust, disponibilidade, tamanho, cross-build e atualização |
| W-O091 | qual identidade, namespace e lifecycle governa registry, yanks e advisories? | nomes globais · namespaces por organização · identidade por chave | squatting, transferência, offline builds, revogação e UX |
| W-O092 | como licenses, policies organizacionais e exceções compõem? | metadata SPDX + policy externa · policy no manifest · attestations separadas | conflitos transitivos, auditoria, override autorizado e interoperabilidade |
| W-O093 | quem pode declarar `verified`, `diverged`, `revoked` ou `yanked`? | registry · threshold de builders · policy do consumidor | governança contestada, recovery, transparência e nenhum score enganoso |
| W-O094 | como editions, deprecation e source compatibility evoluem? | editions opt-in · versionamento sem editions · migrations automatizadas obrigatórias | parser, std implícita, packages, tooling e código longevo |
| W-O095 | qual contrato de testes integra unit, doc, property, fuzz e compile-fail? | runner único com modos · ferramentas separadas sobre manifest comum · apenas unit/doc na v0 | reprodutibilidade, diagnostics, coverage, isolamento e package trust |
| W-O096 | quais guarantees de custo o compilador pode afirmar estaticamente? | somente facts exatos · estimativas intervalares · profiles medidos anexados | não prometer runtime incerto, budgets, imports, LTO e CI reproduzível |
| W-O097 | qual contrato de property behaviors generaliza storage e accessors sem esconder efeitos? | `with Behavior(...)` + declaração própria · `var [behavior]` ao estilo SE-0030 · wrapper nominal · behaviors especiais no core | init vs set, ownership/drop, `modify`/exclusivity, efeitos de get/set, composição, layout, reflection, `self`, Sendable e lowering sem runtime obrigatório |

## Questões promovidas

| Questão original | Estado atual | Decisões candidatas |
|---|---|---|
| W-O043 | **Candidato** | W-C030 e W-C033–W-C035: sem `Any` público, existential explícito, witnesses mínimos e reflection por conformance/reachability |

## Pesquisa ativa

| Tema | Estado atual |
|---|---|
| tagged pointers para escalares/small values | otimização de target; nunca requisito sem representação fallback |
| heap/arena por módulo e `flush` | possível policy de região ou serviço; não lifecycle universal de imports |
| caller-allocated return universal | calling convention a comparar, não regra visível de todo valor |
| WC como linguagem intermediária pública | possível backend/diagnóstico via EmitC; MLIR é a baseline arquitetural |
| módulos singleton e `fork module` | pode reaparecer como `service`/`isolate`, separado de namespace |
| `fn<C>`, `fn<JS>`, `fn<Rust>` | ilhas de implementação da aplicação, via adapters de frontend após uma FFI C segura |
| WLO/WLON | candidato a literal/serialização canônica, fora do parser mínimo |
| wQL, wRPC e RestPC | contratos e bibliotecas do ecossistema, não keywords v0 |
| Computer Units e V6 | runtime/serverless separado |
| tree strings | estrutura especializada para interning/índices, não `String` geral |
| property behaviors | mecanismo tipado dedicado em W-O097, não retorno de annotations genéricas |
| SQLite como storage padrão | adapter oficial e possível storage de tooling, não semântica obrigatória |
| GPU, HDL, OpenMP, SIMD explícito | lowerings futuros depois do pipeline CPU nativo |
| snapshots, PGO e autotest por IA | tooling futuro sobre testes/documentação executável |
| highlighting e parser incremental | Tree-sitter/queries são a projeção estrutural mantida; TextMate é compatibilidade lexical e semantic tokens futuros pertencem a `wls`/HIR |
| unidades físicas, decimal, arrays e cálculo científico | corpus térmico do restaurante; representação, literal, reproducibilidade e lowerings continuam em W-O036/W-O037 |

## Rejeitado por enquanto

- Tratar `undefined`, `null`, `uninitialized` e `empty` como cases universais de todo tipo.
- Fazer `async` significar às vezes concorrência e às vezes paralelismo.
- Prometer que o runtime será “sem event loop”, “sem locks” ou “todo lock-free”; isso é estratégia de implementação, não benefício semântico.
- Dispensar lockfile em uma resolução transitiva reproduzível.
- Confiar em uma hash recebida pelo mesmo canal do artefato como modelo completo de segurança.
- Executar scripts de build/instalação com rede e filesystem irrestritos por padrão.
- Tornar três delimitadores de string e várias combinações de prefixos semanticamente equivalentes.
- Basear identidade de símbolo em hash sem detecção de colisão, metadata de nome e estratégia de evolução.
- Congelar ABI antes de provar tipos, ownership, errors e task lowering.

## Como uma decisão avança

Uma questão passa de **Em aberto** para **Candidato** quando tem exemplos
coerentes, semântica de erro/lifetime e um plano de lowering. Passa de
**Candidato** para **Direção** apenas depois de:

1. parser e formatter;
2. pelo menos dois programas não triviais;
3. um protótipo executável ou análise formal suficiente;
4. comparação com uma alternativa mais simples;
5. impacto em FFI, tooling e compatibilidade documentado.

Decisões futuras devem ganhar um arquivo curto em `design/decisions/` quando houver código para sustentá-las. Até lá, esta tabela é deliberadamente mais honesta que uma falsa especificação completa.
