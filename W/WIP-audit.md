# Auditoria semântica de `Y/WIP.MD`

> **Arquivo histórico · 21 de julho de 2026**
> **Fonte auditada:** blob Git `5b8f5e230ef63705173632fdad2de08a9fff4370`
> (`4.035` linhas)
> **Baseline comparada:** `W/` no commit `7505201`
> **Última reconciliação DB2:** 29 de julho de 2026

Este documento responde a uma pergunta diferente do inventário byte a byte: não
apenas “o arquivo foi preservado?”, mas **cada família de ideia ainda possui um
destino rastreável?** Ele é um índice de proveniência, não uma especificação. Os
estados vigentes continuam em [`W/DESIGN.md`](../../W/DESIGN.md).

## Método

A leitura foi feita integralmente, em ordem, e depois conferida por temas e links.
Como o caderno repete e contradiz a si mesmo, a unidade da auditoria é uma
**família semântica**, não cada frase. Para cada família, este registro aponta:

- as linhas históricas mais representativas;
- o que a fonte realmente explorava, inclusive alternativas incompatíveis;
- onde a intenção vive hoje;
- se a migração cobriu, cobriu parcialmente, rejeitou explicitamente ou deixou
  uma lacuna.

Os rótulos desta auditoria significam:

| Rótulo | Significado histórico |
|---|---|
| **Coberto** | existe destino atual que conserva a pergunta e as alternativas materiais |
| **Parcial** | o tema existe, mas uma alternativa ou dimensão relevante se perdeu |
| **Lacuna** | não havia questão atual capaz de receber a ideia sem reinterpretá-la |
| **Superado** | a alternativa foi comparada e está rejeitada/adiada de forma rastreável |
| **Histórico** | evidência ou sketch preservado, sem razão para virar feature atual |

“Coberto” não quer dizer “decidido” nem “implementado”.

### Controle de referências externas

Uma extração mecânica de URLs `http(s)` do blob encontrou **88 referências
únicas** após remover pontuação Markdown terminal. Todas as 88 ainda aparecem
verbatim em `Y/W/` ou na documentação atual de `W/`; portanto nenhum link do WIP
foi descartado nesta consolidação. Isso prova preservação textual, não que a URL
continue online, segura ou tecnicamente recomendada. A classificação temática
permanece em [historical-references.md](historical-references.md).

## Resultado executivo

A migração anterior preservou a maior parte das **intenções**, mas não foi
exaustiva nas **alternativas**. Quatro famílias exigiram reabertura explícita:

| Família recuperada | Evidência principal no WIP | Falha da migração anterior | Questão atual |
|---|---|---|---|
| domínios/grupos de execução | `470–472`, `723–734`, `2402–2450`, `2561–2624`, `2703–2776`, `3958–3999` | executors foram citados, mas o vínculo module/service/task e a distinção entre isolation, preference e affinity não foram comparados | `W-O100` |
| conjuntos de entrypoints e host profiles | `136–138`, `1224–1258`, `1481–1535`, `1718–1760`, `3167–3210` | profiles tipados foram afirmados sem uma superfície source ou alternativas de binding | `W-O101` |
| matrizes/tensors/ML | `1694`, `2157`, `3064–3069`, `3353–3362` e a intenção científica dispersa | `Tensor` apareceu apenas como item T2; literals, shapes, operadores, broadcasting, devices, autodiff e interchange não foram avaliados | `W-O102` + `W-O082` |
| aplicação de tipos, parâmetros de valor e “type modifiers” | `1549–1568`, `1678–1706`, `2778–2806`, `3824–3835` | refinements e newtypes foram definidos, mas a alternativa `Type<...>` histórica não foi classificada por semântica | `W/DESIGN.md` 3 e D2-097/114/138/200 |

Isso não invalida as escolhas H01–H14 já ratificadas. Invalida apenas a frase de
que elas cobriam **todas** as perguntas materiais do caderno. O addendum
publicável está no
[`arquivo DB1`](archive/db1-2026-07-27/DB1_ADDENDUM.md).

### Conferência contra a DB2 em 29 de julho de 2026

A conferência comparou todas as famílias deste mapa com `W/DESIGN.md`. Nenhuma
família ficou sem destino. Os itens abaixo exigiram atualização ou continuam
deliberadamente abertos:

| Intenção histórica | Destino DB2 | Estado |
|---|---|---|
| `CallbackType`, function pointer e ambiente capturado | 7.5 e D2-192–199 | separado em `fn`, `some fn` e `any fn` |
| named index e arrays estáticos de modifiers | 3.1 e D2-200 | `StaticList<T>` ordenada; índice runtime rejeitado |
| operador matricial | 17 e D2-201 | `@` fechado para ranks 1 e 2 |
| handlers arbitrários em patterns | 5.4 e D2-207 | **Pesquisa** até existir protocolo de pattern |
| `fork module`, hot reload e live patch | 23.6 | **Pesquisa** fora do caminho crítico |
| signals, mouse, keyboard e HID | 13.4 e 13.5 | slots de host tipados, sem keywords por evento |
| GPU, SIMD e HDL | 17, 18 e 23.6 | facts/SIMD na baseline; GPU e HDL por backend |
| GUI, TUI e immediate mode | 14.3 e profiles de host | T2; nenhuma GUI universal |
| dictionaries, hashing, arrays e sort | 16.10 e D2-226–241 | Map/Set insertion-ordered, full keys, views borrowed e stable sort |
| borrowed text, slices e read-only genérico | 9, 16.2, 16.10 e D2-417–423 | `ref` preserva o place; `view` substitui a família histórica `XView` nas projeções core |
| mobilidade entre tasks e domains | 12.7 e D2-424–429 | `transferable` separa move exclusivo de `shareable`; `Send`/`Sync` não viram protocols públicos |
| String storage, capacity e tree string | 16.1, 16.3–16.4, 16.9, 18.4 e D2-430–439 | W0 usa flat unique; SSO é invisível; COW baseline é rejeitado; estruturas indexadas ficam especializadas |

As formas históricas continuam neste arquivo e no caderno. A tabela registra
destino, não aprovação.

## Mapa integral por família

### Superfície, tipos e controle

| Família | Linhas representativas | Conteúdo histórico preservado | Destino atual | Cobertura |
|---|---:|---|---|---|
| funções, labels, retorno e destructuring | `1–82`, `3393–3399`, `3641–3654` | `fn`/`func`, labels Swift-like, retorno, closures e várias ordens de tipo/nome | `W-C001`, `W-C025`, `W-O040`, `spec/syntax.md` | **Coberto** |
| bindings, mutação e efeito local | `35–82`, `3656–3674` | `const`/`let`/`var`, `mut fn`, configuração de função | `W-C002`, `W-C026`, `W-O006` | **Coberto**; config genérica continua **Histórico** |
| “everything is enum” e representação uniforme | `229–429`, `907–1017`, `3911–3916` | enum como soma, base universal, boxed/tagged scalar e estados especiais | `W-C003/004/029/030`, `research/tagged-values.md` | **Superado** como semântica universal; representação segue **Pesquisa** |
| switch/pattern matching | `229–429`, `908–966`, `2080–2135`, `4021–4032` | múltiplos scrutinees, handlers/predicates em cases, binding e exhaustividade | `W-O021/022/038/041`, `spec/syntax.md` | **Parcial**: handler arbitrário permanece dependente de protocolo de pattern |
| guard e early return | `3699–3744` | formas curtas e bloco `else` | `W-C037`, `spec/syntax.md` | **Coberto** |
| operators customizados/opcionais | `2262–2288`, `3637–3640` | declaração de operadores e família `?+`, `?-`, etc. | `spec/types-and-memory.md` mantém APIs explícitas antes de novos operadores | **Superado** por enquanto; faltava apenas esta ligação histórica |
| ranges, progressões e multirange | `3404–3520` | quatro closures, step, produtor lazy, membership, bounds e zip de progressões | `W/DESIGN.md` 15.2 e D2-054/398 | **Coberto**; `MultiRange` vira composição/zip, não tipo assumido |
| promoção numérica, overflow e Unum | `3522–3541`, `3911–3931` | widening automático, intermediários maiores, float alternativo | `W/DESIGN.md` 15.1, 18 e D2-383–397 | **Coberto**; conversão segura substitui widening heurístico; Unum permanece **Pesquisa** |
| newtypes, refinements e limites | `1549–1568`, `1678–1706`, `2778–2806`, `3824–3835` | `String<...>`, bounds, masks, hints de storage, named index e `typeDef` | `W/DESIGN.md` 3, 8.6 e D2-200 | **Recuperado**; `StaticList<T>` preserva ordem sem criar índice runtime |
| captures, closure lifetime e callbacks | `805–835`, `1404–1477`, `2181–2210`, `3577–3635` | captures fracas/fortes/cópia/ref, callback contexto e escape | `W/DESIGN.md` 7.5, D2-192–199 e D2-208/209 | **Recuperado**; `fn`/`some fn`/`any fn` separam pointer, ambiente, owner e drop |
| protocols, inheritance e vtables | `3007–3063` | protocols Swift-like, associated storage, vtable C e composição | `W-C003/033/034`, `W-O050`, `spec/types-and-memory.md` | **Coberto** |
| comptime e type builders | `2860–2963`, `3226–3267` | avaliação compile-time, types construídos e profile data | `W-O013/051`, `design/compiler.md` | **Coberto**; builders arbitrários seguem **Pesquisa** |

### Módulos, execução e lifecycle

| Família | Linhas representativas | Conteúdo histórico preservado | Destino atual | Cobertura |
|---|---:|---|---|---|
| imports, includes, namespace e múltiplos arquivos | `83–163`, `1718–1842` | forms incompatíveis de import/include, módulos globais/internos e collision handling | `W-C011/012/023`, `W-O065–067`, `spec/modules.md` | **Coberto** |
| módulo como singleton/owner runtime | `129–228`, `510–590`, `1478–1547`, `2214–2245` | heap/stack/thread/lifecycle por módulo, `init/deinit/default`, resource budgets | `W-D011`, `W-O023–030`, `spec/modules.md`, `design/modules-and-runtime.md` | **Superado** como default; intenção migrou para `service`/instance |
| services, Computer Units e fine-grained compute | `765–910`, `1224–1258`, `2214–2245`, `2684–2690` | network/process/thread services, single-thread turns e units event-driven | `W-D014`, `W-C017/044`, `W-O023–027`, `research/README.md` | **Coberto** |
| entrypoints múltiplos e orientados a eventos | `136–138`, `1224–1258`, `1481–1535`, `1718–1760`, `3167–3210` | `main`, `fetch`, CLI/stdin, cron, mouse, keyboard, HID, selection pelo build | `W-O101`, `spec/modules.md` | **Lacuna → recuperada** |
| grupos/queues/executors por módulo ou call | `470–472`, `723–734`, `2402–2450`, `2561–2624`, `2703–2776`, `3958–3999` | `.main/.background/.network/.io/.UX`, serial/concurrent, QoS, affinity e `spawn<group>` | `W-O100`, `spec/concurrency.md` | **Lacuna → recuperada** |
| task groups e limites de fan-out | `532–543`, `577–590`, `2634–2645` | `.max(N)`, thread pool, bounded execution | `W-O060/061/069`, `spec/concurrency.md` | **Coberto** |
| async/await vs spawn/paralelismo | `430–509`, `667–751`, `1768–1772`, `3746–3752`, `3966–4019` | corrotina na mesma fila, trabalho paralelo em pool, joins e arrays de tasks | `W/DESIGN.md` 12.7, D2-424–429, `W-D003/004`, `W-C010/043` e `W-O055–064` | **Coberto**; mobilidade separa transfer de sharing; regra rígida “cross-module = thread” foi **Superada** |
| sync/yield/fork e module duplication | `2561–2624`, `2684–2699` | vocabulário `sync/yield/fork`, clone de módulo/estado | `W-O062`, `research/README.md` | **Superado** no core; `fork module` segue **Pesquisa** |
| COW, RCU, locks e “call police” | `2316–2555`, `2807–2816` | policies `.rwLock/.rcu/.cow`, atomicidade e proteção por object/module | `W-O024/055`, `research/long-term-program.md` | **Coberto** como estratégias, não modifiers universais |
| corrotinas, state machines e filas | `644–751`, `2817–2859`, `3268–3324`, `4003–4007` | protothreads, linked lists, frames e scheduler em C | `W-O064`, `design/compiler.md`, spikes históricos | **Coberto** como alternativas de lowering, não runtime escolhido |
| directives, config overrides e hot reload | `1936–1941`, `2148–2180`, `2668–2683` | `.debug`, remote reload, `#embed`, runtime config e dealloc | `W-O087/094`, `research/README.md` | **Superado** no caminho crítico; preservado como **Pesquisa** |
| signals e eventos do SO | `2634–2667`, `3325–3342`, `4034–4035` | signals para wakeup/cancel, IO backend, dispatch de eventos | `W-O063/074/077`, `W-O101` | **Parcial → entry adapter recuperado** |
| output/input gates e durable state | `3676–3686` e inspiração Durable dispersa | executar/reter efeitos perto da boundary e causalidade de outputs | `W-O070`, `design/modules-and-runtime.md` | **Coberto** como **Pesquisa** |

### Memória, layout, C e backend

| Família | Linhas representativas | Conteúdo histórico preservado | Destino atual | Cobertura |
|---|---:|---|---|---|
| ownership, caller/callee e ARC fallback | `430–590`, `1389–1477`, `1620–1717`, `4018–4019` | owner implícito, `ref/copy/transfer`, ARC em escapes, regions/module heap | `W/DESIGN.md` 9 e D2-021–028, D2-103–108 | **Coberto**; owner, placement e allocator são contratos separados |
| mimalloc, arenas e estimativa de recursos | `164–225`, `517–529`, `1542`, `1667` | min/max/current por import/call, arena e allocator alternativo | `W/DESIGN.md` 9.2.1, 9.5–9.6 e D2-405–413 | **Recuperado**; `Arena` T0, `region` lexical, budget lógico e profiles substituíveis |
| tagged pointers/values | `510–529`, `1390–1477`, `3911–3931` e spikes `Y/_w_/C` | bit stealing, small values, bounds e ARC metadata | `W/DESIGN.md` 9.7–9.10 e D2-335–345 | **Coberto** como otimização invisível com fallback |
| struct layout, alignment e packing | `753–764`, `1780–1904`, `3073–3081`, `3400–3402` | layout por módulo, padding, pragma pack e ABI | `W/DESIGN.md` 9.8–9.10 e D2-029, D2-335–345 | **Coberto**; layout W é opaco e C/schema permanecem explícitos |
| C/WC/EmitC/bootstrap | `610–666`, `1739–1904`, `2006–2213`, `3001–3006`, `3360–3365` | tradução C, dialect WC, compiler choice e escape manual | `W-C014/046`, `W-O009–011/090`, `design/compiler.md` | **Coberto** |
| `fn<lang>` e migração gradual | `1843–1904`, `2040–2077`, `3064–3069`, `3641–3653` | bodies C/JS/Rust/Zig/Bend e target GPU | `W-C047`, `W-O042/083/084`, `research/README.md` | **Coberto** |
| GPU, SIMD, OpenMP e HDL | `1694`, `2157`, `3064–3069`, `3353–3362` | kernels, accelerators, intrinsics e backends heterogêneos | `W-O082/084/099`, `research/README.md`, programa LT | **Parcial**; superfície tensor/ML recuperada em `W-O102` |
| zero-terminated arrays e C ergonomics | `3543–3552` | iteração sentinela e views de buffers C | `W-O083`, fronteira C | **Histórico**; não altera `Array` seguro |

### Texto, dados, ciência e ecossistema

| Família | Linhas representativas | Conteúdo histórico preservado | Destino atual | Cobertura |
|---|---:|---|---|---|
| strings, raw, multiline e interpolation | `1273–1388` | `#`, `$`, multiline, concat e várias combinações de delimitadores | `W/DESIGN.md` 16.1–16.6, D2-210–221 e D2-430–439 | **Coberto**; construção incremental pertence a String; `#`, `${}` e dedent foram mantidos; delimitadores equivalentes e concat implícita foram **Superados** |
| Unicode/ICU e tree strings | `1329–1361` e `TK/tree_string.md` | unidade de texto, indices e representação alternativa | `W/DESIGN.md` 16.1–16.2, 16.6, 16.9 e 18.4; D2-210–212, D2-221, D2-225, D2-417–423 e D2-430–439 | **Coberto**; `view String` substitui `StringView`; reads não criam cache lazy; tree string permanece especializada |
| tamanhos, capacity e storage de String | `1549–1604`, `1678–1709`, `2778–2806` | min/max/expected, limite lógico, mask e storage físico | `W/DESIGN.md` 8.6, 16.1, 16.4, 16.9 e 18.4; D2-224 e D2-430–439 | **Coberto**; W0 flat, SSO invisível e reserva mínima separam invariant lógico de layout; COW baseline é **Superado** |
| zero-terminated text e paths nativos | `3036–3048`, `3543–3552` e `Y/_w_/WC.MD:85–117` | `char8_t`, terminador, UTF-8 e fronteira C | `W/DESIGN.md` 16.7–16.8 e D2-222–223 | **Coberto**; `CString`/`Path` substituem sentinela em String/Array |
| SI, quantities e análise científica | intenções físicas dispersas, types limitados e GPU | unidades, valores precisos, integral/limite e otimização por bounds | `W/DESIGN.md` 15, 17–18 e D2-051–058/375–381/396–398 | **Coberto** |
| matrizes, tensors e ML | intenção científica/accelerators em `1694`, `2157`, `3064–3069`, `3353–3362` | falta de notação concreta, mas ambição de cálculo vetorial/accelerated | `W/DESIGN.md` 17–18 e D2-099–106/375–381/397 | **Lacuna de profundidade → recuperada** |
| collections, dictionaries e sort | `2860–3000` | dict/WLON, hashing linear, HH32/XXH64, descarte de keys, linked list/BST, TimSort/fluxsort/blitsort e busca | `W/DESIGN.md` 16.2, 16.10, D2-226–241, D2-417–423 e `research/legacy-spikes.md` | **Coberto**; `view Array<T>` substitui `Slice<T>` público; full key é obrigatória, Map/Set preservam inserção e o algoritmo de sort/hash não vira ABI |
| SQLite/storage | `3676–3696` | key/value conveniente, SQLite padrão e utilitário Unix | `W-O070`, `research/README.md`, `design/modules-and-runtime.md` | **Coberto** como adapter, não semântica universal |
| WLO/WLON e query syntax | `2860–2963` e trechos RPC/query anteriores | literal de dados, parse/stringify e SQL-like operations | `research/README.md`, `ecosystem/services-and-protocols.md` | **Coberto** como **Pesquisa** |
| wQL/wRPC/RestQL/V6 | `1927–1935`, `2006–2077`, `TK/*` | framing, RPC, query e Computer Units | `ecosystem/services-and-protocols.md`, `research/README.md` | **Coberto** |
| GUI/TUI/immediate mode | `2556–2560`, `3554–3556` | microui/Nuklear e stack C+GUI | T2 TUI e host profiles; `W-O101` para eventos | **Parcial → host events recuperados** |

### Tooling, build, packages e testes

| Família | Linhas representativas | Conteúdo histórico preservado | Destino atual | Cobertura |
|---|---:|---|---|---|
| package manager, mirrors e artefatos | `839–1218`, `3936–3949` | remote imports, tiers, source/static/dynamic, registry e auth | `W-C012/015/018/048`, `W-O088–093/098`, `design/packages.md` | **Coberto**; URL em source e no-lock foram **Superados** |
| builds reproduzíveis, snapshots e cache | `839–1218`, `2148–2180`, `2860–2963` | artifacts por hash, PGO/profile, snapshots e remote build | `W-D012`, `W-O012–020/089–096`, design de packages/releases | **Coberto** |
| docs, doctests, co-located tests e debug | `1905–2005`, `3366–3390`, `3565–3575` | docs inline, exemplos executáveis, `.test.w`, generated tests e benchmark IDE | `W-C049`, `W-O095/096`, `design/documentation-and-tests.md` | **Coberto**; autotest por IA segue **Pesquisa** |
| parser/Tree-sitter | `3343–3352`, `3754–3904` e `Y/_w_/grammar.js` | Tree-sitter como parser/tradutor e sketch de grammar | `W-C022`, `W-O007/008`, `tooling/tree-sitter-w` | **Coberto**; tradução semântica pela CST foi **Superada** |
| observabilidade, debug symbols e remote debug | `1936–1941`, `2148–2180`, `2860–2963` | source mapping, `.debug`, live patch e metrics | `W-C035`, `W-O032/094/096`, `ARCHITECTURE.md` | **Coberto**; hot reload segue **Pesquisa** |

## Reconciliação das lacunas com a DB2

Alguns destinos nas tabelas acima nomeiam IDs e arquivos da DB1 arquivada.
Esses nomes preservam a migração original. A tabela abaixo informa o destino
vigente das famílias que ainda estavam parciais.

| Família residual | Destino DB2 | Estado vigente |
|---|---|---|
| custom pattern handlers | `W/DESIGN.md` 5.4 e D2-207 | **Pesquisa**; guard e conversão nomeada são a baseline |
| signals e eventos do host | `W/DESIGN.md` 13.2 e 13.8 | adapters de entry cobrem signals; signal safety continua por target |
| GPU, SIMD, OpenMP e HDL | `W/DESIGN.md` 17, 18, 23.6, D2-093 e D2-368–381 | facts/SIMD/device têm corpus; GPU e HDL continuam **Pesquisa** |
| GUI, TUI e immediate mode | `W/DESIGN.md` 13.8 e 14.3 | host events e T2 preservam a pergunta; toolkit universal não é baseline |
| texto, bytes e strings nativas | `W/DESIGN.md` 16, D2-210–225 e D2-357–367 | **Líder DB2** com decoding, ownership e custos rastreados |
| collections, hashing e sort | `W/DESIGN.md` 16.10 e D2-226–241 | **Líder DB2**; Map/Set insertion-ordered, collisions com full equality, stable sort default |
| ausência, errors e cleanup | `W/DESIGN.md` 8.5, 11 e D2-242–259 | **Líder DB2**; Option, Result/throws, fault boundary e diagnostics estruturados |
| compile-time e type builders | `W/DESIGN.md` 3.6 e D2-260–279 | **Líder DB2**; const fn/init, ConstIR, quotas, materialização e CE0 |
| generics, protocols, enum subsets e typestate | `W/DESIGN.md` 8.6–8.7 e D2-280–306, 323–334 | **Líder DB2**; inference fechada, witnesses, case-sets e transições consuming |
| reflection, synthesis e rest | `W/DESIGN.md` 8.9 e D2-307–322 | **Líder DB2**; metadata opt-in, TypeId local e rest homogêneo |
| memória, arenas e allocator | `W/DESIGN.md` 9, 11.5 e D2-399–416 | **Líder DB2**; placement inferido, `pin`, `shared`, `Arena`, `region`, origem, budget e profiles medidos |

### Resultado da revisão de collections

O caderno propunha guardar somente HH32/XXH64 no lugar da key. Essa forma foi
**Superada**: hashes colidem, e equality precisa da key completa. W pode trocar
o algoritmo e a seed sem mudar source, storage persistente ou ordem de
iteração.

A linked list, a BST, linear hashing e B-tree permanecem técnicas possíveis
para collections especializadas. Nenhuma define `Map`. A baseline usa
complexidade observável e ordem de inserção; buckets e nodes são detalhes.
`UnorderedMap` preserva a pergunta histórica sobre uma variante mais barata,
mas continua **Pesquisa** até demonstrar ganho. O nome `HashMap` não é líder
porque `Map` também usa hashing.

O caderno também comparava Timsort, fluxsort, wolfsort, blitsort e rhsort.
`sort()` agora promete estabilidade e O(n log n), mas não um algoritmo.
`sortUnstable()` declara a troca de garantia. Um algoritmo nomeado só entra em
`std.algorithm` após licença, provenance, fuzzing e benchmark reproduzível.

Arrays terminados por zero continuam apenas na fronteira C. Safe `Array<T>` usa
count, capacity e bounds check. Named indices continuam como `StaticList<T>`
compile-time e não mudam a collection runtime.

### Resultado da revisão de ausência e falha

O caderno tratava `undefined`, `null`, `uninitialized` e estado movido como
possíveis estados universais. Essa forma foi **Superada**. `Option<T>` representa
somente ausência. Definite initialization e move são estados do compiler.
Storage bruto sem inicialização usa `unsafe MaybeUninit<T>`.

O caderno também propunha `Result` implícito somente em debug. Essa forma foi
**Rejeitada** porque mudaria a semântica entre debug e release. `Result<T, E>` é
um valor comum em todos os profiles. `throws E` oferece direct style. `try`
converte e propaga os dois sem depender de exception unwind do host.

A intenção de obter errors claros sem stack rewind foi preservada. O lowering
usa tagged results, control-flow edges e cleanup W. Um error return trace
opcional registra os pontos de propagação em sidecar. Ele não altera o programa.

`try?` foi recuperado como perda explícita de um error recuperável para Option.
Ele não captura panic ou cancelamento. `try!` e force unwrap continuam
rejeitados.

Os error codes, o mapping explícito e os testes inline do caderno continuam
presentes em typed Result, diagnostic codes e doctests. Operadores opcionais
como `?+` continuam em **Pesquisa**. O lowering de `defer` por um bloco comum de
cleanup continua possível, mas não define a source syntax.

O termo isolation boundary agora possui somente o sentido lógico de estado
serializado ou protegido. Panic exige uma fault boundary física, como process,
instância Wasm ou compartment com teardown próprio. Essa separação evita
prometer recovery in-process para um service que compartilha address space.

### Resultado da revisão de compile-time

O caderno propunha `comptime someFunc(...)` e conversão do resultado para WLO.
A intenção foi preservada com `const fn` e `const`. O evaluator produz
ConstValue tipado. Ele não serializa source nem reparseia WLO.

`comptime expression` continua como **Alternativa**. A baseline usa um binding
`const` porque ele exige avaliação e nomeia o resultado. `const fn` continua
callable em runtime e não consegue detectar a fase.

O caderno propunha `type(regex)` e funções que constroem tipos. Essa forma foi
**Rejeitada por enquanto**. Um tipo mantém identidade declarada. Uma `const fn`
de parse ou um refinement valida literals em compile time e inputs em runtime.

Static maps e tabelas continuam preservados. ConstValue guarda pares em ordem
lógica e não guarda hash ou layout. O backend pode escolher switch, tabela
ordenada ou perfect hash sem mudar a semântica.

A proposta `profile someFunc(...)` usava a execução anterior como novo literal.
Essa forma foi **Rejeitada por enquanto**. PGO pode orientar otimização somente
quando a recipe registra o profile. Ele não altera const, tipo ou interface.

As formas `#include`, `#if` e `#define` não entram em W. Target facts usam
`w.target`. Outros inputs usam módulos gerados por tool targets herméticas.
`#embed` continua **Pesquisa** no build, não authority do evaluator.

O seed C e o core W0 implementam CE0. Quotas de steps, heap, call depth e result
impedem loops infinitos e expansão sem limite. O evaluator usa a semântica do
target e nunca executa FFI ou código nativo do host.

### Resultado da revisão de generics e protocols

O caderno exigia generics capazes de substituir macros C especializadas. Essa
intenção foi preservada. O body generic é verificado uma vez. O backend pode
usar monomorphization, shared body ou witness sem mudar a semântica.

O caderno também estudava vtables dentro de structs e symbol hashes globais.
Essas formas não definem o layout W. Uma conformance possui witnesses externos
ao valor. Static dispatch pode eliminar a table. `any P` leva os witnesses
necessários quando o tipo concreto fica apagado.

Associated types não adicionam fields ao conformer. Um primary associated type
fica ligado a `Self` e recebe um `alias` witness explícito. Property behavior
continua o mecanismo para storage e accessors.

Conformance segue coherence nominal. O módulo do tipo ou do protocol pode
declará-la. Imports não escolhem implementação. Conditional conformances não
podem se sobrepor ou usar prioridade.

Labels de argumentos generic foram fechados por kind. Type arguments continuam
posicionais. `const` arguments usam labels. `const _` declara um slot primário
posicional, como o path ordenado de stages.

O path e o enum subset possuem contratos diferentes. `StagePath<[...]>` guarda
uma sequência compile-time. `ServiceStage<[...]>` restringe um valor a um
conjunto canônico de cases. O segundo contrato permite narrowing e switch
exaustivo sem criar wrapper runtime.

O subset também vale para enums de error. `throws ErrorEnum<[...]>` publica
somente as falhas possíveis e reduz o conjunto exaustivo de `catch`. Isso
recupera o valor dos enums fechados sem retomar a hipótese histórica de que
todos os tipos ou representações devem ser enums.

A revisão posterior separa três contratos que usam enum. Um retorno subset
publica somente os cases alcançáveis. Um parâmetro subset recusa estados nos
quais a operação não faz sentido. Um enum com payload mantém juntos o estado e
os dados exigidos por esse estado.

`StagePath<[...]>` não é um enum subset. Ele preserva uma sequência estática.
`ServiceStage<[...]>` normaliza um conjunto. O head define o significado de
`<[...]>`.

Um enum comum possui um case por vez. A forma `value in (.a, .b)` testa
alternativas. Flags simultâneas continuam em `Set` ou em um futuro tipo próprio.
Álgebra pública de case-set permanece como alternativa; a HIR pode usá-la para
flow analysis sem expor novos operadores no source.

A revisão de transições adiciona typestate sem keyword nova. Um argumento
`const` de enum identifica o estado de um owner local. Uma extension
especializada publica as operações daquele estado. `take fn` consome o owner
anterior e devolve o novo owner.

Esse modelo não substitui o enum runtime. State persistido, compartilhado ou
observado por `ServiceRef` continua em storage. Uma API de service usa snapshot
revisionado quando o caller precisa condicionar a próxima transição.

`StagePath<[...]>` agora valida os edges por uma `const fn`. O primeiro edge
inválido deve produzir um diagnostic. Uma `StateGraph<E>` declarativa permanece
em **Pesquisa** para diagramas e análise de reachability.

### Resultado da revisão de reflection, synthesis e rest

O DB1 já separava witness metadata, `TypeId` local e reflection estrutural.
Também exigia conformance opt-in sem annotations. A DB2 recupera esse contrato
em `std.reflect`.

Tooling usa interfaces e HIR. Runtime recebe somente descriptors alcançáveis de
tipos `Reflectable`. Debug symbols continuam separados. Fields privados,
offsets e dynamic construction não entram no descriptor.

Conformance no type head também solicita synthesis de uma família fechada. O
compiler conhece `Equatable`, `Hashable`, `Duplicable` e `Reflectable` por
identidade. User macros e `@derive` continuam rejeitados.

O WIP usava varargs em opções de import. Essa ocorrência preservava a intenção
de uma lista compacta, mas não definia ownership ou ABI. A DB2 usa `T...` para
rest homogêneo e `each values` para expansão.

`each` evita a colisão entre spread postfix e o range unilateral `4...`.
Heterogeneous packs, generic associated types e typed property paths continuam
em **Pesquisa**.

## Alternativas que não devem desaparecer de novo

As seguintes formas não são candidatas atuais, mas precisam continuar
pesquisáveis porque registram uma intenção humana legítima:

- module-level default queue e `spawn<.background>`;
- `export { fn ... }` e `entry { ... }` com handlers declarados no bloco;
- `String<length: ...>` e modifiers agrupados por `;`;
- `[a, b; c, d]` como literal matricial futuro, além de nested arrays;
- handler/predicate arbitrário em `switch`;
- `MultiRange` como zip de progressões;
- capture list `|copy/ref/take/weak ...|`;
- `sync`, `yield` e `fork module`;
- optional operators como `?+`;
- config de função `fn<config>` distinta de `fn<lang>`;
- `comptime expression` e `const { ... }` para pipelines sem binding;
- named type arguments, implicit existential opening e generic associated types;
- `Type<T>`, `\Type.property` e heterogeneous parameter packs;
- autotest gerado, PGO, snapshots e live debug;
- WLO/WLON, SQL-like queries, wRPC e Computer Units.

Preservar não significa reservar tokens. Uma alternativa só volta à superfície
quando tiver semântica, erro, ownership, efeito, lowering, teste e comparação
contra o candidato atual.

## Regra para as próximas migrações

Uma futura auditoria do WIP não precisa reler 4.035 linhas se este blob continuar
o mesmo. Ela deve:

1. conferir `git hash-object -- Y/WIP.MD` contra o blob no cabeçalho;
2. revisar apenas novos commits ou famílias marcadas **Parcial/Lacuna**;
3. atualizar primeiro este crosswalk histórico;
4. registrar uma decisão ou alternativa em `W/DESIGN.md` antes de apresentar uma nova forma
   como candidata no Book, no restaurante ou na grammar;
5. nunca usar a repetição de uma ideia no caderno como voto decisório.
