# Estimativa de recursos no tooling

> **Status:** Pesquisa. Este documento especifica um experimento de compilador,
> CLI e IDE; não cria uma garantia de memória da linguagem nem altera a
> semântica de `import`.

## Direção

W deve tornar custos relevantes fáceis de descobrir sem transformar uma
estimativa em uma promessa falsa. O diagnóstico aparece perto da decisão que o
causa — inicialmente no `import`, depois também em uma chamada, `service`,
`TaskGroup` ou profile — e distingue com rigor:

- bytes presentes no artefato;
- recursos que uma instância reserva ao iniciar;
- recursos que uma operação pode alcançar;
- picos que dependem da concorrência e dos seus limites;
- partes que o compilador não consegue saber.

Um import é uma dependência estática, não uma instância, heap, thread ou grant
de capability. Portanto, “quanto este import usa” deve significar **delta do
artefato e do conjunto de código alcançável no target/profile selecionado**, e
nunca “esta linha alocou N bytes no runtime”. A estimativa de uma instância só
é mostrada se o código realmente cria ou referencia uma instância de execução.
Essa separação preserva o contrato de [módulos](../spec/modules.md) e a escada
de alocação de [memória](memory-strategy.md).

O objetivo prático é apoiar decisões como: “uma receita pode atender mais
pedidos em paralelo dentro de 96 MiB?” e “qual dependência trouxe TLS ou uma
biblioteca de imagem?”, sem esconder incerteza atrás de um número único.

## Candidato: lens por import e por uso

O `w analyze resources` produz um grafo por target, profile, features e lockfile
resolvido. A extensão de IDE usa o mesmo resultado como CodeLens, hover e painel
de detalhes. Um resultado tem sempre escopo e proveniência visíveis:

Até existir um toolchain W, todos os números nos mocks deste documento são
fixtures fictícias para avaliar a UX; `exact` descreve a classe que o valor teria
se viesse de um output materializado, não uma medição já realizada.

```text
import { OvenApi } from kitchen.oven
// resources: artifact +18.4 KiB exact · reachable 42.1 KiB static-estimate
// instance n/a (import estático) · target x86_64-windows / release
```

O resumo curto é apenas uma porta de entrada. Hover ou clique abre a decomposição
e a pergunta que o número responde:

| Campo | Pergunta respondida | Observação |
|---|---|---|
| `direct` | o que esta importação acrescenta se as outras importações atuais permanecerem? | usa custo marginal do grafo resolvido |
| `transitive` | quais dependências chegam por esta importação? | não soma bytes compartilhados duas vezes |
| `reachable` | que código/metadata deste caminho pode entrar no produto? | respeita roots, DCE, features e target |
| `instance` | que baseline uma instância criada deste tipo pode reservar? | só existe para service/isolate/adapter que a declare |
| `operation` | qual range uma chamada ou handler pode acrescentar? | separa steady state, pico e fanout |
| `unknown` | que parcela não tem limite honesto? | permanece explícita; não vira zero |

A IDE deve permitir alternar três vistas: **artefato**, **instância** e
**operação/pico**. A vista padrão no import é artefato; a vista de operação é
ancorada na chamada ou handler, onde o custo realmente nasce.

### Classes de confiança

Cada valor, inclusive um zero, recebe exatamente uma classe de confiança:

| Classe | Significado | Exemplo |
|---|---|---|
| `exact` | contabilizado do output e metadados já materializados | bytes de `.text` após link/strip fixado |
| `upper-bound` | limite provado sob precondições mostradas | frame sem recursão e buffer com capacidade fixa |
| `static-estimate` | cálculo estático útil, mas dependente de hipótese | heap de caminho conhecido com alocador/profile definido |
| `profiled` | intervalo de execuções identificadas, não teto | p50/p95/pico de carga reproduzível |
| `unknown` | não há estimativa defensável | callback FFI sem contrato de recurso |

`profiled` nunca substitui `upper-bound`: a UI mostra ambos quando existirem.
Um valor de perfil inclui dataset, cenário, versão do artefato, seed, target,
profile de runtime, allocator e intervalo de coleta. Dados sem esses inputs são
telemetria, não evidência reprodutível para o build.

### Decomposição mínima

O painel expõe as categorias abaixo, sem somar categorias incompatíveis.

| Categoria | Conteúdo | Forma de exibição |
|---|---|---|
| artefato estático | `.text`, `.rodata`, dados, TLS, tabelas de unwind/metadata e dependências incrementais | bytes direct/transitive/reachable, deduplicados no link final |
| baseline de instância | estado, mailbox, runtime/allocator, região e storage cache reservados ao criar serviço/isolate | reservado vs live; configuração que o fixou |
| stack e frames | stack síncrona, frame de coroutine e continuation | limite quando o grafo é fechado; caso contrário `unknown` com causa |
| heap de operação | alocações transitórias e retidas por request/call | faixa live/peak e ownership/região responsável |
| multiplicador de concorrência | children estruturados, chamadas em voo, mailbox, stream buffer e replicas | fórmula e limite de fanout/parallelism |
| recursos não memória | CPU estimada, handles, conexões, file descriptors, I/O bytes, storage e calls em voo | unidade própria; nunca convertida artificialmente em bytes |

O relatório diferencia `live`, `reserved` e `peak`. Por exemplo, uma arena de
8 MiB pode reservar 8 MiB sem ter 8 MiB live, e um request pode atingir 3 MiB de
peak sem elevar permanentemente o baseline da instância.

## Exemplo: restaurante

O exemplo é deliberadamente pequeno e ilustrativo; os nomes de manifest e
annotations de recurso abaixo são candidatos de tooling, não sintaxe já adotada.

```w
import { KitchenApi } from restaurant.kitchen
// resources: artifact +18.4 KiB exact · reachable 42.1 KiB static-estimate
// instance n/a: o import não cria uma cozinha

import { OrderApi } from restaurant.order_service

fn serveCake(
  request: CakeRequest,
  order: ServiceRef<OrderApi>,
  kitchen: ServiceRef<KitchenApi>,
): Cake async throws KitchenError {
  return try await kitchen.makeCake(request, for: order)
}
```

Depois de o grafo encontrar o `ServiceRef<KitchenApi>` e a instância referenciada,
o painel de uso relacionado ao import poderia mostrar:

```text
restaurant.kitchen — x86_64-windows / restaurant-prod

artifact
  direct             +18.4 KiB  exact
  transitive         +31.7 KiB  exact (shared: oven.codec, not charged twice)
  reachable          +42.1 KiB  static-estimate (LTO/DCE pending)

per KitchenApi runtime instance
  baseline           96–160 KiB static-estimate (mailbox 64, allocator 32–96)
  max configured     4 MiB     upper-bound (profile restaurant-prod)

makeCake per request
  frame              ≤ 2.3 KiB upper-bound
  heap peak          24–96 KiB static-estimate (recipe/input dependent)
  children           2 concurrent; both joined before return
  combined peak      144–352 KiB static-estimate
  unknown            FFI `oven.driver.submit`: no resource metadata
```

Se o restaurante abrir quatro cozinhas e cada uma admitir oito pedidos em voo,
o painel não multiplica cegamente todos os números. Ele exibe a fórmula e suas
premissas:

```text
service fleet: 4 KitchenApi instances × (96–160 KiB baseline)
request admission: 8 / instance × (144–352 KiB request peak)
accounted envelope: 4.9–11.6 MiB, excluding oven.driver.submit unknown
```

Uma `TaskGroup` sem limite transforma a linha em `unknown (unbounded fanout)`;
um limite explícito permite produzir `upper-bound` somente para a multiplicação
que de fato foi provada. Structured concurrency ajuda a encontrar children e
joins, mas não prova sozinha o tamanho de cada alocação nem a duração de I/O.

## Como o compilador chega aos dados

### Metadados publicáveis

O compilador e o registry podem consumir um sidecar versionado de interface de
recurso, assinado junto do artefato, por exemplo `resource.wmeta`. Ele declara
somente contratos verificáveis ou hipóteses rotuladas:

- tamanho/layout/ABI de exports públicos, features e dependências opcionais;
- custos fixos de código, dados, TLS e runtime por target/profile depois do
  link, quando disponíveis;
- limites de mailbox, calls em voo, stream buffers, regions e capacidade de
  serviço definidos pelo profile;
- efeitos de alocação, handles, I/O, blocking e concorrência de funções;
- precondições, versão do schema, toolchain e digest dos inputs;
- campos `unknown`, com motivo, em vez de omiti-los.

Metadata de pacote não é autoridade para mentir sobre binário estrangeiro.
Quando possível, `w verify resources` reextrai valores `exact` do artefato e
marca a declaração divergente. Para source W, o compilador gera metadata a
partir de HIR/MLIR; para C/FFI, wrappers ou manifests precisam declarar o
contrato e a confiança reduz quando ele não é verificável.

O lockfile fixa a versão, features, target e digests que deram origem à análise.
Assim o mesmo conjunto de inputs declarados refaz o mesmo relatório estático;
perfis são anexos identificados, nunca leitura implícita de produção.

### Análise estática

O pipeline candidato reutiliza informação que o frontend já precisa manter:

1. resolve imports, features, generics monomorfizados e roots de produto;
2. constrói call graph e grafo de instâncias/capabilities, incluindo dispatch
   resolvido e edges de async/RPC;
3. faz DCE, LTO-aware reachability e atribui bytes compartilhados uma única vez;
4. calcula layout, stack/frame e alocações prováveis depois de escape analysis e
   lowering de coroutines;
5. propaga efeitos/intervalos pelas calls, preservando o caminho e a causa de
   cada parcela;
6. calcula o máximo de children e buffers quando scope estruturado e limites de
   admission são conhecidos;
7. emite valores, hipóteses e `unknown` para CLI/IDE/CI.

O resultado precisa ser incremental: editar uma função invalida seu sumário e
os chamadores alcançáveis, não reanalisa todo o workspace. O compilador guarda
identidades estáveis de símbolo e hashes do summary, evitando que a IDE recalcule
o grafo completo a cada tecla.

### Limites fundamentais

Recursão não limitada, despacho dinâmico aberto, reflection futura, plugins,
callbacks estrangeiros, `unsafe`, FFI, entrada arbitrária, caches e alocadores
de terceiros podem impedir um teto. Nesses casos o diagnóstico deve apontar a
primeira fronteira que perdeu informação e sugerir uma ação concreta: fechar um
`protocol`, inserir um adapter com metadata, limitar `TaskGroup`, separar uma
operação, ou aceitar `unknown` no budget.

Generics, DCE e LTO também impedem que o custo declarado por um pacote seja um
número universal. O lens calcula contra o executável/serviço atual; o portal do
pacote pode expor apenas intervalos por target/profile e o digest do artefato
de referência.

### O que MLIR/LLVM já tornam viável

O protótipo não precisa inventar toda a infraestrutura de medição:

- o [Data Layout do MLIR](https://mlir.llvm.org/docs/DataLayout/) consulta
  tamanho e alinhamento de tipos dentro de um scope e target definidos; isso
  sustenta layouts e baselines, não peak de heap;
- [`llvm-size`](https://llvm.org/docs/CommandGuide/llvm-size.html) e
  [`llvm-readobj`](https://llvm.org/docs/CommandGuide/llvm-readobj.html)
  inspecionam seções e símbolos do objeto ou imagem final, oferecendo a base
  pós-link para valores `exact` de artefato;
- Clang já pode emitir metadata de tamanho de stack por função com
  [`-fstack-size-section` ou relatórios `.su` com
  `-fstack-usage`](https://clang.llvm.org/docs/ClangCommandLineReference.html),
  um precedente útil para adapters C/FFI.

Esses mecanismos não atribuem sozinhos um delta marginal a cada import e não
provam heap, número de instâncias ou fanout. W ainda precisa preservar a origem
dos símbolos no grafo, comparar produtos com inputs fixos, gerar summaries de
efeitos/recursos e instrumentar o runtime. A infraestrutura existente reduz o
primeiro experimento a integração e atribuição; não elimina as fronteiras
`static-estimate`, `profiled` e `unknown`.

## Estimates não são budgets

Um **estimate** descreve evidência; um **budget** é policy executável. O build
ou deploy pode comparar ambos, mas não converte previsão em garantia:

```text
w analyze resources --target x86_64-windows --profile restaurant-prod
w check resources --budget config/resources.wbudget
w profile resources --scenario dinner-rush --inputs profiles/dinner-rush.json
```

Um `resources.wbudget` candidato teria limites separados para artefato,
baseline por instância, peak por request, memória agregada, CPU, handles, I/O,
storage e calls em voo. A policy pode falhar no CI se um custo `exact` exceder o
limite ou exigir revisão quando um `static-estimate`/`unknown` o atravessa. Ela
não deve recusar uma mudança apenas porque uma medição p95 variou: esse dado
alerta para regressão e exige contexto, não é teto de segurança.

O runtime continua sendo responsável por enforcement quando o profile fornece
quota. Falhas de allocation, cancelamento e cleanup obedecem ao contrato do
serviço; um lens não pode prometer que uma análise estática evita OOM.

## Feedback de execução e privacidade

`w profile resources` mede somente cenários explicitamente escolhidos. Seu
arquivo de perfil contém schema, hashes de binário/lockfile, plataforma,
allocator, limites, carga sintetizada ou identificador de dataset permitido,
seeds, métricas agregadas e período de coleta. Perfis podem enriquecer um hover
como “pico observado”, mas não alteram silenciosamente o resultado estático.

Por padrão, o tooling não envia call traces, nomes de pedidos, payloads ou
telemetria para registry/IDE remota. Produção requer opt-in, redaction de
identificadores e agregação local. Um perfil que não pode ser compartilhado
pode continuar útil no computador do autor, identificado como `profiled local`.

## CLI e experiência de IDE candidatas

```text
$ w analyze resources --entry apps/restaurant.w --view imports

apps/restaurant.w
  restaurant.menu       artifact +3.1 KiB exact       reachable 8.6 KiB static-estimate
                         instance n/a (import estático)
  restaurant.kitchen    artifact +18.4 KiB exact      reachable 42.1 KiB static-estimate
                         related use: KitchenApi instance 96–160 KiB static-estimate
  oven.driver           artifact +12.0 KiB exact      operation unknown (FFI)

total linked payload: 1.84 MiB exact
reachable estimates: 2.06 MiB static-estimate; 1 unknown boundary
```

Na IDE, o CodeLens é recolhido por padrão e acessível por teclado. Cores não
carregam o significado sozinhas: texto e ícones distinguem `exact`, intervalo,
alerta de budget e `unknown`. O painel sempre mostra target/profile/feature e o
comando para reproduzir o número. Explicar o grafo é mais valioso que ordenar
pacotes por uma pontuação opaca.

## Gates e métricas de pesquisa

| Etapa | Gate de avanço | Métrica principal |
|---|---|---|
| 0: artefato | dois builds reproduzem bytes e classificação de seções | diferença `exact` zero para inputs fixos |
| 1: imports | grafo incremental e DCE não contam dependência compartilhada duas vezes | explicação correta em corpus de apps pequenos |
| 2: funções | summaries de stack/heap explicam origem e precondição | falsos limites e `unknown` revisados por humanos |
| 3: structured concurrency | fanout/joins/buffers limitados aparecem no grafo | divergência entre bound e instrumentação |
| 4: profiles | perfil é reexecutável e preserva privacidade | correlação estimate/medido por cenário, sem vender p95 como teto |
| 5: budgets | CI e runtime distinguem alerta, erro de policy e enforcement | recovery correto sob pressão/cancelamento |

O primeiro protótipo deve começar por tamanho de artefato pós-link e um único
aplicativo de restaurante. Stack profunda, heap interprocedural e perfis de
produção entram depois que os summaries puderem ser auditados em texto.

## Riscos

- Um número cedo demais cria falsa segurança e pode incentivar budgets que
  quebram cargas legítimas.
- Somar imports compartilhados ou tratar cada import como uma instância produz
  recomendações erradas e contradiz a semântica de módulos.
- LTO, plugins e FFI podem tornar o resultado caro; o cache incremental e
  metadata por ABI são necessários antes de ligar o lens por padrão.
- Perfis de produção podem vazar comportamento de usuários; coleta deve ser
  local e opt-in.
- Instrumentação muda alocações, scheduling e latência; ela precisa declarar
  overhead e nunca substituir a medição sem instrumentação.

## Status das escolhas

| Tema | Status | Decisão/proposta |
|---|---|---|
| import não tem heap próprio | **Direção** | o lens de import mede artefato/reachability; instância fica explícita |
| resultado com provenance e `unknown` | **Direção** | nenhuma UI reduz ausência de prova a `0 B` |
| classes `exact`/`upper-bound`/`static-estimate`/`profiled`/`unknown` | **Candidato** | mesma taxonomia para CLI, IDE e CI |
| `resource.wmeta` versionado | **Candidato** | sidecar verificável por target/profile/ABI |
| cálculo de peak por call graph estruturado | **Pesquisa** | requer corpus, instrumentation e diagnóstico auditável |
| inferir teto através de FFI/dynamic dispatch aberto | **Rejeitado por enquanto** | exige metadata/adapter ou continua `unknown` |

## Perguntas para a próxima revisão

Estas questões estão registradas em [STATUS.md](../STATUS.md) e continuam
abertas; os exemplos deste documento não as promovem a compromisso de
linguagem/toolchain.

1. **W-O028:** o primeiro UX deve começar em import/artefato, ou mostrar também calls do
   restaurante desde o primeiro protótipo?
2. **W-O029:** qual orçamento é mais valioso para validar cedo: memória por request,
   baseline de serviço, ou payload final?
3. **W-O030:** você prefere annotations de resource no source público, no manifest/profile,
   ou somente metadata inferida/gerada enquanto a sintaxe ainda amadurece?
4. **W-O031:** há um workload real além do restaurante que deve virar segundo corpus — por
   exemplo parser, servidor HTTP ou processamento de imagens?
5. **W-O032:** em que nível um perfil local pode entrar em revisão/CI sem expor dados de
   produção ou transformar hardware específico em requisito do projeto?
