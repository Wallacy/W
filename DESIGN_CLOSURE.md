# Fechamento da baseline de design

> **Status:** Direção · 19 de julho de 2026

Este documento é o mapa de fechamento do W. O registro canônico do estado de
cada escolha continua em [STATUS.md](STATUS.md); aqui as questões são ordenadas
por dependência para que a linguagem não seja implementada sobre decisões
implícitas.

O objetivo não é declarar que a linguagem nunca mudará. É chegar à
**Baseline de Design 1 (DB1)**: uma versão coerente e implementável de toda a
superfície v0, com extensões futuras explicitamente separadas.

## O que significa “100% definido”

Uma fatia da DB1 só fecha quando:

1. cada comportamento público está classificado como **Direção**, **Candidato**,
   **Em aberto**, **Pesquisa** ou **Rejeitado por enquanto**;
2. não existe regra material escondida apenas em exemplo, comentário ou texto
   histórico;
3. a alternativa escolhida tem exemplos positivos e negativos, inclusive de
   composição com as fatias das quais depende;
4. estão descritos tipos, controle, erro, ownership, cleanup, efeitos e custo
   observável aplicáveis;
5. existe um esboço de AST/HIR e lowering suficiente para detectar uma promessa
   impossível ou ambígua;
6. FFI, ABI, portabilidade, segurança, tooling e evolução foram avaliados — ou
   marcados explicitamente como não aplicáveis;
7. ao menos uma alternativa mais simples foi comparada;
8. toda hipótese necessária para decidir possui um teste, oracle e critério de
   promoção definidos antes do resultado;
9. tudo que ficou em **Pesquisa** tem uma baseline funcional que não depende da
   pesquisa para ser correta;
10. a decisão humana final e sua justificativa foram registradas em
    [STATUS.md](STATUS.md).

“Fechada” não quer dizer “otimizada”. Uma representação portátil correta pode
fechar a semântica enquanto uma representação compacta continua em
**Pesquisa**. “Fechada” também não quer dizer “ABI congelada”: a DB1 deve definir
quais layouts são observáveis e quais permanecem internos.

## Regra de trabalho durante o fechamento

- Portal, formatter, AST/HIR, dialeto MLIR e runtime não recebem novas features
  enquanto a DB1 estiver aberta.
- O corpus e pequenos spikes podem mudar apenas para comparar candidatos ou
  refutar hipóteses. Eles são instrumentos de decisão, não evidência de uma
  implementação pronta.
- Cada ciclo trabalha numa única fatia principal. Questões dependentes podem ser
  registradas, mas não são decididas por acidente.
- A visão de máquina apresenta invariantes, custos, failure modes e alternativas.
  A revisão humana avalia legibilidade, prazer, surpresa e vocabulário.
- Ao fim de cada ciclo, uma questão é promovida, continua **Em aberto** com um
  experimento melhor delimitado, ou sai da v0 como **Pesquisa**/**Rejeitado por
  enquanto**.

## Ordem das fatias

| Fatia | Pergunta de fechamento | Questões canônicas | Depende de | Artefato de saída |
|---|---|---|---|---|
| S0. Contrato de observabilidade | O que source, reflexão, ABI e tooling podem observar? | W-O006, W-O014, W-O044, W-O086, W-O094 | princípios atuais | regra de compatibilidade e matriz de observabilidade |
| S1. Representação de valores | Quais layouts/niches/tags existem e qual fallback é obrigatório? | W-O018, W-O043, W-O044, W-O045 | S0 | decisão de representação + matriz de targets |
| S2. Ownership e memória | Como valores nascem, movem, compartilham, falham e morrem? | W-O002–W-O004, W-O016–W-O017, W-O052–W-O055 | S1 | modelo de memória e pseudocódigo de cleanup |
| S3. Strings e Unicode | O que `String` armazena e quanto custam indexação, slices e interoperabilidade? | W-O014, W-O046–W-O048, W-O075 | S1–S2 | contrato completo de texto e literais |
| S4. Sistema de tipos | Como generics, conformances, inference, refinements, closures e conversões compõem? | W-O035, W-O049–W-O053 | S0–S3 | regras de tipos, inference e diagnostics |
| S5. Erros, efeitos e panic | Como falha recuperável, cancelamento, effects e falha irrecuperável atravessam scopes? | W-O005–W-O006, W-O033, W-O053–W-O054, W-O057 | S2–S4 | modelo unificado de exits e cleanup |
| S6. Concorrência e paralelismo | O que `async`, `spawn`, tasks, groups, streams e scheduler prometem? | W-O001, W-O055–W-O064 | S2, S4–S5 | semântica de tasks + contrato do executor |
| S7. Módulos, instâncias e serviços | Como código, estado, autoridade, localidade, durability e isolamento se relacionam? | W-O023–W-O027, W-O065–W-O073 | S2, S5–S6 | modelo de módulos/serviços e lifecycle |
| S8. Stdlib e I/O | Qual é o núcleo portátil e onde ficam blocking, adapters e dados versionados? | W-O026, W-O074–W-O080 | S3, S5–S7 | mapa da stdlib e contratos de capabilities |
| S9. Numéricos e ciência | Como fórmulas preservam overflow, unidades, rounding, shapes e reprodutibilidade? | W-O036–W-O041, W-O049, W-O081–W-O082 | S1, S4, S6 | modelo numérico e corpus científico |
| S10. C e ilhas multilíngues | Como FFI e `fn<lang>` preservam ownership, ABI, provenance e diagnóstico? | W-O042, W-O083–W-O084 | S1–S6 | contrato C primeiro + gate por linguagem |
| S11. Frontend, IR e backend | Como a semântica chega a código nativo sem ser apagada cedo? | W-O007–W-O012, W-O064, W-O085, W-O089–W-O090 | S0–S10 | arquitetura implementável e interfaces dos passes |
| S12. Build e packages | O que entra na identidade do artefato e como resolução/cache permanecem herméticos? | W-O013, W-O019, W-O087–W-O093 | S7–S11 | schemas, resolução e política de build |
| S13. Verificação e distribuição | O que uma assinatura, rebuild, nota W e tier realmente comprovam? | W-O019–W-O022, W-O091–W-O093 | S11–S12 | modelo de evidência e threat model |
| S14. Recursos e tooling | Como custos, budgets, diagnostics, testes e editores explicam a linguagem? | W-O028–W-O032, W-O095–W-O096 | S1–S13 | contrato do lens, corpus e tooling |
| S15. Consistência da superfície | Todas as decisões formam uma linguagem pequena, ensinável e sem sinônimos acidentais? | W-O001, W-O015, W-O038–W-O041, W-O048, W-O086, W-O094 | S0–S14 | tour/cheatsheet normativos revisados do zero |

As dependências não impedem anotar uma ideia posterior. Elas impedem promover
uma escolha posterior usando como premissa algo que ainda está **Em aberto**.

## Inventário por camada

O inventário completo de questões numeradas está em
[STATUS.md](STATUS.md#questões-abertas-prioritárias). Esta visão agrega o que
antes aparecia sem ID:

| Camada | Pendências que agora precisam de fechamento explícito |
|---|---|
| valores/ABI | `Any`, metadata de tipo, layout público, resilience, niches, tags e negociação de profile |
| memória | OOM, panic/unwind, atomics, data races, captures e escape de closures |
| texto | storage, mutabilidade, indexação, slicing, normalização, literais, interpolation e bundles Unicode |
| tipos | promotions/casts, generics/conformance, specialization, inference, refinements e compile-time evaluation |
| tasks | cancellation reason, handle, await múltiplo, erro primário, groups, backpressure, `Send`/`Sync`, streams e scheduler |
| módulos/runtime | arquivos, init, visibility, cycles, local/remote calls, mailbox, durability, output gates e failure boundary |
| stdlib | blocking, filesystem, clocks, randomness, collections, hashing, rede/TLS/HTTP e errors de adapters |
| ciência | decimal/Money, units, strict/reproducible/fast numeric modes, arrays/tensors, aliases e devices |
| interop | geração de wrappers C, annotations humanas, source maps e support matrix de `fn<lang>` |
| compilador | parser normativo, MLIR core, async lowering, ABI W, generics cross-module, cache e incrementalidade |
| produto | editions, profiles, conditional compilation, testes, lock/digest, multi-version, registry e policies |
| distribuição | nota W, envelopes, quorum, closed source, revogação, divergência e builders confidenciais |

## Hipóteses preservadas, mas não presumidas

| Hipótese | Estado da baseline |
|---|---|
| high-bit/tagged addresses | **Pesquisa**; S1 precisa funcionar sem isso |
| mimalloc como profile | **Em aberto** em W-O017; allocator universal é rejeitado |
| regiões/arenas e heap por instância | **Em aberto** em W-O003/W-O004; não nasce de `import` |
| Tree-sitter no compilador | **Em aberto** em W-O007/W-O008; a projeção IDE já pode existir |
| MLIR/LLVM como core | arquitetura **Candidato**; forma de integração continua aberta |
| SQLite durable | **Pesquisa** como adapter; durability não depende de um engine único |
| service/nanoservice | semântica granular é **Direção**; keyword e nome público continuam abertos |
| wRPC/wQL/RestPC | **Pesquisa** sobre contratos explícitos, não sintaxe necessária à v0 |
| `fn<lang>` | **Pesquisa** com forma delimitadora em W-O042 e support matrix em W-O084 |
| WLO/WLON | **Pesquisa**; formatos críticos exigem bytes canônicos independentes |
| GPU/HDL/OpenMP/SIMD explícito | **Pesquisa** depois de um caminho CPU correto |
| tree strings | **Pesquisa** para estruturas especializadas, não contrato de `String` |
| AI/PGO/autotests | **Pesquisa** de tooling; não altera a semântica entre builds |

## Estado atual do fechamento

| Item | Estado |
|---|---|
| inventário inicial | **Candidato**: 95 questões explícitas, mais hipóteses de pesquisa |
| ordem de dependência | **Candidato**: S0–S15 |
| primeira fatia em revisão | **Candidato** em W-C029 para source/fallback; W-O018 e W-O043–W-O045 ainda fecham profiles, `Any`, layout e fronteiras |
| implementação após a DB1 | bloqueada por decisão do projeto; somente spikes de hipótese são permitidos |

O número de questões não mede qualidade nem obriga 95 features. Uma boa revisão
pode fechar várias com uma única regra, fundir duplicatas ou retirar uma família
inteira da v0. O requisito é que isso seja deliberado e rastreável.
