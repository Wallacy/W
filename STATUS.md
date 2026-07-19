# Status e registro de decisões

> **Working Draft · 19 de julho de 2026**

Este documento impede que uma hipótese exploratória seja lida como promessa. Ele
é o índice de maturidade do design, não uma especificação formal.

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
| W-C013 | `foreign c` declara a fronteira C; corpos `fn<lang>` ficam para depois | primeiro resolve ABI, ownership e erros de uma linguagem externa bem suportada |
| W-C014 | frontend preserva semântica num dialeto W/MLIR antes do lowering | ownership, tasks e efeitos não somem cedo num C intermediário |
| W-C015 | source + artefatos reproduzíveis/content-addressed | static libs são preferíveis quando compatíveis, não a única forma de distribuição |
| W-C016 | baseline do primeiro protótipo: prelude pequena, fixa e pura; `io` explícito | lookup não muda quando a stdlib cresce e authority de host permanece visível; a ergonomia final segue em W-O026 |
| W-C017 | baseline do primeiro protótipo: instância `service` explícita com handler serial e turn fechado | estado e eventos têm lifecycle; o default final de reentrância segue em W-O024 |
| W-C018 | registry governa metadata assinada; hosts/CDNs são mirrors de bytes por digest | trocar o mirror não troca identidade nem trust root |
| W-C019 | payload inclui nota W mínima; attestations e envelopes permanecem externos | evita assinatura autorreferente e separa reprodução, autorização e platform signing |
| W-C020 | lens de import mede delta de artefato/reachability e separa custos de instância e operação | mantém `import` estático, evita dupla contagem e coloca o custo perto da decisão que o introduziu |
| W-C021 | `enum E: Error` forma um error set fechado; `try` só propaga o mesmo `E` sem conversão | composição de errors diferentes permanece explícita enquanto W-O033 estiver aberta |
| W-C022 | tooling mantém Tree-sitter + queries como projeção estrutural; TextMate é compatibilidade do VS Code e o scanner do portal é temporário | evita três grammars permanentes sem encerrar W-O007/W-O008 nem confundir CST com tradução semântica |

## Questões abertas prioritárias

| ID | Questão | Alternativas a prototipar | Teste de decisão |
|---|---|---|---|
| W-O001 | `async` e `throws` ficam após o retorno ou antes de `fn`? | `fn f(): T async throws E` · `async fn f(): T throws E` | leitura, parser, autocomplete e tipos de função |
| W-O002 | qual é o algoritmo exato de move implícito? | last-use move · move sempre explícito para `object` · partial field move vs destructuring prévio | mensagens em código real, número de anotações e preparo concorrente dos ingredientes do restaurante |
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
| W-O026 | `io.print` merece exceção na prelude pura? | import explícito · prelude por profile · açúcar fixo | clareza de authority, scripts pequenos, autocomplete e colisões |
| W-O027 | “nanoservice” é nome público e qual é sua unidade mínima? | apenas lente interna · nome de `service` · handler stateless também | state/lifecycle/capability que justifique boundary sem feature soup |
| W-O028 | qual é o primeiro corte do lens de recursos? | somente import/artefato · import + calls do restaurante | precisão, latência incremental e utilidade antes de existir runtime |
| W-O029 | qual budget valida primeiro o modelo? | payload final · baseline por instância · peak por request | dado reproduzível, ação de recovery e valor em CI |
| W-O030 | onde declarar contratos de recursos? | source annotations · manifest/profile · metadata inferida/gerada | legibilidade, ABI, verificação independente e custo sintático |
| W-O031 | qual workload complementa o restaurante? | parser · servidor HTTP · imagens | recursão, I/O, FFI, fanout e alocação com formas diferentes |
| W-O032 | como perfis medidos entram em revisão e CI? | apenas local · anexo redigido · corpus público reproduzível | privacidade, portabilidade e não confundir p95 com teto |
| W-O033 | como compor error sets diferentes sem esconder controle? | `catch` explícito · conversão `from` declarada · injeção única inferida | ambiguidade, ergonomia, ABI e diagnóstico no restaurante |

## Pesquisa ativa

| Tema | Estado atual |
|---|---|
| tagged pointers para escalares/small values | otimização de target; nunca requisito sem representação fallback |
| heap/arena por módulo e `flush` | possível policy de região ou serviço; não lifecycle universal de imports |
| caller-allocated return universal | calling convention a comparar, não regra visível de todo valor |
| WC como linguagem intermediária pública | possível backend/diagnóstico via EmitC; MLIR é a baseline arquitetural |
| módulos singleton e `fork module` | pode reaparecer como `service`/`isolate`, separado de namespace |
| `fn<C>`, `fn<JS>`, `fn<Rust>` | adapters/toolchain plugins após uma FFI C segura |
| WLO/WLON | candidato a literal/serialização canônica, fora do parser mínimo |
| wQL, wRPC e RestPC | contratos e bibliotecas do ecossistema, não keywords v0 |
| Computer Units e V6 | runtime/serverless separado |
| tree strings | estrutura especializada para interning/índices, não `String` geral |
| SQLite como storage padrão | adapter oficial e possível storage de tooling, não semântica obrigatória |
| GPU, HDL, OpenMP, SIMD explícito | lowerings futuros depois do pipeline CPU nativo |
| snapshots, PGO e autotest por IA | tooling futuro sobre testes/documentação executável |
| highlighting e parser incremental | Tree-sitter/queries são a projeção estrutural mantida; TextMate é compatibilidade lexical e semantic tokens futuros pertencem a `wls`/HIR |

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
