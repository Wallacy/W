# Roadmap incremental do W

Este roadmap transforma o [norte imediato](README.md#norte-imediato) numa
sequência de provas executáveis. Ele não promete datas nem mede progresso por
quantidade de features. A medida é uma fatia vertical cada vez mais fiel:

```text
baseline de design fechada
  -> 10–20 programas/goldens
  -> gramática, formatter e diagnósticos
  -> AST e HIR tipada
  -> dialeto W/MLIR
  -> LLVM IR e executável nativo
  -> ownership, efeitos e cleanup
  -> tasks estruturadas
  -> módulos, builds e packages locais
```

Os estados deste documento usam somente o vocabulário de
[STATUS.md](STATUS.md): **Direção**, **Candidato**, **Em aberto**, **Pesquisa** e
**Rejeitado por enquanto**. Um gate aprovado pode promover uma escolha em
`STATUS.md`; o roadmap não promove decisões por si só.

## Princípios de execução

- **Direção:** crescer por programas positivos e negativos, preservando
  semântica de alto nível até os verificadores correspondentes rodarem.
- **Direção:** combinar trabalho top-down, começando na experiência de source e
  nos diagnósticos, com trabalho bottom-up, começando em target, runtime mínimo,
  ABI e verificadores. As duas trilhas se encontram em cada critério de saída.
- **Direção:** manter um caminho baseline portável antes de qualquer layout,
  scheduler ou backend otimizado.
- **Direção:** fechar a [Baseline de Design 1](DESIGN_CLOSURE.md) antes de
  avançar o frontend, AST/HIR, MLIR ou runtime; somente corpus e spikes
  delimitados podem ser usados para decidir hipóteses durante esse fechamento.
- **Candidato:** um seed W-owned no profile C11 portátil `w-seed-c` produz cedo o
  compilador self-hosted; C++/TableGen pode implementar o core MLIR atrás de ABI
  C estreita. Bun fica restrito ao portal atual. A divisão se prova por integração
  real, como descrito na
  [arquitetura do compilador](design/compiler.md#bootstrap-e-fatias-verticais).
- **Gates de protótipo:** parser normativo, fronteira do core MLIR, ABI de errors,
  memória compartilhada, lowering de tasks, EmitC e formato de módulos não são
  premissas escondidas.
- **Rejeitado por enquanto:** congelar ABI antes das provas semânticas e fazer um
  spike executável contar como implementação conforme.

## Caminho crítico e dependências

| Fase | Prova principal | Depende de | Desbloqueia |
|---|---|---|---|
| -1. Baseline de design | todas as decisões materiais da v0 classificadas e coerentes | visão, inventário e experimentos de decisão | implementação sem premissas escondidas |
| 0. Contrato por corpus | 10–20 programas e negativos correspondentes | DB1 e especificações candidatas | gramática e oracles estáveis |
| 1. Frontend explicável | CST/AST, formatter e diagnósticos recuperáveis | fase 0 | HIR tipada |
| 2. Núcleo semântico | resolução, tipos e HIR verificável | fase 1 | dialeto W mínimo |
| 3. Executável nativo | W/MLIR -> LLVM -> objeto/link | fase 2 | provas runtime e ABI |
| 4. Ownership e efeitos | move/borrow/drop/errors corretos em todos os caminhos | fase 3 | captures e frames seguros |
| 5. Tasks estruturadas | concorrência, paralelismo e cancelamento conformes | fase 4 | workloads assíncronos reais |
| 6. Módulos e packages | build hermético, lock e cache local | fases 3–5 | distribuição experimental |
| 7. Consolidação | pipeline reproduzível e superfície limpa | fases 0–6 | decisão humana sobre extração |

As fases 0–7 só avançam depois da DB1. Elas ainda podem revelar que uma decisão
precisa ser reaberta, mas não escolhem semântica ou sintaxe por conveniência de
implementação. Durante a fase -1, o corpus existente é laboratório de design e
permanece congelado salvo quando um experimento delimitado exige comparações.

## Fase -1 — Fechamento da baseline de design

**Status:** **Direção**.

O mapa canônico de trabalho é
[DESIGN_CLOSURE.md](DESIGN_CLOSURE.md). As fatias S0–S15 cobrem source,
semântica, representação, runtime, stdlib, compilador, packages, distribuição e
tooling em ordem de dependência.

H01–H14 já foram ratificados. A auditoria integral do caderno histórico encontrou
quatro omissões e mantém esta fase aberta em [DB1_ADDENDUM.md](DB1_ADDENDUM.md):
W-O100 execution domains, W-O101 host entries, W-O102 tensor/ML e W-O103 value
parameters/refinements. Nenhuma delas entra na grammar apenas por já possuir um
sketch de comparação.

### Top-down

- Escrever os mesmos casos em duas ou três alternativas quando a decisão for de
  superfície, sem promover a preferida apenas porque já aparece no restaurante.
- Revisar legibilidade, surpresa, densidade de tokens, capacidade de ensinar e
  coerência com “Prazer para humanos. Clareza para máquinas.”.
- Consolidar a forma escolhida no tour somente depois de registrar seu estado.

### Bottom-up

- Para cada alternativa, descrever tipos/HIR, lowering, efeitos, lifetime,
  falhas, custo, FFI, portability e evolução suficiente para detectar uma
  promessa inviável.
- Construir somente spikes mínimos com oracle diferencial quando análise não
  bastar: representação de valores, ownership, async lowering, ABI e
  reprodutibilidade são exemplos legítimos.
- Preservar um fallback portável para toda otimização target-specific.

### Critérios mensuráveis de saída

- todas as questões materiais têm ID em `STATUS.md` ou foram deliberadamente
  fundidas/rejeitadas com justificativa;
- o crosswalk histórico cobre integralmente o blob de `Y/WIP.MD` e toda família
  marcada parcial/lacuna ganhou destino explícito;
- cada fatia S0–S15 cumpre os dez critérios de fechamento da DB1;
- nenhuma feature necessária à v0 permanece apenas como **Pesquisa**;
- exemplos, regras negativas e esboços de lowering não entram em contradição;
- a revisão final da superfície não encontra sinônimos ou custos ocultos sem uma
  decisão explícita;
- o usuário aprova a DB1 como base para retomar a fase 0/1.

## Fase 0 — Contrato por corpus

**Status:** **Direção**.

**Evidência candidata atual:** o [corpus de contrato](corpus/README.md) contém
12 positivos, 11 negativos, snapshots versionados e cinco contratos
`executable`. O runner cobre toda família positiva com um negativo e confirma a
mesma CST em duas execuções. A fase permanece aberta até revisão humana dos
programas, outputs e diagnósticos desejados; formatter e diagnostics gerais já
pertencem ao gate da Fase 1.

### Top-down

- Selecionar 10–20 programas dourados pequenos que cubram `main`, bindings,
  funções, controle, tipos básicos, opcionais, errors, ownership explícito e a
  forma superficial de `async let`/`spawn let`.
- Usar [O restaurante W](examples/restaurant/README.md) como programa-guia
  top-down, mas decompor cada regra em goldens pequenos; o cenário narrativo não
  vira um único teste monolítico nem declara sintaxe de **Pesquisa** como pronta.
- Para cada construct aceito, incluir ao menos um caso negativo com diagnóstico
  esperado e uma intenção semântica curta.
- Separar no corpus o subset executável da fase 3 das construções que ainda são
  apenas contratos de frontend.

### Bottom-up

- Definir um runner determinístico e formatos versionados para tokens, CST,
  AST/HIR e diagnostics.
- Reutilizar as fixtures de [tooling](../../../tooling/README.md) no corpus; highlighting
  pode tolerar código incompleto, mas não cria um segundo oracle sintático.
- Registrar saída observável esperada onde houver execução; não usar snapshots
  de IR como substituto de comportamento.
- Fazer cada exemplo apontar para a regra candidata em
  [sintaxe](spec/syntax.md), [tipos e memória](spec/types-and-memory.md) ou
  [concorrência](spec/concurrency.md), e para [módulos](spec/modules.md) quando
  introduzir uma instância ou call entre serviços.
- Extrair `///`, fences e `test ... for` segundo
  [documentação e testes](design/documentation-and-tests.md), sem executar nada
  durante import ou build release.

### Critérios mensuráveis de saída

- entre 10 e 20 programas positivos revisados, todos com resultado ou snapshot
  esperado;
- ao menos um negativo por família sintática presente no corpus;
- 100% dos exemplos classificados como `frontend-only` ou `executável`;
- runner produz o mesmo resultado em duas execuções consecutivas com os mesmos
  inputs;
- nenhuma construção do subset executável da fase depende de regra marcada
  apenas como **Pesquisa**; módulos exploratórios do restaurante permanecem
  `frontend-only` até a alternativa ser promovida.

## Fase 1 — Gramática, formatter e diagnósticos

**Status:** **Candidato** até o corpus provar a implementação escolhida.

### Top-down

- Escrever a EBNF de trabalho junto dos exemplos, incluindo precedência,
  trivia, separação de statements e recovery points.
- Definir diagnósticos estruturados com código, severidade, span primário,
  labels secundários e sugestão aplicável quando segura.
- Fazer o formatter preservar comentários e produzir uma forma canônica sem
  apagar a intenção de efeitos ou ownership.

### Bottom-up

- Implementar lexer e parser recuperáveis com CST serializável e locations em
  UTF-8/source offsets definidos.
- Comparar parser próprio e Tree-sitter apenas se houver duas implementações
  úteis; o corpus, não a ferramenta, é o oracle.
- Adicionar fuzz/property tests ao lexer/parser e round-trip aos casos aceitos.

### Critérios mensuráveis de saída

- 100% do corpus positivo parseia sem erro e 100% dos negativos falha no ponto
  esperado sem crash;
- formatter é idempotente em todo o corpus e preserva comentários nos casos
  dedicados;
- cada erro sintático do corpus possui código estável, span válido e mensagem
  sem depender de stack trace interno;
- parse -> format -> parse preserva a árvore semântica de todos os positivos;
- um arquivo com pelo menos três erros independentes recupera e reporta todos
  sem cascata ilimitada.

## Fase 2 — AST, resolução e HIR tipada

**Status:** **Direção** para a camada; operações e algoritmos são
**Candidato** ou **Em aberto** conforme [STATUS.md](STATUS.md).

### Top-down

- Baixar a CST para uma AST sem trivia e produzir HIR explícita para scopes,
  bindings, calls, controle, tipos e efeitos declarados.
- Resolver nomes e imports lógicos sem rede nem resolução de package implícita.
- Implementar primeiro scalars, `let`/`var`, funções, branches e loops; manter
  opcionais, errors, ownership e tasks representáveis antes de verificá-los por
  completo nas fases seguintes.

### Bottom-up

- Tornar CFG, definite initialization, tipos de expressão e locations
  inspecionáveis em snapshots determinísticos.
- Definir verificadores da HIR que rejeitem referências pendentes, tipos
  inconsistentes e edges de controle malformadas.
- Manter schema e versão do produtor em qualquer serialização; caches continuam
  descartáveis.

### Critérios mensuráveis de saída

- todo programa positivo da fase síncrona tem AST e HIR tipada determinísticas;
- negativos cobrem nome inexistente, duplicação, mismatch de tipo, caminho sem
  inicialização e retorno incompatível;
- nenhum nó HIR aceito fica sem tipo, scope, source location ou regra explícita
  de controle;
- verifier rejeita fixtures HIR malformadas sem depender do frontend;
- mudanças de schema invalidam snapshots/caches de modo explícito.

## Fase 3 — Dialeto W/MLIR até LLVM/native

**Status:** **Direção**. A implementação do core e backends auxiliares seguem
**Em aberto**.

### Top-down

- Definir o menor dialeto W que represente funções, valores, calls e controle do
  subset síncrono sem antecipar layout de features futuras.
- Preservar source locations e produzir dumps legíveis antes/depois de cada
  passe.
- Ligar um `main` W e uma função mínima de runtime/`foreign c` para saída.

### Bottom-up

- Implementar types/ops, parse/print e verifiers do dialeto W.
- Baixar W para dialetos MLIR de apoio, LLVM dialect, LLVM IR, objeto e
  executável, com target triple e configuração registrados.
- Definir panic e overflow de modo idêntico em debug/release; executar verifier
  após cada fronteira de lowering.

### Critérios mensuráveis de saída

- ao menos dois programas não triviais do corpus compilam, linkam e rodam
  nativamente;
- debug e release têm a mesma saída, resultado, panic e overflow observáveis;
- zero operações W permanecem quando o módulo entra no lowering LLVM;
- cada passe possui ao menos um teste positivo e um fixture inválido rejeitado;
- locations de um erro backend/runtime chegam a arquivo e linha W;
- a execução LLVM concorda com o oracle de referência para todo o subset
  executável.

## Fase 4 — Ownership, memória, errors e efeitos

**Status:** baseline de owner único e efeitos explícitos é **Candidato**;
shared ownership, algoritmo de moves e ABI de typed errors são **Em aberto**.

### Top-down

- Ampliar o corpus com structs/enums/`T?`, narrowing, `throws E`/`try`,
  `ref`/`inout`/`take`/`copy`, `defer` e uma fronteira C com buffer/callback.
- Exigir diagnósticos específicos para use-after-move, borrow que escapa, alias
  mutável, valor não inicializado e cleanup impossível.
- Tornar efeitos relevantes parte do tipo e visíveis no call site segundo a
  baseline candidata.

### Bottom-up

- Representar move, borrow, exclusividade, drop, cleanup e edges de error no
  dialeto W antes de qualquer lowering destrutivo.
- Implementar fallback de layout portátil e ABI C com ownership documentado;
  otimizações são comparadas contra esse baseline.
- Exercitar cleanup em retorno normal, erro, early return e panic conforme o
  profile escolhido.

### Critérios mensuráveis de saída

- suite negativa rejeita todos os casos deliberados de use-after-move, borrow
  escapando e alias mutável do corpus;
- contadores/instrumentação confirmam exatamente um cleanup por recurso owned em
  todos os caminhos testados;
- positivos e negativos de effects produzem resultado ou diagnóstico idêntico
  antes e depois do lowering;
- harness C separado troca scalar, struct, buffer e callback sem allocator
  desconhecido nem ownership implícito;
- baseline de layout funciona sem tagged/niche representation;
- sanitizer disponível no target não encontra leak, double free ou invalid
  access nos testes end-to-end.

## Fase 5 — Tasks estruturadas

**Status:** semântica base e nomes `Send`/`Sync` são **Candidato**; lowering e
binding de execution domains de W-O100 são **Em aberto**.

### Top-down

- Implementar `async let`, `spawn let`, `await`, join/cancel e captures mantendo
  a distinção entre concorrência e paralelismo.
- Comparar executor default, `async/spawn on domain` e service/entry isolation
  com UI serial, I/O concorrente e CPU paralelo, sem default runtime por módulo.
- Adicionar um downloader/servidor pequeno, um workload CPU e negativos para
  child não consumido, capture inválida e mutação compartilhada.
- Expor task ID, parent scope e source location de suspension em diagnostics e
  tracing.

### Bottom-up

- Preservar scope, parent/child, effects, ownership e cancellation edges no
  dialeto W até seus verificadores rodarem.
- Implementar scheduler determinístico de teste, executor concorrente,
  pool paralelo limitado, timer e um adapter de I/O.
- Comparar lowering por MLIR Async, LLVM coroutines e state machines/runtime ABI
  no gate de concorrência, usando os mesmos testes semânticos.

### Critérios mensuráveis de saída

- nenhum teste deixa child vivo após a saída do scope;
- cleanup de frames ocorre uma vez em sucesso, erro e cancelamento;
- scheduler determinístico reproduz ordem e falhas com a mesma seed;
- `async let` passa num executor de uma thread e `spawn let` passa com pool
  limitado, sem criar uma thread por task;
- crossing de isolation registra hop e rejeita capture não-`Send`; remapear
  executor no host não muda ordering nem semântica;
- type checker rejeita todas as captures não-sendable e aliases mutáveis
  paralelos presentes na suite;
- downloader/servidor e workload CPU encerram limpos e exibem árvore de tasks
  completa no trace.

## Fase 6 — Módulos, builds e packages locais

**Status:** toolchain/supply chain first-party é **Direção**; schemas e detalhes
do [sistema de packages](design/packages.md) são **Candidato** ou **Em aberto**.

### Top-down

- Definir projeto local mínimo, imports lógicos e mensagens que expliquem toda a
  cadeia de resolução sem acesso de rede implícito.
- Implementar `resolve`, `fetch`, `build --locked`, `explain` e operações de
  cache apenas para as fontes necessárias à fatia vertical.
- Manter packages de aplicação, runtime e biblioteca sob a mesma semântica de
  módulos/ABI, sem congelar distribuição binária prematuramente.

### Bottom-up

- Versionar experimentalmente interface de módulo, metadata ABI, manifesto e
  lockfile; registrar produtor, target, toolchain e inputs semânticos.
- Implementar resolução determinística, lock obrigatório, cache
  content-addressed e build de fonte em sandbox sem rede.
- Associar debug/source metadata e provenance ao artefato produzido.

### Critérios mensuráveis de saída

- um projeto com ao menos duas dependências locais resolve e compila somente com
  lock atual;
- duas resoluções dos mesmos inputs geram lock byte a byte idêntico;
- build offline com cache aquecido passa, e cache miss offline falha com
  diagnóstico explícito;
- alterar source, interface, compiler/runtime, target ou opção semântica invalida
  somente as chaves dependentes esperadas;
- dois builds com inputs capturados produzem grafo e digests idênticos; qualquer
  alegação de artefato bit a bit é testada separadamente;
- nenhuma etapa executa script privilegiado ou instala dependência do sistema.

## Fase 7 — Consolidação e possível extração

**Status:** consolidação é **Direção**; extração para repositório dedicado é uma
decisão futura e explícita do usuário.

Extração não é gate da linguagem e pode acontecer antes desta fase sempre que o
usuário considerar melhor. Antes de um agente propô-la por conta própria, convém
que pelo menos estes critérios de handoff estejam satisfeitos:

- a estrutura canônica de documentação e o estado real da implementação estão
  identificados, mesmo que várias fases continuem abertas;
- checks de links, formatter, testes direcionados e `git diff --check` passam;
- arquivos gerados, dumps, caches, binários e experimentos abandonados estão
  fora da árvore versionada;
- dependências com outros diretórios do monorepo estão inventariadas e foram
  removidas, vendorizadas legitimamente ou convertidas em dependências
  explícitas;
- provenance e instruções de build/teste permitem clonar o estado atual
  sem conhecimento tácito do monorepo.

Cumprir esses critérios não move arquivos automaticamente. O usuário decide se
e quando W será extraído; até essa decisão, W continua incubado em `W/`.

## Gates de decisão

### Gate M — memória e ownership compartilhado

**Status:** **Candidato**, com cobertura da ABI ainda a medir.

Executar depois da fase 3 e antes de tornar shared state necessário às tasks.
Comparar owner único + borrows, `shared T`/ARC, regiões e owner de serviço nos
mesmos casos: buffer FFI, request com arena, grafo com ciclo e pipeline
cancelável. A decisão exige:

- regras de escape/ciclo e cleanup demonstradas por positivos e negativos;
- custo medido de allocations, bytes de metadata e operações atômicas;
- comportamento definido em FFI, error e cancelamento;
- uma baseline portável sem depender de pointer tagging.

Se nenhuma alternativa cobrir os casos com semântica pequena, shared ownership
permanece fora do primeiro slice em vez de ser inferido silenciosamente.

### Gate C — lowering e runtime de concorrência

**Status:** **Em aberto**.

Comparar MLIR Async, LLVM coroutines e state machines/runtime próprio depois de a
HIR e o dialeto W expressarem todas as invariantes. A escolha precisa passar a
mesma suite de parent/child, typed errors, cancellation, cleanup, captures e
debug, e registrar:

- tamanho de frame e número de allocations nos workloads de referência;
- qualidade das source locations e stack lógica;
- suporte real nos targets do bootstrap;
- complexidade do runtime e estabilidade da ABI interna.

Nenhum benchmark isolado pode substituir correctness dos scopes.

### Gate B — backends e implementação do core

**Status:** **Em aberto**.

- O compilador W usa uma ABI C estreita para o core C++/TableGen do MLIR; o
  protótipo mede cobertura das APIs de dialect/pass, pin/build do LLVM,
  portabilidade e tempo de iteração no pipeline mínimo.
- O seed `w-seed-c` é testado com Clang, GCC e MSVC. Dependable C informa a matriz
  de compatibilidade, sem autorizar UB nem substituir o profile W por C89.
- LLVM/native é a baseline de **Direção**.
- EmitC/C é avaliado somente num subset síncrono enumerado, como backend de
  inspeção, oracle ou portabilidade. Ele avança se preservar semântica, produzir
  C legível e justificar o custo de dois caminhos.
- Qualquer backend novo deve executar os mesmos goldens e negativos, preservar
  locations e declarar capabilities/fallbacks por target.

## Trilha paralela visual e de tooling

**Status:** **Direção** para tooling explicável; itens individuais começam como
**Candidato**.

Esta trilha fica congelada durante a fase -1, exceto para corrigir erros ou
visualizar alternativas necessárias a uma decisão. Depois acompanha as fases e
pode bloquear um executável quando revela divergência normativa:

| Marco da trilha | Entrega | Sincroniza com |
|---|---|---|
| linguagem visível | atualizar o [portal/livro](../../../portal/README.md) apenas depois da DB1 e somente com constructs presentes no corpus e status visível | fases -1–1 |
| edição local | TextMate utilizável no VS Code e Tree-sitter testado como parser incremental não normativo | fases 0–1 |
| frontend explorável | visualizações de tokens/CST/AST, formatter e catálogo pesquisável de diagnostics | fases 1–2 |
| semântica explicável | `explain` de tipos, effects, moves e motivos de rejeição; diff HIR/MLIR por passe | fases 2–4 |
| tasks observáveis | árvore de tasks, suspension points, cancelamento e cleanup ligados ao source | fase 5 |
| custos explicáveis | delta pós-link por import primeiro; summaries e perfis com confiança/`unknown` depois | fases 3–6 |
| builds explicáveis | grafo de módulos/packages, motivos de cache hit/miss, lock e provenance | fase 6 |

Critérios permanentes: exemplos visuais passam pelo mesmo runner do corpus; o
portal/livro não introduz sintaxe ausente da especificação; cada visualização
mostra a versão/schema do artefato; e snapshots gerados não viram autoridade.

## Explicitamente fora do caminho crítico

Os itens abaixo podem manter notas, experimentos pequenos e critérios de promoção
no [catálogo de pesquisa](research/README.md), mas não bloqueiam nenhuma fase:

- **Pesquisa:** wQL/wRPC/RestPC como bibliotecas e contratos de ecossistema, não
  keywords v0;
- **Pesquisa:** V6/Computer Units e qualquer runtime **serverless** ou isolate;
- **Pesquisa:** high-bit tagged addresses e NaN-boxing; a decisão da DB1 sobre
  fallback e niches convencionais pertence à fatia S1;
- **Pesquisa:** GPU, HDL, CUDA/HIP, OpenMP e SIMD explícito, posteriores ao
  pipeline CPU nativo;
- **Pesquisa:** SQLite como adapter oficial ou storage interno de tooling, nunca
  storage implícito da linguagem;
- **Pesquisa:** WC público, `fn<lang>`, tree strings, snapshots/PGO e autotests por
  IA;
- **Rejeitado por enquanto:** registry público, serviços hospedados de build,
  installers, auto-update e integração com lojas como requisitos do bootstrap;
- **Rejeitado por enquanto:** otimização sem fallback, macros arbitrárias, hot
  reload e execução remota como condição para declarar a fatia vertical pronta.

Uma pesquisa só entra no caminho crítico após problema concreto, hipótese
falsificável, baseline, positivos/negativos, custo de complexidade e promoção
registrada em [STATUS.md](STATUS.md#como-uma-decisão-avança).

O que acontece depois da v0 também não fica implícito. A
[arquitetura de longo prazo](ARCHITECTURE.md) separa os contratos que precisam
sobreviver à primeira implementação; o
[programa de pesquisa](research/long-term-program.md) organiza conformidade,
Unicode, memória, schedulers, services, WASI, ABI, ciência, supply chain,
tooling/IA e governança em ondas que não bloqueiam o bootstrap.

## Checkpoint de cada fase

Ao fechar qualquer fase:

1. atualizar corpus, positivos, negativos e oracle observável;
2. verificar se especificação, HIR/dialeto, runtime e portal/livro concordam;
3. registrar decisões ou conflitos no `STATUS.md`, sem promovê-los por silêncio;
4. executar os testes mínimos da fase e regressão de todas as fases anteriores;
5. validar links locais, artefatos gerados ausentes e `git diff --check`;
6. publicar um resumo curto de evidência, pendências e próximo gate — sem datas.
