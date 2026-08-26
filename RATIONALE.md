# Justificativas e evidências do design W

> **Status:** complementar e não normativo

[`DESIGN.md`](DESIGN.md) define contratos, estado, pesquisa aberta e ordem de implementação.
Este arquivo registra justificativas, evidência, alternativas e proveniência; nunca define comportamento W.
[`history/`](history) preserva proveniência obsoleta e não decide o W atual.

**Nota de terminologia retirada:** qualquer ocorrência histórica de `T0`, `T1`,
`T2` ou `tier` neste arquivo é proveniência aposentada. Esses rótulos não são
contratos atuais nem nomes de disponibilidade da standard library; a policy
vigente é plana por módulo, capability, target facts, provider e reachability.

## 1. Evidência comparativa

### 1.0 Migração de parâmetros de valor

**Histórico substituído:** a forma `<const rows: usize>` usava uma keyword para codificar um kind
que o head já consegue resolver. A forma corrente usa `rows: usize` e
`_ state: Type` para manter o envelope uniforme. A interpretação histórica de `_` como
um único primary-only slot fica registrada somente como alternativa substituída.
Ela não limita a forma vigente, que dá ao label o papel `optional(name)`. O
parser rejeita binding modifiers no envelope antes do W 1.0. Não existe alias ou
compatibilidade implícita. Esta nota registra a migração. O contrato normativo
permanece em [`DESIGN.md`](DESIGN.md).

O label externo pertence ao papel da call ou do initializer. O nome interno
continua disponível para o body e, em um type head, para a associated contract
value. A omissão de label em um parâmetro generic segue a mesma separação usada
nos parâmetros runtime: ela evita exigir no call site um label que o caller não
precisa escrever, sem remover a exposição estática do type head.

Um type head expõe cada value parameter como associated contract value porque o
valor faz parte da especialização e não de cada instance. Transformá-lo em field
criaria storage, custo de allocation e uma identidade runtime que o contrato não
solicita. Transformá-lo em um único primary slot limitaria `Matrix` e shapes com
mais de um valor. Reordenar labels reduziria a ordem source e complicaria
lookup, formatter e diagnostics.

Estas alternativas permanecem preservadas para comparação:

- `_` como único primary-only slot;
- fields de instance automáticos para todos os values;
- labels que permitem reorder;
- value parameters callable como associated members.

Para scripts, a alternativa rejeitada é executar qualquer statement de módulo
durante import. O body implícito é root-only, sem `args` ou `ctx` inferidos, e
usa o mesmo parser/checker/HIR do produto. Uma função explícita continua sendo
a forma para arguments, `Context`, retorno customizado ou errors escapantes.
Também ficam registradas as alternativas de um runtime de script separado, de
remover o seed C/CMake/Ninja após o self-host, e de tratar Bun como dependência
do produto; a direção vigente preserva a rota de recovery e migra tooling só
depois do stage C.

Para memória e concorrência, naming heuristics e warnings especulativos foram
rejeitados. `shared` resolve somente multiplicidade real de owners; `atomic`
resolve somente uma localização e operação compatíveis. As opções de
partition/move+join, channel/service/domain isolation e lock permanecem
alternativas condicionais aos proof facts, sem uma arquitetura escolhida pelo
diagnostic.

**Exemplo:** pessoas e modelos corrigem o mesmo erro de ownership no restaurante
antes de informar preferência pela syntax.

Cada comparação usa o mesmo programa e quatro tarefas:

1. explicar o que o código faz;
2. reproduzir uma parte após intervalo curto;
3. corrigir um erro preparado;
4. mudar um requisito sem reescrever o programa.

Métricas humanas:

- acerto semântico;
- erro de sintaxe e de runtime esperado;
- tempo e número de consultas;
- confidence;
- preferência, medida por último.

Métricas de modelos:

- parse e type-check;
- testes aprovados;
- diagnostics necessários até a correção;
- tokens de source e de contexto;
- edit distance da forma canônica;
- consistência entre modelos e tokenizers.

O corpus compara, no mínimo:

- units `<>` contra `[]`;
- alias de quantity contra newtype para todo nome físico;
- `spawn<.compute>` contra `spawn<domain: .compute>` e `spawn on .compute`;
- `T<(.member predicate)>` contra `value.member`, `where` e constructor;
- `Array<u8><(.count <= 64)>` contra uma static list no mesmo envelope;
- `: self` explícito com fallthrough contra retorno implícito do receiver e `return self` equivalente;
- `(take value).method()` contra consumo implícito e free function;
- error em `take fn` contra restauração implícita do owner;
- associated member direto contra protocol requirement e mutable type storage;
- struct transparente contra `export` em cada field e export total do tipo;
- pattern nominal `Type(field, ...)` contra record pattern com `{}`;
- tuple scrutinee contra syntax especial para múltiplos valores;
- patterns fechados contra handler customizado pelo usuário;
- `...` externo obrigatório contra exaustividade aberta implícita;
- object encapsulado contra storage público e constructor herdado;
- overload por forma contra ranking por tipos e nomes distintos;
- vários initializers contra initializer único e factories nomeadas;
- computed property property-safe contra method com `try` ou `await`;
- static record/list contra interpretações universais de extension e constraints;
- ponteiro `fn`, `some fn` e `any fn` contra um único callable apagado;
- erasure contextual com policy normal e `erase` fallible contra exigir uma única forma em todos os casos;
- declaração `let x: shared T = value` e movimento explícito com `take` contra factory nominal, `share`/`try share` aposentados e promotion em calls;
- String refinada, resource gate e carrier de boundary contra `InlineString`, reserva e fixed array universal;
- call posicional por valor contra labels e defaults preservados no function type;
- `fn`, `mut fn` e `take fn` contra protocols callable separados;
- signature invariável contra variance e effect widening implícitos;
- `fn<C>` contra `fn<lang: .c>`;
- slot angular nomeado contra case enum posicional em erro e evolução de schema;
- closure `=>` contra `fn(...)`;
- nested matrix contra `;`;
- achatamento sem `from` contra binding de módulo e seleção por braces;
- formatter fixo de 120 colunas contra style configurável e import sorting;
- fail-fast com arbitragem lexical/input contra espera estritamente lexical;
- cancellation como control effect contra `throws Cancellation`;
- execution profile por product/unit contra domain default por módulo;
- loop ou block rotulado contra `goto` e flags de saída;
- `repeat ... while` contra `while true` com `break` final;
- `break label` contra exception usada somente para control flow.
- newline como whitespace contra automatic semicolon insertion;
- semicolon raro de desambiguação contra remoção que altera a CST.
- manifest exclusivo com `{...}` contra `package<...>` inline no source;
- module-run source contra package/workspace dependency metadata e dependency inference;
- module-run context contra package/workspace resolution e ambient registry;
- imports explícitos e root canônica contra scan recursivo, cwd, `PATH`, environment e symlink escape;
- service requirements com deployment binding contra source grants e escalation transitiva;
- entry explícito com cleanup explícito contra execução arbitrária de módulo e estado oculto;
- imports antes das declarations contra imports intercalados;
- body obrigatório em função comum contra prototype solto no top-level;
- `<...>` como contrato local nomeado contra uso como record completo de build.
- contract ligado ao head contra `Array <T>` dependente de whitespace;
- `<(expression)>` contra expression crua dentro do envelope;
- envelopes sequenciais contra fusão de generic arguments e refinement;
- tuple de um elemento com comma contra parentheses de agrupamento;
- closes `>>` contextuais contra exigir whitespace em nested types;
- payloads Bool, String, quantity e size iguais em type application e generic call;
- `if ... else` como expression contra ternary e branch statement only;
- assignment Unit contra assignment que devolve e duplica o value;
- power matemática com exponent prefix contra unary que vence power;
- tail expression explícita contra retorno implícito de todo function body;
- semicolon que preserva discard contra formatter que o remove por aparência;
- URL e URLSearchParams padrão contra aliases HTTP locais;
- constructor Web de Response contra família de helpers por body;
- pipeline lazy contra loop explícito para transformação sem side effect;
- broadcast explícito contra broadcast implícito checked e Julia dotted broadcast;
- `.last` contra arithmetic `count - 1`, Python `[-1]` e C# `^1`;
- labels em ordem fixa contra labels reordenados com default e overload;
- tuple binding fixo contra projections `.0`/`.1` e unpacking starred;
- `data.Batch<Row>` columnar contra `Array<Row>` universal e DataFrame completo;
- `data.Row` synthesis fechada contra `Any`, duck typing e schema implícito;
- binding dynamic, copy/device e release scoped contra coerção e lifetime ocultos.
- nomenclatura `SuspendAccounting` para suspensão HOST/SO contra `HostSuspendPolicy`, booleano e estados inferidos;
- aquisição contextual de owner a partir de `weak T?` contra property `strong` e method `strong()`;
- escopo `Arena.fixed` contra região lexical reservada e scope por closure;
- slot runtime de allocator em `Array<String>(allocator: memory)` contra envelope genérico;
- relação bodyless de borrow entre dois inputs independentes contra mapping implícito sem origem única, comparando receiver preciso, input único, relation-schema Research e carrier nominal.

### 1.1 Cobertura de substituições

**Exemplo:** o caso W-732 executa a mesma busca com `goto`, flags e labels. O
Book mostra a forma W e explica por que as outras formas não entram.

Toda decisão que rejeita uma construção em favor de outra precisa de um caso
comparativo. O caso registra:

1. ID da decisão;
2. tarefa e input iguais;
3. forma vigente;
4. forma rejeitada ou de outra linguagem;
5. comportamento esperado e modo de falha;
6. razão mensurável da escolha.

Uma forma rejeitada não entra no corpus positivo. Ela pode ficar em texto ou em
fixture negativo. O Book deve mostrar a substituição perto do primeiro uso.

Antes do design freeze, o tooling deve publicar cobertura `casos/decisões que
exigem substituição`. A cobertura atual de exemplos por seção não substitui essa
métrica. Nenhuma documentação final pode omitir a alternativa que motivou uma
decisão.

[`tooling/substitution-cases.json`](tooling/substitution-cases.json) mantém a
entrada estruturada. Cada caso liga um requisito desta seção a decisões do
ledger, uma tarefa, a forma vigente, ao menos uma alternativa e quatro medidas.
O checker valida a ligação e o índice publica a razão exata. O comando isolado
sem flag permite inspecionar uma edição parcial. O gate do repository usa
`--require-complete` e falha quando qualquer requisito não possui caso. R0 cobre
os 74 requisitos. Essa contagem fecha o input dos estudos; ela não afirma que
os estudos foram executados. Ela também não substitui a auditoria do ledger
mantida por [`tooling/design-freeze-audit.json`](tooling/design-freeze-audit.json).

O source vigente de um caso deve ser W aceito pelo contrato corrente. Uma forma
substituída pode ser W rejeitado, pseudocode ou outra linguagem. O campo
`language` declara essa origem. O corpus não afirma que o parser W aceita a
alternativa. Estudos humanos e de modelos usam o mesmo `task` e o mesmo input;
eles registram resultados, mas não mudam a decisão sem nova entrada no ledger.

O kernel executável de memória usa a baseline M1. O corpus possui 185 casos e
606 operações, com 79 outcomes aceitos e 106 rejeitados. Cada caso liga
PlaceId, LoanId, dependency edge, OriginSet, escape ou boundary a um symbol real
do Última Luz.
O snapshot declara schema M1. Ele não é uma implementação do compiler ou do
runtime.

O kernel lógico de layout e ABI usa a baseline L0. Seus 78 casos e 96 operações
separam W exact, import expectation, physical call shape, C carrier, foreign
layout, header pairing e recovery de artifact. O snapshot não é uma ABI note
binária. Ele prova as relações que o reader físico `WMeta1` precisa preservar.

**Caso W-732 — sair de loops aninhados.** As formas processam o mesmo input. Um
carrier inválido encerra a busca. Um zero avança a linha externa.

```w
// Forma W vigente.
scan: for row in rows {
  for value in row {
    if value == 0 { continue scan }
    if value > 31 { break scan }
    consume(value)
  }
}
```

```c
/* Alternativa C com goto. */
for (size_t row = 0; row < row_count; ++row) {
  for (size_t column = 0; column < column_count[row]; ++column) {
    uint8_t value = rows[row][column];
    if (value == 0) goto next_row;
    if (value > 31) goto done;
    consume(value);
  }
next_row:
  continue;
}
done:
```

```w
// Alternativa com flag. A condição replica o estado do controle.
var stopped = false
for row in rows {
  if stopped { break }
  for value in row {
    if value == 0 { break }
    if value > 31 {
      stopped = true
      break
    }
    consume(value)
  }
}
```

O label W limita o target a um owner lexical. `goto` aceita outros pontos. A
flag cria estado mutável que pode divergir do control flow.

### 1.2 Baseline estática R0S

**Exemplo:** a forma W e as alternativas do caso de labels recebem digests e
contagens determinísticas antes de qualquer participante ou modelo vê-las.

[`tooling/substitution-surface.snapshot.json`](tooling/substitution-surface.snapshot.json)
mede as 174 formas derivadas do corpus pelo runner nesta revisão. O runner
junta as linhas com LF e sem newline final. A contagem vem do script e muda
quando alternativas cross-language entram ou saem. Para a tarefa e para cada
forma, ele registra:

- bytes UTF-8;
- code points;
- code points que não são whitespace;
- linhas;
- surface lexemes.

O scanner `unicode-surface-1` reconhece strings, character literals,
identifiers Unicode, números e cada punctuation restante. Ele é independente
da grammar W. `surfaceLexemes` não significa token do compiler ou de um modelo.
Um resultado futuro de modelo precisa registrar provider, model, tokenizer,
versão, digest do input e parâmetros de execução.

R0S é uma baseline descritiva. Ela detecta drift do corpus e permite planejar
context windows. Ela não escolhe a forma menor, não compara snippets com escopo
diferente e não prova compreensão, correção ou preferência. Estudos humanos e
de modelos continuam usando as quatro tarefas desta seção e publicam resultados
em outro artefato.

O índice gerado publica total, mediana e máximo dos surface lexemes vigentes.
Essas medidas mostram que R0 contém microformas. Elas são adequadas para recall
de syntax e reparo local, mas não bastam para explicar comportamento, modificar
um requisito ou medir surpresa runtime num programa. Uma rodada R1 precisa
incorporar os casos em slices completos e executáveis do Última Luz. Cada bundle
R1 fixa source base, input, outcome, ordem de apresentação e digest de cada
variante. As variantes diferem somente na construção estudada. Uma variante não
pode remover contexto ou testes para parecer menor.

#### Corpus semântico S0 e diagnostics D0

O corpus S0 usa sources syntax-valid do Última Luz. Cada caso negativo aponta
para um baseline positivo da mesma decisão e muda um único `failureField`. Os
outros fields permanecem calculados, para que um erro de move não passe porque o
checker também perdeu type ou effect.

Um resultado aceito materializa:

1. `resultType`;
2. `category`;
3. `flow` e target quando existe owner;
4. `ownerDelta` em source order;
5. `effectSummary` com sets byte-sorted;
6. `proofFacts` válidos no caminho corrente;
7. `evaluationGraph` com nodes locais e edges direcionais.

Os IDs de node valem somente dentro do record. Eles não dependem de address,
scheduler ou hash order. O grafo registra source order, branch, transfer,
suspension e cleanup sem ordenar caminhos mutuamente exclusivos.

O Última Luz liga patterns, narrowing e branch join a `enum_contracts.w` e
`command.w`; loops e assignment a `collections.w` e `numerics.w`; error e
cleanup a `failure.w`; callables a `callables.w`; memória a `memory.w` e
`hir_memory_oracle.w`; execução a `execution.w` e `scheduler_oracle.w`; staging
a `allocation.w` e `dining.w`; transaction e services aos oracles próprios.

As inversões cobrem static contracts, patterns, match, expressions, effects,
ownership, generic inference e ConstIR. Os casos Const trocam, uma causa por
vez, const safety, ciclo, quota, predicate, error, panic ou argumento compile-time.
Os casos generic removem uma equação, fazem recursion crescer, deixam um domain
sem resolução ou violam label e associated name.

Const evaluation usa como precedentes
[Rust const evaluation](https://doc.rust-lang.org/reference/const_eval.html),
[Zig comptime](https://ziglang.org/documentation/master/#comptime) e
[D CTFE](https://dlang.org/spec/function.html#interpretation). W retém um corpo
comum entre runtime e compile time, mas exige contrato `const`, quotas na recipe
e nenhuma branch baseada na fase.

`semantic-results.snapshot.jsonl` e `semantic-diagnostics.snapshot.jsonl` são
expectativas de design. Eles não provam type checker, evaluator ou ConstIR. O
primeiro frontend S0 deve emitir records compatíveis antes que o status deixe de
ser `design-oracle-input`.

W-846 corrige a promessa de causalidade de `W-CONST-0004`. O diagnostic sempre
preserva `head`, `argument` e `predicate`. Uma causa específica aparece somente
quando uma `ConstRejectionSlice` bounded possui uma causal boundary única,
suficiente e dominante. O witness StagePath negativo inclui `canMove` e
`isValidStagePath`. A call `canMove(.accepted, .completed)` e seu resultado false
podem justificar a causa.
Uma slice truncada, ambígua ou sem causal boundary usa `failure: predicate:false`.

### 1.3 Bundles R1 do Última Luz

Cada bundle mantém o mesmo source base, os mesmos inputs e o mesmo application
outcome. A variante pode mudar uma observação que pertence ao objeto do estudo,
como latência de failure ou provenance visível de um nome.
Os bundles R1 não alteram os sources canônicos do Última Luz.

Os bundles atuais possuem variantes de duas a quatro formas W. Eles usam inputs
primary e adversariais, conforme o estudo. Todos fazem parse sem recovery. Os
testes de oracle host
confirmam os outcomes e as diferenças observáveis declaradas.

A promoção conta IDs R0 únicos citados por ao menos um bundle. Ela mede o
planejamento do corpus. Ela não mede participantes, não ratifica uma forma e
não conta duas vezes um caso usado em mais de um bundle.

#### 1.3.1 Controle de fluxo estruturado

**Exemplo:** um zero abandona a linha atual sem finalizá-la. Um carrier maior
que 31 encerra o scan inteiro. O resultado contém bits e linhas finalizadas.

[`tooling/studies/r1-control-flow/bundle.json`](tooling/studies/r1-control-flow/bundle.json)
deriva de `foldDiagnosticBits` do Última Luz. O bundle fixa dois inputs e seus
outcomes. Ele contém duas variantes W completas:

- `structured.w` usa loop e block rotulados;
- `flags.w` usa flags mutáveis e transfers não rotulados.

As duas variantes mantêm tipos, nome da função, inputs, outcome e testes. O
bundle registra digest de cada source, do source base e do oracle. As ordens
`structured/flags` e `flags/structured` fazem o counterbalancing mínimo. As
quatro tarefas cobrem explicação, recall após delay, reparo preparado e mudança
de requisito. Participantes recebem labels neutros `A` e `B`; IDs, roles, paths
e lista de construções ficam ocultos durante a sessão.

O gate atual prova que ambas as variantes fazem parse sem recovery. Um oracle
host independente executa os dois inputs e exige o mesmo outcome. Essa prova
não executa W. O bundle continua `design-oracle-input` até `w compile` e `w run`
substituírem o oracle host. Estudos humanos e de modelos também continuam
ausentes e aparecem em `evidence.missing`.

#### 1.3.2 Delimitador de units

**Exemplo:** `9.80665<si.m/si.s^2>` declara aceleração. A forma com `[]` usa os
mesmos tokens, mas também parece uma indexação por expression.

[`tooling/studies/r1-units/bundle.json`](tooling/studies/r1-units/bundle.json)
deriva das units do Última Luz. As variantes calculam energia de impacto e tempo
de queda com os mesmos inputs:

- `angle.w` usa a forma vigente `<unit-expression>`;
- `square.w` preserva a alternativa histórica `[unit-expression]`.

As duas variantes fazem parse. A variante square não é uma quantity válida no
design vigente. A CST usa a família de indexação. O estudo mede se essa colisão
melhora familiaridade ou aumenta classificação incorreta, recall e reparo.

#### 1.3.3 Provenance de imports

**Exemplo:** `import std.text` deixa `trim` disponível. `import text from std`
exige `text.trim` e mantém o módulo visível no call site.

[`tooling/studies/r1-imports/bundle.json`](tooling/studies/r1-imports/bundle.json)
deriva do decoder de comandos. As variantes normalizam e classificam o mesmo
input:

- `flattened.w` achata os exports de `std.text`;
- `qualified.w` cria um module binding.

As duas formas permanecem válidas. O estudo não tenta eliminar uma delas. Ele
mede a recomendação idiomática para contexto curto, colisão e múltiplos módulos.

#### 1.3.4 Fail-fast e espera lexical

**Exemplo:** starboard falha no tick 2 enquanto port termina no tick 8. O tuple
await observa a falha no tick 2. A espera lexical observa a mesma falha no tick
8.

[`tooling/studies/r1-fail-fast/bundle.json`](tooling/studies/r1-fail-fast/bundle.json)
deriva de `mixPair`. As variantes retornam o mesmo application error:

- `grouped.w` usa tuple await e fail-fast estruturado;
- `lexical.w` aguarda cada task em ordem lexical.

O oracle mantém o error final igual e mede `observedAt` separadamente. Assim o
estudo testa surpresa de runtime sem trocar o requisito da aplicação.

#### 1.3.5 Envelopes de contrato sequenciais

**Exemplo:** o element type pertence ao primeiro contrato. O predicate seguinte
restringe a lista completa:

```w
StaticList<ServiceStage><(isValidStagePath(.member))>
```

[`tooling/studies/r1-contract-envelopes/bundle.json`](tooling/studies/r1-contract-envelopes/bundle.json)
deriva de `StagePath` no módulo `domain` do Última Luz. As variantes preservam
o mesmo enum, validator, inputs e outcomes:

- `sequential.w` aplica `ServiceStage` e depois restringe o resultado;
- `fused.w` coloca o type e o predicate em uma static list no primeiro
  contrato.

A alternativa fused faz parse porque uma static list é um payload estrutural
válido. Ela não passa no checker vigente. O slot primário de `StaticList` exige
um type, e a lista não informa se o predicate restringe o element ou a lista
completa. O estudo mede explicação do subject, recall, reparo e a adição de um
segundo limite. O oracle host confirma somente a regra de transição. Ele não
executa o evaluator de contratos W.

O par `S0-POS-contract-sequential-static-list` e
`S0-NEG-contract-fused-static-list` fixa a rejeição semântica. A forma fused
produz `W-CONTRACT-0002`: o slot `T` espera um type e recebe uma static list.

#### 1.3.6 Receiver consuming explícito

**Exemplo:** `finish` recebe ownership de `stream` antes do member lookup:

```w
let tail = try (take stream).finish()
```

[`tooling/studies/r1-consuming-receiver/bundle.json`](tooling/studies/r1-consuming-receiver/bundle.json)
deriva de `CommandStream.finish()` no Última Luz. As variantes mudam somente o
receiver da call:

- `explicit.w` usa `(take stream).finish()`;
- `implicit.w` usa `stream.finish()` e depende do mode declarado pelo member.

Os inputs cobrem success e error. Nos dois outcomes hipotéticos, o owner fica
indisponível. A variante implicit faz parse, mas produz `W-OWNERSHIP-0011` no
checker vigente. O estudo mede quando o participante percebe a transferência,
se espera restauração no `catch` e como redesenha a API para permitir retry.

O par `S0-POS-consuming-receiver-explicit` e
`S0-NEG-consuming-receiver-implicit` fixa o diagnostic. A falha ocorre antes do
move. Por isso, ela não afirma que uma call W inválida consumiu o binding. O
oracle host compara somente a semântica hipotética das duas alternativas.

#### 1.3.7 Slot de execution domain

**Exemplo:** o estudo compara a forma vigente com uma variante nomeada:

```w
let port = spawn<.compute> mix(left)
let starboard = spawn<domain: .compute> mix(right)
```

[`tooling/studies/r1-spawn-domain/bundle.json`](tooling/studies/r1-spawn-domain/bundle.json)
deriva de `mixPair` no módulo `execution` do Última Luz. `positional.w` e
`named.w` escrevem as duas formas do mesmo slot opcional. O estudo compara a
intenção de domínio e a preservação de source spelling; ele não introduz alias
no schema corrente.

Um input liga `.compute` a um domain paralelo com capacity um. A call continua
válida, mas não promete simultaneidade. O oracle também envia trabalho a um
domain serial. O dispatch é válido e preserva FIFO sem permitir overlap dentro
desse domain. Esse resultado mede o contrato de link; ele não executa um
scheduler W.

O módulo declara apenas `domains`. O par `S0-POS-module-domain-requirement` e
`S0-NEG-module-parallel-default` preserva a rejeição histórica de um default no
header. A forma vigente também remove esse field do execution profile. `spawn`
e `parallelMap` selecionam o domain no call site. O product fornece somente
bindings, pools e budgets.

O mesmo bundle promove `R0-product-execution-profile`. O package define o
profile, o product seleciona seu ID e cada unit alcançável conserva capacity e
capabilities no packing. Um módulo que tenta selecionar o profile é rejeitado
antes de dispatch. Outro bundle repetiria os mesmos facts de domain e envelope.

#### 1.3.8 Representação e ownership de callables

**Exemplo:** uma função estática, uma route com capture, um counter mutável e um
manifest consuming preservam o mesmo resultado da aplicação, mas não possuem o
mesmo contrato de custo ou ownership.

[`tooling/studies/r1-callable-model/bundle.json`](tooling/studies/r1-callable-model/bundle.json)
deriva de `callables.w` do Última Luz. Três variantes calculam as mesmas gates,
sequência de tickets e contagem do manifest:

- `separated.w` usa `fn`, `some fn`, `any fn`, `mut fn` e `take fn`;
- `unified.w` usa um único `fn` para pointer e capture e não expressa mutation
  ou consumption no tipo;
- `protocols.w` usa `Callable`, `MutableCallable` e `ConsumingCallable` com
  método `call` e witness nominal.

A forma unified deixa uma segunda call do manifest sintaticamente disponível.
Ela precisa de runtime state, de uma regra invisível ou de uma restrição global
para preservar o consumo. A forma protocol preserva o consumo, mas adiciona
nomes, conformances e `.call`. Ela segue a lattice real de
[`FnOnce`, `FnMut` e `Fn` do Rust](https://doc.rust-lang.org/reference/types/closure.html#call-traits-and-coercions);
uma call sobre `some` ainda pode ser especializada e não implica dispatch por
witness em runtime. A forma vigente separa representação de callable mode e
mantém a call direta `value(...)`.

Os dois inputs cobrem capture com manifest não vazio e o limite vazio com counter
zero. O oracle host confirma o mesmo resultado do restaurante e registra
separadamente representation, dispatch, access mode e recovery de allocation.
Ele não executa W nem prova que erasure ficará inline. O bundle promove
`R0-callable-representations`, `R0-callable-modes` e
`R0-erasure-storage`. O mesmo source também prova
`R0-positional-callable-value`: labels e defaults pertencem à declaration, não
ao function type armazenado. O oracle exige argumento posicional na call
indireta e não transporta o default.

A matriz exata de parameter, ownership, result e error promove
`R0-invariant-callable-signature`. Somente callable mode possui a lattice
declarada em W; variance ou widening de effects não aparece por conversão
implícita. Separar esses dois requisitos em outros bundles repetiria o mesmo
function type e o mesmo call site.

#### 1.3.9 Transformação Python sem comprehension W

**Exemplo:** o pipeline seleciona tickets urgentes até um limite e o loop produz
a mesma lista. Um limite zero e uma lista sem match produzem `[]` nos dois casos.

[`tooling/studies/r1-python-transform/bundle.json`](tooling/studies/r1-python-transform/bundle.json)
deriva do pipeline `urgent` de `collections.w`. O bundle mantém os tickets, o
limite e o outcome. As variantes são:

- `pipeline.w` usa o pipeline `.lazy.filter(...).map(...).take(limit).collect()`.
- `loop.w` usa `for`, `append` e `break` para controlar o limite.

Pipeline é a **Forma vigente** para transformação sem side effect. O pipeline
fica lazy até `collect()`. Loop é a **Forma vigente** para controle e side
effects. `limit: 0` termina antes de acessar a collection no loop e `.take(0)`
produz uma collection vazia no pipeline. Nenhuma variante muda ordem ou
avaliação de um ticket além do limite. O oracle host modela essa fronteira com
`inspected`: o caso primário inspeciona um item, limit zero inspeciona zero e
no-match inspeciona toda a entrada.

A [list comprehension do Python](https://docs.python.org/3/tutorial/datastructures.html#list-comprehensions)
é uma alternativa documental. Ela motiva a comparação, mas não entra na
grammar W. Comprehension W continua **Pesquisa** até evidência humana ou de
modelo mostrar uma lacuna. O task de mudança adiciona auditoria com side effect
e mantém `for` como a forma correta para esse efeito.

O oracle host usa um loop bounded para representar o pipeline lazy. Ele confirma
o input primário e os casos adversariais de limite zero e no-match. Parse e
oracle não ratificam source, não executam W e não substituem `w compile` ou
`w run`.

#### 1.3.10 Broadcast de tensor com shape observável

**Exemplo:** uma matriz `[samples, 6]` subtrai um vetor `[6]` após declarar o
shape alvo. Um vetor `[5]` ou um eixo alterado falha como mismatch.

[`tooling/studies/r1-tensor-broadcast/bundle.json`](tooling/studies/r1-tensor-broadcast/bundle.json)
deriva de `calibrated - means.broadcast(to: [samples, 6])` em `horizon.w`. As
variantes preservam o cálculo, os shapes e os inputs:

- `explicit.w` usa `means.broadcast(to: [samples, 6])`.
- `checked-implicit.w` usa `calibrated - means` como alternativa checked.

Scalar expansion continua implícita total. Broadcast entre shapes diferentes
continua explícito na **Forma vigente**. A alternativa checked fica
**Pesquisa** até provar diagnostics, memória e legibilidade. NumPy evidencia a
conveniência do broadcast. Sua documentação também descreve intermediários,
memória ineficiente e menor legibilidade em dimensões maiores. Essa é a
evidência primária do risco que R1 mede, não autoridade W.

O broadcast pontuado de
[Julia](https://docs.julialang.org/en/v1/manual/arrays/#Broadcasting) fica
**Alternativa** documental. A forma `.broadcast(to:)` é a única forma
semântica W vigente. `calibrated - means` é uma variante W parseável, porém não
aceita no design. O task de mudança inclui scalar expansion e preserva falha
explícita para shape não escalar incompatível.
R1 mede a troca entre a conveniência da variante parseável e a forma vigente.

O oracle host confirma shape válido, mismatch e o shape resultante da mudança de
eixo. O eixo não vira provenance no type. Ele não executa tensor W nem escolhe
uma regra implícita.

#### 1.3.11 Acesso relativo ao fim

**Exemplo:** `.last` retorna `"Horizon cake"` para um menu não vazio e `.none`
para um menu vazio. O tipo é `ref String?` em todas as variantes.

[`tooling/studies/r1-end-relative-access/bundle.json`](tooling/studies/r1-end-relative-access/bundle.json)
deriva de `menu.last` em `billing.w`. O bundle compara três formas:

- `last.w` usa `.last`, que absorve empty no optional.
- `count-minus-one.w` usa `count - 1` e `get` depois de um guard.
- `negative-index.w` usa `[-1]` depois de um guard e cria `.some(...)`.

`.last` continua **Forma vigente** para o caso comum e retorna um optional
seguro sem guard no caller. A aritmética é uma **Alternativa** explícita e
exige guard. Negative indexing fica **Rejeitado por enquanto** e exige guard.
A forma exige decisões para signed/unsigned, `-0`, empty e bounds, além de
mudar o contexto de leitura. O índice Python negativo é a evidência primária em
[Python lists](https://docs.python.org/3/tutorial/introduction.html#lists).

`get(fromEnd:)` e `suffix` ficam **Pesquisa**. O operador `^1` de
[C# ranges and indices](https://learn.microsoft.com/en-us/dotnet/csharp/tutorials/ranges-indexes)
fica **Alternativa** documental. W já usa `^` para XOR fora de units, portanto
`^1` não é uma variante parseada. O task de mudança pede segundo item e tail
slice sem remover o optional de `.last` ou os guards das outras variantes.

O oracle host confirma inputs empty e nonempty e o mesmo outcome `String?` sem
um guard explícito na função `.last`. Parse e oracle não tornam negative
indexing uma forma semântica válida.

#### 1.3.12 Ordem de labels em calls

**Exemplo:** `Money(majorUnits: 42, currency: .cr)` produz 4.200 minor units.
Uma chamada com labels reordenados produz o mesmo resultado hipotético no
estudo.

[`tooling/studies/r1-call-label-order/bundle.json`](tooling/studies/r1-call-label-order/bundle.json)
deriva de `Money` e do call em `billing.w`. As variantes preservam a declaração
e o outcome:

- `fixed-order.w` usa a ordem declarada `majorUnits:, currency:`.
- `reordered.w` escreve `currency:, majorUnits:`.

Labels formam uma sequência de call. A seleção de overload usa essa sequência
antes dos tipos. A ordem de declaração continua **Forma vigente**. Um default
em `currency` cria `majorUnits:,currency:` e `majorUnits:`. Um overload com
`currency:,majorUnits:` mantém uma terceira sequência. Uma política unordered ou
reordered colapsaria as duas sequências completas e produziria diagnostic antes
do ranking por tipos. Reordering fica **Pesquisa** e **Alternativa** até R1 medir
ganho sem mudar resolver ou grammar.

A regra é coerente com a ordem fixa de argumentos descrita na
[documentação de funções de Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/functions/).
Keyword reordering do [Python](https://docs.python.org/3/tutorial/controlflow.html#keyword-arguments)
é evidência documental, não autoridade W. O estudo não altera `Money`, o
resolver ou a seleção de initializer.

O oracle host confirma o outcome base, mantém três shapes fixed-order distintos
e registra a colisão somente na política unordered. Ele não resolve uma call W
nem declara default implementado.

O mesmo bundle promove `R0-call-label-order` e `R0-overload-shape`. A evidência
é a mesma: três sequências de labels continuam distintas antes de qualquer
ranking por tipos. Um segundo bundle mudaria apenas o nome da função e
duplicaria o mecanismo medido.

#### 1.3.13 Destructuring de tuple de shape fixo

**Exemplo:** `let (text, foundLine) = try word()` lê text e line de uma única
call. A variante por projections lê `.0` e `.1` do tuple salvo.

[`tooling/studies/r1-tuple-unpacking/bundle.json`](tooling/studies/r1-tuple-unpacking/bundle.json)
deriva de `word()` em `packages/menu-compiler/compiler.w`. As variantes
preservam parse, result e uma avaliação:

- `binding.w` usa tuple binding de shape fixo.
- `projections.w` salva o tuple e usa `.0` e `.1`.

Tuple e struct destructuring de shape fixo continuam **Forma vigente**. As
projections `.0` e `.1` também são **Forma vigente**; elas medem ergonomia e
exigem `copy` ou borrow explícito para um componente `String` move-only. O task de reparo prova que
nenhuma variante chama `word()` duas vezes. Unpacking starred de aridade
variável fica **Rejeitado por enquanto** por ownership, aridade dinâmica e
partial moves. `each collection` continua expansão de call-rest e não
destructuring.

O [unpacking documentado pelo Python](https://docs.python.org/3/tutorial/datastructures.html#tuples-and-sequences)
é evidência de ergonomia. Ele não adiciona starred unpacking à grammar W. O
oracle host confirma sucesso, empty-input error e `wordCalls: 1`. Parse e oracle
não executam o compiler W.

#### 1.3.14 Carrier tabular TAB0

**Exemplo:** o estudo compara `data.Batch<HorizonReading>`, `data.DynamicBatch`
com binding explícito e `Array<HorizonReading>` para o mesmo telemetry do
buraco negro e do restaurante.

O contrato canônico de `Batch`, `DynamicBatch`, schema identity e ownership está
em [14.4.1](DESIGN.md#1441-carrier-tabular). Esta seção mede apenas a evidência do
estudo e não promove a forma para **Forma vigente**.

[`tooling/studies/r1-tabular-carrier/bundle.json`](tooling/studies/r1-tabular-carrier/bundle.json)
fixa `sequence`, `hawkingFlux` e `warning` opcional. As três variantes
preservam o mesmo resumo e output. A variante `typed-batch` é a **Direção** do
carrier mínimo. `dynamic-batch` é complementar quando o schema só existe em
runtime. `row-array` é uma baseline **Direção** válida para algoritmo centrado
em row, mas é rejeitada como carrier tabular universal. Nenhuma dessas fontes
promove a forma para **Forma vigente**.

O bundle inclui casos de null contra NaN, nome duplicado, column length desigual,
mudança de schema entre satellite chunks, copy `.never` com device incompatível,
UTF-8 e offset inválidos, e release exatamente uma vez. A máquina e o oracle
host modelam publicação, schema identity, selection, O(1) access, scan, copy,
device, chunks, owner, views, waits, children, trust, sanitização e limits.
Eles não compilam nem executam W. A evidência corrente é `design-oracle-input`.
`w compile`, `w run`, estudo humano e estudo de modelo permanecem missing.

As referências comparativas são Arrow
[Columnar](https://arrow.apache.org/docs/format/Columnar.html),
[C Data](https://arrow.apache.org/docs/format/CDataInterface.html),
[C Stream](https://arrow.apache.org/docs/format/CStreamInterface.html) e
[Security](https://arrow.apache.org/docs/format/Security.html), além dos
[requisitos](https://data-apis.org/dataframe-protocol/latest/design_requirements.html)
e da [API](https://data-apis.org/dataframe-protocol/latest/API.html) do Python
dataframe interchange. Elas sustentam comparação de columns, chunks, buffers,
copy e lifetime; não definem a semântica W.

#### 1.3.15 Adapters tabulares TAB1

**Exemplo:** o mesmo resumo do horizonte passa por três variantes de W: upload
CSV typed, archive Parquet snapshot e handoff Arrow IPC stream. A variante
`typed` é a **Direção**. C import trusted é uma quarta rota de boundary, não uma
variante de formato.

[`tooling/studies/r1-tabular-adapters/bundle.json`](tooling/studies/r1-tabular-adapters/bundle.json)
fixa rows, schema identity, outcome, digests, quatro tasks, orders, blinding,
primary e adversarial inputs, e o host oracle. O estudo mede clareza do
workflow, preservação semântica e reconhecimento de ownership. Ele não trata
CSV, Parquet e Arrow como substitutos universais.

Os adversariais cobrem quoted CSV dividido entre chunks, header duplicate,
empty-vs-null, invalid UTF-8, row width, negative finite/NaN, footer e offset
Parquet, decompression bomb, logical mismatch, legacy LIST, checksum,
encrypted sem key, source instability, Arrow schema divergence, dictionary
before definition, replacement em file, endian, `copyPolicy: .never`,
alignment, C untrusted, double release, device-as-CPU e cancellation depois de
progress. O oracle modela metadata, events, ownership e progress. Ele não finge
reader binário Parquet ou Arrow.

`tree-sitter` parse, host oracle e cases são evidência corrente. `w compile`,
`w run`, estudo humano e estudo de modelo permanecem missing. A superfície
derivada liga os símbolos de `data_formats.w` aos requisitos TAB1 e ao ledger.

As fontes de formato preservadas são [RFC 4180](https://www.rfc-editor.org/rfc/rfc4180),
[Parquet file format](https://parquet.apache.org/docs/file-format/),
[logical types](https://parquet.apache.org/docs/file-format/types/logicaltypes/),
[page index](https://parquet.apache.org/docs/file-format/pageindex/) e
[encryption](https://parquet.apache.org/docs/file-format/encryption/), mais Arrow
[IPC](https://arrow.apache.org/docs/format/IPC.html),
[C Data](https://arrow.apache.org/docs/format/CDataInterface.html),
[C Stream](https://arrow.apache.org/docs/format/CStreamInterface.html),
[C Device](https://arrow.apache.org/docs/format/CDeviceDataInterface.html) e
[Security](https://arrow.apache.org/docs/format/Security.html). Python, PyArrow e
Polars são evidência ergonômica, não autoridade semântica.

#### 1.3.16 Workflow single-file PYN1 (superseded)

PYN1 é proveniência histórica e não é uma decisão corrente. O companion
[`pyn1-horizon-rejected.w-rejected.txt`](history/archive/pyn1-horizon-rejected.w-rejected.txt)
e o corpus arquivado em
[`history/archive/pyn1-workflow`](history/archive/pyn1-workflow) preservam os
casos antigos de header, body implícito, lock e promotion. Esses artefatos têm
estado `superseded` e não entram nos gates correntes.

A forma corrente é o workflow module-run RU0 de
[`DESIGN.md` §24.1.2](DESIGN.md#2412-module-run-arquivo-único). O oracle host
[`tooling/module-run-machine.mjs`](tooling/module-run-machine.mjs) cobre módulo
normal, entry explícito, roots package/workspace, resolution aninhada, imports
explícitos, identidade sem path físico, context efêmero e cleanup. O corpus
[`tooling/module-run-cases.json`](tooling/module-run-cases.json) possui 12 casos
e 58 operações. Ele não compila, resolve registry, executa W ou fornece CLI.

#### 1.3.17 Sessão/REPL transacional PYN2

**Exemplo:** `limit` muda de `3` para `4`, `snapshot` preserva `6`, `doubled`
fica unavailable, e uma falha de runtime preserva a generation publicada.

[`tooling/repl-session-cases.json`](tooling/repl-session-cases.json) possui casos
positivos e negativos para identities, ordinal/prompt, classificação synthetic,
contexto hermético, command position, snapshot read-only, fases transacionais,
effects observados, adapter transaction, cross-generation mutation, graph
invalidation, drain preflight, degraded post-publish, persistent scope, reset,
stale base, single writer, output markers e quotas. O corpus usa
[`repl_session_oracle.w`](reference/last-light/repl_session_oracle.w) como
referência parseável e liga cada caso a um símbolo do Última Luz.

O corte corrente tem 70 casos e 298 operações: 56 programas aceitos e 14
rejeitados. Há negativos separados para parse/semantic, cada modo de ownership,
base stale/display, preflight/close/reset, cancellation, provider rollback claim,
structured child lifecycle, output partial/zero e cada família útil de quota; o
snapshot JSONL é regenerável.

O transcript canônico separa `w[n]` de `gN`. Ele usa `fn doubled` como compiled
dependent e `let snapshot` como valor avaliado. `var broken: i32 = "x"` não
publica binding. O caso adversarial do black-hole watcher cobre preflight reject,
drain confirmation, post-publish degraded e force boundary no reset.

[`tooling/repl-session-machine.mjs`](tooling/repl-session-machine.mjs) deriva
state, graph fingerprints, trace, receipts, cleanup, effects e bounded history.
[`tooling/check-repl-session-cases.mjs`](tooling/check-repl-session-cases.mjs)
verifica casos, references e snapshot JSONL sem usar `expected` como semântica.
O host test independente é
[`tooling/repl-session-reference.test.mjs`](tooling/repl-session-reference.test.mjs).
O modelo não compila ou executa W e não promete CLI, runtime ou provider.

PYN3 fecha kernel Jupyter, counters, `presentation.Presentable` e export
reproduzível em [24.1.4](DESIGN.md#2414-apresentação-jupyter-e-export-de-notebooks).
DLPack permanece um adapter científico separado. PYN2 registra somente output bounded
e receipts necessários ao session core.

As fontes comparativas Python/codeop, IPython autoreload, Julia world age, Pluto
reactivity e Jupyter messaging continuam evidência delimitada, não contratos W
adicionais. A matriz de alternativas e a descrição do fixture são evidência de
review, sem IDs de ledger artificiais para bookkeeping, status ou tooling.

#### 1.3.18 Carrier tensorial PYN4

**Fixture:** [`reference/last-light/tensor_interop.w`](reference/last-light/tensor_interop.w).
**Evidence:** [`tooling/dlpack-machine.mjs`](tooling/dlpack-machine.mjs),
[`tooling/dlpack-cases.json`](tooling/dlpack-cases.json),
[`tooling/check-dlpack-cases.mjs`](tooling/check-dlpack-cases.mjs),
[`tooling/dlpack-results.snapshot.jsonl`](tooling/dlpack-results.snapshot.jsonl)
e [`tooling/dlpack-reference.test.mjs`](tooling/dlpack-reference.test.mjs).

PYN4 fecha os drafts de `std.tensor` e `std.dlpack` e mantém
`std.tensor@1` e `std.dlpack@1`
missing. A baseline é DLPack 1.3 versioned, trusted in-process, com release
exact-once, capsule one-shot, queue/device identity provider-scoped, transfer
explícito, lease Python bounded e semântica de zero-copy sem cópia de payload.
O fixture não executa W. A máquina host valida positivos e negativos de dtype,
layout, flags, overflow, alignment, shape, queue happens-before, dynamic bind,
materialização, export, cancellation, close/quarantine, GIL/finalization,
provenance, redaction, untrusted input e ausência de hidden copy.

As fontes preservadas são DLPack [C API](https://dmlc.github.io/dlpack/latest/c_api.html),
[Python API](https://dmlc.github.io/dlpack/latest/python_spec.html) e
[release v1.3](https://github.com/dmlc/dlpack/releases/tag/v1.3), além das APIs
CPython de [capsules](https://docs.python.org/3/c-api/capsule.html) e
[thread state, GIL e finalização](https://docs.python.org/3/c-api/init.html), e
[`from_dlpack`](https://data-apis.org/array-api/latest/API_specification/generated/array_api.from_dlpack.html).
O fixture usa carriers trusted, não publica raw pointer, fecha `open` com
`defer async`, mapeia errors explicitamente e mantém o callback scoped até o
drain de queue, Python lease e release.

O teste host independente repete invariantes de lifecycle, release e queue
matching. O checker exige referências ao fixture, cobertura positiva e negativa
para cada decisão PYN4 e um snapshot JSONL regenerável. A máquina não lê raw
pointer, não chama Python, não inicia provider e não é oracle de execução W.

#### 1.3.19 R1E0 — núcleo de expressions

**Exemplo:** `repeat` executa `decimalDigitCount(0)` uma vez e retorna `1`.

**Fixture:** `numerics.w` fornece `decimalDigitCount` e os testes de power.
`allocation.w` fornece `countEmergencyTokens`. `audio.w` fornece
`AudioBlock.clear`. `restaurant.w` fornece `OrderState.advance` e a atribuição
de estado. Os bundles não alteram estes sources.

Os cinco bundles independentes estão em
[`tooling/studies/r1-post-test-loop`](tooling/studies/r1-post-test-loop),
[`r1-conditional-value-block`](tooling/studies/r1-conditional-value-block),
[`r1-assignment-unit`](tooling/studies/r1-assignment-unit),
[`r1-power-precedence`](tooling/studies/r1-power-precedence) e
[`r1-fluent-self`](tooling/studies/r1-fluent-self). Cada bundle usa de duas a
quatro variantes W, inputs primary e adversariais, quatro tasks,
orders counterbalanced, blinding e digests de source base, variantes e oracle.

##### Direção

`repeat { body } while condition` executa o body ao menos uma vez. A condição
trailing é avaliada depois do body. `continue` termina a iteração e ainda avalia
essa condição. `break` sai do loop sem avaliá-la de novo. O cleanup lexical segue
o caminho normal. `repeat` evita a colisão de `do/catch` e torna o post-test
explícito. `while true` com `break` permanece uma alternativa válida para o
estudo, mas não é a forma selecionada.

`if` produz um value quando está em contexto de value. Sem `else`, o statement
produz Unit. Um resultado non-Unit exige `else`. Cada branch é um named value
block. O block aceita statements e usa o tail sem semicolon como resultado. Um
semicolon descarta a expression anterior. Só o branch selecionado executa. Os
branches formam um único join por identidade ou conversão segura. `Never` não
participa do join. W não possui ternary. Function bodies comuns não são value
blocks e exigem `return`. Closure, `if`, `switch` e `unsafe` mantêm seus
contratos próprios.

Assignment exige um place. W resolve o place uma vez e avalia o RHS uma vez.
Depois do sucesso do RHS, W substitui o value e dropa o anterior. Uma falha do
RHS preserva o value antigo. O resultado de assignment e compound assignment é
`()`. Assignment não encadeia e não duplica owner. Compound assignment lê e
escreve o mesmo place uma vez.

Power usa `**` e associa à direita. Power vence unary no lado esquerdo e aceita
prefix no lado direito. Portanto, `-2 ** 2` vale `-4`, `2 ** -3` vale `0.125` e
`2 ** 3 ** 2` vale `512`. `^` continua bitwise XOR. O `^` em `m/s^2` pertence
à grammar de units e não à expression de runtime. A separação é sintática e
deve ficar visível para pessoas e máquinas.

`: self` é o return contract explícito de um reborrow do receiver. Fallthrough,
`return` e `return self` são equivalentes. O bundle seleciona o fallthrough.
Omitir o return type produz Unit. `: self` não é `Self` owned e não é válido em
`take fn`. O contrato não aloca, copia ou move o receiver.

##### Evidência e comparação

`tree-sitter-parse` e os host oracles são evidência corrente. `w-compile`,
`w-run`, `human-study` e `model-study` permanecem missing. Parse não é
ratificação e o oracle não executa W.

As fontes externas delimitam alternativas de ergonomia. Swift SE-0380 compara
`if`/`switch` expressions e branch typing
([proposal](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0380-if-switch-expressions.md)).
O manual de statements de Swift descreve o post-test loop
([reference](https://docs.swift.org/swift-book/ReferenceManual/Statements.html)).
Swift e Rust tratam assignment como operação sem value
([Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/basicoperators/),
[Rust](https://doc.rust-lang.org/reference/expressions/operator-expr.html#assignment-expressions)).
Rust documenta `if` e blocks como expressions
([if](https://doc.rust-lang.org/reference/expressions/if-expr.html),
[block](https://doc.rust-lang.org/reference/expressions/block-expr.html)).
Python registra a precedência de power
([power](https://docs.python.org/3/reference/expressions.html#the-power-operator))
e fornece conditional e assignment expressions somente como contraste
([conditional](https://docs.python.org/3/reference/expressions.html#conditional-expressions),
[assignment](https://docs.python.org/3/reference/expressions.html#assignment-expressions)).
Nenhuma dessas linguagens é autoridade W.

O checker deriva a cobertura dos bundles, variants, tasks e casos R0. O estado
continua `design-oracle-input` até os gates reais substituírem os oracles. Os
IDs W-1148 a W-1154 registram a fronteira de evidência e o status, sem duplicar
a semântica de W-746, W-769, W-770, W-771, W-772 ou W-147.

#### 1.3.20 Criação do primeiro owner `shared`

**Exemplo:** `let root: shared MenuSection = MenuSection(...)` cria o primeiro
owner com a policy normal. Expression e return criam binding local `shared` e
movem o owner com `take`.

O bundle
[`r1-shared-construction`](tooling/studies/r1-shared-construction) compara três
formas parseáveis: declaração `shared` e calls históricas `share`/`tryShare`
mantidas como alternativas rejeitadas. SHC0 agora seleciona a extensão
declarativa fallible `let root: shared T = try T(allocator: memory, ...)` e
fecha o contrato de publicação do control block. Os inputs cobrem temporary,
binding existente, allocator lexical/custom, payload lifetime-dependent,
admission/open separado e falha antes da publicação do handle. O oracle host
verifica consumo, cleanup, origins e failure policy; ele não aloca um control
block W.

Rust separa `Arc::new`/`Rc::new` das variantes `try_new` e documenta
[`Arc::try_new_in`](https://doc.rust-lang.org/std/sync/struct.Arc.html#method.try_new_in)
e o trait [`Allocator`](https://doc.rust-lang.org/std/alloc/trait.Allocator.html)
como uma escolha de provider, não como parte do ownership spelling. [Swift
ARC](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/automaticreferencecounting/)
mantém reference counting automático para instances de class. O working draft
de C++ define [`make_shared` e `allocate_shared`](https://eel.is/c++draft/util.smartptr.shared.create)
com co-allocation opcional. W não copia essas superfícies. W
permite um único contexto declarativo: initializer de binding ou stored field
cujo tipo `shared T` está escrito. Argumento, return, inference e overload não
promovem. Expression e return criam binding local `shared` e movem com `take`.
SHC0 fecha recovery custom com `try` fora do tipo, cleanup prepublication e
origins separadas de payload/control block. `initializerThrows` e o `failure` do
site allocator são eixos distintos; quando ambos usam o mesmo error type, as
edges colapsam em uma, e quando são distintos o error set explícito deve ser
exato e sem duplicatas. `payloadShareable` vem do tipo/HIR,
`counterThreadSafe` vem do plano de control block e mobility vem da travessia
do `AllocationOriginMap`. A capability/profile custom já deve estar aberta
antes da construção; co-allocation permanece otimização.
Tree-sitter e o
oracle host são a evidência atual; `w-compile`, `w-run`, estudo humano e estudo
de modelos permanecem missing.

`Shared<T>` e `shared<T>` foram consideradas como grafias de container. Elas
foram retiradas porque ausência, payload e ownership são eixos diferentes:
`shared T?` é um handle opcional, enquanto `shared Option<T>` compartilha um
payload opcional. A forma prefixa também permanece coerente com `ref`, `inout`,
`weak` e `view`.

`atomic` pertence a outra categoria. `var atomic value: T` modifica o storage
do binding e baixa para `Atomic<T>`. A forma `atomic T` foi rejeitada porque
confundiria uma policy de acesso ao storage com a identidade de um owner.

Um slot `shared<allocator: .name>` também foi retirado. O nome de um provider
não identifica a instância, lifetime, mobility ou deallocator que originou o
control block. Se o provider é a policy geral, o build profile o seleciona. Se
a instância é scoped e precisa de contrato de construction expression que receba
`allocator:`. SHC0 fecha esse contrato declarativo sem criar um slot no tipo;
o slot continua rejeitado.
Assim o allocator não fragmenta APIs em tipos `shared` incompatíveis nem cria
authority ambiental por import.

#### 1.3.21 Slots estáticos em types e calls

**Exemplo:** `ServiceProfile<enabled: true, tables: 8, courses: 4>` e
`start<enabled: true, tables: 8, courses: 4>()` preservam a identidade dos
mesmos slots. Um head que declara `_ tables: usize` também aceita a aplicação
posicional, sem remover `tables` do body ou da associated contract value.

O bundle
[`r1-static-contract-slots`](tooling/studies/r1-static-contract-slots) deriva de
`Matrix<Element, rows: usize, columns: usize>` no módulo `generics` do Última
Luz. As três variantes mantêm dois inputs assimétricos e o mesmo resultado:

- `named.w` usa labels obrigatórios no type head e na generic call;
- `positional.w` torna todos os labels opcionais com `_` e usa ordinals;
- `split-call.w` mantém o type contract completo, mas move `String` e quantity
  para argumentos runtime da call.

O caso adversarial troca `tables` e `courses`, que possuem o mesmo tipo. A forma
nomeada rejeita a ordem de labels antes da publicação. As formas posicionais
continuam bem tipadas e publicam o significado trocado. O segundo eixo verifica
que Bool, String, quantity e size usam as mesmas categorias estáticas em type
application e generic call; a variante split perde essa paridade.

O estudo não torna labels reordenáveis. A forma da call continua ordenada e o
label identifica cada slot nessa ordem. O oracle host deriva outcome, erro antes
da publicação e paridade de categorias. Ele não executa W. `w-compile`, `w-run`,
estudo humano e estudo de modelos permanecem missing.

A evidência do Restaurante no Fim do Universo é concreta: `reference/last-light/
domain.w` declara `ServiceStage`, `isValidStagePath` e `StagePath<_ stages:
StaticList<ServiceStage><(isValidStagePath(.member))>>`; `state_transitions.w`
usa aliases `StandardStagePath` e `CancelledStagePath` com listas ordenadas.
Esses heads são importados pelo produto Last Light. O seed C desta entrega usa
somente heads `struct` locais, portanto comprova binding, labels, spans,
`ConstValue` e records caller-owned para a fatia local, não uma execução
end-to-end do Restaurante nem a avaliação do predicate.

O limite é deliberado: `StaticArgumentRepresentable` continua sendo o predicate
completo de DESIGN §3.6.3, enquanto este seed materializa apenas Bool, integer,
String simples sem escape, enum case contextual e `StaticList`; computed forms,
quantity/size, `Bytes`, nested lists, generic calls, heads importados e avaliação
ConstIR permanecem posteriores ou `UNSUPPORTED`. Assim a evidência sustenta a
representação sem transformar a proposta em implementação ampla do compiler.

Esta fatia agora fecha a primeira prova pós-frontend sem alterar essa fronteira.
`w_seed_generic_validation_run` consome os records normalizados e o programa
ConstIR já lowered. O gate Bun extrai `OrderId`, `ServiceStage`, `canMove`,
`isValidStagePath`, `StagePath` e o path padrão diretamente de
[`reference/last-light/domain.w`](reference/last-light/domain.w). A camada não
reparseia source. O gate passa o witness uma vez pelo pipeline seed. O path
`[.accepted, .reserving, .preparing, .serving,
.completed]` produz `VERIFIED`. Lista vazia, salto direto para `completed` e
duplicata de `reserving` produzem `REJECTED` com W-CONST-0004, os facts mínimos e
o fallback exato `failure = "predicate:false"`,
`rejectionTrace = ["predicate:false"]`. Duas execuções do mesmo witness têm
status, facts e counters idênticos. Testes C separados cobrem Bool, integer com
width/signedness e bytes canônicos, enum full/subset payloadless, StaticList,
quota W-CONST-0003 sem W4, relações inválidas, categorias unsupported e
capacity caller-owned.

O preflight reutiliza o validador estrutural canônico de ConstIR antes de
qualquer predicate. `EVALUATION_FAILED` preserva W-CONST-0003/W-CONST-0006 e o
`w_seed_constir_eval_result`, inclusive counters. Não confunde falha do
evaluator com ausência de capacidade.
`CAPACITY` preserva sentinels quando a arena é insuficiente. A evidência
caller-owned reserva 15 bytes compartilhados para `failure` e o único item de
`rejectionTrace` publicado pela projeção D1. A capacidade é medida antes da
avaliação. O evaluator atual não publica execution dependencies suficientes
para uma slice causal detalhada, por isso a projeção D1 usa o fallback permitido
inteiro. Imported heads, computed arguments, identity final, monomorphization,
runtime e compiler W completo continuam gaps.

W-1460 acrescenta uma evidence pós-validação, interna e versionada, sem
ampliar essa fronteira. O preimage canônico começa com
`w-seed-generic-fingerprint-1`, usa tags estáveis, lengths/counts big-endian,
text UTF-8 length-prefixed e os tipos/ConstValues fechados de DESIGN
§8.7.12. O módulo local e os nomes declarados vêm do frontend normalizado;
spans, source spelling, labels, índices, allocation, receipts, quotas e
versões ambientais ficam fora. O `body_digest` vem do lowering seed como
evidence: a camada não o recomputa nem o chama de prova criptográfica do body.
O digest só é finalizado depois de todos os predicates retornarem
`Bool(true)`. Resultado não `VERIFIED` recebe estado
`NOT_AVAILABLE` e bytes zero; `VERIFIED` fora do subconjunto
encodable recebe `UNSUPPORTED` sem alterar o resultado principal.
Digests diferentes implicam preimages diferentes; um digest igual isolado não
prova que os preimages são iguais nem constitui identidade collision-safe. O
preimage canônico completo é a autoridade desta projeção.

W-1460 também exige uma resolução pós-frontend read-only para domínios
dependentes. `CONCRETE` usa o domínio declarado; `DEPENDENT` usa somente o
`type_index` do argumento `TYPE` anterior, depois de validar ordem, kind,
status, índice e a igualdade de tipo do `ConstValue`. O tipo concreto resolvido
é codificado no preimage, sem nome `T`, índice process-local ou spelling. Por
isso `StaticValue<Bool, true>` e `StaticValue<String, "The final seating">`
sem predicate são evidence verificável e fingerprintável. W-1461 fecha uma
extensão D2 separada para predicate `String` simples, bounded e borrowed; a
conversão continua rejeitando escapes, interpolation, over-limit e outras
features fora do subset.

A evidence é reproduzível no witness real de
`reference/last-light/domain.w`, com module id `restaurant`:
duas aplicações standard têm o mesmo preimage e digest; a rota cancelled
`[.accepted, .cancelled]` é `VERIFIED` e diferente; vazio,
salto e duplicata continuam rejeitados e sem fingerprint. O gate Bun
reconstrói os bytes e calcula SHA-256 de forma independente, além do teste C,
para evitar que um golden emitido pelo C seja a única autoridade.

O gate também lê `reference/last-light/generics.w`, verifica uma vez as
assinaturas de `StaticValue`, `isFinalCallLabel` e `FinalCallValue`, o body
`export const expected = value` e os aliases `EnabledFeature`/`LastCallLabel`/
`VerifiedFinalCall`. Como o body de associated const ainda está fora da
projeção seed, ele deriva witnesses temporários com a assinatura real e body
`{}`; o gate não afirma que `generics.w` inteiro compila. O reconstrutor Bun
calcula, de forma independente, os preimages Bool e String desses witnesses.

Alternativas rejeitadas:

- usar spans, índices, source spelling ou labels como identidade, pois mudanças
  de formatação, allocation ou frontend não mudam a projeção semântica;
- chamar o digest de `TypeId`, `SemanticInterfaceKey`,
  `WAbiKey`, wire/schema ID ou cache/instantiation key, pois faltam
  declaration digest, witnesses, target/profile/edition/compiler/bundle
  versions e os demais dados da identidade final;
- emitir o fingerprint antes de `VERIFIED`, pois relações inválidas,
  quota, capacity, rejeição ou avaliação parcial não podem produzir evidence;
- confiar somente no C, pois um segundo reconstrutor precisa conferir o
  preimage exato e preservar determinismo entre execuções.

#### 1.3.21.1 String source-backed em predicates genéricos (W-1461)

**Motivação:** DESIGN §3.6.7 já inclui `String` em CE0, mas a conversão D1 do
seed não alcançava um predicate. W-1461 fecha somente essa lacuna executável
source-backed no seed C. O Restaurante fornece um witness natural: a chamada
final recebe `"The final seating"` e a função const compara o valor com `==`.

A solução preserva a superfície W. Não cria keyword, operator ou API pública;
usa somente declarations normais no `Last Light`. O frontend normaliza o
literal fechado para bytes UTF-8 na arena `const_bytes`, com campos append-only
e transação all-or-nothing. ConstIR guarda offset/count no node e bytes
borrowed no value. O limite de 4.096 bytes torna quota, lifetime e digest
bounded sem allocation ou heap quota. O body digest inclui tag, length e bytes
canônicos, e o bump corrente `w-seed-constir-4` versiona os digests de propósito.

O subset é deliberadamente estreito: literal simples, parâmetro/local e
`==`/`!=`; ordering, concatenação, member/index, String result,
escapes/interpolation, Unicode APIs, Bytes, imported predicate/head, computed
generic arguments, identity final, compiler, runtime e self-host permanecem
fora. Over-limit e feature não lowerable retornam `UNSUPPORTED` antes de
evaluation. Arena, pointer/count, relation ou type corruption retorna
`INVALID` no preflight. Um resultado verdadeiro é `VERIFIED` com fingerprint;
um falso é `REJECTED` com `W-CONST-0004` e fingerprint indisponível. As
alternativas de implementar String completa, reparsear spelling ou fazer
avaliação downstream sem o validator canônico foram rejeitadas por ampliar a
superfície, quebrar determinismo ou ocultar corrupção.

#### 1.3.21.2 Expressão const tipada escalar em generic value (W-1462)

**Motivação:** a forma parentetizada já pertence à gramática vigente, mas o
seed D1/D2 não publicava uma relação executável para um value generic
calculado. W-1462 fecha somente a fatia escalar que o ConstIR já consegue
lower: literais, grouping, unary e binary operators em uma árvore fechada,
com resultado `Bool` ou integer de width e signedness explícitos. O Restaurante
usa `isUltimateAnswer(value: i64)` e compara o literal `42` com o cálculo
`(6 * 7)`.

O frontend continua caller-owned e tipado. Ele grava `TypedConstExpr`, liga o
record ao application/argument ordinal e publica `TYPED_PENDING_CONST` sem
avaliar nem chamar ConstIR. Immediate não produz receipt `CONST_ARGUMENT`;
somente a forma pending gera uma função ConstIR sintética zero-arg, com origem
explícita, body digest canônico sem span/trivia/spelling e result type
verificado. A validação faz preflight de relações, funções sintéticas,
predicates e capacities, publica `computed_argument_count`, avalia calculated
arguments em ordem e predicates depois, e conserva receipts locais
`CONST_ARGUMENT, PREDICATE`. Quota é agregada entre as duas fases; heap de
scalar é zero. Uma falha da expressão mantém o receipt causal antes de
`EVALUATION_FAILED`.

O fingerprint recebe o valor ConstIR normalizado exatamente no mesmo encoding
do immediate. Assim `42`, `(6 * 7)` e uma cópia do cálculo compartilham o
preimage e digest; `(6 * 6)` é `REJECTED` com `W-CONST-0004` e bytes zero.
Overflow, quota, unsupported call/identifier/String result e corrupção de
origem, relação, aplicação ou type são casos distintos, com zero-step quando
o preflight rejeita. O gate Bun extrai os markers reais de `generics.w` uma
vez, cruza receipts e steps do seed C, reconstrói independentemente o preimage
i64 e calcula SHA-256.

Limites honestos: isto não implementa identifiers/named const, dependencies ou
cycles do graph const, imported heads/predicates, resultado computed `String`,
identity final, compiler/runtime ou self-host. O caso é oracle-backed-current,
não conformance de W completo.

#### 1.3.21.3 Module named const no generic value (W-1463)

**Motivação:** D3 já permitia calcular uma expression escalar no call site,
mas não permitia que a expression usasse um valor nomeado publicado pelo
source. W-1463 fecha a menor extensão útil: declarations `const` já aceitas
pela gramática, com type explícito, no mesmo módulo e com dependências locais
bounded. Isso torna `UltimateAnswer<(ultimateAnswer)>` observavelmente igual a
`42` e `(6 * 7)` sem criar syntax nova ou inferência de initializer.

O frontend continua caller-owned. Ele publica o record da declaration, spans,
declared type, initializer, ranges e relação explícita do identifier, resolve e
tipa em ordem source e mantém `TYPED_PENDING_CONST`; não avalia, não chama
ConstIR e não materializa `ConstValue`. Locals e parameters mantêm precedence.
ConstIR registra a origem `FRONTEND_CONST_DECLARATION`, baixa cada declaration
como função sintética zero-arg e baixa cada referência como dependency `CALL`.
O body digest omite spans, trivia e spelling e inclui a identidade/digest das
dependencies para evitar colisões estruturais.

O preflight do grafo é anterior aos counters de cache e aos steps reais do
evaluator. Relações corrompidas produzem `INVALID` sem step; uma dependency
bem formada fora do subset produz `UNSUPPORTED` sem step; ciclo alcançável
produz `EVALUATION_FAILED` com `W-CONST-0002`, counters zero e caminho
determinístico fechado na ordem causal. Com capacidade de receipt, o ciclo
publica exatamente o `CONST_ARGUMENT` causal antes do retorno; com capacidade
zero, não publica receipt. O limite é 256 dependencies alcançáveis: um grafo
bem formado com 257 declarations é `UNSUPPORTED` com failure
`dependency-limit`, o caso `dependencyLimit`, antes de conversion ou step.
Uma dependency bem formada fora do subset mantém a failure `function`.
Esse limite de grafo não é arithmetic overflow. Uma declaration lowerable como
`const overflowValue: i8 = 127 + 1`
falha durante evaluation com `EVALUATION_FAILED`/`W-CONST-0006`, publica um
único `CONST_ARGUMENT`, não executa predicate e deixa o fingerprint
indisponível. O receipt
`CONST_ARGUMENT` precede o retorno de ciclo, predicates posteriores não
executam e quota continua agregada computed→predicates. O gate
`tooling/check-seed-generic-validation.mjs` lê os markers reais de
`generics.w`, cruza probe C e oráculo Bun independente e registra
`GPF0-W-1463-current`; o witness cobre forward chain, duplicate, rejected,
cycles self/2/3, unreachable cycle, corruption, zero capacity,
`dependencyLimit`, `arithmeticOverflow`, quota e unsupported forms.

Alternativas de resolver durante a execução, materializar initializers no
frontend, aceitar imports/associated const ou usar memoization global foram
rejeitadas. Elas ocultam ownership, alteram ordem de receipts, ampliam o
ambiente de resolução ou introduzem estado sem contrato. Inferência de
initializer, compiler completo, imports, associated const,
cache compartilhável/cross-argument/session, identity final, runtime e self-host
permanecem limites explícitos. A memoização local por invocação é fechada em
W-1464; o caso D4 é
`oracle-backed-current`, não compiler conformance.

#### 1.3.21.4 Memoização local de DAG de module const (W-1464)

**Motivação:** D4 reavaliava o corpo de cada `CALL` de module const. Em uma
árvore compartilhada, essa regra podia repetir o mesmo subgrafo e ocultar o
custo real do argumento, embora o preflight já limitasse o grafo a 256
dependencies. W-1464 fecha somente uma tabela de memoização por invocação de
`w_seed_constir_evaluate`; não fecha o cache compartilhável completo de
§3.6.5.

A tabela é fixa, allocation-free, local e vazia no início de cada evaluation.
A chave é a identidade da declaration no programa fixo da invocação. O
primeiro acesso marca `ACTIVE`, conta um miss e avalia o corpo. Somente sucesso
completo com `ConstValue` válido vira `READY`; o acesso pronto conta um hit,
copia o valor e não reavalia o corpo. O `CALL` do hit ainda cobra seu próprio
step, mas não cria frame nem aumenta call depth. Falha, panic, quota, valor
inválido e estado `ACTIVE` não são reutilizáveis. A próxima invocação não vê
estado anterior. Lookup linear é deliberado: o overhead adicional da tabela é
`O(E*R)`, `R <= 256`, com espaço `O(R)`; esse limite não descreve o custo total
do evaluator, que também faz o lookup próprio de `program_function_for_const`.
Cada dependency de module const alcançada por um `CALL` memoizado na avaliação
generic D5 é avaliada no máximo uma vez. A função usada diretamente como entry
de `w_seed_constir_evaluate` não é pré-semeada na tabela.

O preflight genérico permanece a autoridade causal. Ciclos, zero capacity,
limite de dependencies e corrupção rejeitam antes dos counters de cache e dos
steps reais; ciclos preservam o receipt `CONST_ARGUMENT` causal quando há
capacidade, e zero capacity não publica receipt. `ACTIVE` é somente uma defesa
do evaluator e não altera `W-CONST-0002`, paths ou precedence. Os counters append-only `const_cache_hits` e
`const_cache_misses` ficam em cada eval result e em cada evaluation receipt.
Eles são evidence, não parte do fingerprint, body digest, type identity ou
cache key compartilhável. A quota cobra o trabalho executado: o diamond de
Last Light (`answerSeed`, `firstAnswerHalf`, `secondAnswerHalf`,
`assembledUltimateAnswer`) tem quatro misses, um hit e sete steps; quota 7
permite o argumento isolado e quota 6 falha com `W-CONST-0003`. D3 e D4 linear
mantêm zero hits.

O teste C direto bypassa o preflight generic apenas para exercitar a defesa
`ACTIVE`: o ciclo retorna `W-CONST-0002` com 2 misses, 0 hits, 3 steps e call
depth 3, e a invocação seguinte repete os números. Isso não promove o
evaluator a autoridade causal nem altera o receipt de ciclo do validator.

O witness `GPF0-W-1464-current` liga o marker source-backed real em
`reference/last-light/generics.w` ao probe C e à reconstrução Bun independente
do grafo, source order, counters, steps, reset entre invocações, quota e
falha aritmética não cacheada. Immediate `42`, D3 `(6 * 7)`, D4
`UltimateAnswer<(ultimateAnswer)>` e D5 `UltimateAnswerShared`, inclusive a
aplicação duplicada, publicam o mesmo fingerprint. O caso é
`oracle-backed-current`, não compiler, runtime ou self-host completo.

O limite deliberado inclui cache compartilhável de §3.6.5, cache
cross-argument/session, imports, associated const, initializer inference,
identity final, runtime e self-host. Também não há sintaxe nova, estado global,
cross-thread, cross-program ou persistência.

#### 1.3.21.5 Sessão de avaliação por aplicação (W-1465)

**Motivação:** D5 evitava a reavaliação de uma declaration dentro de uma
evaluation, mas duas arguments irmãos da mesma aplicação ainda começavam com
uma tabela vazia. O Restaurante precisa fatorar naturalmente o mesmo
`assembledUltimateAnswer` em dois value slots sem introduzir syntax nova ou
estado compartilhado entre aplicações.

A forma corrente mantém `w_seed_constir_evaluate` público e inicia uma sessão
privada, vazia e allocation-free para cada chamada direta. O generic validator
cria uma sessão privada imediatamente antes do loop de argumentos calculados.
Ele passa os argumentos `TYPED_PENDING_CONST` em ordem para
`evaluate_in_session`; immediate arguments continuam convertidos na mesma
posição. A sessão termina quando a fase de argumentos termina ou falha.
Predicates continuam chamando a entry pública e, portanto, recebem uma sessão
nova. Nenhum estado de sessão chega ao caller, ao fingerprint ou à próxima
aplicação.

A sessão reutiliza a tabela fixa de 256 declarations do D5. O limite coincide
com `W_SEED_GENERIC_VALIDATION_MAX_CONST_DEPENDENCIES`, porque o preflight D4 já
prova a união alcançável dos DAGs calculados da aplicação dentro desse teto.
Uma asserção estática documenta a igualdade. A chave continua sendo a
identidade de declaration do programa fixo. `ACTIVE` conserva a defesa de ciclo
do evaluator. Somente um resultado completo, válido e bem-sucedido vira
`READY`; falha, panic, quota, valor inválido e `ACTIVE` ficam inutilizáveis.
Não há eviction, heap, persistência, estado entre runs ou API pública.

Os counters continuam append-only por evaluation e receipt. A quota continua
agregada pela mesma sequência de `quota_consume`, e call-depth continua sendo
um teto por evaluation. O witness de dois slots demonstra 7 steps, 4 misses e
1 hit no primeiro argument. O segundo irmão demonstra 1 step, 0 misses e 1 hit.
Quota total 8 aceita os dois. Quota 7 aceita o primeiro e falha o segundo antes
do lookup, com 0 steps, 0 misses e 0 hits na segunda evaluation. Uma nova
aplicação ou run repete 7/1 e reinicia a sessão. Falha no primeiro calculated
argument impede o segundo. Ciclo, corrupção e dependency-limit continuam sendo
decisões de preflight com counters zero.

O Last Light acrescenta `AnswerPair<_ left: i64, _ right: i64>` com o membro
estático `agrees = left == right` e as aliases `ConsistentUltimateAnswer` e
`ConsistentUltimateAnswerDuplicate`, textualmente equivalentes, com
`assembledUltimateAnswer` nos dois slots. O witness
`GPF0-W-1465-current` cruza as duas aplicações, uma quota compartilhada, um novo
run e uma reconstrução Bun independente da preimage de dois i64. O caso mantém
o witness D5 de argumento único e seus 7 steps, 4 misses e 1 hit.

Alternativas rejeitadas:

- expor a sessão ao caller, pois isso cria estado mutável fora do seed compiler;
- usar tabela global, persistente ou compartilhada, pois mistura aplicações,
  runs, programas ou threads e torna quotas e receipts imprevisíveis;
- compartilhar a sessão com predicates, pois a causalidade de predicates
  deixaria de ser uma evaluation independente;
- incluir counters ou sessão no fingerprint, pois factoring equivalente mudaria
  a preimage sem mudar os dois valores semânticos;
- ampliar o teto, usar eviction ou alocar no heap, pois o preflight já prova o
  limite de 256 e a sessão deve morrer com a aplicação.

O caso é `oracle-backed-current`, não compiler, runtime ou self-host completo.
Cache compartilhável de §3.6.5, imports, associated const, initializer
inference, identidade final e sintaxe W nova permanecem fora do bundle.

#### 1.3.21.6 Inferência scalar append-only de module const (W-1466)

**Motivação:** D4/D5/D6 já baixavam declarations locais, mas exigiam uma
annotation redundante em cada node do diamond do Restaurante. O design vigente
já mostra `const pageSize = 4096`, e §15.1.1 fixa `Int` como a identidade pública
de `i64`. W-1466 fecha somente a inferência bounded desse subset, sem criar
sintaxe, sem avaliar ConstIR no frontend e sem prometer um compiler completo.

O frontend publica primeiro todos os nomes e relações de module const locais.
Depois resolve o grafo acíclico independentemente da ordem source e normaliza
os initializers. Uma annotation é constraint fixa. Sem annotation, Bool literal,
expressão booleana ou comparison resolve `Bool`; suffix integer resolve o tipo
exato; identifier propaga o `effective_type` do alvo; e operators usam as
regras de §15.1.2 e precisam de uma solução única. Um componente integer sem
contexto materializa `Int`, isto é, `i64`. Forward references tipadas ou não
tipadas são válidas. O subset continua limitado a literal, grouping, unary,
binary e identifier de module const local, com resultado Bool ou integer de
width/signedness explícitos. Calls, member/index, imports, associated const,
String/Bytes, enum/list/quantity/size, nested generic, runtime e self-host
continuam fora e retornam `UNSUPPORTED` quando a forma é bem formada.

A API é append-only: `declared_type` mantém o índice da annotation source ou
`NONE`; `effective_type` é publicado no record de const e nos receipts/digests
necessários; `has_explicit_type` continua indicando somente presença source.
Symbols, ConstIR e generic validation usam o tipo efetivo. Dry sizing e emit
derivam os mesmos dois campos antes de criar records, portanto a inferência não
depende da existência de records emitidos. A validação preserva mismatch,
unresolved, duplicate e corrupção. Ciclo alcançável, inclusive sem anchor de
tipo, tem precedência causal `W-CONST-0002` e path determinístico; falha antes
de evaluation, receipt posterior, counters e fingerprint, com os limites D4.

O scratch de inferência é `_Thread_local`, temporário por thread e reset por
cada chamada `measure`/`run`; ele não é estado persistente nem parte da
semântica pública. Bases por documento traduzem ordinais locais para o índice
global. O teto explícito é `W_SEED_FRONTEND_MAX_CONST_DECLARATIONS` (32768)
declarations totais; o excesso falha antes de publicar output. A tradução usa
scans locais bounded, com custo máximo O(N²) no total de declarations, sem
alocação heap ou lookup O(N³) evitável.

O schema sobe para `w-seed-frontend-7` e `w-seed-generic-validation-7`.
ConstIR-6 e generic-fingerprint-1 ficam estáveis porque a estrutura lowerada e
o preimage público não mudam. O tag constante de type framing antes do tipo
efetivo preserva o body digest ConstIR-6 das declarations D4 explícitas e dá à
forma D7 inferida o mesmo preimage quando a semântica é igual. Annotation
presence, source order, spans/trivia, trabalho de inferência e índices
process-local não entram no ConstIR body digest nem no fingerprint. A autoridade
do fingerprint continua sendo o domain type canônico e o valor normalizado;
`effective_type` é verificado contra essa relação. Assim declarations
explicitamente anotadas e inferidas com o mesmo tipo efetivo e valor têm o
mesmo fingerprint.

Receipts usam `effective_type` de forma uniforme: `CONST_ARGUMENT` usa o tipo
do value; `PREDICATE` usa o tipo efetivo do argumento de entrada/domínio, não o
`Bool` de `eval_value`; e o receipt de ciclo usa o tipo efetivo do argumento
tipado. A forma bem formada que permanece fora do lowering preserva
`effective_type=NONE` e usa o container ConstIR non-lowerable para auditoria.

O Last Light preserva `export const ultimateAnswer: i64 = 6 * 7` como regressão
explícita e remove annotation somente de `answerSeed`, `firstAnswerHalf`,
`secondAnswerHalf` e `assembledUltimateAnswer`. O oracle Bun reconstrói as
annotations opcionais, resolve o diamond de forma independente e calcula o
preimage sem ler counters do C. Os quatro records têm
`explicit=false`, `declared=NONE` e `effective=i64`; o symbol exportado usa
`i64`. O witness `GPF0-W-1466-current` mantém os counters D6 `7/4/1` e
`1/0/1`, além de integer default, Bool, suffix, propagation, forward/reordered
graph, equivalência explicit/inferred e ciclos ancorado/não ancorado. O caso
incompatível compara path `0,1,0`, `W-CONST-0002`, count, receipt causal,
counters zero e fingerprint indisponível entre o output C e a reconstrução Bun;
o caso multi-slot prova count 2 com um receipt e count 2 com zero receipts em
capacity zero. Mismatch, unresolved, duplicate, unsupported, corruption,
zero capacities, dependency-limit, quota e overflow continuam cobertos pelos
gates existentes.

Alternativas rejeitadas:

- escrever o tipo inferido em `declared_type`, pois isso apaga a distinção entre
  source e solução e torna receipts dry/emit dependentes da ordem de emissão;
- inferir durante evaluation ou deixar ConstIR resolver names, pois isso muda
  ownership, causalidade de ciclos e o limite do frontend;
- incluir annotation, source order ou trabalho do solver no digest, pois formas
  semânticas equivalentes deixariam de compartilhar fingerprint;
- aceitar imports, associated const ou inference contextual além do grafo
  local, pois isso amplia o ambiente e exige identity/cache/runtime não
  fechados.

O caso é `oracle-backed-current`, não compiler, runtime ou self-host completo.
Identity final, imports, associated const, cache compartilhável, runtime e
self-host permanecem gaps explícitos.

#### 1.3.22 Subject de refinement

**Exemplo:** `String<(.scalars.count in 1...40)>` e
`String<(value.scalars.count in 1...40)>` baixam para o mesmo predicate. A
primeira é a forma curta vigente. A segunda torna o candidate explícito.

O bundle
[`r1-refinement-subject`](tooling/studies/r1-refinement-subject) deriva de
`BoundedText` no módulo `domain` do Última Luz. Ele mantém os mesmos títulos,
limites e outcomes em três variantes:

- `contextual.w` usa projection iniciada por `.`;
- `explicit.w` usa o binding contextual `value`;
- `runtime-check.w` remove o refinement do tipo e valida dentro da função.

Um título com 41 scalars mostra a diferença de contrato. As duas formas
refinadas rejeitam um literal inválido na declaração. A variante runtime aceita
o alias e só falha quando a função executa. Um input com astral scalar evita
confundir UTF-16 code units com Unicode scalars.

W-1288 fecha a colisão de nomes: num slot resolvido como refinement, `value` é
o candidate e precede lookup lexical. Um símbolo externo homônimo exige
qualificação. Em outro slot estático, `value` continua um nome lexical comum.
Essa regra mantém `.member` e `value.member` na mesma ConstIR e impede que um
import mude o significado do refinement.

As formas `T where ...` e `T(where: ...)` continuam apenas como alternativas
históricas. A primeira cria uma frase trailing com outro scope; a segunda parece
construção runtime. Elas não precisam entrar na grammar para que o estudo meça
o eixo principal entre subject implícito, explícito e validação fora do tipo.
O oracle host não executa W. `w-compile`, `w-run`, estudo humano e estudo de
modelos permanecem missing.

#### 1.3.23 Members associados diretos

**Exemplo:** `Course.count` e `Course.fromOrdinal(3)` pertencem ao namespace
compile-time de `Course`. Eles não criam uma instance singleton do tipo.

O bundle
[`r1-associated-members`](tooling/studies/r1-associated-members) compara duas
formas com os mesmos cases de `Course`, inputs e outcomes:

- `direct.w` declara `const` e `static fn` diretamente no enum;
- `protocol-requirement.w` introduz um protocol e usa dispatch generic estático.

A forma direta evita um witness quando nenhum consumidor é polimórfico. A forma
com protocol continua válida quando uma função generic precisa exigir o
contrato. O oracle deriva a mesma seleção de course nas duas variantes e mede o
protocol adicional como diferença estrutural, não como diferença de resultado.

O adversarial `static var` é rejeitado antes de publication. Um valor mutável
associado ao processo usa owner explícito no `entry`; state com identity e turns
usa service. Assim, `Type.member` não esconde initialization, synchronization
ou destruction global. O oracle host não executa W. `w-compile`, `w-run`, estudo
humano e estudo de modelos permanecem missing.

#### 1.3.24 Múltiplos initializers por forma

**Exemplo:** `Money(minorUnits: 4_200, currency: .cr)` e
`Money(majorUnits: 42, currency: .cr)` constroem o mesmo valor por duas formas
de call disjuntas. As declarations usam `minorUnits value:` e
`majorUnits value:`: o primeiro nome é o label externo e `value` é interno.

O bundle
[`r1-multiple-initializers`](tooling/studies/r1-multiple-initializers) compara:

- dois initializers com labels externos distintos;
- um initializer com `MoneyInput` como mode value;
- um initializer canônico e `Money.fromMajorUnits` como factory.

As três variantes preservam minor units exatas. A forma selecionada mantém
`Type(...)` nos dois caminhos sem usar ranking por tipos. A variante única
introduz um branch de mode. A factory troca a forma de construção para apenas
um dos inputs.

O input `i64.max` prova a totalidade: depois da conversão exata para `i128`, a
multiplicação por 100 ainda cabe em `i128`. Marcar esse caminho como `throws`
obrigaria `try` para uma falha impossível. Uma futura conversão textual pode
lançar sem contaminar os dois caminhos inteiros. O oracle host não executa W.
`w-compile`, `w-run`, estudo humano e estudo de modelos permanecem missing.

#### 1.3.25 Labels uniformes em callables

**Exemplo:** `fn reserve(order: Order, audit: Audit, id: ReservationId)` aceita
`reserve(order, audit, id)`. A posição de `audit` e `id` não cria labels. Uma
API que deseja comunicar papéis declara-os: `fn move(from source: Point, to
destination: Point)` exige `move(from: current, to: next)`.

A regra anterior tornava o primeiro parâmetro posicional e os seguintes
nomeados. Ela economizava tokens na declaration, mas exigia memorizar uma
exceção por índice e fazia uma simples inserção de parâmetro alterar a forma das
calls seguintes. W-1290 usa quatro formas ortogonais:

- `name: T` é posicional em qualquer índice;
- `named name: T` exige o label `name:` e mantém o mesmo binding;
- `external internal: T` exige `external:`;
- `_ name: T` aceita a forma posicional ou `name:` no mesmo slot.

Initializers e payloads labeled permanecem record-like. Nesses contratos,
`name: T` exige label em qualquer índice porque os argumentos descrevem fields
ou cases, não uma sequência callable comum. Assim, a regra não enfraquece
`Type(field: value)` nem a evolução de schemas.

A máquina de execution ergonomics deriva formas completas antes de type
ranking, detecta colisões por owner e ignora payload declarations de enum. O
gate `check-source-call-shapes.mjs` aplica a mesma derivação ao produto Última
Luz e à std. Ele valida calls diretas resolvíveis no arquivo. Para calls
importadas ou de member, o gate só acusa a migração quando toda declaration
corrente conhecida rejeita a forma e a policy retirada a aceitava. Name
resolution e witness conformance completos continuam responsabilidade do
checker S0. Nenhum desses oracles executa W. Estudo humano e estudo de modelos
permanecem missing.

#### 1.3.26 Posição do contrato de ownership

W-1291 mantém labels e bindings antes de `:` e coloca `ref`, `inout`, `take` e
`const` no início do contrato à direita. A forma `value: take T` comunica que
`value` recebe ownership sem fazer `take` parecer um label externo. Ela também
fica alinhada com `value: shared T`, resultados `ref T`, function types
`fn(take T)` e calls `take value`.

A alternativa `take value: T` aproxima declaration e operação, mas conflita
visualmente e estruturalmente com `external internal: T`. Ela já havia sido
interpretada como label pelo CST em partes da std. `copy value: T` e
`pin value: T` seriam ainda mais problemáticos: são operações do caller e podem
ter custo ou storage observável, portanto não são modos aceitos na assinatura.
O oracle de execution ergonomics e o gate de source mantêm essas formas como
evidence negativa. Eles não executam o checker W.

#### 1.3.27 Operação de ownership no call site

W-1292 exige `ref`, `inout`, `take`, `copy` ou `pin` quando a call cria a
operação sobre um place existente. Exigir um marker em todo argumento de
`ref T` ou `take T` tornaria `inspect(makeValue())` e `store(Value())`
redundantes e permitiria `ref` sobre rvalue, embora `ref` exija place. O outro
extremo, omitir sempre `ref`, esconderia a criação de um loan de owner lvalue.

A regra vigente usa type e value category. Borrow já existente e rvalue novo
passam sem marker; owner place mostra a operação. O source-call gate consegue
rejeitar operações explicitamente incompatíveis e labels inválidos. Somente S0
consegue provar que um argumento sem marker era owner place, borrow ou rvalue.
Os oracles host não substituem essa análise.

#### 1.3.28 R1H0 — estudos de ergonomia de tempo e memória

Este bundle materializa quatro estudos independentes. Cada estudo mantém o
mesmo input, outcome lógico e trace de cleanup entre variantes genuínas ou
explicitamente modeladas como candidatas. A forma com papel `selected` é a
baseline corrente. Ela não recebe ratificação humana ou de modelo por existir
no bundle.

##### Nomes de suspensão

O bundle [`r1-suspend-accounting-names`](tooling/studies/r1-suspend-accounting-names)
fica como evidência histórica. A decisão ASC0 atual usa `HostSuspendPolicy` com
`included`, `excluded` e `unspecified`. `Clock.hostSuspendPolicy` é uma
inspection passiva; a aquisição default é `process.clock()` nonthrowing quando
o Context concede a capability, e a seleção ativa é
`try process.clock(hostSuspend: .included)` (ou a forma longa
`process.context.clock(...)`). O request ativo usa o tipo estreito
`HostSuspendPolicy<[.included, .excluded]>`; `.unspecified` é diagnostic em
compile time, não uma solicitação. Provider unsupported para um case válido
continua uma falha typed antes do trabalho.

Ambas as formas descrevem somente suspensão do HOST/SO. `await`, task e
coroutine não entram nesse fato. O cenário de 60 ms ativos, 50 ms de suspensão
HOST/SO e deadline de 100 ms produz 110 ms e alcance para `included` ou
`counted`. Ele produz 60 ms e ausência de alcance para `excluded` ou `paused`.
No restaurante, 1 minuto ativo e 8 minutos de sleep do host diante de uma
deadline de 5 minutos expiram em `included` no resume. `excluded` retorna com 4
minutos restantes. `unspecified` não sustenta um profile que exige uma das duas
regras. Host suspend inclui sleep, hibernate e VM pause. Não inclui `await` ou
task que apenas suspende W. Os candidatos históricos foram
`HostSuspendPolicy`, `ClockSuspendBehavior` e `HostSleepBehavior`. O design
atual usa `HostSuspendPolicy`; esta nota não reabre a decisão normativa.

##### Aquisição de owner fraco

O bundle [`r1-weak-owner-acquisition`](tooling/studies/r1-weak-owner-acquisition)
usa a forma contextual corrente e registra `.upgrade`, `.strong` e `.strong()`
como alternativas retiradas. As alternativas passam para **Rejeitado por
enquanto**. Os mesmos estados live, expired e de corrida com o último strong
release retornam um owner `shared` ou `none`. Weak não acessa payload. A
linearização ocorre antes ou depois do release final, sem ressurreição de
endereço reutilizado.

##### Escopo de allocator

O bundle [`r1-arena-scope`](tooling/studies/r1-arena-scope) fica como evidência
histórica de uma API retirada. ASC0 escolhe a declaração lexical
`allocator scratch: .fixed<capacity: N> { ... }` ou a forma anônima
`allocator .fixed<capacity: N> { ... }`. Construções diretas e calls cujo callee
publica o slot contextual usam o allocator corrente. A omissão é preenchida
somente por um slot `allocator name: ref Allocator` primeiro, único e standard.
O bloco fecha admission, children/waits/loans/dependents, typed drops e só então
storage.

A restrição anterior, que exigia `allocator:` explícito em toda call, foi
substituída por W-1349. Ela escondia a continuidade do caso de uso sem criar
uma prova adicional. A assinatura continua explícita para type, HIR e ABI, e a
forma explícita continua disponível para override e APIs ordinárias.

Todos os inputs preservam storage fixed e bounded, ausência de alocação do OS,
drop ledger antes de bulk release, proibição de escape, `rehome` antes de uma
fronteira e cleanup em scope exit ou unwind. O oracle compara a mesma ordem de
drop e release. Ele não executa allocator W.

##### Slot runtime do allocator

O bundle [`r1-allocator-runtime-slot`](tooling/studies/r1-allocator-runtime-slot)
usa `Array<String>(allocator: memory)` e uma call customizada
`CustomObj(using: recipe)`. `using:` continua local e livre. Em construction
expressions, `allocator:` é um control argument reservado, canonicamente antes
dos demais, fora da initializer signature. Ele governa somente allocation sites
publicados pelo contrato da construção. `CustomObj(allocator: memory, a: 1,
b: 2)` é válido apenas quando `CustomObj` publica storage alocável. Capability,
origin e mobility vêm do contrato do parâmetro e do `AllocationOriginMap`. O
estudo rejeita `Allocator<(.crossDomain)>` source-visible e inferência pelo
texto `using:`.

A capability `memory` mantém identidade e lifetime. Type e element permanecem
contratos estáticos. Allocation origin, mobility, failure policy e output do
caminho vigente são os facts observados pelo oracle. O label de uma API user não
muda esses facts.

##### Fronteira da evidência

Os quatro bundles registram `tree-sitter-parse` para as variantes `.w` e
`host-oracle` como evidência corrente. `w-compile`, `w-run`, `human-study` e
`model-study` continuam missing. A região é um witness textual reservado, não
parte do parse 71/71. O checker registra essa natureza no campo `parseEvidence`
do variant.

##### Fontes primárias do bundle de memória

O estudo histórico de Arena usa três fontes primárias. O
[LLVM BumpPtrAllocator](https://llvm.org/doxygen/classllvm_1_1BumpPtrAllocatorImpl.html)
documenta alocação bump e reset em lote. O
[C++ monotonic_buffer_resource](https://eel.is/c++draft/mem.res.monotonic.buffer)
define o recurso monotônico para poucos objetos e liberação conjunta. A
[documentação de weak de Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/automaticreferencecounting/#Weak-References)
declara weak em storage optional e usa optional binding. W preserva esse padrão
para o storage weak, mas usa copy explícito para owners já existentes. A leitura
weak é a única aquisição contextual porque não existe owner forte para copiar.

ASC0 também compara [Odin implicit context](https://odin-lang.org/docs/overview/)
e o escopo de cleanup de [`using` em C#](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/statements/using).
O primeiro mostra por que um contexto implícito transitivo em toda call esconderia
efeitos; o segundo é uma comparação de cleanup, não um modelo de allocator W.
Para a estratégia física, [rustc_arena](https://github.com/rust-lang/rust/tree/master/compiler/rustc_arena)
e [LLVM BumpPtrAllocator](https://llvm.org/doxygen/classllvm_1_1BumpPtrAllocatorImpl.html)
informam bulk reclaim, mas não definem a surface source.

Para a seleção de relógio, o provider pode diferenciar
[Linux `CLOCK_MONOTONIC` e `CLOCK_BOOTTIME`](https://man7.org/linux/man-pages/man3/clock_gettime.3.html),
[Windows `QueryUnbiasedInterruptTime`](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryunbiasedinterrupttime)
e [Apple `mach_continuous_time`](https://developer.apple.com/documentation/kernel/3198725-mach_continuous_time)
ou [`mach_absolute_time`](https://developer.apple.com/documentation/kernel/1462446-mach_absolute_time).
Essas fontes sustentam que included/excluded é uma propriedade do provider;
elas não justificam um relógio global ou uma inferência de `.unspecified`.

#### 1.3.29 R1S1 — estrutura de source e formatter

R1S1 promove 21 casos R0 em oito bundles organizados em sete famílias.
O contexto concreto usa estas fontes centrais: `formatting.w`, `generics.w`,
`build.w`, `domain.w`, `billing.w`, `kitchen.w`, `hardware.w`, `callables.w`,
`app.w` e `oracle.w`. Os bundles também podem usar outras fontes reais via
`sourceRefs`. Cada bundle fixa um
`sourceBase` e um symbol real. Snippets compostos ficam na variante e não
alteram o source base. Um `sourceRefs` opcional sustenta constructs adicionais
de fontes reais sem criar uma segunda autoridade.

Os bundles são:

- [`r1-source-boundaries`](tooling/studies/r1-source-boundaries), para newline,
  semicolon e formatter fixo;
- [`r1-static-contract-syntax`](tooling/studies/r1-static-contract-syntax), para
  envelopes ligados, closes nested e contrato local;
- [`r1-data-declaration-surface`](tooling/studies/r1-data-declaration-surface),
  para struct transparente e object encapsulado;
- [`r1-manifest-surface`](tooling/studies/r1-manifest-surface), para o manifest
  data-only e o witness de package inline;
- [`r1-pattern-surface`](tooling/studies/r1-pattern-surface), para patterns
  nominais, tuple scrutinee, cases fechados e rest externo;
- [`r1-callable-property-surface`](tooling/studies/r1-callable-property-surface),
  para property segura, slot de linguagem e closure;
- [`r1-source-phase-surface`](tooling/studies/r1-source-phase-surface), para
  import phase e body de function;
- [`r1-delimited-value-surface`](tooling/studies/r1-delimited-value-surface),
  para matrix nested e tuple de um elemento.

A variante `selected` preserva as formas vigentes dos casos R0. Cada
`alternative` usa somente uma forma já registrada no caso correspondente. Uma
forma que a grammar corrente não aceita usa witness textual não-`.w` com
`parseEvidence.status: reserved-not-parsed`. As variantes `.w` passam pelo
Tree-sitter sem recovery.

Os inputs incluem casos primary, adversariais e candidatos explícitos para cada
variante. Cada oracle remove `expected` antes de derivar o outcome e compara o
resultado exato de cada input. Eles cobrem fronteiras vazias e de limite,
nesting, effects, ownership, open-pattern e import-order conforme o grupo.
O oracle não compila nem executa W.

Todos os bundles permanecem `design-oracle-input`. Tree-sitter parse e host
oracle são evidência corrente. `w-compile`, `w-run`, `human-study` e
`model-study` permanecem missing. A contagem atual é derivada pelos scripts:
45 bundles, 128 variantes, 180 tasks e 69/75 casos R0 promovidos. O conjunto
contém 99 variantes `.w` parseadas e 29 witnesses reservados fora do parse.

### 1.4 Concorrência, paralelismo e execução

Esta seção preserva comparação, precedentes e alternativas. A seção 12 de
[`DESIGN.md`](DESIGN.md) define o contrato corrente de W.

#### 1.4.1 Modelo de execução

As fontes primárias usadas no gate W-1170 são:

- [Swift SE-0296](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0296-async-await.md),
  [SE-0304](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0304-structured-concurrency.md),
  [SE-0414](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0414-region-based-isolation.md)
  e [SE-0417](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0417-task-executor-preference.md);
- a documentação Apple de
  [`dispatch_async`](https://developer.apple.com/documentation/dispatch/dispatch_async),
  [queues seriais](https://developer.apple.com/documentation/dispatch/dispatch_queue_serial)
  e
  [`dispatch_barrier_async`](https://developer.apple.com/documentation/dispatch/dispatch_barrier_async);
- a [especificação de `go`](https://go.dev/ref/spec#Go_statements),
  [JEP 525](https://openjdk.org/jeps/525) e
  [P2300R10](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2300r10.html);
- [LLVM coroutines](https://llvm.org/docs/Coroutines.html),
  [LLVM atomics](https://llvm.org/docs/Atomics.html) e
  [MLIR Async](https://mlir.llvm.org/docs/Dialects/AsyncDialect/).

| Eixo | W | Swift | Go | Java Structured Concurrency | P2300 |
|---|---|---|---|---|---|
| call e `await` | suspende a task atual; não cria child | suspende a task atual | call comum é síncrona | `join` espera no caller | sender/receiver compõe completion |
| criação de child | initializer `async` ou `spawn<domain>` em `let`, estruturados | initializer `async` e task groups | `go f()` cria goroutine | `StructuredTaskScope.fork` | algorithms compõem operation states |
| handle | `Task` linear; join produz value ou outcome | binding ou scope | `go` não devolve handle | `Subtask` | operation state |
| lifetime | scope cancela, drena e faz join | scope espera children | goroutine pode sobreviver ao caller | scope faz join e shutdown | operation state vive até completion/stop |
| erro e cancel | typed errors, outcome e cancel bounded | error/cancel de child | valores e APIs | scope policy | error/stopped completion channels |
| placement | `spawn` escolhe domain tipado | actor/executor preference | scheduler runtime | executor ou virtual thread | scheduler sender |
| ordering | domain serial ou paralelo; barrier explícita | executor e GCD | scheduler/queue | executor | scheduler/algorithm |
| ownership | provas de transfer, share e loans | exclusivity e isolation | disciplina do programa | JMM e synchronization | lifetime/data-race rules de C++ |
| admission | budgets do domain/profile | policy do executor | runtime policy | policy do executor/scope | sender/scheduler contract |
| efeito | `maySuspend` inferido; `async fn` recomenda call | `async` nominal | sem efeito async nominal | sem efeito async nominal | completion signatures |

SE-0414 demonstra que uma análise flow-sensitive pode transferir uma região de
isolation sem expor a região no source. O
[`thread::scope` de Rust](https://doc.rust-lang.org/std/thread/fn.scope.html)
demonstra que join lexical permite borrows non-static, embora a assinatura Rust
publique lifetimes. W preserva a prova internamente: `take`, `copy`, `ref` e
`inout` escolhem a operação; `PlaceId`, `OriginSet` e join delimitam o lifetime
sem annotation de região.

Koka é somente uma referência para
[inferência de effects](https://koka-lang.github.io/koka/doc/book.html).
libdill e libmill são referências de runtime para coroutines e channels
([libdill](https://sustrik.github.io/libdill/),
[libmill](https://libmill.org/)). Nenhum deles define W ou é uma dependency.

O gate W-1170 compara estrutura, placement, ordering, ownership, admission,
cancellation e lowering. Ele passa somente quando o initializer `async` preserva
lifetime, um domain serial preserva FIFO, uma barrier preserva ticket subtrees
e um domain paralelo só produz overlap quando seu profile permite. Lowerings
diferentes precisam preservar outcome, cleanup e trace.

W só pode alegar que “resolveu concorrência e paralelismo” depois que compiler,
runtime e adapters reais passarem E0/E1, MX0 preservar owner graph e drop nos
mesmos schedules e as matrizes de profiles/providers cobrirem os targets
prometidos. Antes disso, a descrição correta é “contrato definido;
implementação missing”.

#### 1.4.2 Domains seriais e barreiras

GCD demonstra dois contratos úteis. Uma queue serial preserva ordem sem exigir
uma thread por queue. Uma queue concorrente privada aceita reads assíncronos e
um write com barrier. W mantém esses contratos como domains tipados e children
estruturados.

W não copia o modelo de objetos inteiro do GCD. A baseline não inclui sync
dispatch, global queue escolhida no call site, target queue, QoS por call,
fire-and-forget ou thread dedicada por lane. Um domain serial estático cobre o
caso comum. Uma lane serial dinâmica cobre somente ordering local, lexical e
bounded quando a quantidade de lanes depende de dados runtime. Estado durável
ou distribuído por key usa service instance; critical section curta usa lock ou
atomic.

A diferença de ownership é essencial. Uma barrier em domain estático pode
participar da prova de `ref` reads e `inout` write porque o compiler conhece o
grafo fechado. Uma referência dinâmica de lane preserva scheduling, mas não
cria essa prova por si só.

#### 1.4.3 Tempo, mobilidade, streams e atomics

Rust usa `Duration` nonnegative em seconds e nanoseconds. Swift documenta
components em seconds e attoseconds. Kotlin aceita duração signed e infinity.
W escolhe signed nanoseconds para diferenças operacionais e não usa infinity
como sentinel:

- [Rust `Duration`](https://doc.rust-lang.org/std/time/struct.Duration.html)
- [Swift time e duration](https://developer.apple.com/documentation/swift/time-and-duration)
- [Kotlin `Duration`](https://kotlinlang.org/api/core/kotlin-stdlib/kotlin.time/-duration/)

O [Rust Reference](https://doc.rust-lang.org/reference/special-types-and-traits.html)
separa `Send` de `Sync`. O
[guia de concorrência de Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/concurrency/#Sendable-Types)
combina transfer, immutability e serialized state em `Sendable`. W preserva
duas provas intrínsecas: `transferable` e `shareable`.

[Swift AsyncSequence](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0298-asyncsequence.md)
usa `next() async throws -> Element?`, fim estável e adapters concretos. W usa
um `Stream` single-pass em vez de separar sequence e iterator.

W exige order atômica constante no source, direção também usada pelo
[Swift Atomics](https://github.com/apple/swift-atomics/blob/main/Sources/Atomics/Types/UnsafeAtomic.swift).
LLVM fornece o precedente de lowering e memory model, não a autoridade
semântica de W.

Os papers WG14
[N1525](https://open-std.org/jtc1/sc22/wg14/www/docs/n1525.htm) e
[N1479](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1479.htm) separam
atomicidade de ordering e tratam data race como erro. Eles sustentam W-1174:
`load<.relaxed>()` ainda é atomic; um read comum concorrente dos mesmos bytes
não vira válido por aceitar staleness.

Release sequences e compare-exchange foram comparados com o
[LLVM LangRef](https://llvm.org/docs/LangRef.html#cmpxchg-instruction). Fences,
compiler fences e a separação entre success e failure foram comparadas com as
APIs de Rust para
[`fence`](https://doc.rust-lang.org/std/sync/atomic/fn.fence.html),
[`compiler_fence`](https://doc.rust-lang.org/std/sync/atomic/fn.compiler_fence.html)
e [`Atomic`](https://doc.rust-lang.org/std/sync/atomic/struct.Atomic.html).
Pointer e palavra dupla continuam disponíveis para runtime/provider unsafe,
mas não entram em `Atomic<T>` safe: atomicidade do bit pattern não preserva por
si provenance, owner lifetime, deallocator ou reclamation.

#### 1.4.4 Liveness e providers

O gate de closure e liveness compara
[Swift SE-0304](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0304-structured-concurrency.md),
[C++ P3149](https://wg21.link/P3149),
[Tokio fairness](https://docs.rs/tokio/latest/tokio/runtime/#detailed-runtime-behavior),
[Tokio cancellation safety](https://docs.rs/tokio/latest/tokio/macro.select.html#cancellation-safety)
e [Erlang supervision](https://www.erlang.org/doc/system/sup_princ.html).
[CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/fileio/cancelioex-func)
e
[io_uring cancellation](https://man7.org/linux/man-pages/man7/io_uring_cancelation.7.html)
delimitam races de completion e cancelamento. Essas fontes informam testes e
alternativas. Elas não definem outcome, cleanup ou reclamation em W.

O [scheduler oneTBB](https://uxlfoundation.github.io/oneTBB/main/specification/source/task_scheduler.html)
informa capacity efetiva e oversubscription. Os channels foram comparados com
[`tokio::sync::mpsc`](https://docs.rs/tokio/latest/tokio/sync/mpsc/) e seus
[permits](https://docs.rs/tokio/latest/tokio/sync/mpsc/struct.Sender.html), e os
happens-before com o [memory model de Go](https://go.dev/ref/mem). O
[mutex assíncrono de Tokio](https://docs.rs/tokio/latest/tokio/sync/struct.Mutex.html)
expõe o risco de manter guards durante suspension. O
[runtime Tokio](https://docs.rs/tokio/latest/tokio/runtime/#detailed-runtime-behavior)
explicita premissas de fairness. W transforma essas observações em contratos
próprios de ownership, closure, admission e profile.

As topologias de fan-out reforçam por que uma fila universal não basta. O
[`broadcast` de Tokio](https://docs.rs/tokio/latest/tokio/sync/broadcast/)
mantém um ring bounded, duplica sob demanda e informa quantos valores um
receiver lento perdeu. O
[`watch` de Tokio](https://docs.rs/tokio/latest/tokio/sync/watch/) mantém somente
o valor mais recente e precisa distinguir leitura de leitura que também avança
o cursor para evitar repetição na corrida com uma mudança.

[`SharedFlow`](https://kotlinlang.org/api/kotlinx.coroutines/kotlinx-coroutines-core/kotlinx.coroutines.flow/-shared-flow/)
permite replay, capacidade extra e três policies de overflow, não fecha e não
carrega failure. [`StateFlow`](https://kotlinlang.org/api/kotlinx.coroutines/kotlinx-coroutines-core/kotlinx.coroutines.flow/-state-flow/)
sempre possui um valor, conflates por equality e também não fecha. A
[`concurrent_bounded_queue` de oneTBB](https://oneapi-spec.uxlfoundation.org/specifications/oneapi/v1.1-rev-1/elements/onetbb/source/containers/concurrent_bounded_queue_cls)
oferece FIFO MPMC bounded, mas não define task scope, cancellation ou ownership
estruturado.

Essas diferenças não escolhem um default para W. A baseline compõe `TaskGroup`,
`Stream`, `Channel`, services, `ReadableStream.tee` e `SnapshotCell`. Um adapter
especializado pode oferecer outra policy, mas precisa nomear loss, replay,
close, failure, duplication, limits e owner graph. `WeightedChannel` também não
entra: peso informado pelo caller é accounting de aplicação, não prova de
memória ou trabalho físico.

A documentação do kernel Linux sobre
[RCU](https://docs.kernel.org/RCU/whatisRCU.html) separa removal de reclamation.
Ela também exige que readers anteriores terminem antes do reuse. W preserva
essa divisão em `SnapshotCell`, mas não expõe grace periods no safe source.

#### 1.4.5 Exclusão mútua residual

[`std::sync::RwLock` do Rust](https://doc.rust-lang.org/std/sync/struct.RwLock.html)
delega a priority policy ao sistema operacional e documenta um caso em que uma
segunda read pode deadlock com um writer em espera. O
[`sync.RWMutex` do Go](https://pkg.go.dev/sync#RWMutex) escolhe outra regra:
readers novos esperam um writer pendente, read recursiva é proibida e não há
upgrade ou downgrade.

Essas APIs mostram que “muitos readers, um writer” não define fairness,
reentrada, upgrade ou placement. A
[barrier de Dispatch](https://developer.apple.com/documentation/dispatch/dispatch-barrier)
oferece uma unidade mais próxima do W: jobs anteriores drenam, o write executa
sozinho e jobs posteriores só iniciam depois. W acrescenta tickets, children,
cleanup e prova de loans ao mesmo padrão.

O
[SRW lock do Windows](https://learn.microsoft.com/en-us/windows/win32/sync/slim-reader-writer--srw--locks)
expõe shared/exclusive access, mas declara que não é fair nem FIFO. O
[`pthread_rwlock_rdlock`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/pthread_rwlock_rdlock.html)
também condiciona admission a writers bloqueados e ao profile de scheduling.
Logo, nenhum primitive do host fornece sozinho uma policy lógica portátil.

A documentação de
[`Mutex` de Swift](https://developer.apple.com/documentation/synchronization/mutex)
confirma o caso residual: critical section curta sobre state usado por threads
síncronas. Ela também mostra o custo de uma superfície nominal e de uma closure
utilitária. O
[`os_unfair_lock`](https://developer.apple.com/documentation/os/os_unfair_lock_lock)
bloqueia eficientemente e não promete fairness; exigir FIFO em W impediria esse
lowering sem melhorar o resultado de um programa data-race-free.

O guia oficial de
[data-race safety de Swift](https://www.swift.org/migration/documentation/swift-6-concurrency-migration-guide/dataracesafety/)
separa isolation domain de lock e alerta que suspension encerra uma critical
section. Essa fronteira coincide com W: task-owned state usa domain ou service;
o body de `lock` nunca suspende. A declaração `shared`, a operação `lock` e a
HIR de places permitem ao compiler verificar o padrão sem `Mutex<T>` no tipo.

W-1257 a W-1260 substituem a direção W-1181 a W-1192. `lock`, `await lock` e
`try lock` compartilham uma gate lógica da allocation `shared`; wrappers
`Mutex`, `AsyncMutex` e `ReadWriteLock` saem da safe std antes de 1.0. Read-heavy
usa `SnapshotCell`; tasks usam domain barrier; scalar usa atomic. O fallback
exclusivo permanece para callback/FFI síncrono e state local realmente
compartilhado. Um adapter `unsafe` ainda pode expor RW lock quando um benchmark
e um target contract provarem ganho que essas formas não cobrem.

Condition variables foram consideradas pelo mesmo critério. Separar predicate,
lock e notification cria uma protocol surface em que lost wakeup e lifetime do
waiter pertencem ao caller. Channel, task outcome e service carregam state,
ownership, cancellation e close no mesmo contrato. O runtime continua livre
para usar condition, futex ou parking internamente.

`Once` raw também não entra. Const/module initialization resolve o caso
estático. `var Lazy` cobre o caso tardio sem publicar uma primitive de estado.
W-1255 rejeita a barreira cíclica genérica: TaskGroup, domain barrier ou service
já carregam identity, saída e failure no lifecycle correto.

Atomic waiting possui uma fronteira menor. O
[draft C++](https://www.eel.is/c++draft/atomics.wait) confirma que esperar por
mudança evita polling e também registra o limite ABA. O
[`WaitOnAddress`](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress)
e o
[`WakeByAddressSingle`](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-wakebyaddresssingle)
oferecem a primitive no Windows. O
[futex do Linux](https://docs.kernel.org/locking/robust-futexes.html) usa o
mesmo padrão de comparar, estacionar e acordar. A
[especificação WebAssembly Threads](https://webassembly.github.io/threads/core/exec/instructions.html#exec-memory-atomic-wait)
também separa wait e notify e mantém uma queue por endereço.

W não copia o blocking de thread dessas APIs. `Atomic.wait` é uma suspensão de
task com cancellation e lifetime estruturados. O provider pode usar as
primitives do host, mas precisa preservar tickets, drain e a ausência de wake
perdida. A notification não publica dados sozinha; o edge continua vindo da
load acquire que observa uma release.

#### 1.4.6 Inicialização lazy concorrente

O [`lazy` do Swift](https://docs.swift.org/swift-book/LanguageGuide/Properties.html)
não garante uma única execução sob primeiro acesso concorrente. Stored type
properties do Swift possuem uma garantia diferente. Essa diferença mostra que
o nome `lazy` sozinho não define concorrência.

O [`LazyLock` do Rust](https://doc.rust-lang.org/std/sync/struct.LazyLock.html)
bloqueia contenders e usa poisoning após panic. O
[`OnceLock`](https://doc.rust-lang.org/std/sync/struct.OnceLock.html) separa
consulta non-blocking, wait e inicialização. O
[`sync.Once` do Go](https://go.dev/src/sync/once.go) prova que um CAS isolado é
insuficiente: um contender precisa esperar a conclusão do winner. A mesma API
também demonstra o deadlock de reentrada.

W preserva a garantia útil e rejeita a superfície acidental. Um único `Lazy`
possui winner e publicação definidos. Ownership e isolation selecionam flag
local, storage serial ou estado atômico com parking. Panic falha a fault
boundary, portanto W não precisa de poisoning recuperável. Reentrada dinâmica
falha a mesma boundary em vez de bloquear para sempre.

O acesso continua property syntax, mas não ganha um efeito invisível. A
interface publica `blockingWhenContended`. Um domain non-blocking rejeita o
acesso sem prova de isolamento ou inicialização anterior. Async lazy, I/O no
initializer e retry falível exigiriam outra API e permanecem fora da baseline.

O modelo LZ0 mede a semântica comum entre lowerings. Ele não escolhe a primitive
do provider. Futex, parking lot, mutex interno ou uma forma target-specific são
alternativas físicas, desde que preservem winner, edges, cancellation e drop.

#### 1.4.7 ATOM2 — fechamento do contrato atômico

ATOM2 supersede o resultado Research de ATOM1 e fecha o gate de design ATOM0-G1.
O estudo problem-first usa os mesmos witnesses do restaurante Last Light e
compara quatro fronteiras:

| Eixo | Caso | Decisão corrente |
|---|---|---|
| A | `SignEpochWord { state: SignState, generation: u32 }` publica uma versão. | Carrier canônico compiler-synthesized é promovido dentro de `Atomic<T>` e `var atomic`, sem syntax nova; encoding é bit a bit, LSB-first em declaration order, com high bits zero. |
| B | `MenuHandle { slot: u32, generation: u32 }` identifica um `Menu`. | Handle value-only e owner table são composição de library. Generation exhaustion aposenta o slot e falha allocation. |
| C | Readers observam snapshots e nodes aposentados. | `SnapshotCell`/domain são safe. Adapter especializado `unsafe` é permitido somente como implementation-evidence gap. |
| D | Pointer, tagged pointer e RCU universal pretendem uma primitive geral. | Rejeitado. Atomicidade não prova provenance, lifetime, ABA ou reclamation. |

O carrier A aceita somente record fechado com `Bool`, fixed-width integers ou
enum sem payload. A codificação canônica é bit a bit: `Bool` é `0/1`, unsigned
usa o width exato, signed usa two's complement no width exato, e enum usa o
ordinal de declaração com pelo menos `ceil(log2(caseCount))` bits. Fields seguem
declaration order, o primeiro ocupa os bits menos significativos e os offsets
fazem parte do encoding explícito. High bits não usados do menor carrier são
zero. Endian físico é fact de provider/ABI, não valor lógico. W safe não constrói
padrões inválidos. O compiler deriva `Copy`, lifetime-independent e drop-free,
ignora caller facts e escolhe o menor carrier suportado entre 1 e 128 bits.
Nested records, custom/non-injective encoding, float, `usize`, `isize`, pointer,
owner, borrow, view, allocator origin e drop field são rejeitados. Load, store,
exchange e compare-exchange usam os memory orders vigentes; expected e desired do
CAS são codificados pelo mesmo encoder e comparados por representação canônica.
Fetch arithmetic não é derivado.

Target profile separa native width, lock-free width e fallback capability.
`lockFree: true` exige fact exato e nunca usa fallback. Sem o pedido, um carrier
nativo ou ausente pode usar somente fallback declarado, allocation-free por
instância/operação e compatível com o contexto. `allocation: true` é rejeitado;
tabela global ou pré-reservada exige receipt/profile explícito. `blocksThread` é
separado de `parking`, mas `parking: true` exige `blocksThread: true`; fallback
que bloqueia thread só entra em contexto com blocking e é rejeitado em
signal/interrupt, freestanding e cooperative worker.
Lock-free não significa wait-free. Atomicidade e lifetime continuam contratos
diferentes. SemanticInterfaceKey muda por mudança pública. WAbiKey e
RepresentationMap só carregam o carrier quando há crossing W ABI exato. Provider
digest-only muda recipe, RuntimeClosure e artifact evidence. C ABI não recebe
`Atomic<T>` diretamente.

Load, store, exchange e compare-exchange são `neverSuspend` e não são
cancellation points. `Atomic.wait` permanece API separada `maySuspend`; não há
suspensão de task escondida nos quatro operations. `parking: true` exige
`blocksThread: true`; não se pode rotular parking como nonblocking apenas por
`taskSafe`.

O handle B valida a generation antes do dereference e retorna `None` para stale.
Ao atingir `0xffffffff`, o slot é retired e a nova alocação falha. Wrap não é
uma solução para ABA. Tagged pointer continua rejeitado mesmo com CAS ou tags.

O adapter C exige domain, participants, registration, access/exit,
unlink/retire, quiescence, typed drop, raw reclaim, bound, deleter context,
orders, target progress, fault behavior e shutdown. Callback persistent exige
unregister → in-flight drain → destroy → unpin. A forma permitida permanece
`unsafe` e não declara provider ou runtime implementado. SnapshotCell e domain
barrier são as rotas safe.

O estudo durável está em
[`tooling/studies/atom2-atomic-contract`](tooling/studies/atom2-atomic-contract),
com corpus, dois reducers host, checker, bundle, snapshot e oracle em
`tooling/atom2-atomic-contract-*`. São 47 casos em quatro eixos. As mutations
cobrem bit direction/order, signed/enum code, high bits, target/fallback,
release/acquire order, allocation, blocksThread/parking/context, ABA/generation exhaustion, drop,
panic, OOM, cancellation, shutdown, FFI drain, SemanticInterfaceKey e WAbiKey. O resultado
tem zero status Research ativo. ATOM1 permanece somente como proveniência
histórica superseded.

As fontes primárias sustentam limites diferentes. O draft C23
[`N3220`](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3220.pdf) separa
atomicidade, memory order e macros de lock-free. A documentação de
[`core::sync::atomic` do Rust](https://doc.rust-lang.org/stable/core/sync/atomic/index.html)
expõe widths como facts do target e não promete wait-free. A proposta de
[`Synchronization.Atomic` do Swift](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0410-atomics.md)
exige operações explícitas e lock-free, não wait-free. Python usa objetos de
shared memory e wrappers de lock. Linux RCU separa removal de reclamation e
aguarda readers antes de reuse.

W compile/run, target probes, provider behavior, stress, debug receipts, FFI
drain execution e estudos humano/modelo continuam implementation-evidence gaps.
Eles podem reabrir o design somente se contradisserem uma invariável fechada.

#### 1.4.8 GEN1 — suspensão incremental e ergonomia

GEN1 foi o estudo histórico que informou e estreitou o gate de design
`GEN0-R1` sem promover uma primitive de frame. O gate agora está fechado pela
forma estreita de GEN2; métricas e witnesses GEN1 permanecem evidência de
proveniência, não uma decisão corrente. O estudo durável está em
[`tooling/studies/gen1-incremental-suspension`](tooling/studies/gen1-incremental-suspension),
com corpus, máquina, snapshot e teste host em
[`tooling/gen1-incremental-suspension-cases.json`](tooling/gen1-incremental-suspension-cases.json),
[`tooling/gen1-incremental-suspension-machine.mjs`](tooling/gen1-incremental-suspension-machine.mjs),
[`tooling/gen1-incremental-suspension-results.snapshot.jsonl`](tooling/gen1-incremental-suspension-results.snapshot.jsonl)
e [`tooling/gen1-incremental-suspension-reference.test.mjs`](tooling/gen1-incremental-suspension-reference.test.mjs).
As variantes reutilizam `streams.w`, `execution.w`, `state_transitions.w`, os
oracles de ownership/lifecycle/scheduler/liveness e `restaurant.w` por
`sourceRefs` e digest. As três variantes `.w` são witnesses parseáveis;
`compiler-stream-block.txt` é um Research witness reservado e
`public-resumable-frame.txt` é um witness reservado intencionalmente rejeitado.

O mesmo trace do restaurante é executado por duas máquinas independentes: um
frame com program counter e slots suspensos, e um loop que retorna estado e
token linear. A equivalência exige
o mesmo grafo de owners, commits e happens-before, resultado tipado,
cancelamento e sequência cleanup/drop/drain. Só packing e trace físico mudam.
O corpus cobre pull simples, locals vivos na travessia, diálogo com valor de
resume, failure typed continuável ou terminal, delegação equivalente a
`yield-from`, view borrowed e `next`, canais cap0/cap1, cancel antes/depois do
commit, child nested, lease de callback FFI e resume terminal/double/concurrent/
late.

O estudo mede declarações source reais por slices aplicáveis, não repete
`expected`: conceitos públicos, handoffs de ownership, pontos explícitos de
effect/cancel/cleanup, estado oculto, adições públicas de type/ABI e operações
de source. Cada símbolo é único, tem digest próprio e só é comparado no mesmo
cenário. LOC é somente contexto. A composição A (`Stream`/adapters/tasks), a máquina nominal B
e os canais bounded C cobrem os traces do oracle. As slices de ergonomia
comparam somente o mesmo cenário. O helper constrói dois pares de `Channel`
bounded e devolve endpoints owned; ele resolve somente diálogo. O frame/resume
público é intencionalmente rejeitado. O bloco compiler-owned que GEN1 mantinha
como Research-candidate foi estreitado e promovido por GEN2 para
`stream <[capture_item, ...]> { ... yield (take|copy) value }`; frame público,
`send`/`throw`/`close`, `yield-from`, scheduler yield e FFI resume continuam
rejeitados. GEN0 continua `composable` para o problema comum, com diálogo em
`Channel`; `GEN0-custom-frame` não é uma subcapability corrente. A pergunta de
ergonomia de GEN1 (`humanDecisionPending`) é histórica: GEN2 fornece o contrato
e o corpus de design, enquanto compile, run, provider, runtime stress e estudos
humano/modelo permanecem gaps de implementação/evidência em W-1438/W-1440.

W-1354 é explicitamente superseded por W-1437. A disposição Research e a
pergunta aberta de GEN1 ficam preservadas como proveniência, mas não podem
reabrir `GEN0-R1` nem contradizer a forma estreita corrente.

Antes dos witnesses reservados, o fixture parseável `builder-helper.w` mede um
helper de biblioteca que cria e devolve dois pares de endpoints com `capacity`
explícito. Ele é evidência de ergonomia somente para diálogo, não um novo
contrato de channel ou de suspensão.

As comparações registradas no estudo usam somente fontes primárias: o draft C23
N3096, POSIX cancellation e message queues, LLVM Coroutines, Rust Reference e
std `Future`/MPSC/scoped threads, e Python Language Reference, PEP 342/380 e
asyncio TaskGroup/Queue. Elas são limites comparativos, não contratos herdados.

#### 1.4.8a GEN2 — expressão `stream` e emissão owned estreita

GEN2 fecha a decisão de design por contrato e corpus, mas separa essa decisão da
lacuna de implementação. O estudo durável está em
[`tooling/studies/gen2-stream-yield`](tooling/studies/gen2-stream-yield), com
casos, máquina, snapshot e teste host em
[`tooling/gen2-stream-yield-cases.json`](tooling/gen2-stream-yield-cases.json),
[`tooling/gen2-stream-yield-machine.mjs`](tooling/gen2-stream-yield-machine.mjs),
[`tooling/gen2-stream-yield-results.snapshot.jsonl`](tooling/gen2-stream-yield-results.snapshot.jsonl)
e [`tooling/gen2-stream-yield-reference.test.mjs`](tooling/gen2-stream-yield-reference.test.mjs).
São 20 casos (7 positivos e 13 negativos), com cinco ganhos ergonômicos e duas
perdas de cerimônia explícita de capture nos cenários de cancelamento aberto.
Dois reducers independentes (`switched-frame` e
`returned-state`) derivam o mesmo owner graph, happens-before, resultado,
cancelamento e cleanup; packing, PC e token não são parte do resultado.

A forma promovida é uma expressão compiler-owned, escrita
`stream <[take source, copy config, ref stable]> { ... }`, que retorna
`some Stream<Item, Failure>` quando aparece no corpo de uma função. A capture
list pode ser vazia (`stream <[]>`), mas nunca é implícita: cada item é
avaliado, preparado e movido/copied/referenced na construção, antes de o
`Stream` ser retornado. O binding `take` do parent fica indisponível após a
construção; `next` não pode escolher uma capture ambiental. O bloco usa somente
`yield take value` ou `yield copy value` entrega `Item` owned: `take` move e
invalida o binding, `copy` exige `Duplicable` e preserva o original. Bare
`yield value` produz `W-YIELD-0002`; copy de item não `Duplicable` produz
`W-YIELD-0011`. `await` e `try` permanecem explícitos, `return` sem valor é
terminal e `defer` faz cleanup. O
pull tem cursor exclusivo e capacidade zero; não há prefetch, buffer ou
scheduler implícito. Cancelamento e drop seguem o protocolo de `Stream`.
Captures usam as regras existentes de `copy`, `take`, `ref` e `weak`; `inout` não
é capture mode e causa diagnóstico.
Falha é o `Failure` declarado. Yield de view/borrow/inout, `yield` em `defer`,
`yield-from`, `send`/`throw`/`close`, retorno de valor, falha sem tipo,
concurrent/reentrant `next`, frame/resume público e resume por FFI são rejeitados.
O lowering físico é privado e não publica frame, token, scheduler, layout ABI,
reflection ou identidade de debug.

`stream` e `yield` agora são keywords reservadas reais. O corpus e as fixtures
que usavam `stream` como binding foram migrados somente para `source` ou
`cursor`; o identificador público `Stream` (tipo) permanece. Um uso antigo de
`stream` como nome produz `W-STREAM-0001`, para tornar a migração explícita em
vez de alterar silenciosamente o CST de `lock state { ... }`. O parser conserva
esse negativo de lock e a nova expressão aparece apenas com o token literal
`stream`, capture list e bloco. `W-YIELD-0010` torna a ausência de capture
explícita e impede que uma função escapante resolva `source` somente no primeiro
`next`.

Esta promoção é uma conclusão de design, não uma afirmação de implementação.
Continuam faltando `w-compile`, `w-run`, runtime stress, provider, estudo humano
e estudo de modelo, além de debug/ABI/reflection e hot-reload/FFI end-to-end.
Esses gaps bloqueiam a promoção de qualquer frame ou generator geral. As
comparações oficiais usadas como limites são [Python generators](https://docs.python.org/3/reference/expressions.html#yield-expressions),
[PEP 342](https://peps.python.org/pep-0342/), [PEP 380](https://peps.python.org/pep-0380/),
[PEP 525](https://peps.python.org/pep-0525/), [Swift AsyncSequence](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0298-asyncsequence.md),
[Swift AsyncStream](https://developer.apple.com/documentation/swift/asyncstream),
[Rust Iterator](https://doc.rust-lang.org/std/iter/trait.Iterator.html),
[Rust Future](https://doc.rust-lang.org/std/future/trait.Future.html),
[Rust coroutines](https://doc.rust-lang.org/beta/unstable-book/language-features/coroutines.html)
e [C23 N3096](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf).

W-1439 e W-1440 permanecem `implementation-evidence-gap`: o parser, o corpus
e o catálogo de diagnósticos registram a superfície e a intenção, mas não
provam semantic checker/compiler nem a captura construction-time em runtime.
W-1437 é a decisão de design corrente; não transforme parser green ou oracle
host em claim de execução.

#### 1.4.9 IPC1 — memória mapeada e IPC process-shared

IPC1 informa e estreita o gate de design `IPC0-R1`. A etiqueta **Pesquisa**
abaixo é a proveniência do corpus antes de ASIC0. ASIC0 fecha como design
current os contratos condicionais de adapter/provider para A e B, sempre com
receipts explícitos, e registra W-1448 como implementation-evidence-gap. Isso
não promove API, syntax, compiler, runtime ou provider implementation. O
fallback vigente permanece `SnapshotByteSource`, wire tipado, Arrow e service
channel bounded. O estudo usa o Restaurante no Fim do Universo para comparar
uma telemetria de horizonte e um snapshot de menu.

A é um snapshot mapeado imutável. Seu layout é relocatable e
pointer-free. O payload usa offsets e índices relativos, nunca pointers,
owners, borrows, capabilities ou values com drop. O header publica magic,
version, schema identity, schema digest, layout digest, length, alignment,
endianness e generation. O reader valida esses campos antes de criar a view.

Cada generation é um objeto/extent imutável separado. Um catálogo/selector
publica a generation corrente; a lease guarda `objectIdentity+generation` e
impede reuso enquanto permanece viva. O writer faz stage e hash de um objeto
novo. Para uma requisição durable, a ordem é request, flush de dados e
metadata, release do selector, flush do selector/namespace e receipt terminal.
Uma publicação visibility-only não inventa receipt. Um crash antes do release
do selector mantém a generation anterior. Um crash depois do selector e antes
do receipt deixa visibility viva, mas recovered current desconhecida; um crash
depois do receipt é sucesso. O resultado não pode inferir sucesso físico a
partir de uma write ou de `FlushViewOfFile` isolado.

O reader observa uma generation imutável por uma lease. Uma generation stale
exige `observe-generation`, drop/close, unmap e remap explícito; um read com
lease divergente é rejeitado. A view não escapa de seu lease e `drop-view` vem
antes de unmap. Resize ou truncate com views vivas é rejeitado. O último unmap
encerra synchronization objects que já não têm owner. `shm_unlink` remove o
nome POSIX, mas referências existentes mantêm o objeto. No Windows, mapping
handles e views têm lifetimes separados.
Essas diferenças físicas retornam outcomes normalizados, não uma equivalência
de nomes ou handles.

B é um carrier de bytes/wire bounded em memória mapeada, não um
`Channel<T>` genérico nem uma coleção de referências compartilhadas. Capacidade,
slot count, slot size, schema, layout digest e generation vêm do header
mapeado validado, nunca de `input.capacity`; `header.length` é igual ao extent
mapeado e `slotCount*slotSize` cabe no segmento `slots`. Cap0 não tem slots e exige
rendezvous send/receive pareado; capN deriva ocupação dos slots. O contrato
existente de `Channel` permanece: o owner local retorna antes de commit e o
carrier recebe bytes wire canônicos depois de commit; o receiver valida
length/schema/checksum e materializa um novo owner W.
Cancelamento antes do commit devolve o owner. Cancelamento depois do commit
mantém o payload no channel. Backpressure bloqueia ou falha de forma explícita.
O trace prova no máximo um owner por slot committed, não exactly-once
distribuído. O estudo não cria um channel raw de shared references.

Cada slot usa layout relativo e facts do provider para width, order, alignment e
lock-free progress do atomic process-shared. Um atomic comum de W ou uma `Arc` não prova
escopo entre processos. Width, order, alignment e progress não podem ser
forjados pelo caller. Um producer que falha em writing faulta a generation;
um slot full já committed sobrevive e pode ser materializado pelo reader depois
do crash do producer. Um reader que falha em reading/materialize faulta a
generation. Um supervisor pode abrir uma generation nova somente após a ordem
total fault, stop-access, drain, drop-view, unmap, close e reopen de generation
maior. Não existe
reparo in-place oculto. Um provider lock-free é preferido. Um fallback blocking
robust é profile separado e não bloqueia worker cooperativo de modo invisível.

IPC2 torna wake uma receipt explícita. Bounded polling é o fallback
cross-process quando timeout e cancelamento são bounded. Windows
`WaitOnAddress` é same-process e é rejeitado para IPC; um wake Windows
cross-process exige Event, Semaphore ou Mutex nomeado, com ACL, namespace e
handle lifecycle. POSIX robust process-shared mutex registra owner death como
typed fault; o reducer não finge equivalência com Windows. ATOM2 fornece
somente o carrier value-only, allocation-free e never-suspending. Scope
process-shared, address-free, width, order, alignment, progress e wake
continuam receipts do provider; `lockFree: true` exige exact target fact e não
é inferido de `Atomic<T>`.

O provider é a autoridade para target kind POSIX ou Windows, object identity,
generation, access rights, lease/unmap, address independence, schema/layout
digests, atomics process-shared, backing volatile/durable, flush receipt, delete
behavior e crash outcome; apenas `allowedLayouts[]` e `allowedSchemas[]` são
autoridade, sem digests provider singulares. O adapter `unsafe` precisa
conservar a ordem stop-access, unregister-callback, drain, drop-view, unmap e
close-handle. Nenhum callback ou access ocorre após unmap. O fallback explícito para snapshot/wire é o resultado quando um target
não prova layout, lifetime, atomic scope ou crash.

O corpus separa famílias de backing: POSIX file-backed e Windows file-backed
podem publicar snapshots duráveis somente com receipt de dados e metadata;
POSIX `shm_open` e Windows pagefile mappings publicam carriers voláteis, sem
promessa de durabilidade após reboot. Windows não recebe um `unlink` POSIX
inventado: o nome do kernel object permanece enquanto houver referências, e
withdrawal imediato é unsupported ou exige broker/versioned-name Research.

O corpus usa duas reducers independentes com state/event derivado de operações
ordenadas. A reducer POSIX registra `open-file`/`shm_open`, `mmap`, `msync`,
`fsync`, `shm_unlink` e `munmap`; a reducer Windows registra
`CreateFileMapping`, `MapViewOfFile`, `Interlocked`, `FlushViewOfFile`,
`FlushFileBuffers`, `UnmapViewOfFile` e `CloseHandle`. Cada caso seleciona um
binding publicado, mas facts vêm da tabela provider. Cada caso precisa das duas
projeções e de um compact logical outcome igual; divergência é rejeitada. O
expected do caso é uma asserção. Ele não seleciona o resultado da máquina.

O estudo cobre horizon writer/reader em bases diferentes, generation objects e
selector catalog, menu publication, header corrupto, offsets/extents/overlap/
overflow, stale generation, unlink e last-handle lifecycle, crash antes e
depois da publicação, durability receipt, cap0/capN, full/backpressure,
commit/cancelamento, checksum e slot header, crash por actor em cada estado,
recovery ordering, unrelated-process no-fault continuation, terminal channel e
lifecycle audits, atomic unsupported, provider binding/fact mutations, view
escape, resize, FFI close ordering e fallback ao baseline.
As variantes `.w` cobrem somente composições vigentes. Os textos `w-reserved`
preservam a proveniência pré-ASIC0: A/B são contratos current apenas na forma
condicional de adapter/provider fechada por ASIC0, enquanto pointer nativo e
provider oculto permanecem rejeitados.

As fontes primárias são POSIX Issue 8 para
[`mmap`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/mmap.html),
[`shm_open`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/shm_open.html),
[`shm_unlink`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/shm_unlink.html),
[`msync`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/msync.html)
e [memory synchronization](https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap04.html#tag_04_12).
As fontes Microsoft são [`CreateFileMapping`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfilemappinga),
[`MapViewOfFile`](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile),
[`InterlockedCompareExchange64`](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockedcompareexchange64),
[`FlushViewOfFile`](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-flushviewoffile)
e [`FlushFileBuffers`](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers).
Rust [`atomic`](https://doc.rust-lang.org/std/sync/atomic/), [`Arc`](https://doc.rust-lang.org/std/sync/struct.Arc.html)
e [external blocks](https://doc.rust-lang.org/reference/items/external-blocks.html)
separam target facts, ownership process-local e FFI. Python
[`SharedMemory`](https://docs.python.org/3.14/library/multiprocessing.shared_memory.html)
expõe close/unlink e tracker lifecycle sem definir o layout tipado de IPC1.

O produto durável está em
[`tooling/studies/ipc1-mapped-ipc`](tooling/studies/ipc1-mapped-ipc), com corpus,
reducers, checker, teste host e snapshot em `tooling/ipc1-mapped-ipc-*`.
O corpus IPC2 tem 69 casos e 138 projeções. O probe POSIX observado é
digest-backed: WSL2/GCC executou dois processos, endereços distintos,
header/commit/read, stale-name rejection, remap generation 2 e cleanup; o
receipt liga source e transcript por SHA-256. Esse fato não é execução W nem
provider readiness. O probe Windows continua missing, assim como
`w-compile`, `w-run`, provider, crash-recovery, durability, human-study e
model-study. ASIC0 fecha A/B como contratos condicionais e mantém C universal
(`Mapped<T>`, `shared T` e raw pointer) rejeitado. A fila de documentação mantém
exemplos pareados de C/POSIX, Rust e Python para o guia
`guides/problems/process-shared-data`. LOC ou ergonomia estrutural não fecham
essa fila.

### 1.5 Memória, layout, errors e cleanup

Esta seção preserva precedentes usados nas seções 9 a 11 de `DESIGN.md`. W usa
as fontes como evidência de riscos e alternativas, não como semântica herdada.

A definite initialization em duas fases foi comparada com a
[inicialização do Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/initialization/).
[`MaybeUninit`](https://doc.rust-lang.org/core/mem/union.MaybeUninit.html) mostra
o risco de representar bytes ainda inválidos. O compact constructor de
[records Java](https://docs.oracle.com/en/java/javase/15/docs/specs/records-jls.html)
mostra a validação antes da publicação do aggregate.

Swift também possui
[properties read-only com `async` e `throws`](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0310-effectful-readonly-properties.md).
W preserva property-safe accessors. Um método nomeado mantém `try`, `await` e
efeitos operacionais visíveis no call site.

#### 1.5.1 Loans, captures, pinning e ownership compartilhado

A distinção entre place e value e o modelo relacional de loans têm precedentes
no [Rust Reference](https://doc.rust-lang.org/reference/expressions.html#place-expressions-and-value-expressions)
e em [Polonius](https://rust-lang.github.io/polonius/rules/relations.html).
A precisão de captures foi comparada com
[Rust closures](https://doc.rust-lang.org/reference/types/closure.html#capture-precision),
[Swift SE-0446](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0446-non-escapable.md)
e [Clang Lifetime Safety](https://clang.llvm.org/docs/LifetimeSafety.html).

A [API `Pin` do Rust](https://doc.rust-lang.org/std/pin/) demonstra que
estabilidade de endereço é um contrato específico, não propriedade de todo
pointer. [`Arc`](https://doc.rust-lang.org/std/sync/struct.Arc.html) separa
payload, owners fortes, owners fracos e allocator; thread-safe reference count
não torna o payload shareable. W mantém essas distinções sem expor lifetime
variables no source.

O [ARC de Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/automaticreferencecounting/)
documenta ciclos entre instances e entre uma instance e uma closure. Capture
lists permitem `weak` e `unowned`, mas a capture forte continua default. A
documentação de [`Arc`](https://doc.rust-lang.org/std/sync/struct.Arc.html#breaking-cycles-with-weak)
declara que ciclos fortes não são liberados e usa `Weak` para parent links;
`Arc::new_cyclic` entrega somente um `Weak` durante a construção.

W escolhe uma fronteira mais estrita: uma closure escapante nunca ganha retain
ou transferência de owner move-first por inferência. O prefixo `<[copy name]>`,
`<[take name]>` e `<[weak name]>` torna a edge observável. `unowned` não entra
porque duplicaria `ref` sem prova ou introduziria trap/use-after-free.
O compiler rejeita apenas o componente forte fechado que consegue provar como
destruction-dependent; grafo dinâmico não recebe certeza inventada.

**Migração pré-1.0.** A forma anterior `capture(mode name, ...)` apareceu em
prototipagem, mas misturava uma call-like shape com a closure. A forma vigente
`<[mode name, ...]> (params) => body` separa o contrato contextual do callable.
O prefixo não é `StaticList` nem argumento genérico. Captures implícitas ficam
sem prefixo. A forma antiga deve falhar no parser ou no diagnostic de migração.
Não há camada de compatibilidade. Esta troca preserva a ordem source para
diagnostics, mas não congela o layout do environment.

As formas `.upgrade()`, `.strong`, `.strong()` e `share()` foram mantidas apenas
como alternativas históricas rejeitadas. A forma vigente lê `weak T?` em target
normal e produz atomicamente `shared T?`; a declaração `shared` cria o primeiro
owner e um owner existente move-se com `take`. A aquisição e a criação ficam
visíveis no tipo e no binding, sem call de linguagem.

O artigo
[Concurrent Cycle Collection in Reference Counted Systems](https://pages.cs.wisc.edu/~cymen/misc/interests/Bacon01Concurrent.pdf)
mostra que coleta localizada e concorrente é possível, mas exige algoritmo,
metadata e sincronização próprios. Por isso um coletor pode voltar como
profile estudado, sem definir `shared` ou mudar o drop da baseline. O censo de
debug/test registra e reporta; ele não coleta.

O experimento de
[in-place initialization do Rust](https://github.com/rust-lang/lang-team/issues/336)
mostra que construir primeiro e mover depois não atende todos os tipos
sensíveis ao endereço. O
[placement new de C++](https://eel.is/c++draft/new.delete.placement) constrói num
endereço conhecido, mas deixa lifetime e cleanup ao código low-level.
[`MaybeUninit`](https://doc.rust-lang.org/core/mem/union.MaybeUninit.html) também
mostra o risco de expor storage parcial e drop manual.

W usa a forma já existente `try pin Type(...)`. Argumentos ficam em staging,
o storage estável é reservado e o initializer escreve no destination final.
Definite initialization mantém `self` indisponível até o commit. Isso evita
uma nova annotation, um carrier parcial safe e uma segunda família de
initializers. `pin take value` continua útil quando o valor completo já existe.

As formas `pin init`, `pin let`, `let pin`, `take<.pin>` e field annotations
ficam rejeitadas. Elas dividem a regra entre declaration e call site ou misturam
move com policy de storage. Self-reference armazenada por source safe também
fica rejeitada. Um adapter `unsafe` pode usar raw storage e publicar somente um
wrapper completo. O compiler ainda pode fixar frames internos que ele próprio
constrói e verifica.

Rust separa criação normal e recuperável em `Arc::new`/`Arc::try_new` e
`Rc::new`/`Rc::try_new`. O
[ARC de Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/automaticreferencecounting/)
torna a alocação de class instance contextual, mas também torna toda class uma
reference type. O
[working draft de C++](https://eel.is/c++draft/util.sharedptr) preserva factories
de shared ownership e recomenda uma única allocation. W escolhe um verbo
explícito para a mudança de ownership, separa policy normal de recovery e deixa
co-allocation para o optimizer.

#### 1.5.2 Allocators e scopes lexicais

[`Allocator` de Rust](https://doc.rust-lang.org/std/alloc/trait.Allocator.html)
e o
[`remap` de Zig](https://ziglang.org/download/0.14.0/release-notes.html#Allocator-API-Changes-remap)
informam strong failure, resize in-place e relocation. W deixa fallback e
commit no caller e registra origem por receipt.

ASC0 substitui a antiga surface `Arena` por uma declaração lexical:
`allocator scratch: .fixed<capacity: N> { ... }` ou
`allocator .fixed<capacity: N> { ... }`. O bloco cria owner, lease e scope.
Construction sites diretos no corpo usam o allocator corrente. Calls cujo callee
publica `allocator name: ref Allocator` no primeiro slot também podem omitir
`allocator:`. A assinatura permanece explícita, e a cadeia só propaga quando
cada intermediário publica o slot. Um parâmetro contextual pode aparecer somente
primeiro e uma vez:
`fn decode(allocator memory: ref Allocator, frame: ref Bytes): Frame`.
Esse slot entra em signature, resource/interface facts, HIR e ABI. Provider
facts, mobility e origin ficam na HIR e no `AllocationOriginMap`, não em um tipo
source refinado.

`.fixed<capacity: N>` usa reserva de lowering por frame, task ou agregado, sem
buffer de caller. Placement e capacity são gates de target/profile; recursion
multiplica a reserva, e overflow, placement unsupported ou admission falha antes
do body, sem fallback oculto. A forma sem `try` exige prova de reservation
estática e admission infallible, incluindo recursion fechada. Uma admission
dinâmica exige `try allocator` e não cria binding nem entra no body em falha.
`.bounded<budget: N>` limita commit sobre um provider e permanece Research;
ASC0 não o aceita como plan ativo. Um
plan customizado aceita um descriptor lógico `AllocatorPlan` versionado com
`providerDigest: [u8; 32]`, failure, deallocator e mobility. Esse descriptor é
o contrato lógico `std.memory.AllocatorPlan` com `AllocatorPlanDescriptor` e
`AllocatorLease`; o protocol usa `const descriptor` e um `take fn open()`
consuming. O compiler chama `open()` antes do body; a lease fecha o provider em
`deinit` exatamente uma vez. O usuário não chama `open` ou `close`. A interface
executável, provider e lowering continuam gates de implementação.
`RestaurantPool(backing: ref processMemory, budget: 4<iec.MiB>)` é válido
somente se publicar esse descriptor. Provider raw é `unsafe`/versioned;
composição de plans contratados é safe. O bloco é dono de um lease; o provider
backing pode sobreviver a ele. Typed drops ocorrem
antes de reclaim físico, e return/break/throw/cancel usam o mesmo unwind.
`Arena` permanece somente termo de lowering.

O uso de `Array<String>(allocator: memory)` segue essa separação. `allocator:` é
um control argument reservado em construction expressions, canonicamente antes
dos argumentos comuns e fora da initializer signature. Ele governa somente
allocation sites publicados pelo contract. `using:` permanece livre em calls
comuns. `Array<String, allocator: memory>()` mistura as duas fases e permanece
**Rejeitado por enquanto**. Adoption family, progress e limits vêm do join
entre descriptor, provider profile e recipe; não são facts derivados do
descriptor sozinho.

[mimalloc](https://github.com/microsoft/mimalloc) permanece provider candidate,
não default sem evidência. Suas
[arenas](https://microsoft.github.io/mimalloc/group__arenas.html) e
[heaps](https://microsoft.github.io/mimalloc/group__heap.html) mostram por que
major version, mode, allocate/free domains, cross-thread behavior, hardening,
unload e target matrix pertencem ao profile.

#### 1.5.3 Provenance, niches, tags e reclamation

O lowering estrito foi comparado com
[`ptrtoaddr`](https://llvm.org/docs/LangRef.html#ptrtoaddr-to-instruction),
[`ptrtoint`](https://llvm.org/docs/LangRef.html#ptrtoint-to-instruction),
[`llvm.ptrmask`](https://llvm.org/docs/LangRef.html#llvm-ptrmask-intrinsic) e
[Rust Strict Provenance](https://doc.rust-lang.org/std/ptr/index.html#strict-provenance).
W rejeita round-trip integer universal e exposed provenance na baseline.

Niche optimization foi comparada com a
[null pointer optimization de Rust](https://doc.rust-lang.org/core/option/#representation)
e os
[extra inhabitants do ABI Swift](https://github.com/swiftlang/swift/blob/main/docs/ABI/TypeLayout.rst).
Nenhuma garantia de layout atravessa uma boundary sem fingerprint compatível.

Tagged addresses e hardening foram confrontados com o
[Tagged Address ABI](https://docs.kernel.org/arch/arm64/tagged-address-abi.html),
[MTE](https://docs.kernel.org/arch/arm64/memory-tagging-extension.html),
[pointer authentication](https://llvm.org/docs/PointerAuth.html) e
[CHERI](https://ctsrd-cheri.github.io/cheri-c-programming/background/cheri-capabilities.html).
Esses recursos podem competir pelos mesmos bits ou exigir metadata externa.
Por isso hardening e tooling vencem compactação opcional.

Destruction foi comparada com
[Swift SE-0390](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0390-noncopyable-structs-and-enums.md)
e [`Drop` de Rust](https://doc.rust-lang.org/stable/core/ops/trait.Drop.html).
O trabalho WG21 sobre
[RCU](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2545r2.pdf)
mostra que registration, allocator, deleter context e shutdown fazem parte da
reclamation. W não oculta esses eixos em `Atomic<ptr>`.

#### 1.5.4 Errors e panic

A ergonomia de `try` parte do
[Error Handling de Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/errorhandling/).
[Swift typed throws](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0413-typed-throws.md),
[Rust Result](https://doc.rust-lang.org/std/result/index.html) e
[Zig error return traces](https://ziglang.org/documentation/master/) delimitam
typed effects, valores armazenáveis e trace sidecar. O
[Rust Reference](https://doc.rust-lang.org/stable/reference/panic.html) também
separa panic de error recuperável. W usa teardown da fault boundary e não expõe
unwind recuperável no source.

#### 1.5.5 Texto limitado e storage

[`SmallString`](https://llvm.org/doxygen/classllvm_1_1SmallString.html) usa
storage inline de `SmallVector` e pode crescer além desse storage. O
[`inplace_vector` P0843R14](https://www9.open-std.org/JTC1/SC22/WG21/docs/papers/2024/p0843r14.html),
[`static_string`](https://www.boost.org/library/latest/static_string/) e
[`ArrayString`](https://docs.rs/arrayvec/latest/arrayvec/struct.ArrayString.html)
colocam capacidade fixa no tipo. Essas formas resolvem footprint previsível e
ausência de allocation dinâmica. Elas também criam conversões, overflow e APIs
paralelas.

W separa o limite do valor, a policy de recursos e o layout da boundary. Um
`String<(.bytes.count <= N)>` publica o valor aceito. O gate
`no-general-allocation` prova o grafo operacional. Um adapter publica count,
extent e bytes quando outra ABI precisa observar o layout.

Essa composição preserva uma única API de texto. Ela permite que o mesmo source
use storage inline, uma arena ou um carrier flat conforme o produto. Um tipo
especializado continua justificado quando muda operações ou complexidade, como
Rope ou texto indexado.

### 1.6 Services, packing e deployment

O modelo separa graph lógico, packing físico e deployment. workerd declara
service bindings na configuração do caller. WebAssembly Components liga imports
tipados a exports tipados durante composition. W acrescenta ownership, effects,
budgets, lifecycle e `ServiceRef` estruturado. Cloudflare fixa bindings no
deploy; W resolve a escolha no startup dentro do envelope do artifact.

As referências principais são:

- [Cloudflare service bindings](https://developers.cloudflare.com/workers/runtime-apis/bindings/service-bindings/);
- [workerd](https://github.com/cloudflare/workerd);
- [JavaScript-native RPC](https://blog.cloudflare.com/javascript-native-rpc/)
  e [Cap'n Web](https://blog.cloudflare.com/capnweb-javascript-rpc-library/);
- [sandbox e seccomp](https://blog.cloudflare.com/sandboxing-in-linux-with-zero-lines-of-code/);
- [WebAssembly Component Model](https://component-model.bytecodealliance.org/design/components.html)
  e [WIT worlds](https://component-model.bytecodealliance.org/design/worlds.html);
- [OCI manifests e indexes](https://github.com/opencontainers/image-spec/blob/main/manifest.md).

Esses precedentes não definem a stack W. `ServiceIR`, `ServiceLink`, wRPC,
`ServiceTransport`, packing e deployment mantêm contratos próprios. Cap'n
Proto, Cap'n Web e gRPC permanecem foreign links, não dependências estruturais.

W escolhe bindings nominais e adapters fixados no lock porque runtime lookup e
registro aberto mudariam authority depois do build. Outro workflow durável é
outro root iniciado por service ou supervisor, não um child com inheritance
especial. Essa composição mantém `WorkId`, effect, cancellation e recovery
visíveis. O output gate usa um commit provider por turn; dois providers exigem
outbox ou steps explícitos em vez de 2PC escondido.

### 1.7 Snapshot de maturidade retirado do documento normativo

Este snapshot registra planejamento de 10 de agosto de 2026. Ele não define a
linguagem e pode envelhecer sem alterar um contrato.

Este checkpoint estima trabalho de design. Ele não mede implementação. O
[índice gerado](DESIGN-INDEX.md) contém as contagens atuais e os intervalos de
leitura.

| Eixo | Estimativa | Evidência e limite atual |
|---|---:|---|
| superfície e semântica estática | 97–98% | G0–G5 fecham syntax, F0 fecha a forma canônica inicial, S0 integra semantics e D0 fecha diagnostics estruturados; checker e catálogo completo ainda precisam de oracles executáveis |
| compilador, runtime e ecossistema | 75–85% | as camadas e os contratos estão definidos; spikes de HIR, ABI, scheduler, wire e resolver ainda podem corrigir o design |
| ergonomia com evidência | 65–72% | R0 cobre 74/74, R0S mede a superfície derivada por script e R1 possui 25 bundles contrabalanceados do Última Luz que promovem 45/74 casos R0; participantes e modelos ainda não foram executados |
| validação executável | 55–65% | Tree-sitter, F0, S0, wire, R0/R1, M1, E0, B0 e P0 cobrem oracles iniciais; ainda não existe formatter, type-checker, evaluator, interface checker, HIR, scheduler, adapter ou runtime W |
| prontidão para design freeze | 70–80% | existe uma baseline coerente; faltam cinco ciclos de fechamento abaixo |
| prontidão para repository próprio | 90–95% | W possui autoridade, tooling, std e produto de referência separados; a extração não depende do design freeze |

As faixas são estimativas de planejamento. Uma contagem de decisões não prova
correção. Um item **Provável** precisa de um spike quando o resultado pode mudar
source, tipo, ABI ou comportamento runtime.

Os ciclos restantes para o design freeze são:

1. ratificar syntax, formatter e diagnostics com o corpus da seção 1 de [`RATIONALE.md`](RATIONALE.md);
2. provar memória, ownership, ABI e FFI com modelos pequenos e independentes;
3. provar tasks, services, transaction e wWire com modelos de fault injection;
4. fechar wrappers de metadata, resolver, rebuild, reprodução e
   `bootstrap.w0`; o container WMeta1 já possui baseline byte-exact;
5. fixar módulos, target facts e host profiles, revisar targets e fechar o
   contrato público.

Pesquisas que possuem fallback não bloqueiam o freeze. Elas permanecem
experimentais ou em packages separados. Uma pesquisa bloqueia somente quando
pode alterar um contrato corrente.
Um oracle ou spike descartável pode produzir evidência de design. Formatter,
checker, scheduler, runtime, provider e compiler de produção pertencem à seção
26 e não são pré-requisitos do freeze documental.
Mover W para outro repository também não muda este checkpoint.

### 1.8 Catálogo comparativo de viabilidade

Este catálogo foi retirado do documento normativo. Ele resume evidência e risco
de implementação. A forma corrente permanece na seção temática de `DESIGN.md`.
O índice gerado usa esta tabela somente como projeção.

| Família | Classe vigente | Motivo |
|---|---|---|
| owner único, borrow e whole-value move | **Possível agora** | análise e lowering conhecidos |
| provenance separada de address | **Possível agora** | HIR e LLVM preservam a distinção |
| `Address` sem reconstrução de pointer | **Possível agora** | `ptrtoaddr` e index width por address space são conhecidos |
| `Address<space: S>` | **Possível agora** | static contract separa spaces; target data layout fixa mapping e `Bits` |
| `withAddress` com provenance do receiver | **Possível agora** | lowering pointer-based evita exposed provenance |
| cópia tipada de pointers com estado externo | **Possível agora** | loads/stores tipados e target data layout preservam o carrier |
| pinning interno de task frame | **Provável** | lowering conhecido; drop e projection exigem corpus |
| `pin` e `Pinned<T>` públicos | **Provável** | M1 fecha estado lógico e falha consuming; FFI persistente precisa de protótipo |
| placement local sem annotation | **Possível agora** | escape e frame analysis conservadores fornecem fallback stack |
| gate sem allocator geral | **Provável** | call graph e allocation facts são conhecidos; FFI exige summary |
| `Arena` baseline com scope explícito | **Alternativa** | R1 preserva a forma como estudo histórico; ASC0 usa allocator lexical, e async e destruição física continuam gates de implementação |
| allocator explícito por `using` | **Possível agora** | origem e deallocator acompanham o owner |
| mobilidade derivada da origem | **Provável** | M1 separa origem local/cross-domain; FFI e matriz de providers exigem protótipo |
| allocator geral por build profile | **Possível agora** | profile gera runtime requirement e plan fixa provider exato |
| `shared` + `weak` sem cycle collector | **Provável** | M1/S0 fecham lifecycle, captures e ciclos lógicos; provider e censo reais ficam pós-freeze |
| initializers `async`/`spawn<domain>` estruturados | **Possível agora** | state machine e runtime mínimo delimitados |
| modules sem lifecycle e imports herméticos | **Possível agora** | contrato estático simples |
| UTF-8 owned e views | **Possível agora** | representação portátil com fallback |
| String flat com owner único no W0 | **Possível agora** | pointer/count/reserva/origin e validação UTF-8 são conhecidos |
| carrier comum de String/Bytes consuming | **Possível agora** | ambos são buffers baseline owned; type safety permanece nas conversões |
| SSO invisível | **Provável** | Swift e SmallString provam viabilidade; threshold e target exigem benchmark |
| COW como baseline de String | **Rejeitado** | desloca allocation, budget, failure e deallocator para mutation futura |
| cache lazy por String | **Rejeitado na baseline** | read não deve alocar, mutar owner ou exigir synchronization |
| `view T` genérica para projeções core | **Possível agora** | provenance e descriptor são definidos por família; `ref` cobre o place completo |
| fato de imutabilidade profunda | **Provável** | owner único e fields fechados são verificáveis; capabilities e foreign storage exigem fallback conservador |
| UTF-8 incremental e maximal subpart | **Possível agora** | estado máximo de três bytes e algoritmo Unicode versionado |
| adoção de `Bytes` por `String` | **Provável** | owner transfer é claro; reuse depende do allocator/layout |
| Bytes, paths nativos e C strings distintos | **Possível agora** | fronteiras conhecidas; conversões preservam perda e terminador |
| graphemes default e normalização versionados | **Possível agora** | tabelas Unicode geradas; custo linear permanece visível |
| `InlineString` com capacity no tipo | **Rejeitado por enquanto** | refinement, gate de allocation e carrier físico cobrem os três contratos sem outro tipo textual |
| strict numerics e overflow verificado | **Possível agora** | backend oferece operações adequadas |
| primitives de bits integer portáveis | **Possível agora** | counts, reverse e byte swap possuem lowering para intrinsics ou fallback; a superfície é pura e não exige allocation |
| literal exato até materialização | **Possível agora** | big integer e rational decimal ficam no frontend |
| conversões pelo domínio completo | **Possível agora** | tabela fechada e facts de refinement decidem sem heurística |
| float strict e total-order wrapper | **Possível agora** | IEEE e backend fornecem as operações necessárias |
| ranges com quatro closures | **Possível agora** | representação, membership e iteration discreta são separáveis |
| BigInt, Rational e FixedDecimal em `std.math`/`std.decimal` | **Provável** | algoritmos conhecidos; API, OOM e limites exigem corpus |
| `f16`, `bf16` e quantization em `std.quant` | **Provável** | MLIR preserva storage/expressed type; targets exigem fallback |
| Posit, Unum e decimal float | **Rejeitado por enquanto** | FixedDecimal, Rational e IEEE binary cobrem a baseline sem novo real universal |
| schema fechado de contrato estático | **Possível agora** | AST/HIR simples; corpus angular já existe |
| referências `.member` contextuais | **Possível agora** | expected type e refinement subject fecham a resolução |
| associated constants, functions e types | **Possível agora** | lookup estático e witnesses nominais são conhecidos |
| generics com primary associated types | **Possível agora** | inference fechada, coherence nominal e lowering híbrido definidos |
| subsets fechados de enum | **Possível agora** | case-set normalizado, flow narrowing e layout base definidos |
| typestate por parâmetro de valor enum + `take fn` | **Provável** | lookup e ownership são conhecidos; diagnostics e code sharing exigem corpus |
| `TypeId` e reflection opt-in | **Possível agora** | descriptor alcançável não expõe layout nem storage privado |
| synthesis de protocols core | **Possível agora** | families fechadas e witnesses normalizados |
| parâmetros rest homogêneos | **Provável** | call shape é fechado; ownership e lowering exigem corpus |
| packs heterogêneos e GAT | **Rejeitado por enquanto** | rest, tuple, primary associated types e methods generic cobrem a baseline |
| metatype e dynamic construction | **Rejeitado por enquanto** | generics, factory e enum preservam relações estáticas |
| visibilidade efetiva por tipo de membro | **Possível agora** | interface e HIR usam normalização determinística |
| destructuring nominal de struct | **Possível agora** | pattern e modos de borrow fechados |
| switch exaustivo e tuple scrutinee | **Possível agora** | ordem, guards e patterns fechados possuem análise conhecida |
| diff de interface e SemVer | **Provável** | regras básicas fechadas; conflitos de resolução exigem corpus |
| receiver consuming `take fn` | **Possível agora** | whole-value move e drop state já são necessários |
| retorno fluente `: self` | **Provável** | reborrow é conhecido; borrow suspenso exige corpus |
| overload por forma de call | **Possível agora** | seleção por labels ocorre antes do type-check |
| vários initializers e delegação total | **Possível agora** | flow analysis e grafo de delegação são conhecidos |
| computed property property-safe | **Possível agora** | accessors e borrow do receiver possuem lowering direto |
| static record e static list | **Possível agora** | payload const; cada head ainda precisa de schema |
| `fn`, `some fn` e `any fn` | **Provável** | tipos e drop são conhecidos; escape e erasure exigem corpus de custo |
| services serial-turn e `ServiceRef` async | **Provável** | exige protótipo de mailbox, deadlock e trace |
| `ServiceIR` e `interface.lock` | **Possível agora** | interface semântica, IDs estáveis e diff são técnicas conhecidas |
| `ServiceLink` separado de `ServiceTransport` | **Possível agora** | local, component, native RPC e foreign RPC exigem lowers completos distintos |
| wRPC unary e capability tables | **Provável** | lifecycle está fechado; session, disconnect e security exigem fault tests |
| service streams com dois créditos | **Provável** | source, errors, ownership, créditos e relay estão fechados; fairness e fault injection exigem protótipo |
| `pipeline` dependente | **Provável** | forma source e DAG estão fechados; runtime, arbitragem e routing exigem protótipo |
| output gate por commit dependency | **Design fechado** | B0/SR0 e W-1244 fixam frontier, terminal receipt, owner drain e single-provider; provider real continua gate |
| wWire `exact` e `compatible` | **Provável** | layout, registro e dois codecs de seed fecham a direção; decoder e fuzzer são gates |
| introdução direta entre três services | **Rejeitado por enquanto** | relay preserva consentimento, attenuation e revocation sem outro protocol |
| `SupervisorRef` process-local | **Provável** | owner, admission, cancellation e outcome estão fechados; restart exige oracle |
| bindings tipados e runtime graph data-only | **Possível agora** | requirements, providers, imports e exports fecham por interface no link |
| packing de service graph | **Provável** | partição e index são simples; ABI entre units e fast path exigem protótipo |
| deployment plan e lock por digest | **Provável** | resolução é direta; placement, adapters e rolling update exigem runtime |
| workflow durável por steps | **Provável** | superfície, replay e effect policy estão fechados; journal, crash oracle e migration exigem protótipo |
| `<unit>` e units customizadas | **Provável** | type/lowering coerentes; ergonomia precisa de corpus |
| refinements e value parameters | **Provável** | exige evaluator, proof budget e ABI identity |
| interval, case-set, shape e alias facts na HIR | **Possível agora** | análises conhecidas; fallback conserva checks e largura |
| remoção de checks por prova verificada | **Possível agora** | range e control-flow facts possuem lowering direto |
| largura de operação e SIMD por refinement | **Provável** | precisa preservar overflow, accumulator e cost model por target |
| storage estreito não escapante | **Provável** | exige boundary analysis, repacking e benchmark de cache/code size |
| optimization record e `w explain performance` | **Possível agora** | facts e decisões já existem nos passes; schema precisa ser estável |
| theorem prover ou SMT geral no build | **Rejeitado** | ProofFacts bounded cobrem a baseline e preservam reproducibility |
| property behaviors | **Possível agora** | `Lazy` fecha estado e lowering; composição arbitrária continua ausente |
| obrigação linear genérica de async close | **Rejeitado** | scopes e cleanup específicos preservam errors sem async destructor universal |
| entries e host profiles | **Provável** | default handler é claro; adapters e slot schemas precisam de protótipo |
| `hostBindings` no product | **Possível agora** | símbolos e slots são estáticos; o linker valida a assinatura |
| package manifest data-only | **Possível agora** | grammar separada, schema fechado e evaluator ausente |
| workspace data-only com lock compartilhado | **Possível agora** | members exatos, identity e contexts são verificações estáticas |
| usages separados de dependência | **Possível agora** | reachability e target role fecham product, build, test e benchmark |
| features somente no grafo | **Possível agora** | união aditiva evita conditional source e defaults ocultos |
| target variants disjuntas | **Possível agora** | predicates positivos, case único e interface matrix são verificações estáticas |
| source snapshot por allowlist | **Possível agora** | module expansion, PackagePath e digest produzem uma árvore fechada |
| build transform tipada | **Provável** | host profile e CAS são diretos; sandbox cross-platform exige protótipo |
| `WInterface` semântica versionada | **Possível agora** | schema data-only separa API, facts e chunks do cache interno |
| encoding publicável de metadata | **Possível agora** | `WMeta1` fixa envelope, profiles, subset CBOR, limits, corpus W0 e dois readers independentes |
| `WAbiKey` exata | **Possível agora** | target, call ABI e policies globais formam uma key; layouts compartilhados usam `RepresentationMap` |
| runtime contract set reachability-linked | **Possível agora** | requirements e offers usam o mesmo modelo tipado da toolchain |
| C façade com body W | **Possível agora** | `fn<abi: .c>` usa carriers explícitos e calling convention do target |
| W dynamic library por digest | **Provável** | loader e ABI note são conhecidos; parity e unload exigem protótipo |
| ABI W resiliente entre releases | **Rejeitado por enquanto** | source rebuild, C, component e service schema evitam runtime permanente |
| native dynamic library como sandbox | **Rejeitado** | loader e symbol boundary não contêm memory corruption ou panic |
| parâmetro de chamada `const` | **Possível agora** | evaluator e call checking já existem; ABI pode apagar o requisito |
| mensagem HTTP, ownership e admission | **Possível agora** | types, stream e limits estão fechados; adapters ainda precisam de corpus |
| RestPC com HTTP QUERY | **Possível agora** | RFC 10008 fixa segurança, idempotência, content negotiation e cache key; effect checking e adapters ainda precisam de corpus |
| adapter HTTP nativo e worker | **Provável** | sockets e WASI existem; parity, cancel drain e headers exigem implementação |
| SQL estático e rows tipadas | **Possível agora** | descriptors e bind são diretos; schema completo depende de bundle |
| `transaction` estruturada local e remota | **Provável** | source, provider único, scope e unknown commit estão fechados; adapters e fault tests ainda faltam |
| cache local com limite e read-through | **Provável** | algoritmos são conhecidos; eviction, cancellation e custo exigem protótipo |
| target identity e matrix build | **Possível agora** | recipes independentes evitam falsa identidade entre payloads |
| target spec com platform contract | **Possível agora** | schema fechado separa runtime floor, CPU e SDK |
| availability check por platform contract | **Possível agora** | join de requirements alcançáveis é análise estática fechada |
| toolchain plan por roles | **Possível agora** | constraints, seleção e digests são análise data-only |
| system SDK importer | **Provável** | Apple, Windows e vendors exigem adapters e closure hashing por instalação |
| remote execution sem mudar a plan | **Provável** | sandbox e inputs estão fechados; parity cross-host exige corpus |
| envelope sem assinatura reproduzível | **Provável** | ordering e metadata podem ser normalizados por packager |
| assinatura universal bit a bit | **Rejeitado** | timestamp, notarization e authorities externas produzem delivery records |
| WASI 0.3 native async component | **Provável** | standard estável; target e guest toolchains ainda amadurecem |
| desktop/server LLVM targets | **Provável** | backends existem; runtime, SDK e CI ainda são trabalho W |
| Android e Apple mobile | **Provável** | ABI e SDK existem; lifecycle, packaging e signing exigem adapters |
| Cortex-M e RISC-V firmware | **Provável** | backends existem; freestanding runtime e device descriptions exigem corpus |
| NVVM, ROCDL e SPIR-V device bundle | **Provável** | MLIR oferece lowerings; kernel subset e transfer precisam de protótipo |
| ASIC/FPGA como target geral | **Rejeitado** | kernels exportados podem usar adapter específico; W comum não promete synthesis de hardware |
| nanoservices co-localizados | **Provável** | service graph permite fast path; equivalência física exige trace e fault tests |
| profile TechEmpower | **Provável** | sete source oracles existem; adapters, harness fixado e medição ainda faltam |
| tensors ranked, `@` e views | **Provável** | MLIR ajuda; API e device model precisam de protótipo |
| integer tensor com accumulator inferido por range | **Provável** | prova é conhecida; panic, widening e kernel dispatch exigem corpus |
| float matrix modes strict/fast/reproducible | **Provável** | cada mode precisa de oracle numérico e matriz de targets |
| niches de null e bit pattern inválido | **Possível agora** | validity facts e fallback explícito fecham a semântica |
| low-bit interno por alignment provado | **Provável** | exige lowering de provenance e corpus de FFI, atomics e sanitizer |
| high-bit addresses e NaN boxing | **Rejeitado na baseline** | dependem de target, process, hardening e tooling; optimization interna mantém fallback |
| universal tagged pointer ou object header | **Rejeitado** | conflita com ABI, hardening, capability pointers e valores sem metadata |
| fingerprint de representação sem provider noise | **Possível agora** | schema físico, `WAbiKey`, runtime closure e recipe já estão separados |
| task lexical com outcome após cleanup | **Possível agora** | state machine, ownership, cleanup e join possuem ordem fechada |
| fail-fast com arbitragem declarada | **Possível agora** | cancel edge e drain são conhecidos; ordem lexical/input é estática |
| cancellation snapshot bounded | **Possível agora** | enums e bitsets fechados não exigem allocation no sinal |
| deadline monotônico local | **Provável** | timers e clock virtual são conhecidos; races exigem corpus adversarial |
| deadline remoto strict | **Possível por transport profile** | profile com timebase provada oferece strict; os demais usam approximate |
| schema portátil de execution domains | **Possível agora** | IDs, capabilities e regras de binding são estáticos |
| execution profile data-only | **Possível agora** | domains, pools, fallbacks e máximos fecham no link |
| task admission sem fila ilimitada | **Provável** | reserva e handle canceled são fechados; pressure e wakeups exigem protótipo |
| capacity compartilhada em paralelismo aninhado | **Provável** | runtime inicial precisa provar liveness e ausência de oversubscription |
| `Stream<Item, Failure>` single-pass | **Possível agora** | cursor mutável, Optional terminal e error effect possuem lowering direto |
| `for try await` | **Possível agora** | sugar local para `next()`; borrow do cursor e effects permanecem visíveis |
| `Channel<T>` MPSC bounded | **Provável** | ownership e estados estão fechados; fairness, cancellation e custo exigem protótipo |
| permits de channel | **Provável** | capability linear fecha capacity; close e suspension longa exigem oracle |
| WorkQueue, broadcast, watch e weighted channel | **Rejeitado na baseline** | TaskGroup, Stream, services, tee e SnapshotCell compõem os casos sem esconder loss ou accounting |
| `ByteSource`/`ByteSink` async-first | **Possível agora** | short progress, EOF e errors possuem resultados fechados |
| read por append em reserva privada de `Bytes` | **Possível agora** | initialized count e commit ocultam storage ainda não inicializado |
| cancellation de I/O com completion drain | **Provável** | backends possuem completion; runtime e borrow checker precisam de oracle |
| filesystem com rights estáticos e offset posicional | **Provável** | handles e syscalls existem; profiles e diagnostics exigem protótipo |
| adapters blocking com quota | **Provável** | pool bounded preserva semântica; cancellation física depende da API |
| backends readiness/completion equivalentes | **Provável** | contrato comum está fechado; matriz de targets deve provar os mesmos traces |
| gather write com segments borrowed | **Possível agora** | rest homogêneo, prefix progress e fallback sem allocation fecham a superfície |
| scatter read por `ReadBatch` | **Possível agora** | owner único, views somente inicializadas, `.full` e fallback de um read fecham a superfície |
| file-to-sink por `TransferPlan` | **Possível agora** | snapshot posicional, limite, scratch owned e progress tipado fecham a semântica; operação nativa exige capability interna |
| `transferable`/`shareable` estruturais | **Possível agora** | fields, captures, borrows, cleanup e interface compilada fornecem facts fechados |
| data-race freedom e happens-before | **Possível agora** | ownership, tasks, channels, services, locks e atomics fornecem edges fechados |
| `var atomic` e orders estáticas | **Possível agora** | superfície baixa diretamente para atomic load/store/RMW/cmpxchg |
| fallback atomic não lock-free | **Provável** | runtime striped lock preserva semântica; signals e freestanding exigem profile |
| `Atomic<T, lockFree: true>` | **Possível agora** | target e alignment resolvem o contrato em compile time |
| `lock` scoped da linguagem | **Possível agora** | HIR de place, body fechado e cleanup runtime fecham unlock sem guard |
| `await lock` residual | **Possível agora** | cancellation e unlock possuem contrato LM1; domain/service continuam preferidos |
| `ReadWriteLock<T>` na safe std | **Rejeitado** | domain barrier, SnapshotCell e lock exclusivo cobrem a baseline com menos policy |
| condition variable na safe std | **Rejeitado** | channel, task outcome e service unem evento, ownership e cancellation |
| `Atomic.wait/notify` suspensivo | **Possível agora** | fast path, tickets, cancellation, lifetime, ABA e provider possuem contratos fechados; runtime real e benchmark continuam gates |
| barreira cíclica/reutilizável | **Rejeitado** | W-1255 usa TaskGroup, domain barrier ou service conforme o lifecycle; uma primitive universal esconderia perda, generation e failure |
| `SnapshotCell<T>` | **Possível agora** | `read`, `snapshot` e `publish` fecham versões, edges e reclamation sem API RCU no caller |
| RCU genérico safe | **Rejeitado** | reclamation, ABA e leitura longa exigem adapter `unsafe` especializado |
| facts trusted para FFI e synchronization customizada | **Possível agora** | somente provider ou foreign interface fixa target, digest e negative facts |
| domain default por módulo | **Rejeitado** | import não possui instance, lifecycle ou executor |
| QoS na syntax de `spawn` | **Rejeitado** | policy no profile ou group não parece garantia de ordering ou deadline |
| `bootstrap.w0` e self-host antes de tasks | **Provável** | subset fechado; seed C e adapter MLIR precisam de prova |
| mimalloc como profile | **Provável** | API e build são conhecidos; versão, targets e foreign mix exigem benchmark |
| mimalloc universal | **Rejeitado por enquanto** | origem estrangeira, versão e targets impedem um default sem evidência |
| SQLite como durability universal | **Rejeitado** | adapter oficial é útil; semântica universal não é portátil |
| seccomp por módulo importado | **Rejeitado** | import não é uma security boundary |
| sandbox portátil por process/Wasm | **Provável** | depende do host, mas preserva o contrato |
| `fn<C>` com static archive | **Design fechado; provider missing** | façade C, foreign unit, recipe e build hermético estão definidos |
| `fn<Rust>`/`fn<Swift>` | **Design comum fechado; providers missing** | adapter hermético agrupa runtime e usa façade C tipada |
| álgebra simbólica completa no core | **Rejeitado** | package experimental preserva evolução |
| custom operators e precedência do usuário | **Rejeitado** | piora parser, tooling e previsibilidade |
| macros/annotations universais | **Rejeitado** | cria uma segunda linguagem e hidden behavior |

### 1.9 Auditoria comparativa C, Rust e Swift

**Exemplo:** W oferece a capability de acesso a MMIO que C permite. W não copia
o qualifier `volatile` para toda variável.

A auditoria de 4 de agosto de 2026 usa estas fontes primárias:

- [ISO C23 draft N3096](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf);
- [Rust Reference](https://doc.rust-lang.org/reference/) e
  [Rust standard library](https://doc.rust-lang.org/std/);
- [Swift language reference](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/)
  e [Swift standard library](https://www.swift.org/documentation/standard-library/).

O objetivo é cobrir necessidades. Igualdade de feature ou syntax não é o
critério.

#### 1.9.1 Primitives portáveis de bits

A comparação usa fontes primárias e orienta a ergonomia, não copia APIs externas.
O [integer primitive de Rust](https://doc.rust-lang.org/stable/core/primitive.u32.html)
reúne largura, counts, reverse de bits, swap de bytes e famílias checked ou
wrapping em operações const. O protocolo
[FixedWidthInteger de Swift](https://developer.apple.com/documentation/swift/fixedwidthinteger)
expõe largura fixa, counts de zeros e a propriedade
[`byteSwapped`](https://developer.apple.com/documentation/swift/fixedwidthinteger/byteswapped)
como transformação do mesmo tipo. Os builtins de
[Zig](https://ziglang.org/documentation/master/) definem `@clz`, `@ctz`,
`@popCount`, `@bitReverse` e `@byteSwap`, incluindo resultado definido para
zero e o sign bit em reverse. O
[LLVM Language Reference](https://llvm.org/docs/LangRef.html) lista intrinsics
overloaded para popcount, leading/trailing zero, bitreverse e byte swap, mas
registra que nem todo target suporta toda largura.

W sintetiza essa evidência em `bitWidth` e seis associated functions em lower
camel case. Counts retornam `UInt`, zero retorna a largura lógica e as
transformações retornam o mesmo tipo. O resultado não depende de endianness do
host. O compiler pode escolher instruction, intrinsic ou fallback equivalente.
Nenhuma dessas APIs promete tempo constante ou resistência a side-channel.
Funnel shift, carryless multiply, bit deposit/extract e hints continuam fora do
core porque exigem contratos de target ou de segurança próprios.

| Família | C23 | Rust | Swift | W vigente |
|---|---|---|---|---|
| control flow | labels, `goto`, loops, switch | labeled loops e expressions | labeled loops, `repeat` e pattern switch | loops/blocks rotulados, post-test loop, switch exaustivo, sem `goto` |
| tipos | scalar, struct, union, enum | algebraic types, traits, generics | value/reference types, protocols, generics | struct, object, enum, protocols, refinements e generics |
| memória | pointers e lifetime manual | ownership, borrow e unsafe | ARC, exclusivity e unsafe pointers | ownership, borrow, pin, allocator origin e `unsafe` |
| erros | codes, `errno`, nonlocal jump | `Result`, panic e `?` | typed throws, `try`, defer | typed throws, `try`, Result, panic e defer |
| concorrência | threads e atomics | threads, atomics e async libraries | tasks, actors e isolation | structured tasks, domains, services, channels e atomics |
| compile time | preprocessing e constant expressions | const, traits, macros e build | generics, macros e compiler attributes | ConstIR bounded, contracts e build transforms, sem macro AST |
| systems | ABI C e implementation extensions | FFI, attributes e inline assembly | C interop e platform SDKs | C façade, host slots, MMIO, placement e `fn<Asm>` |
| biblioteca | C library | core, alloc, std e crates | core std e packages | módulos `std.io`, `std.net`, `std.tensor`, `std.runtime.*` e packages ligados por reachability |
| ciência | complex e math | numeric core, ecosystem maior | numeric core, Accelerate packages | units, Complex, matrix, tensor e modes numéricos explícitos |

W cobre todas as famílias necessárias para compiler, server, desktop shell,
mobile host, firmware, service, ciência e accelerator. Essa conclusão é de
design. Ela não afirma que compiler, runtime ou SDK existem.

As principais formas deliberadamente ausentes e suas substituições são:

| Origem | Forma ausente | Exemplo | Substituição W |
|---|---|---|---|
| C | preprocessor textual | `#define CAPACITY 64` | `const capacity = 64` e target variants |
| C | `goto`, VLA e nonlocal jump | `goto next_row` | loops/blocks rotulados, `repeat`, owners e errors estruturados |
| C | promotions implícitas e overflow unchecked | `short + int` | conversão total e numeric policies nomeadas |
| C | raw varargs, bitfields e unions safe | `fn log(char*, ...)` | wrapper, `c.vaList` e layout foreign explícito |
| Rust | syntax pública de lifetime | `fn head<'a>(value: &'a T) -> &'a T` | inference conservadora e borrows diagnosticados |
| Rust | macros que reescrevem AST | `derive(...)` | ConstIR, synthesis core e build transform |
| Rust | deref coercion definida pelo usuário | call aceita wrapper por deref oculto | conversões únicas e facts estruturais |
| Swift | inheritance de classe e ARC universal | `class Cook: Employee` | composition, object owner e `shared` explícito |
| Swift | force unwrap, `try!` e optional implícito | `order!` | pattern, `try`, `try?` e error explícito |
| Swift | custom operator e wrapper annotation | `infix operator <~>` | operators fixos e property behavior nominal |

O corpus R0 liga cada ausência de source comparável à decisão, à forma vigente,
à diferença observável e ao material futuro do Book. Alternativas sem diferença
de source, fallbacks, itens históricos e waivers usam classes distintas no
freeze audit. Essa classificação é governança de evidência; ela não acrescenta
semântica ao W.

### 1.10 Evidência Python→W PYN0

PYN0 torna Python um público inicial de primeira classe nas seções 0.1 e 0.4.
A [Python Developers Survey 2024](https://lp.jetbrains.com/python-developers-survey-2024/)
teve mais de 30 mil respostas. Ela mostra uso em análise de dados, web,
machine learning, data engineering, scraping, pesquisa acadêmica e automação.
Exploração e processamento de dados aparecem em 51% das respostas. Pandas e
NumPy aparecem em 80% e 75%.

A survey mede separadamente Jupyter, individual-file workflows e packaging.

Os modos de arquivo, stdin, `-c`, `-m`, interativo e `-i` vêm da
[documentação do interpretador Python](https://docs.python.org/3/tutorial/interpreter.html).
Defaults, keyword arguments, unpacking, comprehensions, generators,
collections e scripts são ergonomia documentada no
[tutorial Python](https://docs.python.org/3/tutorial/). Notebooks combinam
code, prose, data, rich output e controls. A documentação do
[Jupyter](https://docs.jupyter.org/en/stable/) e o protocolo
[jupyter_client](https://jupyter-client.readthedocs.io/en/stable/) são as
referências autoritativas para o tooling e o protocol do kernel.

NumPy relaciona concisão e desempenho a vectorization e broadcasting em código
compilado. A documentação de
[NumPy vectorization](https://numpy.org/doc/stable/user/whatisnumpy.html) e
[broadcasting](https://numpy.org/doc/stable/user/basics.broadcasting.html)
serve como evidência de uso. O
[Python Array API standard](https://data-apis.org/array-api/latest/purpose_and_scope.html)
é checklist de interoperabilidade. Ele não é autoridade semântica de W.

As afirmações de ergonomia usam uma fonte primária por construção. Python
documenta [list comprehensions](https://docs.python.org/3/tutorial/datastructures.html#list-comprehensions)
e [negative indexing](https://docs.python.org/3/tutorial/introduction.html#lists).
NumPy documenta [broadcasting](https://numpy.org/doc/stable/user/basics.broadcasting.html). Julia documenta
[array broadcast](https://docs.julialang.org/en/v1/manual/arrays/#Broadcasting).
C# documenta [from-end indices](https://learn.microsoft.com/en-us/dotnet/csharp/tutorials/ranges-indexes).
Swift documenta [fixed argument order](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/functions/).
Essas fontes motivam alternativas documentais. Elas não são autoridade de
syntax ou semântica W.

A matriz separa linguagem, tooling, standard library, ecossistema e interop.
“Gap real” significa ausência no design corrente. Não significa que uma
implementação esteja atrasada.

#### 1.10.1 Matriz operacional preservada

O bloco abaixo saiu de `DESIGN.md` para separar auditoria de produto do
contrato corrente. Ele preserva alternativas, gaps e critérios de estudo. As
seções normativas citadas continuam sendo a autoridade.

**Exemplo:** uma pessoa pode testar `w run path/file.w -- input.csv` e depois
abrir `w repl` sem aprender ownership antes. A boundary científica continua
explícita.

PYN0 trata pessoas que usam Python como público inicial sem adotar um core
dinâmico. A matriz abaixo separa linguagem, tooling, standard library,
ecossistema e interop. Um gap indica ausência no design, não atraso de
implementação.

O carrier tabular e a regra de binding typed ficam em
[14.4.1](DESIGN.md#1441-carrier-tabular). Os adapters CSV, Parquet e Arrow
ficam em [14.4.2](DESIGN.md#1442-adapters-tabulares).
PYN0 não promove DataFrame ou duck typing a surface W.

##### Linguagem

| Motivo de adoção Python | Cobertura W atual | Gap real | Camada correta | Estado |
|---|---|---|---|---|
| Público de dados, web, ML, pesquisa e automação | Tour, tipos estáticos, ownership, effects e módulos reachability-linked | entrada Python-first e sequência pedagógica para o Tour | linguagem, Tour e documentação | **Direção** |
| Defaults, keyword arguments, unpacking, comprehensions, generators e collections | defaults, labels, patterns, ranges, collections e closures já têm contratos parciais | R1 compara pipeline, loop, broadcast, end-relative access, labels e tuple binding; comprehension e starred unpacking continuam sem grammar | linguagem e estudo R1 | **Pesquisa** |
| Arquivo, stdin, `-c`, `-m` e modo interativo | source file, `entry` e CLI de package existem | fluxo low-ceremony com regras herméticas e sessão | tooling e fronteiras de package | **Direção** |
| Dados exploratórios sem object model global | `json.Value`, `data.Batch<Row>`, `data.DynamicBatch`, schemas e reflection opt-in | adapters de CSV, Parquet e Arrow, além de inferência bounded no tooling | std, tooling e schemas | **Direção** |
| Interop com objetos Python sem duck typing | protocols nominais, C façade, adapters e `unsafe` explícito | lifecycle, GIL, interpreter e effects precisam de bridge visível | adapter e fault boundary | **Direção** |

O exemplo de ergonomia é uma comparação textual e pseudocódigo. Ele não fixa
uma forma nova:

```text
Python: total = 0; total = sum(value * value for value in values)
W pseudocode: var total = 0; for value in values { total += value * value }
PYN0:         comparar comprehension documental, pipeline e loop no R1
```

PYN0 não adota duck typing, monkey patching, dynamic global object model, GIL,
ambient imports ou unchecked reflection. `Any` e reflection permanecem opt-in.
Dados exploratórios usam `json.Value` explícito, schema ou carrier tabular. A
inferência de schema pertence ao tooling e publica seus limites.

##### Tooling e workflow interativo

| Motivo de adoção Python | Cobertura W atual | Gap real | Camada correta | Estado |
|---|---|---|---|---|
| Arquivo único com argumentos e execução repetida | `w run <path/file.w>` e entry explícito | seleção de `.default` ou `--entry Name` em módulo normal | CLI e resolver | **Direção** |
| REPL com edição, history e completion | parser, checker e HIR normais | session transacional, generations e custo de invalidação | tooling e HIR | **Direção** |
| Notebook para code, prose, data e rich output | LSP, diagnostics e outputs estruturados | kernel Jupyter, protocol tipado e export canônico | tooling e produto | **Direção** |
| Reexecução offline e provenance | resolution aninhada, artifact digests e deployments nomeados | gates de registry/provider e execução real | resolver e release | **Direção** |

`w run path/file.w -- <args>` usa o módulo normal. Sem `--entry`, ele seleciona
somente o descriptor explícito `.default`; `--entry Name` seleciona o descriptor
nomeado. Dentro de package ou workspace, a resolution aninhada do owner fornece
o grafo. Fora de projeto, o contexto efêmero aceita somente std e imports locais
explícitos. O resolver não pesquisa ambiente ou path e não baixa remote
implicitamente.

O source graph do arquivo contém somente imports explícitos. Sem package
context, a root local é o diretório do módulo. Com package context, a root é o
`build.w` selecionado pela regra de workspace vigente. `w context` mostra a
seleção discoverable, o manifest, o workspace, a resolution e as roots antes
da execução. Não há recursive scan, cwd scan, `PATH` scan ou environment
discovery. `w run path/file.w` exige um entry explícito. Ele não cria execução
arbitrária de módulo e não baixa statements finais para wrapper privado.

Uma falha do package/product efêmero não deixa manifest ou estado oculto.

PYN1 é histórico superseded. Header `script`, body implícito, `w script`, lock
standalone e `--with` não são formas correntes. A forma corrente usa somente os
records `package`/`workspace` e suas operações de dependency:

```text
w run path/file.w -- input.csv
w add package@constraint --as package_alias
```

`w resolve` e `w update` alteram a resolution do owner. A implementação de
resolver, CLI e provider permanece missing.

W-1245 permanece ledger histórico superseded por W-1412 e W-1415. A v0 mantém
somente os records P0 explícitos e não cria uma representação inline paralela.

`w repl` usa o parser, checker e HIR normais. Ele não cria dynamic mode. Cada
submission é transacional. Uma falha não altera a session. Uma declaração
aceita cria uma generation nova. Uma redefinição invalida compiled dependents e
torna esses bindings dependentes indisponíveis. O sistema nunca usa esses
bindings como stale nem os recompila implicitamente. Somente uma resubmission
explícita cria uma generation e executa effects novos. O prompt abaixo é
somente transcript de estudo:

```text
w repl
w[0]> let limit: i32 = 3
w[1]> let doubled = limit * 2
w[2]> let limit: i32 = 4
invalidated: doubled (generation 2, replaced by generation 3)
w[3]> var broken: i32 = "x"
error: generation remains 3
```

Antes de substituir uma generation, a session fecha admission, solicita
cancellation e drena structured children e waits, encerra loans e views, e
executa drops dos owned values conforme E1. Se o drain falha ou foreign
retention permanece, redefinição e reset são rejeitados ou escalam conforme a
boundary policy. O sistema nunca libera estado vivo. Uma failed submission
preserva a generation corrente. A session pode `reset`, salvar source canônico e
explicar invalidation e cost. O transcript é evidência de tooling. Ele não é
source W de release por default.

PYN2 fecha a forma executável desse contrato de design. Ele separa
`SessionId`, `SessionIncarnation`, `ExecutionOrdinal` e `GenerationId`, torna
receipts e fases machine-readable e corrige o transcript para `fn doubled` como
compiled dependent e `let snapshot = limit * 2` como valor avaliado. O fixture e
o oracle ficam em [24.1.3](DESIGN.md#2413-sessão-e-repl-transacionais).

W-1246 evita uma confirmação booleana ou modal desconectada do state. O token de
drain é one-shot e identifica sessão, generation, closure, ação e deadline.
`:drain` não repete a submission rejeitada; a pessoa decide quando resubmeter.
W-1247 rejeita persistir um heap interativo. Receipts bounded e redacted podem
ser exportados, mas valores vivos, tasks, resources e capabilities não podem ser
restaurados como se fossem source reproduzível.

Jupyter kernel é **Direção** de tooling e produto, não linguagem. PYN3 fecha o
adapter sobre o session model, o protocol `presentation.Presentable` e o export
canônico em
[24.1.4](DESIGN.md#2414-apresentação-jupyter-e-export-de-notebooks).
MIME e data têm limites declarados. `interrupt_request` solicita structured
cancellation. Não finge matar foreign code:

```text
execute_request(source) -> execute_reply(generation)
display_data(w-rich-output, bounded-mime-data)
interrupt_request -> cancellation_event(structured)
```

Notebook não é artifact ou release source por default. Antes de release, o
usuário exporta `.w` ou package canônico em ordem canônica. O export não faz
hidden replay de effects. W-1250 fixa `w notebook check`,
`w notebook export` e `:receipts`; cada comando recebe paths explícitos. A
implementação do kernel continua pós-freeze.

##### Standard library e ecossistema

| Motivo de adoção Python | Cobertura W atual | Gap real | Camada correta | Estado |
|---|---|---|---|---|
| NumPy e ciência numérica | `std.math`, `std.tensor`, shapes, `@` e device transfer | adapters Python e corpus de interoperabilidade | módulos std e adapter first-party | **Direção** |
| DataFrames e dados colunares | `std.data`, `std.csv`, `std.parquet`, `std.arrow`, schemas e database rows tipadas; TAB0 e TAB1 fecham adapters, contracts e host evidence como design | DataFrame completo fica package first-party; seguem dependency/import-root, providers reais e evidence dos gates de latency | módulos std concretos, package first-party e codecs | **Direção** |
| Plotting | `std.presentation` e PYN3 fecham o carrier bounded para texto, imagem, JSON e vendor JSON | renderer e backends de plot | first-party package ou third-party | **Pesquisa** |
| Package registry e descoberta | resolver, lock, registry e provenance de package | descoberta para workflow de arquivo único | tooling e ecossistema | **Pesquisa** |
| Amplitude de otimização e ciência | `std.math`, `std.tensor` e packages first-party | breadth de solvers, optimization e providers | packages first-party e third-party | **Pesquisa** |

Dataframe completo fica em package first-party antes de entrar na std estável.
TAB0 fecha `data.Batch<Row>`, `data.DynamicBatch`, schema identity, chunks,
copy/device policy e release. TAB1 fecha declarations, contracts, oracles e
host evidence para o workflow de CSV, Parquet e Arrow como design. W não promete
um clone de pandas.

Gaps de std e ecossistema permanecem separados:

- módulos std: carrier tabular mínimo, CSV e format contracts, e rich-display
  protocol somente após evidência;
- first-party: operações de DataFrame, plotting API e backends, Jupyter kernel,
  Python bridge e adapters DLPack/Arrow;
- third-party: providers científicos de otimização, plot e formatos que ainda
  não possuem contrato W.

##### Interop científico e Python

| Motivo de adoção Python | Cobertura W atual | Gap real | Camada correta | Estado |
|---|---|---|---|---|
| Tensor interchange sem cópia oculta | `std.tensor` e device transfer explícita | DLPack precisa de adapter com copy, device, stream, ownership, lifetime e release provados | adapter first-party | **Direção** |
| Dados colunares entre runtimes | `std.data` (`data.Batch<Row>`, `data.DynamicBatch`), schemas e C façade | Arrow C Data precisa de adapter com schema e release explícitos; CSV e Parquet ficam para TAB1 | adapter first-party | **Direção** |
| Buffer protocol do Python | C pointers e ownership de FFI | buffer pertence à bridge Python e não ao core W | bridge Python | **Direção** |
| W-from-Python e Python-from-W | C ABI e data interchange já são boundaries | stable C/Python APIs, lifecycle e fault policy | bridge, service ou fault boundary | **Direção** |

[DLPack Python](https://dmlc.github.io/dlpack/latest/python_spec.html),
[Arrow C Data Interface](https://arrow.apache.org/docs/format/CDataInterface.html)
e o [Python buffer protocol](https://docs.python.org/3/c-api/buffer.html)
fornecem os contratos de lifetime e release que o adapter deve provar. O
[Python Array API standard](https://data-apis.org/array-api/latest/purpose_and_scope.html)
não substitui esses contratos.

TAB0 fecha o carrier W em [14.4.1](DESIGN.md#1441-carrier-tabular). O contrato não
promove Arrow, Python dataframe interchange, CSV ou Parquet a autoridade W.

Um contrato de adapter deve tornar os recursos observáveis. `DLPackLease` e
`ArrowArrayLease` são nomes lógicos candidatos, não API ou syntax vigente. O
bloco é ilustrativo:

```text
DLPackLease { copy: explicit, device: explicit, stream: explicit,
              owner: explicit, release: required }
ArrowArrayLease { schema: explicit, buffers: bounded, owner: explicit,
                  release: required }
```

`fn<Python>` não é forma reservada do core baseline. Python não produz static
library previsível por default e possui runtime e object model próprios. CPython
ordinário executa por bridge, service ou fault boundary explícita. Um adapter
AOT manifest-resolved pode ser candidato conforme 19.2. Lifecycle, GIL,
interpreter e effects ficam visíveis no adapter.

##### Ergonomia e critérios de performance

| Motivo de adoção Python | Cobertura W atual | Gap real | Camada correta | Estado |
|---|---|---|---|---|
| Transformações curtas e legíveis | loops, pipelines, collections e closures | comparar comprehension documental, pipeline e loop | ergonomia R1 | **Pesquisa** |
| Operações array concisas | shapes estáticos e broadcast explícito | checked broadcasting e forma explícita de broadcast | linguagem R1 e `std.tensor` | **Pesquisa** |
| Acesso relativo ao fim | ranges, `.last` e arithmetic com guard | `get(fromEnd:)` e `suffix` ficam em **Pesquisa** | linguagem R1 | **Pesquisa** |
| Indexação negativa | ranges e accessors com bounds explícitos | Python-like `[-1]` é **Rejeitado por enquanto** por signed/unsigned, empty e bounds | linguagem R1 | **Rejeitado por enquanto** |
| Tuple e calls de baixo atrito | tuple binding, projections `.0`/`.1`, labels em ordem | labels reordenáveis ficam em **Pesquisa** | linguagem R1 | **Pesquisa** |
| Feedback imediato | HIR e tooling têm custo declarado | gates separados de first-result e steady-state | tooling e evidence | **Direção** |

PYN0 preserva labels em ordem como forma vigente. R1 mede labels reordenáveis,
mas não muda lookup ou reproducibility antes de demonstrar ganho. Comprehension,
checked broadcasting e labels reordenáveis ficam **Pesquisa**. `get(fromEnd:)` e
`suffix` também ficam **Pesquisa**. Negative indexing é **Rejeitado por
enquanto**. Pipeline, loop, `.last`, tuple binding e projections `.0`/`.1`
continuam **Forma vigente** sob suas regras. `.last` retorna um optional seguro;
arithmetic e subscript exigem guard explícito, e projections exigem `copy` ou
borrow explícito para componente move-only. Starred unpacking permanece
**Rejeitado por enquanto**.

Os gates de performance são separados:

- `time-to-first-result`: cold/warm single-file hello, edit-run incremental e
  uma transaction de 10 cells com redefinition e invalidation;
- `steady-state`: collection transforms, CSV throughput, tensor
  elementwise/broadcast/matmul CPU e zero-copy DLPack/Arrow overhead.

O corpus compara output e semântica antes de tempo. Cada registro informa
compiler version, target e hardware. Não há número fixo nem vitória declarada.
O gate compara HIR interpreter, ORC JIT, incremental native e outro backend
rápido. Nenhum segundo runtime vira autoridade semântica.


### 1.11 Alternativas retiradas do design corrente

Os itens abaixo permanecem comparáveis, mas não definem a forma vigente. A
origem identifica a seção normativa que contém a decisão atual.

**Origem: 3.5 Parsing, formatter e gate**

**Alternativa:** preservar `where` e receiver implícito no corpus comparativo.
Slots nomeados continuam aceitos para comparação e diagnostics. O
formatter emite a forma curta quando o schema não é ambíguo.

**Origem: 6.2 Visibilidade**

**Alternativa:** exigir `export` em cada field, como o opt-in de Swift e Rust.
Outra alternativa exporta todos os membros de qualquer tipo exportado. A forma
líder exporta somente os componentes de um struct transparente.

**Origem: 6.4 Evolução da interface exportada**

**Alternativa:** todo pattern externo pode ser exaustivo e qualquer field novo
é major. Outra alternativa exige um modifier de resiliência no tipo. A forma
líder evita annotations e torna a aceitação de fields futuros visível no uso.

**Origem: 7.2 Funções**

**Alternativa:** consumir o receiver sem marker no call site, como Swift e Rust.
Outra alternativa usa somente uma free function com parâmetro `take`. A forma
líder mantém a transferência visível e preserva method lookup.

**Origem: 7.2.1 Overloads por forma de call**

**Alternativa:** tipos e constraints podem escolher o melhor candidato. Outra
alternativa proíbe todo overload e exige nomes distintos. A forma vigente permite
APIs naturais e mantém a seleção local, finita e reproduzível.

**Origem: 7.4 Patterns de struct**

**Alternativa:** usar `{field}` como record pattern. Outra alternativa usa
posições sem nomes. A forma vigente reutiliza `Type(...)`, mantém labels nominais
e evita reservar `{}` para um segundo modelo de record.

**Origem: 7.5 Valores callable e closures**

**Alternativa:** um único `fn` apagado simplifica annotations, mas oculta capture,
dispatch e possível allocation. Outra alternativa usa protocols `Fn`, `FnMut` e
`FnOnce`. A forma vigente reutiliza `some`, `any`, `mut` e `take`.

**Origem: 8.3 Construção e inicialização**

**Alternativa:** usar somente um initializer canônico e factories nomeadas.
Outra alternativa permite `init?`, `async init` ou delegação após inicialização
parcial. A forma vigente aceita vários initializers por forma, delegação total e
falha tipada.

**Origem: 8.4 Propriedades computadas**

**Alternativa:** permitir accessors com efeitos, observers e static computed
properties. Esses recursos precisam superar o corpus de previsibilidade antes
de entrar.

W-1478 encerra a alternativa de argumentos por aplicação de behavior. O
[Swift SE-0258](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0258-property-wrappers.md)
permite argumentos de initializer, backing sintetizado e uma projection
adicional. Essa combinação atende wrappers com configuração runtime. Ela também
cria outro caminho de initialization, storage e API da property.

O Restaurante separa os casos. `Versioned.modify` usa `defer` para atualizar um
mutation epoch depois do borrow. Um limite estático do forno pertence ao tipo
lógico. Um auditor runtime pertence ao service ou método que possui a
authority. Essa divisão evita capture ambiental e mantém `try` e `await` no
call site. Por isso, W aceita somente `initialValue` e não expõe argumentos,
projection ou backing na aplicação do behavior.

**Origem: 12.7 Mobilidade e captures**

**Alternativas:** `T<mobility: .transferable>` usa um static slot nomeado.
`T: Send`, `T: Sync` e `T: Sendable` usam marker protocols públicos. As formas
ficam rejeitadas no design vigente porque permitem conformance nominal para uma propriedade
que safe W deve derivar.

**Origem: 14.2.7.1 Rede SDK0 e o carrier `std.net`**

**Alternativa:** um socket global ou um constructor público de `Network`
esconderia authority e permitiria SSRF. A forma vigente exige capability do
host e valida cada tentativa.

**Origem: 14.2.7.1 Rede SDK0 e o carrier `std.net`**

**Alternativa:** expor nomes como `"tcp4"` ou herdar file descriptors levaria
semântica do target para source comum. O gate interno SDK0 usa tipos fechados.
Ele não aceita raw sockets, descriptors herdados ou socket-option escape hatch.

**Origem: URL e URLSearchParams**

**Alternativa:** manter String para URL e method reduziria tipos no SDK. Essa
forma repetiria parse e perderia custom method validado.

**Origem: URL e URLSearchParams**

**Alternativa:** clone sem limite manteria a assinatura Web. Um consumidor lento
poderia reter o body inteiro. Esta forma não entra na baseline.

**Origem: 14.3.2.1 Erros de documento HTTP**

**Alternativa:** `Response.text`, `.bytes`, `.stream` e `.html` removeriam um
argumento. Eles duplicariam `BodySource` e divergiriam da API Web.

**Origem: 14.3.2.1 Erros de documento HTTP**

**Alternativa:** confirmar entrega quando o handler retorna exigiria aguardar o
socket. Isso removeria streaming e prenderia o request root ao peer.

**Origem: Integração HTTP, streams e service transfer**

**Alternativa futura:** um live-control edge explícito poderia transportar um
signal independente por RPC. A implementação de workerd possui serialização
especial para AbortSignal em sua boundary RPC; consulte
[`basics.c++`](https://github.com/cloudflare/workerd/blob/main/src/workerd/api/basics.c++)
e
[`http.h`](https://github.com/cloudflare/workerd/blob/main/src/workerd/api/http.h).
Essa evidência preserva a alternativa, mas não publica a superfície no SDK0.

**Origem: 14.3.9.1 Adapters direcionais de schema**

**Alternativa:** um serializer universal baseado em reflection reduziria
boilerplate, mas criaria schema implícito, custo não bounded e incompatibilidade
de nomes. Fica **Rejeitado**.

**Origem: 14.3.9.1 Adapters direcionais de schema**

**Alternativa:** chamar o output determinístico de canonical JSON ou JCS daria
uma garantia de signature que o profile não prova. Fica **Rejeitado**.

**Origem: 14.4.1 Carrier tabular TAB0**

**Alternativa:** um `Table<Row>` estável ou um DataFrame completo poderia
combinar batches e operações. Fica em package first-party para manter o core
pequeno. **Rejeitado por enquanto:** `Any` universal, duck typing, reflection
unchecked, union silencioso de schema, carrier row-array universal, copy
implícito, release duplicada e ABI física derivada do layout.

**Origem: 19.2 `fn<Language>`**

**Alternativa:** um raw body com fence hash permitiria recovery sem conhecer a
linguagem. Ele acrescentaria uma segunda delimiter syntax, ainda exigiria o
adapter para validar o body e tornaria migração menos direta. W mantém braces e
um scanner fixado no adapter. Source que precisa de preprocessor ou delimiter
mais complexo usa uma foreign unit externa. O fence fica **Rejeitado**.

A façade C é um carrier de link, não uma conversão do source para C. Um provider
agrupa ilhas compatíveis para evitar runtimes e símbolos duplicados. Isso é
especialmente importante para `staticlib` Rust, que pode incluir dependencies e
partes do runtime, conforme a
[Rust Reference](https://doc.rust-lang.org/reference/linkage.html). Compartilhar
LLVM pode habilitar LTO, mas não prova ABI, layout, runtime ou ownership. A
[MLIR Dialect Conversion](https://mlir.llvm.org/docs/DialectConversion/) exige
conversões e regras de legalidade explícitas.

Target triple, sysroot, include paths e libraries precisam ser inputs da recipe,
nunca escolhas ambientais do host. O
[guia de cross-compilation do Clang](https://clang.llvm.org/docs/CrossCompilation.html)
mostra essa separação. C é o provider de bootstrap. Rust, Swift, Zig, C++,
Fortran e adapters AOT de linguagens com runtime são candidatos, não promessas
do core.

**Origem: Performance e alternativas**

**Alternativa:** CBOR único sem directory reduz o header. Ele exige scan completo
para achar um body e não separa digests ou failure locality.

**Origem: 20.4.9 Libraries W e version skew**

**Alternativa:** uma ABI W resiliente permitiria trocar libraries sem rebuild.
Ela exigiria field accessors, indirect value operations, metadata estável,
nonexhaustive enums e runtime permanente. O
[modelo de library evolution do Swift](https://www.swift.org/blog/library-evolution/)
mostra o custo e recomenda ativá-lo somente quando client e library evoluem
separadamente. W deixa essa forma **Rejeitado por enquanto**. Source rebuild,
C façade, component e service schema cobrem evolução independente.

**Origem: 21.1.1 Workspace**

**Alternativa:** manter configuração de workspace apenas fora do repository
facilita overlays pessoais. Ela torna CI, lock e seleção de members menos
observáveis.

**Origem: 21.1.3 Features sem defaults ocultos**

**Alternativa:** features condicionais dentro de qualquer statement reduzem o
número de modules. Elas aumentam o número de programas possíveis por arquivo e
dificultam interface diff, testes e leitura por ferramentas.

**Origem: 21.1.4 Variantes por target**

**Alternativa:** uma expressão Boolean geral permite `not`, flags privadas e
nesting. Ela dificulta prova de disjointness e deixa o resultado dependente de
inputs abertos.

**Origem: 21.1.4 Variantes por target**

**Alternativa:** escolher o case mais específico permite overrides curtos. A
regra cria priority implícita quando o manifest cresce. A v0 exige cases
disjuntos.

**Origem: 21.1.7 Source snapshot publicável**

**Alternativa:** publicar todos os arquivos não ignorados reduz configuração.
Ela transforma uma policy local e mutável em boundary de distribuição.

**Origem: Envelopes e artifacts compostos**

**Alternativa:** usar sempre a toolchain instalada no host reduz o setup
inicial. Ela deixa uma atualização de IDE, SDK ou environment mudar a recipe sem
uma decisão no projeto.

**Origem: Envelopes e artifacts compostos**

**Alternativa:** distribuir todos os SDKs com W aproxima a ergonomia cross-target
do Zig. Ela não é possível para todo SDK, licença ou device pack. W distribui o
que pode e usa providers explícitos para o restante.

**Origem: Envelopes e artifacts compostos**

**Alternativa:** fixar um archive por module simplifica inspection. Ela impede o
compiler de escolher uma granularidade melhor para incremental build, LTO e
dead stripping. Module continua uma unidade semântica; archive é uma decisão da
recipe.

**Origem: 23.1.7 Calls dependentes e “Time Travel”**

**Alternativas:** `CallPipeline.build { ... }` parece uma closure comum e pode
sugerir record-replay runtime. O builder fluente repete operações e projections
que o type checker já conhece. Tornar toda call uma promise lazy esconde a
boundary em código comum. As três formas ficam rejeitadas no design vigente.

Outras grafias e superfícies preservadas no corpus comparativo:

- named type arguments, incluindo `Result<Success: Dish, Failure: Error>`;
- `comptime expression` e `const { ... }` sem binding;
- wildcard import com exceções nomeadas;
- `Void` como nome alternativo para `()`;
- seletor explícito de overload por forma;
- refinements escritos com `where` ou constructor;
- álgebra pública de case-sets;
- `T<[P, Q]>` e cláusula postfix `where` para protocol composition;
- `Type<T>` como metatype preservado e `\Type.member` como property path;
- `pin let` e `let pin` para local pinned lexical;
- `struct<layout: .c, packing: 1>`, `packed` e `aligned`;
- picoseconds, femtoseconds e attoseconds no clock operacional;
- `transaction using provider` com `tx` implícito;
- `entry(args, ctx) { ... }` e `async entry`;
- vários entry descriptors selecionados em runtime;
- `io.transfer` com scratch explícito no call;
- `Int` target-width, literal default `i32` ou `BigInt` default;
- fields de wire com unidade no nome, como `tickDurationSeconds`;
- dCBOR, offset directory e TLV intercalado no WMeta.
- `{ args in body }` e `fn(args) { body }` como closure syntax;
- initializer self-referential por uma construção pinned dedicada;
- lista ou nesting arbitrário de property behaviors;
- protocol genérico para datagram sem um segundo transporte equivalente;
- `slice`, `span`, `borrow` e `readonly` no lugar de `view`;
- matrix literal `[1 2; 3 4]`;
- “A última linguagem que você vai precisar aprender” como slogan.
- ordinals no source, hash do nome e ordem de declaração no lugar de
  `interface.lock`;
- metadata PEP 723 em comment, sibling manifest obrigatório, package-only e
  CLI `--with` no workflow PYN1 superseded.

### 1.12 Evidência da sessão transacional PYN2

A classificação `complete`/`incomplete` segue a separação entre console e
`compile_command` documentada por [Python `code`](https://docs.python.org/3/library/code.html)
e [Python `codeop`](https://docs.python.org/3/library/codeop.html). O modo W é
hermético e tipado, portanto não copia o namespace mutável do Python.

[IPython autoreload](https://ipython.readthedocs.io/en/stable/config/extensions/autoreload.html)
é evidência de patch/reload e de seus caveats. W não promete patch automático
nem replay de effects.

[Julia world age](https://docs.julialang.org/en/v1/manual/worldage/) é evidência
de visibilidade exata por world. W usa generation explícita, invalidation por
hard edge e resubmission explícita.

[Pluto reactivity](https://plutojl.org/en/docs/reactivity/) é evidência de rerun
implícito por dependência. W recusa rerun implícito porque ele repetiria effects.

[Jupyter messaging](https://jupyter-client.readthedocs.io/en/latest/messaging.html)
é evidência para session, counters, `busy`/`idle`, request identity e replies.
Jupyter/rich output é PYN3. PYN2 reserva somente receipts, snapshots e output
bounded do session core.

| Sistema | Fato usado | Decisão W |
|---|---|---|
| Python | namespace mutável e console com completeness | `w repl` hermético, parser/checker/HIR normais |
| IPython | autoreload aplica patch com caveats | sem patch ou replay automático |
| Julia | world age separa visibilidade | generation e HIR version exatos |
| Pluto | dependência reativa reroda cells | hard invalidation e resubmission explícita |
| Jupyter | session/counter/status/request messages | protocolo rico fica PYN3 |

As fontes sustentam somente esses fatos. Elas não definem syntax W, runtime W,
rollback externo ou rich display.

Alternativas humanas permanecem explícitas: `_`/`ans` como binding implícito do
último resultado fica em **Pesquisa**. O baseline mostra tail result para
display sem criar binding ou generation; `;`/discard suprime o display. Reactive
rerun continua recusado porque repetiria effects. `let snapshot` continua um
valor calculado; somente a função com lookup compilado é dependente.

O corpus [`tooling/repl-session-cases.json`](tooling/repl-session-cases.json),
o snapshot JSONL e o teste host exercitam identidades, parser versus semantic
classification, expressions/statements/loops/calls/await/spawn/defer, transações,
effects sobreviventes e provider outcomes, snapshot versus hard edge com
BindingId/version/kind, imports locked, Copy staging, snapshot, adapter e
deferred-no-fail, cinco rejeições de ownership, preflight derivado, scopes
independentes, post-publish degraded, reset, stale opaque base, FIFO writer,
reader durante staging, active/queued cancellation, quit/drain, quotas por
família, redaction, outputs e bounds. O fixture parseável é
[`repl_session_oracle.w`](reference/last-light/repl_session_oracle.w), com o
mapa do transcript e o watcher do buraco negro. O README do produto mapeia a
aceitação e os adversariais.

O estado desta subseção é **Direção** de design e oracle. PYN2 não implementa
CLI, compiler, checker, HIR, runtime, provider, resource drain ou Jupyter.

### 1.13 Evidência de apresentação, Jupyter e export PYN3

O fixture do Última Luz deve cobrir uma prévia bounded do cardápio, leitura do
sensor do buraco negro, tensor em device sem copy, output textual do Bistromath,
erro redacted, cancellation e export de cells. Os casos adversariais incluem
HMAC inválido, replay, media duplicada, JSON fora do limite, active content,
silent mutation, password persistence, idle prematuro, counter usado como
generation, cell invalidada e export com effect unknown.

Os modos normativos de falha do bundle usam `W-PRESENTATION-0001`,
`W-PRESENTATION-0002`, `W-PRESENTATION-0003`, `W-PRESENTATION-0004`,
`W-PRESENTATION-0005`, `W-PRESENTATION-0006`, `W-PRESENTATION-0007`,
`W-PRESENTATION-0008`, `W-PRESENTATION-0009`, `W-PRESENTATION-0010`,
`W-JUPYTER-0001`, `W-JUPYTER-0002`, `W-JUPYTER-0003`, `W-JUPYTER-0005`,
`W-JUPYTER-0006`, `W-JUPYTER-0007`, `W-JUPYTER-0008`, `W-JUPYTER-0009`,
`W-EXPORT-0001`, `W-EXPORT-0002`, `W-EXPORT-0003`, `W-EXPORT-0004`,
`W-EXPORT-0005`, `W-EXPORT-0006` e `W-EXPORT-0007`. Esses códigos descrevem
somente falhas de media, segurança, lifecycle, bounds, prova ou export. O
tooling não cria IDs de ledger para bookkeeping.

PYN3 fecha design e oracles. Ele não implementa ZeroMQ, kernel process,
sanitizer, notebook frontend, compiler, runtime ou provider. DLPack continua um
bundle próprio. Plotting e DataFrame completo continuam packages
first-party ou third-party sobre `std.presentation`.

W-1248 mantém presentation append-only. `display_id`, update e clear exigiriam
um handle de frontend com owner, cancellation, drain e quota próprios. Progresso
append-only cobre o baseline sem criar esse lifetime oculto. W-1249 limita
Jupyter history a tail bounded; range e search exigiriam retenção e índice de raw
source incompatíveis com redaction por default. W-1250 fixa a primeira CLI para
evitar três nomes abstratos diferentes em documentação, testes e produto.

### 1.14 Resultado das pesquisas consolidadas

Esta tabela preserva o snapshot que precedeu o fechamento das formas correntes.
Ela não cria uma segunda lista normativa de features.

**Exemplo:** W-1477 torna `ReadBatch` vigente. `inout T...` fica rejeitado. Os
dois resultados tratam a mesma necessidade sem deixar uma decisão ambígua.

Todos os itens antes classificados como **Pesquisa** possuem agora uma saída:

| Grupo | Provável ou possível | Rejeitado ou adiado |
|---|---|---|
| tipos | typed property path e `StateGraph` const | anonymous sum/record, constraint list, GAT, packs e existential opening |
| compile time | `WMeta1` com chunks CBOR | callable const indireto, SMT geral e autotuning no build |
| memória | texto bounded por composição, trusted foreign facts e layout privado por evidence | `InlineString`, public unpin, high-bit baseline, cache contract no tipo e async-close universal |
| execução | dynamic execution-domain selection, topology types, advanced atomics, fences e sync | QoS em `spawn`, permit type rule, `yield`, safe RCU e service reentrant |
| workflow | roots explícitos por service/`SupervisorRef`, steps e outbox | child workflow/`continueAsNew` intrínsecos, durable race, absolute core sleep, user compaction e 2PC implícito |
| I/O | `ReadBatch`, `io.transfer` e commit-provider SPI interno fechado | zero-copy implícito, `flush` universal e transaction multi-provider |
| services | wWire, resolver nominal, adapters lock-fixed e `PersistentRef` posterior | registry do core, custom adapter SPI na v0, 0-RTT, opaque capability relay, direct introduction e distributed ref equality |
| foreign | source islands separadas e adapters Rust/Swift após C | library unload físico e ABI W resiliente sem matriz de targets |
| numeric e target | FixedDecimal, Rational, Complex e device kernels | Posit/Unum universal, unit sem delimiter e ASIC/FPGA como target geral |

Um item **Provável** ainda precisa de implementação e oracle. Isso não o torna
uma pergunta sem decisão. Um gate pode reprovar a abordagem e exigir nova
evidência. Até isso ocorrer, a alternativa e o fallback acima permanecem
canônicos.

### 1.15 Evidence de memória e execução

Esta seção registra os ensaios que sustentam as seções 9 e 12 de
[`DESIGN.md`](DESIGN.md). Contagens e limites não definem a linguagem. O
contrato corrente continua no documento normativo.

#### Memória M1

M1 mantém uma forma aceita e uma inversão para cada regra crítica:

| Regra | Forma aceita | Inversão rejeitada |
|---|---|---|
| reborrow | `M1-exclusive-parent-disjoint-children` | `M1-reborrow-child-widens-parent` |
| duplicated child | `M1-duplicate-child-releases-parent-after-all-ends` | `M1-duplicate-child-keeps-parent-frozen` |
| ProofFacts | `M1-proof-fact-index-inequality` | `M1-proof-fact-wrong-place-rejected` |
| active variant | `M1-enum-active-variant-fields-disjoint` | `M1-enum-active-variant-wrong-place-rejected` |
| dependency copy | `M1-copy-shared-edge-releases-after-both-drops` | `M1-copy-shared-edge-blocks-until-both-drops` |
| owner drop | `M1-owner-drop-unblocked-after-dependent-drop` | `M1-owner-drop-blocked-by-stored-edge` |
| dependency access | `M1-exclusive-dependency-write` | `M1-shared-dependency-write-rejected` |
| dependency overlap | `M1-exclusive-dependency-disjoint-field` | `M1-exclusive-dependency-overlaps-shared` |
| dependency identity | `M1-array-ref-duplicate-origin-preserved` | `M1-ambiguous-dependency-origin` |
| dependency join | `M1-dependent-store-joins-origins` | `M1-dependent-join-reads-source` |
| service boundary | `M1-immortal-service-with-boundary-capabilities` | `M1-immortal-service-needs-boundary-capabilities` |
| persistence boundary | `M1-immortal-persistence-with-schema` | `M1-immortal-persistence-needs-schema` |
| await | `M1-await-stable-referent-accepted` | `M1-await-stable-aggregate-unstable-referent` |
| pin | `M1-pinned-handle-move-with-active-loan` | `M1-pinned-handle-drop-with-active-loan` |
| interface | `M1-bodyless-result-slots-remain-distinct` | `M1-interface-witness-divergence` |
| body result mapping | `M1-interface-body-maps-origins` | `M1-interface-body-missing-result-slot` |
| import interface key | `M1-abi-exact` | `M1-interface-key-presence-asymmetric` e `M1-interface-keys-both-absent` |
| FFI inline | `M1-language-function-explicit-proof` | `M1-language-function-needs-proof` |
| ciclo shared | `M1-weak-capture-breaks-strong-cycle` | `M1-closed-strong-cycle-rejected` |
| censo após drain | `M1-live-root-is-not-residual-cycle` | `M1-drained-residual-cycle-rejected` e `M1-unrelated-root-does-not-hide-residual-cycle` |

[`borrowed_values.w`](reference/last-light/borrowed_values.w) mostra as formas W
positivas. [`memory-transition-cases.json`](tooling/memory-transition-cases.json)
contém as inversões e os estados esperados.

#### Alocação A0

A0 modela:

- layout, alignment, zero-size e limits do provider;
- allocate, zeroing, excess capacity, resize e fallback;
- strong failure e exact-once deallocation;
- origin token, provider lifetime e mobility de domain;
- loans, pinning, address leases e relocation;
- progress requirements, bulk release e rehome;
- logical retirement separado de physical reuse.

O modelo usa bytes pequenos e providers host independentes. Ele não mede um
allocator real. Benchmarks de `system`, mimalloc e fixed pertencem ao gate de
implementação. A0 não repete cleanup tipado de M1 nem aritmética física de L0.

#### Representação

Os spikes antigos permanecem como corpus adversarial:

| Hipótese | Resultado | Motivo |
|---|---|---|
| dois low bits distinguem integer, float, compound e shared | rejeitada | reduz range e precisão, altera IEEE e exige alignment universal |
| high bits guardam length, subtype ou reference count | fora da baseline | address width, MTE, PAC, ABI e mutation variam por target |
| pointer identifica owner, object ou generation | rejeitada | reuse de endereço, ABA, move e provenance são fatos diferentes |
| uma word tagged substitui todos os valores W | rejeitada | aggregates, capabilities, SIMD, C ABI e device pointers exigem carriers próprios |
| heap implícita por módulo controla todo lifetime | rejeitada como default | import não cria instance; `Arena` e service ownership cobrem o caso delimitado |

Uma implementação interna pode recuperar uma dessas técnicas quando preserva o
valor lógico, oferece fallback e passa os oracles diferenciais. O
[`representation_oracle.w`](reference/last-light/representation_oracle.w)
aplica a matriz de fronteiras ao Última Luz.

ABI e linker usam como evidência
[LLVM parameter attributes](https://llvm.org/docs/LangRef.html#parameter-attributes),
[LLVM calling conventions](https://llvm.org/docs/LangRef.html#calling-conventions),
[layout](https://doc.rust-lang.org/stable/reference/type-layout.html) e
[ABI nativa](https://doc.rust-lang.org/reference/items/external-blocks.html#abi)
de Rust, a
[separação de ABI e library evolution do Swift](https://www.swift.org/blog/abi-stability-and-more/)
e sua
[calling convention](https://github.com/swiftlang/swift/blob/main/docs/ABI/CallingConvention.rst).
Modules, symbols, components e otimização foram comparados com
[Clang modules](https://clang.llvm.org/docs/Modules.html),
[mangling v0 de Rust](https://doc.rust-lang.org/beta/rustc/symbol-mangling/v0.html),
[WIT](https://component-model.bytecodealliance.org/design/wit.html),
[ThinLTO](https://clang.llvm.org/docs/ThinLTO.html) e a
[C API do MLIR](https://mlir.llvm.org/docs/CAPI/). Essas fontes sustentam a
separação dos contratos; não definem uma ABI W por precedente.

#### Niches, tags e headers considerados

`Option<ref Oven>` pode usar null porque `ref Oven` não aceita null. O mesmo
raciocínio não comprime automaticamente `Option<Option<ref Oven>>`: um null
separa dois estados, não três. Uma tag explícita continua sendo o fallback que
preserva todos os cases.

Um enum subset reduz os cases que o caller precisa tratar, mas não cria uma
promessa pública de tamanho. Especialização interna pode remover tags
impossíveis. ABI e persistence continuam ligadas ao layout ou schema publicado.

Alignment oferece somente candidatos a low bits. A escolha também precisa de
provenance preservada, operação válida no address space, tooling compatível e
conversão para a forma canônica em cada fronteira. Allocation estrangeira usa o
alignment efetivo, não o alignment nominal que W desejaria. Essa foi a razão
para rejeitar um contrato de tagged pointer no source.

Reference count, generation, allocator identity e deallocator não cabem num
contrato geral de address bits. Eles mudam, podem sofrer wrap ou precisam
sobreviver à canonicalização. Uma generation curta também não elimina ABA.
Epoch, hazard pointer e reclamation equivalente permanecem escolhas internas da
estrutura que precisa dessa garantia.

Os exemplos `BellHandle`, `shared MenuSection` e `ServiceRef<Kitchen>` pedem
metadata diferente. Um header universal desperdiçaria storage e confundiria
owner, capability e runtime identity. Por isso owner único pode ser headerless,
`shared` pode usar control block e service state fica no host.

NaN boxing foi mantido fora da baseline porque W preserva payload de NaN e
signed zero em `f64`. Um carrier dinâmico interno pode pesquisar a técnica com
fallback. Ele não pode mudar arrays de `f64` nem tornar a otimização observável.

#### Domains, channels e scheduler

O [`domain_oracle.w`](reference/last-light/domain_oracle.w) cobre herança de
initializer `async`, domínio explícito de `spawn`, FIFO serial, gate `.parallel` e
redução de capacity.

Dispatch barrier e read/write lock resolvem problemas relacionados, mas não
idênticos. A documentação da Apple define barrier como exclusão entre work items
anteriores e posteriores de uma private concurrent queue; numa queue serial ou
global, o barrier se comporta como dispatch comum. A mesma documentação mantém
queues seriais FIFO e recomenda target queues para evitar pools físicos por
queue:

- [dispatch barrier](https://developer.apple.com/documentation/dispatch/dispatch_barrier_async);
- [dispatch queues](https://developer.apple.com/documentation/dispatch/dispatch-queue).

Por outro lado, o
[`RwLock` do Rust](https://doc.rust-lang.org/std/sync/struct.RwLock.html) protege
storage com readers simultâneos e um writer, mas deixa priority ao OS. O
[`RWMutex` do Go](https://pkg.go.dev/sync#RWMutex) bloqueia novos readers quando
um writer espera e proíbe upgrade, downgrade e recursive read locking.

W conserva domains seriais estáticos, lanes seriais dinâmicas e barrier de
domain para trabalho estruturado. W-1259 retira `ReadWriteLock<T>` da baseline:
`SnapshotCell` cobre versão imutável, `lock` cobre exclusão síncrona e um adapter
especializado continua possível quando benchmark e target justificarem.

CH0 usa [`streams.w`](reference/last-light/streams.w) como source e a
[`channel-machine.mjs`](tooling/channel-machine.mjs) para derivar o estado. O
[`channel-cases.json`](tooling/channel-cases.json) possui 47 casos e 333
operações: 28 aceitos e 19 rejeitados. Doze testes independentes não leem o
snapshot. O conjunto cobre:

- dois producers e um consumer com capacity 0, 1 e 64;
- fechamento pelo último sender e close gracioso pelo receiver;
- receiver abortivo com itens e permits pendentes;
- recuperação do item em `.full` e `.closed`;
- cancellation antes e depois de cada commit;
- FIFO por sender e nenhuma ordem presumida entre senders;
- `trySend` sem bypass;
- equivalência lógica entre estratégias ring e mutex.

Cada item termina num receiver, num error que devolve o owner ou num cleanup.
O checker rejeita item sem terminal, drop duplicado, permit não consumido e
endpoint ainda vivo no fim do scope. O snapshot registra estado lógico e trace
físico separadamente.

CH0 não executa W e não prova scheduler ou provider. O perfil de implementação
ainda precisa repetir os casos com uma, duas e quatro worker threads, TSan, leak
sanitizer e allocation fault injection. `Stream.cancel`, tee e adapters bounded
continuam nos ensaios próprios de stream, sem alterar a semântica de channel.

#### False sharing e layout de contenção

False sharing precisa de state compartilhado, execução em cores diferentes e
ao menos uma write. Separar fields pode melhorar throughput, mas aumenta
footprint e pode deslocar o gargalo. Por isso W não transforma uma dica de cache
em parâmetro semântico de `Atomic<T>`.

Os precedentes mostram o mesmo limite por caminhos diferentes:

- [C++ P0154R1](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0154r1.html)
  define destructive e constructive interference sizes como recomendações de
  quality-of-implementation para `alignas`, não como garantia física;
- [OpenJDK JEP 142](https://openjdk.org/jeps/142) usa padding de fields
  contended e registra o custo de memória e a dificuldade de manter alignment;
- o [guia do kernel Linux](https://docs.kernel.org/kernel-hacking/false-sharing.html)
  recomenda primeiro medir e então separar hot fields, evitar writes ou usar
  state per-CPU com agregação;
- [`CacheLinePad` de Go](https://go.dev/src/internal/cpu/cpu.go) usa tamanho por
  arquitetura como aproximação, sem detecção runtime;
- [`CachePadded<T>` de Crossbeam](https://docs.rs/crossbeam-utils/latest/crossbeam_utils/struct.CachePadded.html)
  é um wrapper de layout com estimativas específicas por arquitetura.

| Alternativa | Resultado W | Motivo |
|---|---|---|
| `Atomic<T, cache: .isolated>` | rejeitada | mistura semântica atômica com uma hipótese de layout e target |
| wrapper `CachePadded<T>` safe universal | rejeitado | propaga detalhe físico, aumenta tipos e ainda não garante a microarquitetura real |
| padding/reorder privado automático | vigente | preserva semântica quando layout e footprint não são observáveis |
| counters por child/domain + join | vigente e explícito | muda publicação e snapshots, portanto deve aparecer no source |
| record físico em adapter/target | vigente na boundary | offsets e alignment são verificáveis; desempenho continua medido |

IL0 modela a decisão do optimizer com facts fechados. O corpus não mede uma
cache real. O gate de implementação precisa executar perfis com e sem layout,
em mais de um número de cores, e publicar throughput, latency, misses, footprint
e recipe inputs. Uma regressão ou ganho não reproduzível desativa a decisão do
profile sem alterar o programa.

#### I/O assíncrono

O oracle do restaurante precisa inverter:

- vazio, EOF depois de data e partições de 1 a 4096 bytes;
- short read e short write em cada posição;
- error antes e depois de progress;
- cancellation antes de submit, durante espera e depois de completion;
- destination sem mutation quando cancellation vence;
- buffer vivo até cancel drain;
- reads posicionais fora de ordem e cursor sequencial sem offset perdido;
- backends blocking, readiness e completion com o mesmo resultado;
- gather vazio, acima do limite nativo e partial dentro ou entre segments;
- fallback, `writev` e `WSASend` com o mesmo byte stream;
- `Stream<view Bytes>` sem allocation depois da reserva;
- limits antes de growth, leak sanitizer, TSan e fault injection.

#### Cobertura executável

Estas eram as contagens em 11 de agosto de 2026:

| Perfil | Corpus | Resultado | Host independente | Limite principal |
|---|---:|---:|---:|---|
| M1 memory transition | 185 casos, 606 operações | 82 aceitos, 103 rejeitados | checker puro | não executa W |
| A0 physical allocation | 48 casos, 123 operações | 15 aceitos, 33 rejeitados | 13 testes | não mede allocator real |
| L0 layout e ABI | 78 casos, 96 operações | 27 aceitos, 51 rejeitados | 10 testes | não implementa linker, importer ou backend |
| execution ergonomics | 77 casos | 31 positivos, 44 negativos, 2 informações | 26 testes | não compila, não resolve value category e não agenda W |
| E0 concurrency | 73 casos, 677 operações | 38 aceitos, 35 rejeitados; 10/10 origens HB | 17 testes | valida witness; não enumera execuções |
| E1 liveness | 41 casos, 473 operações | 19 aceitos, 22 rejeitados | 7 testes | não prova clock, OS I/O ou terminação de user code |
| MX0 ownership + execution | 46 casos, 274 operações | 23 aceitos, 23 rejeitados | 14 testes | compõe modelos; não executa checker, scheduler ou runtime W |
| CH0 bounded channel | 47 casos, 333 operações | 28 aceitos, 19 rejeitados | 12 testes | não implementa scheduler, runtime ou provider W |
| LM1 language lock | métricas derivadas pelo checker | sync/await/try e alternativas | testes host | não implementa runtime/provider |
| SP0 snapshot cell | 27 casos, 82 operações | 14 aceitos, 12 rejeitados, 1 fault | 7 testes | não implementa reclamation físico |

Esta tabela é um registro histórico de 11 de agosto de 2026. A linha M1 foi
supersedida pela projeção corrente de 16 de agosto de 2026: 79 aceitos e 106
rejeitados após a regra de origem única; não use a contagem histórica como
status atual.

E0 cobre lifecycle, cancellation, fail-fast, as dez origens de happens-before,
races, modification order, fences, RMW, extents e tickets de barreira. Ele não
prova liveness, fairness, preemption, oversubscription, task-frame allocation,
reentrância de service, device scope, reclamation, ABA ou execução distribuída.

E1 separa scheduler, clock e provider do contrato de closure. MX0 usa um único
witness para testar owner graph e task lifecycle juntos; ele impede que duas
provas isoladas escondam copy, share, rollback ou drop divergente. LM1 cobre
loans, busy sem body, cancellation, unlock, drop e fault boundary. SP0
cobre readers antigos e novos, publicação concorrente, retirement bounded,
drop, OOM antes de publish, close e estratégias equivalentes.

CH0 fecha ownership linear, admission, permits, cancellation e lifecycle do
channel bounded, sem prometer a estratégia física.

Os profiles de representação também passam pelo mesmo corpus lógico nas formas
portátil e otimizada. Sanitizers exercitam o fallback e, quando possível, a
forma compacta. Diferença observável bloqueia a otimização. SP0 inverte readers
antigos e novos, erro, publicação concorrente, retirement bounded, drop único,
OOM pré-publicação, close e estratégias físicas equivalentes. O scheduler de
teste injeta clock, entropy e I/O, registra decisões e reproduz joins, cancel
points, overload, drain e falha sem tornar a instrumentação observável.

### 1.16 Evidence de boundaries, packages e releases

#### Boundary effects B0

O corpus B0 contém 39 sequências e 320 operações ligadas a symbols do Última
Luz. Ele aceita 25 e rejeita 14. A máquina cobre:

- staging, envelope commit, admission, closed turn, output gate e delivery;
- cleanup único do envelope e output retido até confirmação e drain;
- `commitFailed`, `unknownOutcome(effectId)` e cancellation distintos;
- retry idempotente com o mesmo effect ID e bloqueio de `atMostOnce`;
- transaction com begin, body, commit, abort e `unknownCommit`;
- pipeline linear e diamond, fail-fast, drain e capability settlement.

Os negativos incluem delivery antes do drain, segundo closed turn, retry
inseguro, abort depois de commit request, forward reference e resolução
prematura. B0 recebe admission, evidence e effect IDs resolvidos. SR0 acrescenta
queue bounded, deduplication, journal, crash recovery e generation. Os dois são
modelos host: não executam adapter, transport, clock, storage, wWire, database
ou sistema distribuído.

#### Package e release P0

O corpus P0 contém 44 casos e 379 operações. Ele cobre:

- snapshot assinado, rollback, expiry, delegation e equivocation;
- resolução determinística, features, members e realms;
- lock estrutural, source inventories e active source sets;
- CAS, mirrors listados e reconstrução offline;
- recipe hermética, source tree, toolchain e environment declarado;
- artifact identity, quorum independente e papéis de assinatura;
- estados ortogonais de advisory, yank e revocation.

P0 usa digests SHA-256 tagged sobre records canônicos como identity do oracle. A
produção mantém algoritmo e schema versionados no envelope. O lock fixa a
estrutura da resolução; mudar bytes inventariados troca source-tree digest e
recipe, enquanto adicionar, remover ou mudar o papel de um source invalida o
lock.

Builder, operator, credential, execution root e executor registram
independência. Eles não entram na identity da recipe. Evidência incompleta é
rejeitada antes da comparação. P0 não implementa resolver, registry, CAS,
signature, prerelease SemVer, TUF, Sigstore, download, sandbox, archive reader,
path normalization ou rebuild real.

### 1.17 Fontes e perfis operacionais retirados do design normativo

Esta seção preserva referências, alternativas de provider e perfis de medição.
Ela não adiciona API nem amplia uma garantia do `DESIGN.md`.

#### Signals, I/O e perfil Web

O contrato de signals usa o executor em vez de executar W dentro do callback
bruto. As referências de host são
[POSIX `sigaction`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/sigaction.html)
e
[Windows `SetConsoleCtrlHandler`](https://learn.microsoft.com/en-us/windows/console/setconsolectrlhandler).

Os adapters de I/O comparados incluem IOCP/`WSASend`, `io_uring`, `writev`,
epoll, kqueue, poll, WASI e pools blocking bounded. W-1477 fecha a superfície
portátil de scatter read e file-to-sink. `ReadBatch` é o único owner dos
segments e nunca publica memória não inicializada. `TransferPlan` mantém o
intervalo posicional, o progresso e o scratch de fallback. `io.readMany` e
`io.transfer` podem usar uma capability interna de provider, mas o resultado
não promete a estratégia física.

`readv` e `WSARecv` confirmam preenchimento ordenado de buffers e limites de
descriptor específicos do host. `sendfile` e `TransmitFile` confirmam que a
transferência direta é especializada, possui limites e pode exigir fallback.
Rust `Read::read_vectored` e `Write::write_vectored` demonstram um fallback no
primeiro buffer, mas W evita os wrappers públicos `IoSlice`/`IoSliceMut` porque
`Bytes` já guarda initialized count e ownership. Referências verificadas em
2026-08-25:

- [`readv` e `writev`](https://man7.org/linux/man-pages/man2/writev.2.html);
- [`WSARecv`](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsarecv);
- [`WSASend`](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasend);
- [`sendfile`](https://man7.org/linux/man-pages/man2/sendfile.2.html);
- [`TransmitFile`](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-transmitfile);
- [Rust `Read::read_vectored`](https://doc.rust-lang.org/std/io/trait.Read.html#method.read_vectored);
- [Rust `Write::write_vectored`](https://doc.rust-lang.org/std/io/trait.Write.html#method.write_vectored).

O profile Web compara conceitos com
[WinterTC](https://wintertc.org/),
[Minimum Common Web API](https://min-common-api.proposal.wintertc.org/) e
[workerd](https://github.com/cloudflare/workerd). A comparação não declara W
como ECMAScript e não cria `globalThis`.

Blob e FormData usam como evidência o
[File API](https://w3c.github.io/FileAPI/), o
[FormData Standard](https://xhr.spec.whatwg.org/#interface-formdata) e a
[extração de BodyInit do Fetch](https://fetch.spec.whatwg.org/#bodyinit-unions).
O File API motivou bytes imutáveis, slice sem mutação, type normalizado e decode
UTF-8 com replacement. O FormData Standard motivou lista ordenada, repetição,
append, set e delete. W rejeitou Blob como file authority, constructor dinâmico
de parts, DOM form constructor, boundary controlada pelo caller e materialização
multipart eager. A composição `shared Bytes` fecha o value sem outro provider;
somente o codec wire continua no provider HTTP já necessário.

#### Performance e benchmark

O profile de performance usa evidência de
[LLVM `range` metadata](https://llvm.org/docs/LangRef.html#range-metadata),
[MLIR Vector](https://mlir.llvm.org/docs/Dialects/Vector/),
[MLIR Linalg](https://mlir.llvm.org/docs/Dialects/Linalg/),
[atomics no LLVM](https://llvm.org/docs/Atomics.html) e o
[modelo UTF-8 de Swift](https://www.swift.org/blog/utf8-string/).

O perfil Última Luz para
[TechEmpower Framework Benchmarks](https://www.techempower.com/benchmarks/)
preserva as sete famílias públicas: JSON, single query, multiple queries,
cached queries, fortunes, updates e plaintext. O harness precisa validar
method, path, headers, clamp `1...500`, queries distintas, `Sync` PostgreSQL,
read-modify-write, escaping UTF-8, cache real e limites de recursos antes de
medir throughput. A
[visão dos testes](https://github.com/TechEmpower/FrameworkBenchmarks/wiki/Project-Information-Framework-Tests-Overview)
e o
[repositório FrameworkBenchmarks](https://github.com/TechEmpower/FrameworkBenchmarks)
são a referência do workload.

O registro de evidência fixa harness, hardware, kernel, database, topology,
source, lock, compiler, runtime, artifact, warmup, duração e repetições. Perfis
separados medem processo único, nanoservices co-localizados, processos
separados e component host. Uma variante não pode remover validação, retornar
constants nem trocar business logic para melhorar o ranking. O estado de
submissão do projeto externo é evidência mutável e deve ser verificado antes de
publicar qualquer claim.

#### Precedentes de syntax, tipos e tooling

A separação de callables compara
[Swift SE-0111](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0111-remove-arg-label-type-significance.md),
[function pointers do Rust](https://doc.rust-lang.org/reference/types/function-pointer.html),
[closures do Rust](https://doc.rust-lang.org/reference/types/closure.html) e a
[ABI de Clang Blocks](https://clang.llvm.org/docs/Block-ABI-Apple.html).

Refinements com range têm precedente no
[Ada Reference Manual](https://docs.adacore.com/live/wave/arm22/pdf/arm22/arm-22.pdf),
e [Liquid Types](https://escholarship.org/uc/item/0vx7j8zc) demonstra predicates
verificáveis. Generics usam como evidência a
[monomorphization do rustc](https://rustc-dev-guide.rust-lang.org/backend/monomorph.html),
a [inference de Go](https://go.dev/ref/spec#Type_inference) e os
[opaque types de Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/opaquetypes/).
Units angulares têm precedente nas
[units of measure de F#](https://learn.microsoft.com/en-us/dotnet/fsharp/language-reference/units-of-measure).
O custo source de labels de type arguments aparece nos
[named type arguments do Scala 3](https://docs.scala-lang.org/scala3/reference/experimental/named-typeargs-spec.html).
O [TypeScript Handbook](https://www.typescriptlang.org/docs/handbook/unions-and-intersections.html)
é a comparação para unions e intersections anônimas; W mantém sums nominais.

O schema de diagnostics compara
[JSON do rustc](https://doc.rust-lang.org/beta/rustc/json.html),
[Language Server Protocol](https://microsoft.github.io/language-server-protocol/)
e [SARIF 2.1.0](https://docs.oasis-open.org/sarif/sarif/v2.1.0/sarif-v2.1.0.pdf).
W mantém D0 menor e usa adapters para esses formatos. Para I/O, os nomes
`Reader`/`Writer`, `AsyncRead`/`AsyncWrite` e `Input`/`Output` foram comparados;
`ByteSource`/`ByteSink` tornam direção e unidade explícitas sem repetir o efeito
`async` no nome.

Typestate usa como evidência o
[Embedded Rust Book](https://docs.rust-embedded.org/book/static-guarantees/typestate-programming.html),
suas
[zero-cost abstractions](https://docs.rust-embedded.org/book/static-guarantees/zero-cost-abstractions.html),
[Typestates for Objects](https://www.cs.cmu.edu/~aldrich/courses/819/deline-typestates.pdf)
e as
[regras de Durable Objects](https://developers.cloudflare.com/durable-objects/best-practices/rules-of-durable-objects/).
As formas adiadas com precedente em Swift são
[typed key paths](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0161-key-paths.md),
[síntese estrutural](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0185-synthesize-equatable-hashable.md)
e
[parameter packs](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0393-parameter-packs.md).

Outras comparações retiradas das regras correntes incluem:

- statement boundaries em
  [Swift](https://docs.swift.org/swift-book/ReferenceManual/LexicalStructure.html),
  [Go](https://go.dev/ref/spec#Semicolons) e
  [Rust](https://doc.rust-lang.org/reference/statements.html);
- visibilidade e records em
  [Rust](https://doc.rust-lang.org/reference/visibility-and-privacy.html),
  [Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/accesscontrol/)
  e
  [Java](https://docs.oracle.com/en/java/javase/26/docs/api/java.base/java/lang/Record.html);
- library evolution em
  [Swift](https://www.swift.org/blog/library-evolution/),
  [`non_exhaustive` de Rust](https://doc.rust-lang.org/reference/attributes/type_system.html)
  e [Semantic Versioning](https://semver.org/spec/v2.0.0.html);
- receiver ownership em
  [Swift SE-0377](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0377-parameter-ownership-modifiers.md)
  e
  [methods de Rust](https://doc.rust-lang.org/reference/items/associated-items.html#methods);
- labels e overloads em
  [Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/declarations/),
  [C#](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/expressions#1264-overload-resolution)
  e [Go](https://go.dev/doc/faq#overloading);
- opaque parameters em
  [Swift SE-0341](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0341-opaque-parameters.md),
  narrowing em
  [TypeScript](https://www.typescriptlang.org/docs/handbook/2/narrowing.html)
  e enums exaustivos em
  [Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/enumerations/).

#### Memória, execução e SDK

[mimalloc](https://github.com/microsoft/mimalloc) permanece um provider
candidato, não um default sem evidência. Versão, modes, origem de allocation,
sanitizers, unload e cross-domain free pertencem ao profile e à recipe. O
contrato normativo aceita `.system`, `.none` ou um runtime contract exato sem
prometer um allocator específico.

Transactions e closed turns foram comparados com
[`PREPARE TRANSACTION` do PostgreSQL](https://www.postgresql.org/docs/current/sql-prepare-transaction.html),
[transactions](https://www.sqlite.org/lang_transaction.html) e
[savepoints](https://www.sqlite.org/lang_savepoint.html) do SQLite,
[Swift actors](https://www.swift.org/swift-evolution/#SE-0306),
[input gates de Durable Objects](https://blog.cloudflare.com/durable-objects-easy-fast-correct-choose-three/)
e o desenho de
[nanoservices do workerd](https://github.com/cloudflare/workerd).

O network draft comparou [`std::net`](https://doc.rust-lang.org/std/net/),
[`net` de Go](https://pkg.go.dev/net),
[UDP de Tokio](https://docs.rs/tokio/latest/tokio/net/struct.UdpSocket.html),
[capabilities WASI](https://github.com/WebAssembly/WASI/blob/main/docs/Capabilities.md),
[TCP sockets de Workers](https://developers.cloudflare.com/workers/runtime-apis/tcp-sockets/),
[RFC 6724](https://www.rfc-editor.org/info/rfc6724),
[RFC 8305](https://www.rfc-editor.org/info/rfc8305),
[RFC 5952](https://www.rfc-editor.org/info/rfc5952) e
[RFC 8085](https://www.rfc-editor.org/info/rfc8085/). Esses precedentes não são
conformance de um provider W.

Os overloads de radix comparam o
[`FixedWidthInteger` de Swift](https://developer.apple.com/documentation/swift/fixedwidthinteger/init(_:radix:))
e [`from_str_radix` de Rust](https://doc.rust-lang.org/std/primitive.i64.html#method.from_str_radix).
W mantém erro typed e um refinement `2...36`; não herda `nil` ou panic por
radix inválido.

Os contratos de dados compararam
[Apple Codable](https://developer.apple.com/documentation/swift/encoding-and-decoding-custom-types),
[Serde](https://serde.rs/),
[String UTF-8 de Swift](https://www.swift.org/blog/utf8-string/),
[`String`](https://doc.rust-lang.org/stable/alloc/string/struct.String.html),
[`Vec`](https://doc.rust-lang.org/std/vec/struct.Vec.html),
[`HashMap`](https://doc.rust-lang.org/std/collections/struct.HashMap.html) e
[slices de Rust](https://doc.rust-lang.org/std/primitive.slice.html),
[`Ref`](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0519-borrow-inout-types.md)
e [`Span`](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0447-span-access-to-contiguous-storage.md)
de Swift,
[dictionary de Python](https://docs.python.org/3/reference/datamodel.html#dictionaries),
[`LinkedHashMap`](https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/util/LinkedHashMap.html)
e [`SmallString`](https://llvm.org/doxygen/classllvm_1_1SmallString.html). W não
herda layout, derivation, hashing ou threshold desses precedentes.

Texto nativo e sentinelas foram comparados com
[`OsString`](https://doc.rust-lang.org/std/ffi/struct.OsString.html) e
[`CString`](https://doc.rust-lang.org/std/ffi/struct.CString.html) de Rust. A
fronteira C também usa como evidência a separação de ownership da
[safe C++ interop de Swift](https://www.swift.org/documentation/cxx-interop/safe-interop/).
Tipos numéricos especializados consultam
[LLVM](https://llvm.org/docs/LangRef.html),
[MLIR Arith](https://mlir.llvm.org/docs/Dialects/ArithOps/) e
[MLIR Quant](https://mlir.llvm.org/docs/Dialects/QuantDialect/). Esses
precedentes não definem layout ou conversão W.

#### Targets e toolchains

LLVM, WASI, Android e MLIR fundamentam a matriz inicial de targets. Essas
fontes explicam viabilidade; somente CI e os gates W publicam suporte:

- [targets configuráveis do LLVM](https://llvm.org/docs/CMake.html);
- [política de targets experimentais do LLVM](https://llvm.org/docs/DeveloperPolicy.html);
- [WASI 0.3](https://bytecodealliance.org/articles/WASI-0.3) e
  [WIT async](https://component-model.bytecodealliance.org/design/wit.html);
- [MLIR GPU](https://mlir.llvm.org/docs/Dialects/GPU/),
  [NVVM](https://mlir.llvm.org/docs/Dialects/NVVMDialect/),
  [ROCDL](https://mlir.llvm.org/docs/Dialects/ROCDLDialect/) e
  [SPIR-V](https://mlir.llvm.org/docs/Dialects/SPIR-V/);
- [ABIs do Android NDK](https://developer.android.com/ndk/guides/abis).

O bootstrap em stages compara a prática do
[rustc](https://rustc-dev-guide.rust-lang.org/building/bootstrapping/what-bootstrapping-does.html)
e da [toolchain Go](https://go.dev/doc/install/source). A rota de alta confiança
usa [diverse double-compiling](https://dwheeler.com/trusting-trust/). O plano de
toolchain também consultou
[cross-compilation do Clang](https://clang.llvm.org/docs/CrossCompilation.html),
[Bazel toolchains](https://bazel.build/extending/toolchains),
[Android NDK](https://developer.android.com/ndk/guides/other_build_systems),
[Apple command-line tools](https://developer.apple.com/documentation/xcode/installing-the-command-line-tools),
[MSVC](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line),
[WASI SDK](https://github.com/WebAssembly/wasi-sdk), [LLD](https://lld.llvm.org/)
e
[`SOURCE_DATE_EPOCH`](https://reproducible-builds.org/docs/source-date-epoch/).

#### Packages, releases e transportes

Os invariantes de workspace, feature e source vêm de
[Cargo workspaces](https://doc.rust-lang.org/cargo/reference/workspaces.html),
[Cargo features](https://doc.rust-lang.org/cargo/reference/features.html),
[Cargo dependency sources](https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html)
e [Go workspaces](https://go.dev/ref/mod#workspaces). W não herda o resolver ou
a semântica de feature dessas ferramentas.

Target variants também foram comparadas com
[Bazel platforms](https://bazel.build/versions/9.0.0/extending/platforms),
[Swift Package Manager](https://docs.swift.org/package-manager/PackageDescription/PackageDescription.html),
[`cfg` de Rust](https://doc.rust-lang.org/reference/conditional-compilation.html)
e [build constraints de Go](https://pkg.go.dev/cmd/go#hdr-Build_constraints).
W mantém a seleção no manifest e não em comments, filenames ou statements.

O modelo de distribuição compara a especificação
[TUF 1.0.26](https://theupdateframework.github.io/specification/v1.0.26/),
[Sigstore](https://docs.sigstore.dev/),
[Rekor](https://docs.sigstore.dev/logging/overview/),
[SLSA provenance](https://slsa.dev/spec/v1.2/provenance) e
[Reproducible Builds](https://reproducible-builds.org/docs/plans/). Nenhuma
dessas fontes transforma transporte, transparency log ou identidade efêmera em
trust policy suficiente por si só.

#### Authority origin e continuidade de registry W-1469

As fontes primárias usadas para W-1469 são:

- [TUF specification 1.0.26](https://theupdateframework.github.io/specification/v1.0.26/), §§5.2, 5.3 e 6.1.
  A fonte exige trusted root out-of-band e roots sequenciais.
  Durante a rotação, exige o threshold da root anterior e o threshold da root
  nova. Ela também descreve persistência da root e verificações de rollback e
  freeze.
- [Git user manual](https://git-scm.com/docs/user-manual), seção “The Object Database”.
  A fonte afirma que o mesmo conteúdo em dois repositories recebe o mesmo
  object name. Ela separa blob, tree, commit e tag. Um commit aponta para uma
  tree snapshot e para parents.
- [Git hash-function transition](https://git-scm.com/docs/hash-function-transition.html).
  A fonte define object names SHA-1 e SHA-256 e mostra que o formato e as
  referências mudam durante a transição.
- [Go Modules Reference](https://go.dev/ref/mod), seções “Modules, packages, and versions”
  e “Module paths”. A fonte usa module path para naming e para localizar
  repository, subdirectory e version.

W usa essas fontes por inferência. TUF motivou `trustedGenesis` out-of-band,
versions sequenciais e o dual-threshold old/new. `trustedCheckpoint` é um
checkpoint resolver-owned persistido entre chamadas, não outro trust input
out-of-band. W mantém somente a continuidade bounded de roots. W-1469 não é
TUF conformant e não implementa targets, snapshot, timestamp, expiry,
freshness, freeze ou registry completo.

O Git user manual sustenta a separação entre snapshot e repository authority.
Um commit, tree ou object hash pode identificar bytes ou uma snapshot sem
identificar o repository que deve autorizar um package. A transição SHA-1/SHA-256
reforça que o hash do Git não é uma authority W estável por si só. Go module path
separa naming e localização. Ele não é um trust anchor W.

W rejeita estas alternativas:

- conceder authority por alias ou URL;
- usar a chave current como identidade;
- aceitar gênese autoassinada sem trusted anchor;
- comparar digest sem carregar bytes completos;
- fazer replay desde a gênese em toda chamada ou aplicar um limite vitalício;
- tratar commit ou tree Git como repository authority;
- tratar o lock como trust source.

Síntese adversarial do Restaurante:

- `last-light/restaurant`, `fiction/chart` e `last-light/menu-compiler`
  compartilham os mesmos bytes completos de `AuthorityOrigin`, mas têm scoped
  names distintos e, portanto, `PackageIdentity` distintas;
- a rotação válida muda evidence e checkpoint, mas preserva os bytes de origin;
- alias e mirror mudam evidence ou transporte, mas não mudam origin;
- uma gênese alternativa muda origin, mesmo quando o alias e os demais campos
  parecem iguais; e
- o lock registra refs CAS e não cria trust: o resolver carrega os bytes
  completos e os compara com o trust input.

Para metadata W, Protobuf foi descartado porque sua
[serialização não é canônica](https://protobuf.dev/programming-guides/serialization-not-canonical/).
Cap'n Proto preserva acesso direto e
[canonicalization](https://capnproto.org/encoding.html#canonicalization), mas
writers comuns não emitem essa forma por default e o bootstrap teria pointer
trees, alignment e traversal accounting adicionais. Ele continua referência
de RPC, não formato de metadata W.

O estabelecimento wRPC usa como evidência
[TLS 1.3](https://www.rfc-editor.org/rfc/rfc8446.html),
[`tls-exporter`](https://www.rfc-editor.org/rfc/rfc9266.html),
[TLS para RPC](https://www.rfc-editor.org/rfc/rfc9289.html),
[QUIC TLS](https://www.rfc-editor.org/rfc/rfc9001.html) e
[SPIFFE](https://spiffe.io/docs/latest/spiffe-about/spiffe-concepts/). A
seleção não publica keys, credentials ou capability tokens no audit.

#### Workflows, supervisors e journals

O desenho de trabalho runtime-owned e durável compara:

- [JEP 525](https://openjdk.org/jeps/525) para subtasks confinadas;
- [`waitUntil`](https://developers.cloudflare.com/workers/runtime-apis/context/)
  para lifetime bounded de trabalho auxiliar;
- [regras](https://developers.cloudflare.com/workflows/build/rules-of-workflows/),
  [sleep/retry](https://developers.cloudflare.com/workflows/build/sleeping-and-retrying/)
  e [events](https://developers.cloudflare.com/workflows/build/events-and-parameters/)
  de Cloudflare Workflows;
- [constraints](https://learn.microsoft.com/en-us/azure/durable-task/common/durable-task-code-constraints)
  e [versionamento](https://learn.microsoft.com/en-us/azure/durable-task/common/durable-orchestration-versioning)
  de Durable Task;
- [atomic commit](https://www.sqlite.org/atomiccommit.html),
  [transactions](https://www.sqlite.org/lang_transaction.html) e
  [WAL](https://www.sqlite.org/wal.html) do SQLite;
- [Durable Objects](https://developers.cloudflare.com/durable-objects/best-practices/rules-of-durable-objects/)
  e seus [alarms](https://developers.cloudflare.com/durable-objects/api/alarms/);
- [Orleans timers/reminders](https://learn.microsoft.com/en-us/dotnet/orleans/grains/timers-and-reminders)
  e [Erlang supervisors](https://www.erlang.org/doc/system/sup_princ.html).

A superfície vigente usa `WorkKeyRef.start`/`tryStart`, `work.step`,
`work.sleep` e `work.wait`. Child workflows, fan-out determinístico e adapters
`waitUntil` bounded continuam candidatos. `spawn<owner: ...>`, detach por drop,
call one-way, actor reentrant e persistência automática de frame não entram.

### 1.18 Evidência de síntese KM0 e device scopes DEV0

DEV0 mantém launch de accelerator sob as quatro formas de execução do W. A
comparação não escolhe um backend como semântica da linguagem:

- o [MLIR GPU dialect](https://mlir.llvm.org/docs/Dialects/GPU/) separa module,
  function, binary/offloading, launch, memory spaces e async tokens;
- o [MLIR Async dialect](https://mlir.llvm.org/docs/Dialects/AsyncDialect/)
  explicita dependencies e permite execução física sequencial;
- o [CUDA Programming Guide — asynchronous execution](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/asynchronous-execution.html)
  e a seção de [streams e events](https://docs.nvidia.com/cuda/cuda-programming-guide/03-advanced/advanced-host-programming.html)
  distinguem queue order de synchronization entre streams;
- a [SYCL 2020 specification](https://registry.khronos.org/SYCL/specs/sycl-2020/html/sycl-2020.html)
  usa queues, events, explicit dependencies e async errors;
- o [DLPack core](https://dmlc.github.io/dlpack/latest/) e o
  [Python Array API exchange](https://dmlc.github.io/dlpack/latest/python_spec.html)
  tornam device, stream handoff e copy policy observáveis.

KM0 fecha a etapa anterior ao launch. O
[SPIR-V](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html) modela
modules com entry points estáticos, call trees, execution environment e
specialization constants. O
[CUDA Programming Guide](https://docs.nvidia.com/cuda/cuda-programming-guide/)
mantém a configuração de execução explícita no launch. O
[WGSL](https://gpuweb.github.io/gpuweb/wgsl/) liga pipeline e entry point e
valida o código alcançável por esse entry point. A metadata de
[AMDGPU code objects](https://llvm.org/docs/AMDGPUUsage.html) confirma que o
artifact precisa registrar facts específicos do target.

Essas fontes sustentam a separação entre dependência lógica e schedule físico.
Elas não autorizam W a copiar a API ou a promessa de liveness de um provider.
KM0 usa esses fatos para famílias estáticas, especializações finitas e artifacts
target-specific. DEV0 mede a projeção runtime comum: owner, loan, queue,
receipt, completion, cleanup e outcome. Nenhuma fonte autoriza registry runtime,
JIT implícito ou transfer escondida.

Alternativas rejeitadas:

- migrar W code arbitrário para device por um `spawn` comum;
- inserir host/device transfer ou CPU fallback sem prova;
- expor stream integer, raw event ou pointer como safe authority;
- fire-and-forget, drop como async cleanup ou completion sem receipt;
- impor um scheduler físico universal a CPU, GPU, DSP e ASIC.

### 1.19 Evidência de recovery de services SR0

SR0 fecha a lacuna entre o lifecycle abstrato de B0 e a recuperação de uma
instance depois de falha de processo ou rede. O modelo não muda a superfície de
service do W.

As fontes primárias separam responsabilidades que W também mantém separadas:

- os
  [input e output gates de Durable Objects](https://blog.cloudflare.com/durable-objects-easy-fast-correct-choose-three/)
  bloqueiam delivery durante storage pendente e retêm outputs até o commit;
- [Cap'n Proto RPC](https://capnproto.org/rpc.html) usa capabilities, torna a
  falha de conexão observável e reduz round trips com promise pipelining sem
  fingir que uma call remota é local;
- [Cap'n Web](https://blog.cloudflare.com/capnweb-javascript-rpc-library/)
  preserva dependent calls e object capabilities sobre transportes Web;
- o [atomic commit do SQLite](https://www.sqlite.org/atomiccommit.html) define um
  ponto de decisão recuperável e destaca a necessidade de crash tests;
- o [WAL do SQLite](https://www.sqlite.org/wal.html) permite readers concorrentes
  com um writer, mas exige shared memory no mesmo host e uma policy de
  checkpoint;
- [supervisors de Erlang/OTP](https://www.erlang.org/doc/system/sup_princ.html)
  limitam burst e taxa sustentada de restarts;
- [alarms de Durable Objects](https://developers.cloudflare.com/durable-objects/api/alarms/)
  mostram delivery at-least-once e retry bounded.

Esses sistemas não definem a semântica de W. Eles sustentam a separação entre
turn serial, commit authority, transporte, deduplication, supervision e timer.
Em especial, promise pipelining reduz latency, mas não amplia uma transação.
WAL melhora concurrency local, mas não prova filesystem remoto nem durability
fora do profile selecionado.

SR0 usa um journal lógico, não o layout do SQLite. A machine host deriva
mailbox, quotas, input commit, effect policy, output frontier, generation,
recovery action, dedup record, tombstone, compaction com receipt, disconnect e
shutdown. O
corpus injeta faults em cada fronteira e liga os casos a symbols do Última Luz.
Ele não executa W, wWire, database, filesystem, network ou provider.

Alternativas rejeitadas:

- exactly-once universal para efeitos remotos;
- retry mutante com novo `effectId` ou retry at-most-once depois de dúvida;
- persistir stack, task frame, borrow, pointer ou capability de conexão;
- mailbox, journal, outbox, retry ou deduplication sem budget;
- FIFO global entre senders ou reentrância default;
- transaction implícita entre dois commit providers;
- usar connection ID como effect identity;
- aceitar suffix incompleto, checksum inválido ou schema divergente;
- aceitar compaction ou completion física sem receipt registrado;
- deixar completion de geração antiga publicar no state novo;
- restart ilimitado ou alarm tratado como exactly-once.

### 1.20 Escapes de sistema e fronteiras de target

A seção 19 fecha as superfícies necessárias para firmware e runtime sem tornar
detalhes de backend parte da linguagem comum. Estas fontes primárias sustentam
as separações adotadas:

- o
  [LLVM Language Reference — volatile memory accesses](https://www.llvm.org/docs/LangRef.html#volatile-memory-accesses)
  preserva acessos volatile, mas não lhes dá synchronization entre threads;
- o
  [CMSIS-SVD register schema](https://arm-software.github.io/CMSIS_5/SVD/html/elem_registers.html)
  separa width, access, read side effects, modified writes e constraints;
- o
  [Swift Task Local Values proposal](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0311-task-locals.md)
  liga binding imutável, inheritance e lifetime à árvore estruturada;
- o
  [JEP 506](https://openjdk.org/jeps/506) separa scoped values imutáveis de
  thread-local mutável e limita inheritance à concorrência estruturada;
- o
  [LLVM Language Reference — thread local storage models](https://www.llvm.org/docs/LangRef.html#thread-local-storage-models)
  mostra que TLS depende do target e pode usar modelos físicos distintos;
- a
  [documentação de `LocalKey` do Rust](https://doc.rust-lang.org/std/thread/struct.LocalKey.html)
  mantém referências TLS dentro de closure e registra limites reais de
  destructors por platform;
- o
  [LLVM Language Reference — explicit sections](https://www.llvm.org/docs/LangRef.html#global-variables)
  trata section como assertion dependente do target;
- o
  [atributo `used` do Rust](https://doc.rust-lang.org/reference/abi.html#the-used-attribute)
  preserva um item no object, mas não impede o linker final de removê-lo;
- o
  [LLVM inline assembler contract](https://www.llvm.org/docs/LangRef.html#inline-assembler-expressions)
  torna constraints e flags, não o texto opaco, a base para correctness;
- a
  [Rust Reference de inline assembly](https://doc.rust-lang.org/reference/inline-assembly.html)
  explicita operands, ABI clobbers e memory options por architecture.

Essas fontes não definem W. Elas mostram por que `volatile`, TLS destructor,
object-file retention e assembly text não bastam como promessa segura. W move
esses facts para target manifest, host slot, product recipe ou adapter
hermético.

CTX0 aplica essa separação ao Restaurante Última Luz. O identificador do pedido
acompanha somente callees e children estruturados. O contador nativo pertence
à thread física e não acompanha uma task depois de uma suspensão. Isso evita
um mapa ambiental, uma cópia por child e uma falsa equivalência entre task,
domain e thread.

O parser canônico preserva o body externo byte a byte e usa o scanner do
adapter. A projeção Tree-sitter materializa um leaf opaco com external scanner;
somente o adapter fixado no lock fornece prova de build. A
[documentação de external scanners do Tree-sitter](https://tree-sitter.github.io/tree-sitter/creating-parsers/4-external-scanners.html)
explica state serializável, `mark_end` e precedência durante recovery. A
[documentação de language injection](https://tree-sitter.github.io/tree-sitter/3-syntax-highlighting.html#language-injection)
separa o range da árvore W da árvore usada apenas para highlight. Essas APIs
suportam a projeção; elas não definem a semântica de W nem substituem o scanner
hermético do adapter.

Alternativas rejeitadas:

- `var volatile`, integer-to-register safe e read-modify-write genérico de MMIO;
- annotation de interrupt no body sem slot, budget, acknowledgement ou policy;
- task-local mutável, authority escondida ou inheritance por service/wire;
- TLS de resource com cleanup best-effort na safe std;
- source annotation de linker e `used` tratado como retention do payload final;
- naked function, clobber implícito, call ou unwind escondidos em `fn<Asm>`;
- syntax distinta de `fn<Language>` para cada linguagem ou unit externa;
- compilation unit nomeada no source W em paralelo ao package/build graph.

### 1.21 Filesystem capability e I/O posicional

As fontes primárias abaixo sustentam uma semântica W única, sem copiar flags ou
handles de um host:

- o
  [POSIX `openat`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/open.html)
  mostra resolução relativa a um descriptor e separa acesso, criação e falha;
- o
  [POSIX `pread`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/read.html)
  lê em offset explícito sem alterar cursor compartilhado;
- o
  [POSIX `renameat`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/rename.html)
  ancora source e destination em diretórios e distingue atomicidade de
  namespace;
- o
  [WASI filesystem](https://wa.dev/wasi%3Afilesystem)
  rejeita paths absolutos e resolução que escapa da base capability;
- o
  [Windows `CreateFileW`](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)
  combina desired access, creation disposition e handle nativo;
- o
  [Windows `ReplaceFileW`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)
  confirma que replace é uma operação própria e não uma sequência portátil de
  delete + rename.

W usa essas evidências para separar `FileSystem` root-scoped, rights estáticos,
creation, I/O posicional, cursor owned, mutation de namespace e durability.
Nenhum contrato W promete que nome textual é identidade, que rename persiste
após crash ou que cancellation libera buffers antes do provider drain. O root
profile também limita path units, componentes e travessias de symlink; containment
não autoriza busca ilimitada.

Alternativas rejeitadas:

- cwd, filesystem ou path lookup ambiental;
- `OpenOptions` como builder de flags Boolean runtime;
- `Array<String>` para nomes do host;
- cursor compartilhado em todo `File`;
- `metadata.size` seguido de write como append;
- listagem recursiva, sorting ou symlink traversal implícitos;
- rename como durability ou delete + rename como replace atômico;
- async destructor ou sync escondido no drop.

### 1.22 Error portátil de I/O

A taxonomia usa quatro evidências primárias:

- o
  [Rust `std::io::ErrorKind`](https://doc.rust-lang.org/std/io/enum.ErrorKind.html)
  mostra o custo de uma lista ampla e non-exhaustive para application code;
- o
  [WASI filesystem](https://wa.dev/wasi%3Afilesystem)
  separa error codes de filesystem do resource de error de stream;
- o
  [WASI I/O](https://wa.dev/wasi%3Aio)
  mantém o error físico opaco e permite extração por adapter específico;
- o
  [POSIX.1-2024](https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html)
  permite mais de um error aplicável e não fixa a ordem de detecção.

W mantém um enum menor e edition-frozen. `.other` absorve condições futuras;
`IoOperation` preserva a call lógica W e `IoCause` mantém evidence do target sem
expor authority ou tornar o código nativo parte do resultado de domínio.

Alternativas rejeitadas:

- copiar `errno`, WASI ou todos os cases atuais do Rust;
- enum non-exhaustive que força wildcard para todo switch;
- usar syscall interna como operação pública;
- serializar código ou texto nativo no resultado de domínio;
- `wouldBlock`, EOF, interrupção ou cancellation como `IoErrorKind`;
- `retryable: Bool` sem operation, progress, idempotência e deadline.

### 1.23 Relógio operacional e deadlines

As APIs maduras convergem em separar medição monotônica de calendário. O
[WASI clocks](https://wa.dev/wasi%3Aclocks) define uma origem monotônica opaca,
resolução explícita e subscription. O
[Rust `Instant`](https://doc.rust-lang.org/stable/std/time/struct.Instant.html)
é opaco e nondecreasing, mas documenta que steadiness e o tratamento de system
suspend variam por plataforma. A
[documentação `time` de Go](https://pkg.go.dev/time) separa leitura monotônica
de wall time e remove a parte monotônica ao serializar. A
[Swift SE-0329](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0329-clock-instant-duration.md)
separa `Clock`, `Instant` e `Duration`; a
[proposta de deadlines de Swift](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0526-deadline.md)
reforça deadlines absolutos como forma composável.

W escolhe um único clock operacional por entry/fault root. Essa regra evita
generic identity pública e impede comparar acidentalmente origens diferentes.
`Duration` é data signed exata; `Instant` e `Deadline` são copies dependentes do
root, sem raw timestamp. O provider declara resolução e se system suspend entra
na conta. `.unspecified` mantém honestidade quando o target não oferece uma
garantia estável.

**Pesquisa: sensor físico.** Um restaurante pode medir o trânsito de um fóton
entre dois sensores FPGA com um período em picoseconds. O exemplo usa a
dimensão SI, um carrier racional exato e um contador físico explícito. O
contador pertence ao device adapter; o provider continua missing.
O snippet pressupõe `si` do módulo `std` e imports seletivos de `std.math` e
`std.time`.

```w
import si from std
import { BigInt, Rational } from std.math
import { Duration } from std.time

unit detectorTick = 4<si.ps>
alias SensorDuration = Quantity<si.Duration, Rational<BigInt>>
let tickPeriod: SensorDuration = 1<detectorTick>
let tickCount: u64 = detector.readTicks()
let transit: SensorDuration = tickPeriod * tickCount
let elapsed: Duration = try Duration.exactly(transit)
```

`si.ps` é a unit SI de picoseconds, não uma dimensão. `detectorTick` é uma unit
custom derivada dessa unit. Esse caso não transforma picoseconds em resolução de
`Duration` e não define uma API de device. A conversão é explícita e checked.
Se a escala não cabe em signed i128 nanoseconds, o adapter mantém a quantity
física ou devolve um erro de range.

**Pesquisa: tempo civil.** Candidatos como `WallClock` e `Timestamp` poderiam
separar data UTC, timezone e calendário de `Clock`. Outra opção seria um
`CivilTime` com `Calendar` e `TimeZone` nominais. Estes exemplos são apenas
direções de pesquisa. Nenhuma syntax ou API pública foi decidida.

Tempo civil fica separado porque UTC, timezone, calendars, leap seconds e
serialization têm contratos diferentes. Misturar os dois faria uma correção de
data alterar timeout ou elapsed time. TIME0 testa as regras de design, mas não
mede um clock nem implementa scheduler ou timer.

### 1.24 Evidência SDM0: matriz de diagnostics e resultado semântico

SDM0 avança a prova de design pedida em DESIGN §24.4. A matriz host em
`tooling/semantic-diagnostic-matrix-cases.json` deriva records de eventos
estruturados. Ela não repete expectativas como resultado de uma máquina W.

O corpus source S0 mantém 132 casos, com 66 positivos e 66 negativos. Cada
regra source usada no corpus exige um positivo, uma inversão negativa única,
`rule` igual, `failureField` exato e um diagnostic catalogado. W-785 e W-788
possuem pares source reais. W-791 e os contratos D0/S0 meta usam o corpus host.

A máquina SDM0 comprova estas dimensões:

- sete campos de `SemanticResult` e sete campos de `CheckerContext` derivados;
- entry, continue, back-edge e break em fixed point, com widening monotônico;
- interface AST→HIR única, schema `w-ast-hir-s0` versão 1 e domains que só
  acrescentam facts;
- records D0 com spans UTF-8 half-open, fases e catálogo fechados, facts sem
  segredos, fixes com digest e prova, causalidade, poison, ordenação e limite;
- política sem demotion de errors e sem supressão source, boundary lex/parse,
  namespace, profiles, facts de parse e cobertura por decisão.

Os testes host alteram graph edge, contexto implícito, back-edge inseguro,
schema de backend, fact secreto, fix stale ou sobreposto, poison cascade,
ordenação, truncation, demotion, namespace, profile e boundary UTF-8. Os
O source Last Light é um assay de expected-use, owner/effect/control e loop;
CheckerContext completo continua uma interface interna coberta somente pela
matriz host. Os checks cobrem o corpus SDM0 regenerado. As contagens de casos, outcomes,
decisões e mutações ficam no índice gerado. O status continua
`design-oracle-input`. Nenhuma execução de
checker, compiler, formatter, runtime ou provider é alegada.

No limite D0, `limit` conta apenas roots normais preservados; o sentinel
`W-DIAGNOSTIC-0001` é sempre o último record e `facts.emitted` conta somente
esses roots preservados.

### 1.25 Evidência FZ0 de frontend

FZ0 fecha o primeiro ciclo de ratificação uniforme para source, CST/formatter e
diagnostics. O corpus único em `tooling/frontend-freeze-cases.json` tem seis
famílias normalizadas, G0–G5, e o snapshot é derivado pelo checker, não escrito
por uma segunda tabela. Cada família aponta para um arquivo real de
`reference/last-light/`, com digest SHA-256 e símbolo que ocorre uma vez. O
checker faz parse do arquivo e rejeita recovery; para cada família ele também
faz parse independente dos F0 input/output, rejeita recovery, compara a CST
nomeada e verifica a forma canônica de bytes (LF, indentação, limite de linha e
newline final). Esses F0 pares são oracles byte/CST. O checker não chama nem
simula um formatter.

As evidências adversariais têm três formas separadas. A mutação syntax-invalid
remove um token de um F0 e exige recovery mais um D0 `source.parse` com facts
exatos. A inversão syntax-valid usa um par S0 positivo/negativo com a mesma
regra, baseline, `failureField`, digest do valor de baseline e um único D0
catalogado com os `requiredFacts` do catálogo; ela cobre parâmetros de valor
genéricos, captures `<[...]>`, quatro formas de execução e o slot contextual de
allocator. O waiver de source só aparece para a seleção de entry de módulo:
RU0 fornece a rejeição de descriptor ausente, com motivo registrado porque não
há uma inversão S0 genuína para esse contrato de workflow. Labels opcionais e
as formas named/anonymous do allocator ficam nos pares F0, e os markers exigem
que a mesma construção apareça na fonte Última Luz e na evidência positiva.
Esse D0 é um registro de waiver: sua phase, `failureField`, `waiver` flag e
lista de outcomes ligam cada rejeição RU0 ao código e à razão correspondentes;
ele não é output de um compiler nem substitui um par S0.

O Restaurante no Fim do Universo fornece os seis witnesses reais (`semantic_matrix.w`,
`horizon_tool.w`, `generics.w`, `command.w`, `execution.w` e `allocation.w`).
O checker rejeita refs missing, stale ou duplicadas, pares expected-echo e
decisões que não sejam exercitadas pelo F0/S0 ligado; o snapshot atual registra
19 decisões, uma mutação syntax-invalid, quatro inversões S0 e um waiver. A
separação entre recovery estrutural e diagnóstico exato segue a descrição do
Recovery AST e da verificação `-verify` no
[Clang Internals Manual](https://clang.llvm.org/docs/InternalsManual.html#recovery-ast),
que preserva estrutura e localizações sem prometer semântica. A exigência de
snapshot sem diagnostics extras também é compatível com o fluxo de UI tests do
[rustc Dev Guide](https://rustc-dev-guide.rust-lang.org/tests/ui.html); nenhuma
dessas fontes é surface copiada para W.

### 1.26 Evidência BRX0 de expressividade de borrow de ordem superior

BRX0 audita o limite de provenance para resultados borrowed sem introduzir
nomes de lifetime no source. O source fixture parseável
[`reference/last-light/borrow_expressivity.w`](reference/last-light/borrow_expressivity.w)
usa Last Light como produto real. A máquina host
[`tooling/borrow-expressivity-machine.mjs`](tooling/borrow-expressivity-machine.mjs)
deriva mappings e edges de `inputs`, `results`, `bodyTrace`, `problemTrace` e
pares estruturados; ela não lê um mapping esperado para decidir o resultado.

O corpus
[`tooling/borrow-expressivity-cases.json`](tooling/borrow-expressivity-cases.json)
tem 24 casos: 17 mappings aceitos, cinco rotas Research e quatro negativos
de invocation. O snapshot
[`tooling/borrow-expressivity-results.snapshot.jsonl`](tooling/borrow-expressivity-results.snapshot.jsonl)
é escrito pelo checker
[`tooling/check-borrow-expressivity-cases.mjs`](tooling/check-borrow-expressivity-cases.mjs)
e registra mappings, OriginSets deduplicados, edges individuais, diagnostics,
artefatos e digest do componente de mapping. Os testes host independentes em
[`tooling/borrow-expressivity-reference.test.mjs`](tooling/borrow-expressivity-reference.test.mjs)
passam 12 grupos adversariais.

O resultado é uma decisão B restrita, não uma mudança de grammar. A1 fecha
member requirement quando o receiver é a única origem compatível; body-derived
free mapping fecha quando o body fornece a origem exata; callable cria loan
fresh por invocation e liga o resultado ao último uso, sem deixar edge
persistent entre calls; `any fn` conserva o mapping e rejeita somente escape
dinâmico. Stream `next` bloqueia enquanto uma view live conflita com storage
reutilizado e permite o próximo item depois do fim da view. Factory de
`map`/`filter` move source para um adapter owner; o `next` do adapter é a
operação receiver-shaped, e o trace host deriva união/transitividade de
OriginSet para o item.

A2 fecha sem nova syntax quando a origem é única: receiver compatível para
instance/member, exatamente uma entrada compatível para free/static/protocol
bodyless, ou uma origem exata fornecida pelo body. Duas ou mais entradas
independentes sem receiver ou corpo autoritativo são rejeitadas com
`W-BORROW-0011`; o diagnóstico informa a declaração, o resultado e o conjunto
de origens compatíveis, sem manter um fallback morto por default all-inputs. B1
(pares relacionais no schema) continua candidato BRX2 Research para contratos
owned por requirement/interface; B2 (sum/aggregate nominal) é a alternativa
explícita de API e não preserva o resultado borrowed direto. O corpus agora
mostra um positivo bodyless de origem única, um negativo ambíguo e a alternativa
nominal owned, além de injetar mapping missing, stale, duplicate (result e
source) e forged, witness, implementation, `interface.lock` e mapping-component
digest divergentes.

Os precedentes externos servem somente como limite comparativo. O
[Rust Reference, associated items](https://doc.rust-lang.org/stable/reference/items/associated-items.html)
usa GAT para o padrão `LendingIterator`, porque o tipo do item varia com o
borrow do receiver; BRX0 não copia essa syntax. O
[Swift SE-0456](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0456-stdlib-span-properties.md)
fecha o caso estreito de property/member `Span` por dependency inferida do
callee, mas mantém relações gerais dependentes de anotações explícitas. Isso
apoia A1 sem provar uma solução para A2; a comparação não é uma decisão de
compatibilidade ou de implementação de W.

O estudo R1 em
[`tooling/studies/r1-borrow-expressivity`](tooling/studies/r1-borrow-expressivity)
separa baseline, aggregate e witness de relation schema. A evidência atual é
parse Tree-sitter e oracle host; compile, run, estudo humano e estudo de modelo
continuam missing. BRX0 não implementa compiler, runtime ou provider e não
publica lifetime metadata em runtime. O blocker W-1351 fecha no baseline por
origem única e rejeição ambígua; somente a relação owned BRX2 permanece Research,
sem promover syntax normativa.

### 1.27 BRX2 — relações de borrow por contrato

BRX2 informa a extensão Research pós-baseline sem reabrir BRX0. A máquina e o corpus em
[`tooling/brx2-borrow-relations-machine.mjs`](tooling/brx2-borrow-relations-machine.mjs)
e [`tooling/brx2-borrow-relations-cases.json`](tooling/brx2-borrow-relations-cases.json)
derivam status, route, relation, edges, `OriginSet`, `SemanticInterfaceKey` e
digests a partir de inputs estruturados. O fixture Last Light continua sendo
[`reference/last-light/borrow_expressivity.w`](reference/last-light/borrow_expressivity.w),
com `selectPrimary` e symbols reais; o estudo em
[`tooling/studies/brx2-borrow-relations`](tooling/studies/brx2-borrow-relations)
mantém baseline, aggregate nominal e witness reservado separados.

A é a composição atual: receiver/member, body-derived exact e bodyless com uma
única entrada compatível fecham a origem; bodyless com múltiplas entradas
independentes rejeita `W-BORROW-0011`, e `init` com resultado borrowed/view
continua rejeitado. B é um candidato data-only de schema HIR/WInterface, sem nova
syntax de lifetime e sem metadata runtime. A relação é owned pelo requirement
ou pela interface; provider, implementation e cada witness devem prová-la e
usar slots/modes canônicos, não o caller. Witness específico divergente fica
rejeitado para generic/open dispatch. O digest relacional participa da
`SemanticInterfaceKey`, do interface lock e da expectativa provider/consumer;
substitution exige igualdade exata/invariance, e o ABI/WAbi não ganha carrier.
Uma definição sealed pode congelar essa relação, mas isso continua candidato
Research até haver verifier HIR, separate compilation e evidência de provider.
Conclusão do estudo: existe um fechamento relacional data-only no oracle para
inputs estruturados, mas ainda não existe um mecanismo fechado comprovado para
W que preserve simultaneamente W-914, OriginSet, borrow edges,
`SemanticInterfaceKey`, substitution/variance, separate compilation,
diagnostics e ABI. A recomendação é manter a relação como Research e não
promover sua spelling.

C é o aggregate nominal/owned sum: é uma API segura alternativa e muda a forma
do resultado, portanto não fecha automaticamente o borrowed result direto. D
rejeita caller/call-site claims, witness-only mapping, Rust-like lifetime/GAT
spelling, runtime lifetime table, hidden conservative escape, ambient
inference, macros/annotations e universal conservative escape. O oracle cobre
56 casos, inclusive múltiplos resultados, branch union, transitive mapping,
callable fresh loans, any-fn erasure, Stream live view, await cleanup/cancel,
boundaries, source-overlap edge occurrences, static/immortal non-dynamic edges,
separate-compilation receipts e stale/missing/duplicate/forged
relation/interface/provider refs. Invocation status is recorded separately
from declaration decision; a rejected invocation does not silently count as a
fully accepted declaration.
Independent results now derive from non-dependent result slots, while static
and immortal behavior derives from input slot facts and edges. Legacy result
flags and `artifacts.verified` are rejected; verification is receipt-based and
requires scope plus stage.
The snapshot separates 16 current, 14 Research, and 26 rejected declaration
routes; 13 exact Research candidates are distinct from 10 exact rejected
relations, and eight invocation negatives are tracked independently. Forty-eight
invocations are accepted; eight are rejected, including six cases whose
declaration decision remains accepted.

As fontes comparativas são evidência, não surface de W: [C23 N3096](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf)
mostra ponteiros e contratos de validade manuais, sem prova estática da relação;
[Rust Reference associated items](https://doc.rust-lang.org/stable/reference/items/associated-items.html)
usa GAT/LendingIterator para amarrar o item ao borrow do receiver; e [Swift
SE-0456](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0456-stdlib-span-properties.md)
fecha o caso estreito de borrowing de `Span`. A evidência atual é source-ref,
parse/oracle host e refs oficiais; case-level `assay.kind: independent-assay` is
host assay ground truth, not compiler evidence, and is excluded from
relation/interface keys. A trace is not compiler evidence for a bodyless
current interface. Compile, run, HIR verifier, provider/linker, foreign execution,
separate compilation e estudos humano/modelo continuam missing. O gate de
promoção exige duas derivações independentes, witnesses exatos, lock/provider
digest agreement, invariance/substitution, ABI sem metadata runtime e
diagnósticos adversariais. Pare e mantenha Research se qualquer passo exigir
caller claim, runtime lifetime state, divergência witness-specific ou mudança
de WAbi/grammar.

### 1.27.1 BRX3 — cláusula source para relação aberta

BRX3 materializa a decisão pré-1.0 que BRX2 deixou em Research. O problema
continua sendo `selectPrimary(primary: ref String, fallback: ref String): view
String` em requirement/interface bodyless. A forma vigente é a cláusula
contextual `borrows(...)`, após `return` e `throws`, em requirement/interface e
function type:

```w
static fn select(primary: ref String, fallback: ref String): view String
  borrows(0: [primary, fallback])
type Selector = fn(ref String, ref String): view String borrows(0: [0])
```

Identifiers source resolvem para parameter slots. Function types usam ordinals
porque não possuem names. O CST preserva pair/source order e comments. A HIR
resolve names, valida índices não negativos e modes, e ordena somente o payload
canonical `BorrowRelation/1`. Cada dependent result slot aparece uma vez.

Requirement/interface possui a autoridade. Body/default, implementation,
witness e provider verificam igualdade exata. Caller/call-site não publica
relation. Existential, generic/open conformance e substitution usam a mesma
relação invariável. O payload entra em `WInterface`, `SemanticInterfaceKey` e
`interface.lock`. WAbi, ABI calling convention e runtime não ganham relation
field ou lifetime table. FFI ainda exige owner/pin e drain conforme os contratos
existentes.

O corpus [`tooling/brx3-borrow-relations-cases.json`](tooling/brx3-borrow-relations-cases.json)
possui 27 casos. A máquina [`tooling/brx3-borrow-relations-machine.mjs`](tooling/brx3-borrow-relations-machine.mjs)
reutiliza o oracle de provenance BRX2 e acrescenta resolução source,
canonicalização ordinal, diagnostics de clause, body conflict, witness/provider,
generic variance, existential, await/stream/FFI e rejeição WAbi/runtime. O
snapshot host tem 11 casos aceitos e 16 rejeitados. Isso prova o contrato de
design, não compiler, HIR, separate compilation, provider, linker, FFI
execution, runtime, stress ou estudo humano/modelo. Esses itens permanecem
implementation-evidence-gap.

W-1381–W-1384 e W-1436 registram a transição de BRX2 Research para BRX3 source
vigente. W-BORROW-0011 permanece para bodyless sem autoridade. W-BORROW-0012 é
o perfil bounded para slot, mode ou prova divergente. Antes de W 1.0 não há
compatibilidade implícita para declarations sem a nova cláusula.

### 1.28 CAP0 — matriz de capacidades por problema

CAP0 é uma fonte editorial de staging para guias futuros. Ele não é um
contrato adicional em `DESIGN.md`. A matriz estruturada está em
[`tooling/capability-matrix-cases.json`](tooling/capability-matrix-cases.json).
O pacote de estudo está em
[`tooling/studies/cap0-capability-matrix`](tooling/studies/cap0-capability-matrix).
O estudo usa os oito eixos pedidos: BRX0, ATOM0, GEN0, SYN0, CYC0, IPC0,
SRV0 e DYN0.

O método começa pelo mesmo problema operacional. A comparação registra um
mecanismo estrangeiro somente depois de descrever esse problema. C, Rust e
Python entram por fontes primárias oficiais. A matriz não registra maturidade,
popularidade, comunidade, downloads ou cópia de feature.

As fontes primárias centrais são o
[draft C23 N3096](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf),
o [Rust Reference](https://doc.rust-lang.org/stable/reference/items/associated-items.html),
o [Python Language Reference](https://docs.python.org/3/reference/expressions.html#yield-expressions),
e as APIs oficiais de [Python eval/exec](https://docs.python.org/3/library/functions.html#eval),
[POSIX shm_open](https://pubs.opengroup.org/onlinepubs/9699919799/functions/shm_open.html)
e [POSIX mmap](https://pubs.opengroup.org/onlinepubs/9699919799/functions/mmap.html).
Os refs específicos incluem [Rust atomics](https://doc.rust-lang.org/std/sync/atomic/),
[await](https://doc.rust-lang.org/reference/expressions/await-expr.html),
[macros](https://doc.rust-lang.org/reference/macros.html),
[Python class creation](https://docs.python.org/3/reference/datamodel.html#customizing-class-creation),
[Python shared memory](https://docs.python.org/3/library/multiprocessing.shared_memory.html),
[POSIX waitpid](https://pubs.opengroup.org/onlinepubs/9699919799/functions/waitpid.html)
e [sigaction](https://pubs.opengroup.org/onlinepubs/9699919799/functions/sigaction.html).

Cada eixo tenta primeiro a composição W vigente. A tentativa usa Last Light
como cenário concreto e aponta para símbolos reais. A matriz separa três
dimensões de design:

- `languageDesign` registra o que a forma da linguagem consegue expressar para
  o mesmo problema;
- `stdDesign` registra o que a superfície projetada da std cobre para esse
  problema;
- `userDefinableInDesign` registra o que uma abstração segura de usuário pode
  definir para esse problema no design atual.

As justificativas ficam dentro de cada nível. Elas não rebaixam o problema por
causa de uma extensão Research ou de um mecanismo estrangeiro rejeitado. Os
componentes da tentativa W têm IDs estáveis; subcapacidades componíveis apontam
para esses IDs, e subcapacidades Research apontam para o gate e para a evidência
Last Light que bloqueiam o problema.

Esses campos não alegam compiler, runtime, provider ou standard-library
implementation. O campo `evidence` mantém essa fronteira explícita.

A `rota` é derivada por
[`tooling/capability-matrix-machine.mjs`](tooling/capability-matrix-machine.mjs)
a partir das subcapacidades com `scope`/`role` `problem`, `extension` ou
`foreign-mechanism`. Cada subcapability Research aponta para um
`nextStudyGate.kind: design`; gates `kind: evidence` guardam somente prova de
provider ou execução. A classificação da rota usa somente as subcapacidades do
problema; uma extensão Research ou um mecanismo estrangeiro rejeitado não
rebaixa uma rota componível. `exactGap` descreve apenas o residual do problema
e se alinha à rota. O checker rejeita rota forjada; não aceita um campo de fatos
que apenas repita a classificação. A matriz escalar não colapsa
subcapacidades mistas: um problema pode ser componível enquanto uma primitive
estreita permanece Research ou um mecanismo estrangeiro permanece rejeitado.
O estudo também rejeita refs locais ausentes ou stale, refs duplicadas na mesma
lista, campos de maturidade, feature-copying e claims de implementação.

Os resultados atuais são estes:

| Eixo | Rota do problema | Limite deliberado |
|---|---|---|
| BRX0 | Componível | Origem única em receiver/body/entrada bodyless fecha o baseline; ambiguidade rejeita `W-BORROW-0011`; relação owned BRX2 permanece Research. |
| ATOM0 | Componível | Wrapper sobre atomics existentes e records value-only fechados compõem. Pointer, tagged pointer e RCU universal permanecem rejeitados. Adapter de reclamation especializado exige `unsafe` e evidência de implementação. |
| GEN0 | Componível | Stream e tasks cobrem produção; a expressão estreita GEN2 reduz a cerimônia com captures explícitas e `yield take`/`yield copy` (copy exige `Duplicable`). Diálogo usa Channel bounded; frame/send/throw/resume público é rejeitado e compile/runtime/provider permanecem gates de evidência. |
| SYN0 | Componível | Synthesis compiler-owned, transform hermético e o module set `.w` separado de SYN2 cobrem artefatos. C2 recipe/IR, phase in-process e injection no módulo corrente permanecem rejeitados; compiler/provider/run são gaps de implementação. |
| CYC0 | Componível | Weak edge, owner e drain fecham o grafo. Collector transparente não entra no core. |
| IPC0 | Vigente | Snapshot e IPC tipado continuam fallback. ASIC0 fecha A/B mapped como contratos condicionais de adapter/provider com layout, atomics, crash, capability e receipts; C universal é rejeitado e W-1448 mantém a evidência de implementação. |
| SRV0 | Vigente | Services, faults, generations e recovery actions formam o design. Journal e crash provider são gates de evidência. |
| DYN0 | Componível | Dados, plugins tipados, generations, REPL e transforms atendem hot change. Eval e active-frame patching são rejeitados como mecanismos. |

ATOM0-G1 agora possui o estudo durável
[`ATOM2`](tooling/studies/atom2-atomic-contract/README.md). O corpus e o
snapshot registram quatro fronteiras e promovem somente o carrier canônico
value-only fechado: A entra em `Atomic<T>`/`var atomic`, B usa handle e owner
table com exhaustion checked, C mantém `SnapshotCell` e adapter `unsafe`
especializado, e D rejeita pointer/tagged pointer/RCU universal. A rota do
problema permanece Componível. Target, provider, stress, debug, FFI drain e
compile/run são gaps de evidência, não um gate Research de design.

GEN0-R1 agora possui o bundle durável [`GEN2`](tooling/studies/gen2-stream-yield)
com path, digest e claim em `nextStudyGate.studyRefs` do CAP0. O GEN1 anterior
permanece evidência histórica: compara Stream/adapters/tasks, máquina nominal,
canais bounded e witnesses reservados, mas não reabre um gate Research. GEN2
fecha a decisão de design para a expressão `stream <[capture_item, ...]>` com
`yield take`/`yield copy`, cursor exclusivo e capacity zero; frame/send/throw/resume público
continua rejeitado e diálogo permanece Channel bounded. Bundle, oracle e parser
integram o estudo ao aggregate; compile, run, provider, stress e estudos
humano/modelo ainda são gaps de implementação/evidência.

SYN0-R1 possui a proveniência SYN1 e o fechamento de design
[`SYN2/DYN2`](tooling/studies/syn2-dyn2-closure). O estudo SYN1 e o oracle
[`syn1-typed-generation-machine.mjs`](tooling/syn1-typed-generation-machine.mjs)
continuam como evidência de base e não como implementação.
O estudo começa pelo Restaurante: A mantém `Hashable`/`Reflectable`,
`data.Row`, kernel synthesis finita e declarations manuais; B mantém o
transform `final.menu` como artifact de dados typed, sem declarations; C
estuda um W0 build transform que publica um module set content-addressed com
um ou mais files `.w`, provenance e source map. O Tree-sitter atual valida o
parse sem recovery. Um `frontendReceipt` Research fornece os facts de type,
ownership, effect e ConstIR que ainda não possuem evidência de compiler. O
frontend proposto reabre cada file como source unit nova antes de freeze;
AST/HIR não é injetada na unidade em execução.

A matriz SYN1 cobre 65 casos host independentes, incluindo 22 outcomes
candidatos aceitos. Eles não são módulos implementados. Uma mudança de field/enum
altera content e `SemanticInterfaceKey`; mudança somente de docs/source map ou
private body não altera a key. Um artifact target-neutral compartilha a mesma
identity lógica em duas projeções e pode alterar somente o artifact físico;
facts de product target declarados podem especializar a recipe/interface.
Host target, mtime, random, time, environment, network e filesystem não
declarados são negativos. Paths físicos validam authority/provenance, mas não
entram nas identities. Action events terminam em `tool-finish`; o oracle observa
somente staged output e parse/source-shape. Um `requiredPhaseTrace` registra o
contrato candidato parse → name → type → ownership → effect → ConstIR →
interface diff → freeze → publicação de interface → consumer. Ele não prova
execução semântica. Ciclos e execução pós-freeze são rejeitados.

A action recipe key deriva tool artifact, execution platform, inputs typed,
schemas, dependency receipts, output descriptor/source profile, graph receipt,
product-target/ABI receipts declarados, capabilities, quotas e version; ela
não contém output digest. O result/module identity deriva do conteúdo de saída.
Paths de handles read-only ficam fora da key. Tool success pode publicar o
action result/CAS após validar container/binding/schema/bounds/digests; falha
posterior de parse/receipt/map preserva esse result, mas não publica interface
ou compiler cache. Failure, cancellation, quota e panic antes do result
descartam staging; action events provam cleanup/drain exatamente uma vez.
Success não inventa cleanup. Source maps só dão fix quando uma mapping cobre o
span gerado e aponta ao byte span editável com digest atual; source spans podem
se sobrepor para many-generated-to-one-source, mas generated spans não podem
ser ambíguos e endpoints respeitam boundaries UTF-8. Diagnóstico gerado sem
origem não inventa fix. Duplicata, map stale, UTF-8/syntax,
output/capability/effect/ownership/import/order/collision e source-ref mutation
são negativos.

C2 (typed declaration recipe/IR fechado) é rejeitado quando duplica o frontend
e não resolve nada além de C. Proc macro, annotation, decorator, metaclass,
eval/exec, textual AST mutation e current-module injection são D,
intencionalmente rejeitados. A rota do problema SYN0 continua Componível e a
subcapability de introdução de declarations agora é um contrato de design
bounded, sem claim de implementação. As fontes C23,
Rust (macros, proc macros, Cargo build scripts e `cfg`) e Python (class
creation, `eval`/`exec`) explicam tradeoffs e não são evidência de W.

O estudo usa source W real com digests e símbolos Last Light, quatro fixtures
`.w`, quatorze artifacts candidatos `.w` e witnesses `.txt` reservados para as
dispositions. Ele inclui `reference/last-light/menus/final.menu`, duas
projections de target, target registry host, mutations e snapshot determinístico.
Parse Tree-sitter/source-shape, oracle host e refs primárias são evidence current.
Compiler/name/type/ownership/effect, ConstIR, run, target compiler/provider e
estudos humano/modelo permanecem
missing. Nenhum artefato afirma compiler, runtime, provider ou build service
implementado.

Assim, DYN0 não classifica o problema inteiro como rejeitado. O problema
compõe com gerações tipadas. Somente o mecanismo de eval arbitrário recebe
`foreignMechanismDisposition: intentionally-rejected`. O mesmo cuidado vale
para CYC0, GEN0 e ATOM0. A matriz não converte um mecanismo estrangeiro em um
gap por feature.

Cada eixo preserva move-first ownership, `ref`/`inout`/`shared` contextual,
cleanup estruturado, domains e tasks, typed errors, fault boundaries,
interface/ABI identity, artifacts reproduzíveis e ausência de authority
ambiental. O campo `globalSimplification` procura uma regra única que reduza
primitive, boundary ou lifecycle adicional. O bloco `documentation` mantém
pergunta, audiência C/Rust/Python, três snippets curtos de pseudocódigo original
ligados a fontes primárias, exemplo W como `source-ref` com digest, contraste
pedagógico, when-to-use, target futuro único, `renderHint: paired` e
`docsStatus: queued`. Os snippets explicam o mecanismo estrangeiro; não são
citações nem sintaxe W. O W source-ref aponta para Last Light e não duplica
source em snippet. O conteúdo é staging source para guias pós-freeze, não o
Book final, sem duplicar a autoridade normativa de `DESIGN.md`.

O snapshot
[`tooling/capability-matrix-results.snapshot.jsonl`](tooling/capability-matrix-results.snapshot.jsonl)
é gerado pelo checker. A versão corrente registra oito eixos, 17
subcapacidades, 149 refs e oito alvos de documentação enfileirados. Os testes host em
[`tooling/capability-matrix-reference.test.mjs`](tooling/capability-matrix-reference.test.mjs)
cobrem rota forjada, cobertura adulterada, maturidade, refs missing/stale/
duplicadas, snippets missing/long/duplicated e W snippet indevido, além de
documentação ausente. Nenhum desses artefatos afirma compiler, runtime ou
provider pronto.

### 1.29 CYC1 — ciclo explícito e liveness condicional

O estudo CYC1 informa o gate CYC0-G1 sem alterar `DESIGN.md`. Ele começa pelo
Restaurante: `MenuSection` mantém parent fraco e children shared; o hub de
observers exige callback lease e drain; services, plugins, listeners, caches,
listas duplamente ligadas, referências de actor, registrations estrangeiras e
recursos (file/socket) têm fronteiras de owner ou de shutdown diferentes. O
modelo [`cyc1-explicit-cycle-machine.mjs`](tooling/cyc1-explicit-cycle-machine.mjs)
deriva eventos de admission, edges strong/weak, close/unlink, unregister,
cancel, callback enter/exit, drain, quiesce, drop, destroy/unpin/reclaim e
census. Tarjan, reachability, SCC, ordem de drop e boundary opaca são fatos da
máquina; `expect` não escolhe o resultado.

O corpus tem 41 casos. Três SCCs strong fechados derivam
`W-OWNERSHIP-0014`; três ciclos residuais após admission close e drain derivam
`W-MEMORY-0001`; duas fronteiras estrangeiras ocultas permanecem `unknown`.
Weak parent/capture, explicit close, lifecycle drain, roots vivos,
registrations FFI, deadlines de service call, cancel/panic, resource finish,
cross-domain control blocks, lock/ABA, weak acquisition sem resurrection,
self-weak em duas fases, linked lists e drop iterativo têm cases separados.
O fixture de resource coloca `file` e seu owner em um SCC de duas edges
`lifecycleDrain`; o drain remove as edges e `finish` assíncrono é pré-requisito
para quiescence. O fixture de socket mantém panic/fault separado de reclaim.
Census é somente diagnóstico bounded depois de admission close, todos os drains
e quiescence; ele não libera nem coleta. Foreign hidden root/edge sem adapter é
`unknown`, não uma prova silenciosa de leak ou reclaim.

A rota CYC0 continua Componível: (A) weak edges; (B) owner, close, drain ou
arena explícitos; (C) census opt-in para diagnóstico; (D) collector transparente
ou finalizer oculto rejeitados. O ciclo forte criado em runtime não é uma falsa
rejeição estática: ele precisa de close/drain e pode deixar residual explícito.
Self-weak exige publicação pós-construção; constructor support que escape `self`
durante partial init é witness rejeitado. A longa cadeia de drops requer suporte
de lowering iterativo; o estudo marca isso como preocupação inconclusiva,
não comportamento implementado nem collector.

A lacuna semântica adicional isolada pelo estudo é a liveness condicional de
weak-key/ephemeron, em que o value pode manter a key viva. Ordinary weak não
resolve esse value→key edge. Visibilidade foreign, provider e liveness de
fronteiras continuam gaps de evidência ou de adapter, não uma prova de que a
linguagem deve coletar. O estudo testa três composições sem primitive nova:
generation/ID cache com key detached, owner-scoped cache lease com
invalidation/close explícito e detached
value sem back edge strong. Os dois witnesses que ainda pedem pesquisa ficam na
subcapability extension `CYC0-conditional-liveness`; eles não rebaixam a rota
do problema nem autorizam API, syntax, compiler, runtime, provider ou collector.
O fixture source-shaped usa os símbolos distintos
`generationIdCacheWithInvalidation`, `ownerScopedLeaseWithClose` e
`detachedValueWithoutBackEdge`; eles demonstram composição de biblioteca, não
uma regra ephemeron. `service.callCycle` também é metadata limitada a
`metadata` ou `external`, nunca uma escolha do caller.

As fontes primárias [C23 N3096](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf),
Rust ([`Rc`](https://doc.rust-lang.org/stable/std/rc/struct.Rc.html),
[`Arc`](https://doc.rust-lang.org/std/sync/struct.Arc.html),
[`Weak`](https://doc.rust-lang.org/std/rc/struct.Weak.html) e
[`Drop`](https://doc.rust-lang.org/std/ops/trait.Drop.html)),
Python ([`gc`](https://docs.python.org/3/library/gc.html),
[`weakref`](https://docs.python.org/3/library/weakref.html) e
[lifecycle](https://docs.python.org/3/c-api/typeobj.html)) e Swift
[ARC](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/automaticreferencecounting/)
e [atomics](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0410-atomics.md)
sustentam a matriz comparativa. Tree-sitter parse, refs com
digest, oracle host, snapshot e testes de mutação são evidência corrente;
compile, run, stress provider, provider e estudos humano/modelo continuam
missing. Só considerar uma primitive conditional-liveness se um problema
bounded exigir identidade ou semântica ephemeron observável e as composições de
generation/ID, owner lease ou detached value mudarem esse requisito ou
falharem, sob census pós-drain, cleanup determinístico e nenhuma fronteira
estrangeira oculta. Até lá não há motivo para disfigurar a linguagem com um
collector implícito.

### 1.29.1 CYC2 — fechamento sem subcapability ativa

CYC2 fecha o problema bounded de cache que CYC1 isolou. O corpus
[`tooling/cyc2-conditional-liveness-cases.json`](tooling/cyc2-conditional-liveness-cases.json)
registra três composições baseline: generation/ID com key detached e
invalidation explícita, owner-scoped lease com `close`, e detached value sem
back edge strong. Todas usam close admission, drain, quiescence e census.

Weak-key comum e ephemeron value→key são **Rejeitado por enquanto** para a
baseline. Transparent collector, finalizer oculto e reanimation são
**Rejeitado**. O checker exige zero active Research e zero collector side
effects. O oracle não coleta, executa `deinit` ou altera release.

Runtime/provider/compiler/linker, FFI execution, stress e OOM são
implementation-evidence-gap. O estudo não mantém uma Research subcapability
para esses artefatos. CYC1 continua QA histórico/current para SCC, residual,
unknown foreign boundary e ordem de cleanup.

A reabertura futura exige um caso Last Light bounded que precise identidade
observável da key e reachability ephemeron, falha das três composições após
drain/census, cleanup determinístico, orçamento finito, nenhuma foreign hidden
edge e evidência independente de compiler/runtime/provider. O caso qualificado
abre somente uma revisão futura. Ele não adiciona syntax, API ou collector ao
baseline.

### 1.30 DYN1 — comportamento dinâmico versionado

O estudo DYN1 informa DYN0-G1 e fornece a base para o fechamento
[`SYN2/DYN2`](tooling/studies/syn2-dyn2-closure); seus artefatos são host
design-oracle evidence, não comportamento de compiler/runtime/provider. Ele começa por um
problema controlado no Restaurante no fim do Universo: a cozinha troca uma
generation de plugin durante o serviço, a sala mantém snapshots da receita, o
observatório recebe completions atrasadas e a conta pode atravessar restart ou
deploy. A máquina em
[`tooling/dyn1-versioned-behavior-machine.mjs`](tooling/dyn1-versioned-behavior-machine.mjs)
deriva fatos somente de recipe, artifact/index/lock, interface, WAbi, runtime
closure, source-map/documentation, schema, target, capability/effect, isolation,
quota e receipts. Caller não fornece `status`, `route`, compatibilidade,
publicação, drain, rollback ou autoridade como booleans; `expect` é somente uma
guarda de mutation.

O corpus [`dyn1-versioned-behavior-cases.json`](tooling/dyn1-versioned-behavior-cases.json)
tem 70 casos A/B/C/D; o snapshot deriva route/status, três projections e as
contagens de cleanup. A cobre REPL committed snapshots, invalidation de
compiled hard dependencies, inspector read-only e export/import canônico. B
cobre generations de service/plugin tipado, schema exact/compatible,
`SemanticInterfaceKey`, `WAbiKey`, `RuntimeClosureKey`, target A/B e reducers
local/split independentes. Em schema `compatible`, candidate
`SemanticInterfaceKey`/`ServiceIRKey` novos são ligados por receipt old/candidate
com decisão explícita. C fica isolada em
`DYN0-persistent-generation-reference`: uma `GenerationReference` bounded e
read-only que liga facts de generation entre restart/deploy; não é
`PersistentRef<P>`, não carrega capability/authority e não converte para
`ServiceRef`. Inspector de snapshot committed
já é composição de A; C agora é o contrato estreito de identity facts e receipt
de migration/resolve, sem live state. Ele não é uma nova forma de reflection e
não rebaixa DYN0.
Uma seleção concorrente aceita candidates ready somente com um winner receipt
atômico; empate, duplicate, ausência de receipt ou candidato stale falha com fato explícito.
D rejeita eval/exec, monkey patch, active-frame/debugger write, ambient lookup,
native dynamic library como sandbox, autoridade por nome e `dlclose` com callback
vivo.

Cada troca segue `prepare → validate → preflight → ready → switch` com publicação
atômica. Depois do switch a admissão antiga fecha; children, waits, loans,
streams, callbacks e resources cancelam/drain; a ordem segue
`unregister → inFlight drain → destroy → unpin → release`; process/Wasm/component
acrescentam `unmap`, enquanto native exact-WAbi retém mapping até o fim da runtime
island. Completion,
message e capability da generation antiga são stale e rejeitados. Falha antes da
publicação preserva a antiga. Falha de drain depois da publicação deriva
`degraded`, nunca rollback. Rollback só deriva de provider receipt antes da
publicação. Crash pré-publicação preserva a antiga como fault-boundary ou
unknown-effect quando o provider outcome foi perdido. Crash pós-publicação
mantém a nova committed, com degraded somente se o drain falhar.

Identidades de semantic interface, ABI, runtime closure, recipe, artifact,
source-map e documentation não são uma hash única. Schema exact rejeita drift;
schema compatible aceita apenas mudanças fechadas. Target A/B usa facts distintos
de registry, WAbi e artifact físico. Local exige WAbi exato. Split exige
ServiceIR/schema e pode usar WAbi target-specific. Ambos devem ter owner graph,
generation/publication, interface result, effect outcome, cleanup/drain,
capability state, stale events, selection, crash/degraded e export/import
lógicos iguais; physical trace pode divergir.
Artifact/index/digest/lock bastam para identidade; PATH, name, mtime, ambiente e
registry implícito não são lookup authority.

Export reúne source/package/workspace resolution, recipe/artifact/interface/source-map, receipts,
provenance, redactions e bound. Import executa `reopen → parse → check →
resolveReceipts`, depois reparseia e revalida. Não restaura heap, task, loan, capability, `ServiceRef` ou
provider handle. Cases adversariais cobrem receipts stale/missing/duplicate/
forged, source-map stale, digests, quotas, callback unload, FFI, stale generation,
cancel, crash e isolamento process/Wasm/component. Native dynamic library nunca
é sandbox. Os reducers separados forçam `projection-divergence` sob mutation,
em vez de ecoar a mesma decisão.

Assim, DYN0 continua Componível para A/B, C fica fechado como referência
bounded read-only e o arbitrary eval fica rejeitado sem criar feature de
linguagem. `languageDesign` permanece partial: compiler, runtime, provider,
std provider, isolamento real, stress e estudos humano/modelo continuam
missing. O fechamento promove o contrato de identity/receipt, não um provider
de migration. O stop condition continua uma derivação local/split consistente,
sem stale publish, leak, unbounded resource ou autoridade oculta; qualquer
evidência real de compiler/provider, OOM/fault, FFI e isolamento é registrada
como implementation-evidence-gap. O manifesto liga explicitamente CAP0
(`DYN0-versioned-change`, classificação Componível, cases, snapshot e
`SYN2/DYN2` com digest) e DYN0-G1.

As comparações usam fontes primárias oficiais: [C23 N3096](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf),
[POSIX `dlopen`](https://pubs.opengroup.org/onlinepubs/9699919799/functions/dlopen.html),
[POSIX `dlclose`](https://pubs.opengroup.org/onlinepubs/9699919799.orig/functions/dlclose.html),
Rust ([trait objects](https://doc.rust-lang.org/reference/types/trait-object.html),
[`TypeId`](https://doc.rust-lang.org/std/any/struct.TypeId.html) e
[Cargo build scripts](https://doc.rust-lang.org/stable/cargo/reference/build-scripts.html))
e Python ([importlib](https://docs.python.org/3/library/importlib.html),
[inspect](https://docs.python.org/3/library/inspect.html) e
[`eval`](https://docs.python.org/3/library/functions.html#eval)). Os snippets do
manifesto são pseudocódigo original bounded, não citações longas nem evidência de
W. O estudo host, snapshot, source refs e mutation checker são evidence; não são
compiler, runtime, provider, sandbox ou implementação.

#### HRD0 — proveniência da decisão hot reload dev-only

HRD0 registra a comparação problem-first em
[`tooling/studies/hrd0-hot-reload-dev`](tooling/studies/hrd0-hot-reload-dev), sem
adicionar decisão normativa ou novo ID de linguagem. O corpus tem 20 casos e cinco
mutations adversariais independentes para cleanup (step físico extra, step lógico
ausente e ordem errada), contrato nominal duplicado e drift de interface. A decisão corrente é um runner de tooling somente para
desenvolvimento: ele recompila/reabre units W normais pelo frontend existente e
faz `prepare → validate → preflight → ready → switch`; não há syntax, profile de
source/package ou modo dinâmico de release. A invocação (`w dev` ou
`w run --watch`) permanece tooling-owned e não selecionada.

A e B são composição corrente: snapshot REPL e generation de service/plugin
tipado usam identidades exatas de schema, effects, capabilities, source map,
package, `SemanticInterfaceKey`, `WAbiKey` e `RuntimeClosureKey`; a generation
antiga fica retida até admission close e drain, roots novos entram somente na
nova e nenhum heap/task/loan/frame/`ServiceRef`/callback/provider handle migra.
O cleanup lógico comum contém cancelamento, drains, unregister, in-flight drain,
destroy e release. `unpin` e `unmap` só entram no plano físico quando facts
declaram pin/FFI ou mapping (mapping nativo fica retido); a equivalência local e
split compara apenas o resultado lógico, pois o trace físico pode divergir.
Rollback tem receipt somente antes de publication; falha de drain depois da
publication é degraded/fault e nunca rollback. C agora é o contrato de design
bounded de generated module set content-addressed: W0 hermético emite units `.w`
com provenance/source-map, reabre cada unit normal e só publica interface depois
de parse/name/type/ownership/effect/ConstIR; action-result, CAS, interface e
compiler cache continuam identidades separadas. A spelling de invocação permanece
tooling-owned. D rejeita sete routes: production
dynamic mode, native como sandbox, active-frame write, eval/exec, live-state
migration, live `dlclose` callback e module injection.

O witness comum [`hot_reload_dev_contract.w`](reference/last-light/hot_reload_dev_contract.w)
declara nominalmente os events, outcomes, input/result e identity fields; os
witnesses local/split importam exatamente esses tipos. Functions de event/outcome
mostram a mesma fronteira local/split; todos continuam fixtures parseáveis e não
alegam execução. O bundle, oracle host, reducers independentes,
mutation checker, snapshots, Tree-sitter e referências oficiais são evidence
corrente. Compiler/type/effect/ownership real, runtime/provider/std-provider,
isolamento, stress e estudos humano/modelo permanecem gaps. W-1398/W-1399 e o
generated-module closure classificam a fronteira sem alegar implementação;
compiler/CAS/provider/runtime/target/OOM/FFI/isolation têm IDs de gap separados
no estudo SYN2/DYN2. Contagem, digest e snapshot demonstram apenas o oracle host;
implementation evidence ainda é necessária antes de qualquer claim de execução.

#### SYN2/DYN2 — proveniência do fechamento

O bundle [`SYN2/DYN2`](tooling/studies/syn2-dyn2-closure) reutiliza os estudos
SYN1, DYN1 e HRD0 por referências com digest, em vez de os substituir. A máquina
de fechamento deriva resultados de facts e não lê `status`, `route` ou `expected`
do chamador. Ela reusa os reducers independentes já validados de DYN1 e HRD0;
esta máquina não alega possuir um segundo par de reducers. O checker valida
containment, roles, source/official refs, chain de digests e snapshot antes de
aceitar os 17 casos (12 correntes, cinco rejeitos).

O `GenerationReference` corrente tem exatamente oito campos de dados:
`generationId`, `artifactDigest`, `recipeDigest`, `semanticInterfaceKey`,
`schemaDigest`, `targetReceipt`, `resolveReceipt` e `migrationReceipt`. O target
receipt contém WAbi/runtime-closure somente para o target exato. Resolve/migration
apenas re-resolvem ou rebindam identity/schema; não migram heap, task, loan,
frame, capability, `ServiceRef`, callback ou provider handle. Campo extra,
ausente, duplicado ou estado vivo é rejeitado. Isto não é `PersistentRef<P>` de
§23.1.6: não carrega capability ou authority e não converte para `ServiceRef`;
W-1398 C fecha somente a identidade da generation.

### 1.31 HUM0 — programa de evidência humana e de modelos

HUM0 materializa um protocolo cross-cutting para ergonomia humana e de modelos
sem reabrir a semântica de W. O produto continua o Restaurante no Fim do
Universo, mas o protocolo trabalha por problema, não por snippet ou preferência
de sintaxe. Os oito slices são:

1. diagnostics e `w explain` em turnos de pedido;
2. ownership, borrow, `shared` e `weak` na raiz de menu e seleção;
3. allocator contextual e rehome de menu staged;
4. execution forms, labels, suspension e placement;
5. tasks, channels bounded e backpressure;
6. services, turnos fechados e generations;
7. package/build hermético e REPL transacional;
8. FFI callback lease, registration optional e unsubscribe guardado; o drain
   externo é uma obrigação do oracle antes de destroy → unpin → reclaim.

Cada slice referencia um ou mais symbols reais de `reference/last-light` com
digest verificado e pelo menos dois oracles host independentes com digests. O
input primary e o adversarial mantêm o mesmo `problemKey` e `outcomeKey`; a
variação só exercita a fronteira do problema. Cada slice tem exatamente as
tasks `explain`, `recall`, `repair` e `change`, duas ordens counterbalanced,
blinding e listas separadas de `hiddenInternalFacts` e `explainableFacts`.

O `stimulus` é uma janela bounded de bytes UTF-8 reais, derivada por
`sourceRefId`, símbolo único, `beforeLines`, `afterLines`, `maxBytes` e digest.
Ela começa e termina em limites de linha. O adversarial aplica uma única
mutation find/replace na mesma janela; `mutation` e `expectedRepair` ficam em
`observerOnly` e nunca entram no input visível. O machine extrai os bytes,
confirma bounds, UTF-8, unicidade, digest e a relação primary/adversarial, e os
tasks `repair`/`change` apontam somente para o stimulus adversarial.

IDs de instância D0, PlaceId, LoanId, OriginSet, control block, GenerationId
real, worker/thread/queue físico, endereço, PID, clock, locale, segredo,
payload, ponteiro e implementação de host não aparecem no input participante.
`w explain` pode mostrar somente facts determinísticos de owner/move/borrow/drop,
effects, allocator origin/mobility, logical trace/receipts, dependency e
invalidation, distinguindo fact, estimate, measurement e unknown. No FFI, o
source prova registration optional e unsubscribe guardado; a prova de drain de
callbacks em voo permanece externa e deve vir do oracle/explain. O protocolo
proíbe `expected`, `status`, `route`, `role`, `path`, `digest` e `oracle` no
input visível; esses campos são somente metadata de validação ou provenance de
um registro futuro. O renderer participant-only entrega apenas cenário, tarefa,
instrução, source e label blinded.

O snapshot HUM0 deriva apenas prontidão estrutural atual: oito slices, 32 tasks,
zero registros humanos e zero registros de modelos. Não calcula score,
preferência, ergonomic win, compreensão ou promoção. `human-study`,
`model-study`, `w-compile`, `w-run` e providers permanecem missing. O contrato
humano futuro exige `participantIdHash` sha256, background não-vazio em
C/Rust/Python/W, tempo e queries não negativos, confiança obrigatória 1–5,
outcomes exatos `semantic`/`repair`/`change` em `pass|fail|inconclusive` e
`observerReceiptDigest` sha256, sem PII. O contrato de modelo exige provider,
model, version e tokenizer não vazios, params JSON fechado, input/observer
digests sha256, tokens `input`/`output`/`total` com soma e os mesmos outcomes,
sem resultado caller-owned.

O stop condition é o primeiro expected echo, outcome forjado, source/oracle
stale ou ausente, símbolo não encontrado, vazamento de identidade interna,
problema/outcome divergente, duplicata, métrica manual ou desacordo do oracle.
Nesse caso a coleta para, o slice continua Research e um caso independente é
obrigatório antes de qualquer revisão normativa. Mesmo com records futuros,
preference só pode ser coletada depois das tasks objetivas e nunca promove uma
forma automaticamente. HUM0 não altera `DESIGN.md`, grammar, generated source,
Last Light, compiler, runtime, provider ou portal.

O estado corrente de evidência permanece explícito: source refs, oracles host,
counterbalance, blinding, checker e snapshot são `design-oracle-input`; não são
participantes, modelos, compiler ou runtime. Na cobertura atual, execution
ergonomics é 80 casos (32 positivos, 46 negativos, duas informações) e 26
testes host; a tabela datada de 11 de agosto abaixo preserva a contagem
histórica e não é sobrescrita por HUM0.

O ledger W-1400–W-1411 registra o protocolo, os contratos de resultado, a
fidelidade das mutações, o renderer fechado e o stop condition sem escolher uma
forma normativa.

### 1.32 PKG1 — identidade do owner e transação do root físico

PKG1 fecha a inconsistência em que `workspaceDigest` incluía bytes de
`resolution`. A forma corrente usa somente `build.w` como documento físico,
com records `package` e/ou `workspace` diretos. `resolution` e `deployments`
permanecem records aninhados.

O host deriva três identidades independentes:

- `ownerDigest` usa o owner basis sem `resolution` e `deployments`;
- `resolutionDigest` usa o record lógico completo e referencia `ownerDigest`;
- `deploymentDigest` usa cada deployment nomeado e liga artifacts, plans e
  receipts explicitamente.

O caminho físico, comments e formatação não entram no owner basis. Alteração de
dependency, member ou policy muda o owner. Refresh de resolution preserva o
owner. Alteração de deployment preserva owner e resolution.

`w resolve` grava somente a resolution. `w add`, `w remove` e `w update`
preparam owner e resolution juntos. O host valida a closure, aliases, contexts
e policy antes de formatar um replacement completo. `--dry-run` não escreve.
Uma falha deixa os bytes anteriores.

O compare-and-replace usa o digest exato que o host leu. Um writer concorrente
produz stale-write. O host não faz merge e não usa last-write-wins. O temp
reside no mesmo directory e recebe cleanup após toda saída.

POSIX e Windows usam reducers independentes. POSIX usa `rename`, flush do file
e receipt de sync do parent directory. Windows usa `ReplaceFile`, flush e
reopen/verify. `atomicVisible` não prova `crashDurable`. Durability é true
somente com receipt explícito do provider. A ausência permanece
`evidence-missing`.

O oracle [`pkg1-project-transaction-machine.mjs`](tooling/pkg1-project-transaction-machine.mjs)
deriva outcomes dos events e records. O corpus cobre stale/missing/duplicate
references, caller echo, forged digests/receipts, reducer divergence, alias
collision, context closure, solve failure, crash boundaries e cleanup. O estudo
[`PKG1`](tooling/studies/pkg1-project-transaction) registra identity split e
atomic replacement como current. Durable provider receipts ficam Research.

As fontes primárias são POSIX
[`rename`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/rename.html),
Windows [`ReplaceFile`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilea),
Cargo [workspaces](https://doc.rust-lang.org/cargo/reference/workspaces.html),
Cargo [manifest versus lock](https://doc.rust-lang.org/cargo/guide/cargo-toml-vs-cargo-lock.html)
e Python [`pylock.toml`](https://packaging.python.org/en/latest/specifications/pylock-toml/).
Essas fontes orientam o oracle. Elas não provam compiler, runtime, package
manager, provider ou fault probe real.

### 1.33 Defaults de protocol sem herança

W não usa herança de implementação ou storage. Protocols fornecem substituição
nominal e podem refinar outros protocols, mas refinement agrega somente
requirements. Reuso stateful continua em composição nominal.

Um protocol declara requirements sem body. O módulo que declara o protocol pode
publicar uma implementação default em `extension Protocol`. A forma separada
mantém o contrato visível, permite constraints no head da extension e evita que
o protocol se comporte como uma classe-base. A conformance grava a escolha do
witness; uma implementação própria vence o default e imports posteriores não
mudam a seleção.

Defaults não adicionam storage, não ampliam effects ou ownership e não usam
prioridade, specificity ou ordem de import. Overlap é erro. Extensions de outros
módulos podem oferecer helpers para lookup estático, mas não publicar ou trocar
o witness default. A assinatura e disponibilidade do default entram na
interface; o body continua artifact de implementação separado.

Delegação automática não entra nesta decisão. Se forwarding explícito se tornar
ruído recorrente, uma forma futura deverá baixar para members HIR observáveis,
sem lookup dinâmico, linearização, `super` ou estado herdado.

### 1.34 AVF0 — availability e configuração gradual tipada

AVF0 separa três problemas que outras plataformas costumam agrupar como
feature flag. Uma feature de package seleciona aditivamente um grafo estático
antes da compilação. Availability decide se uma declaração já compilável pode
ser usada no target e provider selecionados. Uma feature de runtime escolhe um
valor tipado dentro do programa já autorizado.

A separação impede que configuração dinâmica vire authority. Uma flag não
carrega módulo, habilita dependency, concede capability ou effect, altera
`SemanticInterfaceKey`/`WAbiKey` nem torna legal uma declaração indisponível.
Todos os ramos alcançáveis permanecem no graph, nos effects, na runtime closure
e no audit. Availability é resolvida antes da política de runtime.

Package features permanecem current e aditivas. A forma de runtime é uma
composição de std/provider: chave nominal tipada, fallback do mesmo tipo,
contexto com fields declarados, owner/expiry, snapshot imutável, schema identity
separada da configuration generation/digest, rollout determinístico e exposure
explícita. Config stale ou ausente retorna fallback sem ampliar authority.

O corpus pré-ASIC0 registra como Research o binding que permitiria ao compiler
estreitar uma declaração após evidence de availability. ASIC0 fecha como design
current o contrato de facts autenticados e binding typed fail-closed, sem nova
keyword ou runtime API; W-1449 mantém os gaps de compiler, diagnostics e
provider. O witness textual usa `available(...)` apenas para estudar tipo,
ownership, effects, fallback e diagnostics; ele não é syntax W aceita. Boolean,
versão textual do OS, deployment field ou runtime flag não é evidence de
provider.

O corpus AVF0 tem 38 casos em package, availability, runtime e composição: 14
aceitos, 24 rejeitados e sete rejeições explícitas de authority amplification.
Cloudflare Flagship, Swift availability e OpenFeature são fontes primárias de
comparação. O oracle Bun, Tree-sitter, snapshot e source refs não provam
compiler, runtime ou provider.

O stop condition de implementação exige dois domínios reais,
compiler/type/effect/ownership, provider receipts, snapshots atômicos, rollout
estável, exposição auditável, expiry/owner e projeções local/split. Até essa
evidence, runtime flags continuam library composition e o contrato current de
availability binding não implica compiler ou provider pronto.

### 1.35 SEC0 — modelo de segurança amplo por perfil físico

SEC0 amplia o estudo de segurança para além de memória e paralelismo. O ponto
de partida é o que W já consegue compor: ownership e borrow, type/effect checks,
capability roots, domains, services, channels, filesystem scopes, resource
limits, `WAbiKey`, `SemanticInterfaceKey`, foreign boundaries e receipts de
packages. O estudo não transforma esses contratos em uma promessa de sandbox.

Safe W mantém invariantes irredutíveis de memory, type, effect, capability,
input e resource. API access exige capability explícita, effect declarado,
mediation e attenuation. Lookup ambiental, authority por string, capability
amplification, arbitrary evaluation e current-frame mutation são rejeitados.
Secret values usam lease e não entram em serialização ou audit log. Input
traversal, allocation, concurrency e network têm budgets explícitos.

Um check provado pode ser elidido. Um check não provado permanece, rejeita o
build ou entra em um `unsafe` explícito com ABI, provenance, bounds, cleanup,
allocator, effect e review facts. `unsafe` não cria unchecked UB seguro. FFI
raw pointers, debugger access, dynamic loading e callback retention permanecem
boundaries explícitas.

SEC0 modela seis perfis: trusted native CPU, sandboxed native process,
WebAssembly component, multi-tenant isolate, embedded freestanding e
FPGA/ASIC hardware-partitioned. Cada perfil declara threat model, residual
risk, product minimum e deployment controls. Todos mantêm os mínimos comuns
`memory-safety`, `effect-capability-checks`, `input-bounds` e `supply-chain`,
além dos controles físicos próprios. Runtime protection pode ser substituída
ou omitida somente por static proof, hardware enforcement ou external
mediation. A exceção `threat-model-not-applicable` é separada em
`threatExclusions`, limitada a isolation/tenant/side-channel e exige receipt de
`policy-review`. Toda proteção exige receipt fechado com issuer/stage
compatíveis, escopo de profile/target e digests SHA-256.
`runtimeEnforcement: present` somente admite basis `runtime-enforcement`,
issuer `runtime-provider` e
stage `runtime`; `omitted` proíbe essa basis e exige static proof, hardware,
mediação externa ou a exceção revisada. O profile fixa um target do registry e
um artifact digest; cada receipt deve coincidir exatamente com esses valores.
Boolean, feature flag, ambient configuration ou performance switch nunca
satisfaz essa substituição.

Deployment pode reduzir budget ou escolher um hardening profile. Ele não pode
enfraquecer o product minimum. Physical target changes podem alterar `WAbiKey`,
runtime closure e hardening receipts. Eles preservam `SemanticInterfaceKey`
quando o contrato público não muda. Mudança pública altera a key semântica.

Side channels exigem threat model e residual risk. Timers, cache, scheduler,
memory layout, concurrency e resource use não têm uma solução universal. O
profile deve registrar clock policy, scheduler policy, concurrency policy e
mitigations. A ausência de residual risk é uma falha do modelo.

Patch e supply chain ligam source digest, lock digest, recipe, artifact,
signature, attestation e deployment admission em ordem. Um patch incompleto ou
um receipt caller-owned não publica. O estudo separa isolation, input/resource,
secrets, audit, capability/API mediation, FFI, tenant boundary e attestation.
Cloudflare Workers, Linux seccomp, WebAssembly, WASI, RATS e Sigstore são
fontes primárias de comparação. Nenhuma fonte externa é autoridade sobre W.

O corpus SEC0 possui 101 casos, 24 aceitos, 77 rejeitados, 11 outcomes current e
13 outcomes Research como labels históricos do source corpus pré-ASIC0, seis
profiles, 16 authority rejections e quatro caller-echo rejections. ASIC0 fecha
como design current os seis profile schemas, o residual budget de side-channel,
patch attestation e ordered deployment/hardening receipts; W-1450 mantém a
evidence de implementação. O oracle Bun, os fixtures
Tree-sitter, o snapshot e os source refs são design evidence. Eles não provam
compiler, runtime, provider, sandbox, hardware, attestation verifier ou
deployment control plane. O stop condition exige facts de compiler, provider e
hardware, receipts de artifact/hardening, fault injection, secret lifecycle,
side-channel residuals, FFI tests e evidência local/split para os seis perfis.

### 1.36 FRC0 — encerramento final das gates de pesquisa

FRC0 fecha o snapshot de processo que existia em W-707, W-731 e W-1408 até
W-1450 e valida W-1451, W-1452 e W-1453 como decisões
`oracle-backed-current` depois do PFU0. Ele reutiliza FZ0, a classificação do
ledger e HUM0. Ele não copia payload e não produz compiler, runtime, provider,
resultado humano ou resultado de modelo.

| Decisão | Current | Adversarial | Fact derivado |
|---|---|---|---|
| W-707 | completude G0–G5, source refs e snapshot | família FZ0 ausente | `FZ0-freeze-completeness` falha closed |
| W-731 | uma disposition por decisão, `Research=0` no snapshot até W-1453 e lista exata das gates posteriores | decisão W-1408 removida | `freeze-research-close` falha closed |
| W-1408 | HUM0 com 8 slices, 32 tasks, 0 human, 0 model e stop-on-first | registros human/model e preference/score forjados | `HUM0-promotion` falha closed |

O corpus `tooling/final-research-closure-cases.json` contém exatamente uma
rota current e uma adversarial por decisão. A máquina deriva o resultado dos
facts de cada cópia. Ela ignora ID, `expected`, status, score, preference e
qualquer métrica fornecida pelo caller. O resultado current é
`oracle-backed-current`, com `evidence.current` limitado a source refs,
corpora/máquinas reutilizados, host oracle, mutation checks, snapshot e parse
thin. `w-compile`, `w-run`, compiler, runtime, provider, `human-study` e
`model-study` permanecem `evidence.missing`.

O manifest fixa a cadeia de artefatos, digests, containment e roles. O bundle
R1 fixa duas variantes W finas e parseáveis, ordem de apresentação, blinding e
oracle host. O stop condition cobre stale digest, caller echo, manual count,
registro human/model, preference/score, decisão/caso ausente ou duplicado,
source escape, categoria errada e qualquer `Research` residual. W-1471,
W-1473, W-1474 e W-1475 reabriram gates depois do snapshot; DRC0 fechou suas
stop conditions. FRC0 preserva os gaps de implementação e não promove compiler,
runtime ou provider.

### 1.37 Gate SOTA de performance e matriz de responsabilidade

Esta seção prepara a implementação de performance. Ela é evidência comparativa
e não contrato normativo. A matriz não cria syntax, API ou W-ID. No snapshot em
que foi criada, ela não reabriu `Research=0`; W-1471, W-1473, W-1474 e W-1475
foram abertas posteriormente e não pertencem a esse gate.

O artigo de 2026 sobre o expoente de multiplicação ([arXiv:2608.16884](https://arxiv.org/abs/2608.16884))
relata o limite teórico `omega < 2.371177`. Esse resultado pertence à
complexidade algébrica. Ele não é um claim de GEMM prático. A aula de [MIT 6.172 sobre matrix
multiplication](https://ocw.mit.edu/courses/6-172-performance-engineering-of-software-systems-fall-2018/d0c73dd51c79b95196a2e6faa824e1b4_MIT6_172F18_lec1.pdf)
mostra efeitos de ordem de loops, flags, paralelismo, tiling,
divide-and-conquer e vetorização. A palestra sustenta workloads e oracles
medidos, não uma escolha de semântica.

O [AlphaEvolve](https://arxiv.org/abs/2506.13131) mostra uma rota de busca que
edita código e usa avaliadores. Ele não autoriza autotuning durante o build W.
O uso permitido é pesquisa offline com digest, workload, oracle e stop
condition registrados.

Esta matriz é um seed mínimo extensível, não um catálogo exaustivo. Ao abrir um
bundle para um hotspot, atualize as fontes primárias e as alternativas correntes.
Registre target, CPU/device, toolchain, provider, dataset, data e os digests das
fontes e entradas. Primeiro verifique correção diferencial; só depois faça
timing. Registre warmup, repetições, distribuição e variância, além de memória,
inicialização, packing e custo de compilação quando aplicável. Use pelo menos
dois baselines independentes quando razoável ou registre por que um baseline é
suficiente. Não generalize de um benchmark.

Na divisão de responsabilidade, a linguagem define semântica, tipos,
ownership, effects, shapes e numeric modes. O compiler escolhe provas,
transformações, especialização e lowering. Runtime, provider e library escolhem
packing, microkernels, dispatch, device e measurement. Trabalho pequeno e
estático pode receber lowering para código inline quando fatos e modelo de
custo fecham. Trabalho grande ou irregular fica no provider ou na library. Sparse e
graph seguem uma rota separada de dense matrix. `.strict`, `.fast` e
`.reproducible` continuam explícitos.

Cada linha exige cinco campos. O problema delimita o workload. A fonte primária
registra alternativas observáveis. O owner recebe a decisão operacional. O
workload e o oracle fixam a medição. A stop condition encerra a busca sem
promover um resultado local a claim geral. O seed pode crescer quando um novo
hotspot exigir outra alternativa, mas não muda a semântica nem reabre
`Research=0`.

| Domínio | Problema, alternativas e fonte primária | Owner | Workload e oracle | Stop condition |
|---|---|---|---|---|
| dense matrix/tensor | GEMM, contractions, layouts e devices. Compare [BLIS](https://doi.org/10.1145/2764454), [MLIR Linalg](https://mlir.llvm.org/docs/Dialects/Linalg/), [MLIR Transform](https://mlir.llvm.org/docs/Dialects/Transform/), [cuBLAS](https://docs.nvidia.com/cuda/cublas/index.html), [oneDNN](https://uxlfoundation.github.io/oneDNN/), [ReproBLAS](https://www.netlib.org/utk/people/JackDongarra/WEB-PAGES/Batched-BLAS-2016/Day1/10_Demmel_ReproBLAS.pdf) e [IREE](https://iree.dev/reference/bindings/c-api/). | compiler para provas e lowering. Provider ou library para packing, microkernel e dispatch. | Shapes pequenas estáticas e grandes densas. Compare `@` nos modos `.strict`, `.fast` e `.reproducible` contra um oracle de valores e pelo menos um baseline BLAS; adicione um segundo baseline ou justifique sua ausência. | Pare quando a diferença semântica for zero, o fallback estiver coberto e a medição ficar dentro do ruído da matriz alvo. Não use `omega` como métrica de GEMM. |
| sparse/graph | SpMV, traversal e semirings não têm o mesmo custo de dense GEMM. Use a [GraphBLAS C API specification](https://graphblas.org/docs/GraphBLAS_API_C_v2.1.0.pdf) e implementações de referência como alternativa. | provider ou library especializado. Compiler fornece shape, alias e effect facts. | Matrizes com graus uniformes e skewed, BFS, PageRank e semirings. Oracle compara semântica de ausência, ordem permitida e resultado por semiring. | Pare quando formatos sparse, locality e dispatch forem medidos no workload. Não insira fallback dense implícito. |
| FFT/signal | Planejamento, radix, real/complex e reuse de plans. Use [FFTW3](https://fftw.org/fftw-paper-ieee.pdf) e bibliotecas de device como alternativas. | provider ou library. Compiler conserva shape, units e numeric mode. | Tamanhos smooth, prime e batched, sinais reais e complexos. Oracle verifica erro numérico, ordem strict e repetição de plan. | Pare quando plan creation, execution, memory e reproducibility tiverem budgets publicados. |
| Unicode/JSON | Validação, transcoding, structural scan e number parse. Compare [simdutf](https://simdutf.github.io/simdutf/) e o [simdjson paper](https://arxiv.org/abs/1902.08318). | std/provider ou library. Compiler pode vetorizar somente com facts provados. | Corpus ASCII, Unicode misto, entradas inválidas, JSON pequeno e grande. Oracle valida bytes, scalars, errors e offsets. | Pare quando o fast path e o fallback produzem o mesmo contrato e a taxa medida tiver variância conhecida. |
| collections/hash | Probes, rehash, locality e estabilidade de referência. Use as [Swiss Tables](https://abseil.io/about/design/swisstables) como alternativa de layout e probing. | std/library. Compiler escolhe especialização local. | Hit, miss, delete, rehash, chaves adversariais e iteração. Oracle verifica equality, hash, ownership e ordem declarada. | Pare quando memória, latência e invalidation atenderem o contrato. Não transforme ordem incidental em semântica. |
| allocator | Free lists, locality, cross-thread free, security e contention. Compare o artigo do [mimalloc](https://www.microsoft.com/en-us/research/publication/mimalloc-free-list-sharding-in-action/) e o allocator do target. | runtime/provider. Language ownership, drop e budget continuam fixos. | Objetos curtos e long-lived, burst, threads, tamanhos e falhas de budget. Oracle verifica origin, cleanup, bytes cobrados e OOM. | Pare quando o lowering preservar owner graph e os ganhos superarem o custo em workload representativo. |
| scheduler/tasks | Work stealing, queues, fairness, cancellation e placement. Use [Scheduling multithreaded computations by work stealing](https://doi.org/10.1145/324133.324234) como fonte primária. | runtime/provider. Compiler entrega o DAG estruturado e seus effects. | DAG fully strict, skewed tasks, join, cancel, deadlines e domains. Oracle verifica ordering, drain, budget e outcome. | Pare quando join e cleanup forem diferenciais e a medição reproduzir o perfil alvo. |
| I/O | Batching, queue depth, buffers, completion e cancellation. Compare [io_uring(7)](https://man7.org/linux/man-pages/man7/io_uring.7.html) e [CreateIoRing](https://learn.microsoft.com/en-us/windows/win32/api/ioringapi/nf-ioringapi-createioring). | runtime/provider. Language effects, ownership e close permanecem explícitos. | Files e sockets, sync e async, queue depth, short progress e faults. Oracle verifica bytes, errors, close order e bounded queues. | Pare quando throughput não ocultar errors, cancellation ou cleanup e o provider declarar unsupported. |
| regex | Matching seguro, compilation, memory budget e SIMD. Use o [RE2 source e contrato](https://github.com/google/re2) como alternativa não backtracking. | library/provider. Compiler só aplica transforms sem mudar o automaton. | Corpus normal e adversarial, Unicode, captures suportados e pattern limits. Oracle verifica resultado, tempo linear declarado e falha por budget. | Pare quando a policy de segurança e o subset de syntax forem claros. Não prometa features de backtracking sem prova. |
| compression | Throughput, ratio, streaming e bounded memory. Use a [Zstandard format specification](https://www.rfc-editor.org/rfc/rfc8878.html) e a [reference implementation](https://github.com/facebook/zstd). | library/provider. Compiler não escolhe formato wire. | Corpus textual e binário, níveis, streams longos, checksum e erro truncado. Oracle verifica bytes decodificados, frame e error code. | Pare quando compatibilidade de formato ou limite de profile estiver declarado e a medição cobrir ratio e throughput. |
| hashing | Throughput, tree parallelism, streaming e exact digest. Use a [BLAKE3 specification](https://github.com/BLAKE3-team/BLAKE3-specs) e a implementação oficial. | library/provider. Compiler pode vetorizar sem alterar a função. | Vetores vazios, chunks de 1 KiB, tamanhos grandes, keyed e derive-key. Oracle compara digests e streaming state byte a byte. | Pare quando todos os vectors forem exatos e a dispatch por target tiver recipe reproduzível. |
| tabular/query | Batches, selection, compression, joins e operator fusion. Compare o [formato vetorizado do DuckDB](https://duckdb.org/docs/lts/internals/vector) e o [paper do DuckDB](https://duckdb.org/pdf/SIGMOD2019-demo-duckdb.pdf?file=SIGMOD2019-demo-duckdb.pdf). | compiler para facts e transforms. Runtime/provider/library para vectors, packing e dispatch. | Queries TPC-like, filtros seletivos, nulls, strings, joins e batches pequenos. Oracle verifica rows, order, null semantics e numeric modes. | Pare quando o batch size escolhido tiver evidence, o resultado diferencial for zero e o fallback escalar estiver coberto. |

O resultado desta matriz entra na recipe somente após a stop condition. Um
benchmark isolado pode orientar uma hipótese. Ele não muda a API, o W-ID, o
numeric mode ou a alegação de implementação. A matriz deve ser revisada quando
o target, o workload, o provider ou o oracle mudar.

### 1.38 PFU0 — encerramento de usabilidade pré-freeze

PFU0 fornece a evidência host-only para três decisões depois do snapshot
histórico FRC0. W-1451, W-1452 e W-1453 são agora
`oracle-backed-current`; FRC0 verifica `Research=0` nesse limite histórico.
As gates W-1471, W-1473, W-1474 e W-1475 foram abertas depois e fechadas por
DRC0. PFU0 não trata a máquina host como compiler, runtime, provider ou
resultado humano.

| Gate | Controle vigente | Alternativa avaliada | Rejeitado |
|---|---|---|---|
| W-1451 | `build.w` direto e data-only com um ou dois records top-level, em qualquer ordem; no máximo um `package` e um `workspace`, pelo menos um. Package-only selecionado em contexto standalone possui `resolution`/`deployments`; package-only membro de workspace omite esses fields e o workspace declarado é o owner; workspace-only ou package+workspace: workspace possui, package omite. `workspace.members` aponta para dirs cujo `build.w` contém package. | nenhuma forma alternativa é promovida | arquivo vazio, records duplicados, wrapper físico `build.w {}`, package inline, nested workspace member, glob, scan ambiental, source executável e owners duplicados; `package.w`/`workspace.w` sem shim |
| W-1452 | APIs de service retornam explicitamente `some Stream<Item, Failure>`. A chamada via `ServiceRef` sempre acrescenta `ServiceFailure` na fase de abertura/admission; o erro da função chamadora deve ser `ServiceFailure` ou ter exatamente uma conversão total de `ServiceFailure`. Separadamente, o `Failure` terminal permanece no stream e deve admitir `ServiceFailure`. `Channel` é explícito em capacity, endpoints, ownership, backpressure e `close`; mailbox e stream permanecem distintos. | nenhuma promoção de `stream fn`; a forma geral é rejeitada por capturas, lifecycle e erro ambíguos | `stream fn`, client-stream, bidi, Channel implícito, capacity implícita, `ServiceRef` sem `await`, closed-turn change ou colapso de `ServiceFailure`/`Failure` |
| W-1453 | `get`/`set`/`modify` continuam vigentes em property stored, computed e behavior. `init` bypassa accessors; assignment simples usa `set`/replacement; mutation compound usa `modify` uma vez; `return inout` é pre-borrow e `defer` retoma pós-borrow; drops ocorrem uma vez; notificação externa é método/service/channel nomeado. | nenhuma promoção de observer spelling | `willSet`/`didSet`, observers implícitos, cópia oculta de old value, backing type/deinit oculto e notificação externa sem nome |

O estudo host/oracle é determinístico e source-backed. O corpus e a machine não
aceitam `expected` ou resultado fornecido pelo caller; `bundle.inputs[].expected`
é rubric metadata R1, fica oculto por `blinding.hide` e nunca entra em
`validateCorpus`/`evaluateCase` nem é mostrado ao participante. As nove rotas
(current, candidate e adversarial em cada família) derivam facts e mutations:
manifesto build.w, streaming de saída com canais explícitos e lifecycle de
property. O manifest candidate é aceito como current-control; os candidates de
`stream fn` e `willSet`/`didSet` são rejected-route. A evidência não afirma
compiler/runtime/provider e fecha Research somente no escopo W-1451–W-1453.

W-1480 substitui posteriormente somente a rejeição PFU0 de client-stream e
bidirectional-stream. A rejeição de `stream fn`, Channel implícito, capacity
implícita e colapso das fases de failure continua vigente. SVC0 é a autoridade
de estudo para as quatro direções atuais.

Artefatos canônicos: `tooling/pfu0-pre-freeze-usability-cases.json`,
`tooling/pfu0-pre-freeze-usability-machine.mjs`,
`tooling/check-pfu0-pre-freeze-usability.mjs` e
`tooling/studies/pfu0-pre-freeze-usability/`. As fontes reutilizadas são os
contratos de build.w, stream/Channel/mailbox e property no atlas de Last Light;
os digests são verificados pelo checker. O stop condition rejeita qualquer
Research residual global, stale digest ou caller echo. A decisão de freeze usa
o resultado PFU0 e não afirma compiler, runtime ou provider.

### 1.39 AEG0 — App Essentials Gate

AEG0 fecha cinco fronteiras de aplicação depois do PFU0. W-1454–W-1458 são
`oracle-backed-current`. O bundle usa fatos derivados por máquina host e
testemunhos finos. Ele não cria syntax, keyword, manifest field ou provider.

| Gate | Forma vigente | Contrafactual rejeitado | Evidência de implementação |
|---|---|---|---|
| W-1454 | Capability nominal não forjável, provider/profile/digest explícitos, owner ou lease ligado a root+generation, operações com effect/error/ownership/bounds/complexity/cancellation, values portáveis separados de handles, borrow/move explícito entre tasks e rebind de host para service/process | lookup ambiental, initializer público, capability forjada, handle no wire, child sem borrow/move, owner ou generation ausente | compiler, runtime, provider, FFI, isolamento, stress e estudos continuam missing |
| W-1455 | `std.time` operacional permanece. `UtcTimestamp` é `WireValue` portable; `Instant` e `Deadline` continuam locais. Data civil, local datetime, timezone e calendar são values explícitos. Conversão exige provider/database profile com version/digest e zone/calendar/profile explícitos. Gap/fold rejeita por default ou exige policy. Locale, calendar, timezone e wall clock não são ambientais. Deadline usa clock operacional, nunca tempo civil. Leap-second/smear policy fica no profile; não há conversão implícita | global `wallNow`/`now` civil, timezone/locale/calendar implícitos, deadline dirigido por relógio civil, conversão automática ou catálogo/provider inventado | provider de timezone/calendar, compiler, runtime e estudos continuam missing |
| W-1456 | Package/profile geral separa secure provider-backed de deterministic explicit-seed. Secure não aceita seed, fallback ou downgrade e exige bytes bounded, integer uniforme checked e erro tipado. Deterministic é replayable e não satisfaz secure. Draw order é owner-local. Context HTTP projeta o mesmo contrato. Somente seed/profile determinístico pode entrar em test receipt; secure seed/draw/bytes não entram em receipt/log/diagnostic | seeded secure, secure→deterministic fallback, inheritance implícita, secure seed/draw/bytes em receipt/log/diagnostic, handle no wire ou contexto ambiental | provider, compiler, runtime, entropy source, stress e estudos continuam missing |
| W-1457 | Packages específicos declaram ByteSource/Sink, profile+digest, streaming e quotas separadas para encoded, logical, allocation, depth e ratio. Offset/progress errors são tipados. Cancellation não desfaz bytes committed. Dictionary/state tem owner explícito. Codec/schema e compression transform têm identity e limits distintos | `Codec<T>` universal reflection-driven, primitive/syntax nova, filename/magic/locale/env inference, quota compartilhada ou rollback de bytes committed | codecs, compression providers, compiler, runtime, fuzz e estudos continuam missing |
| W-1458 | Crypto passa por package/provider capability ligada pelo deployment. Algorithm/profile são typed e pinned. Secret/key handle é opaque, move-only e nonextractable por default, com purpose/audience/generation scope. Lifecycle tem dois caminhos: acquire→active→revoking→revoked→released para revoke/rotation, ou acquire→active→expired→released para expiry. Revoke fecha nova admission e drena operações admitidas. Host controla rotation/expiry/zeroization. Secret não entra em wire/storage/log/diagnostic/receipt | `std.crypto` universal, vault/global lookup, plaintext/env lookup, secret no wire, algorithm string, fallback ou downgrade | provider, compiler, runtime, HSM/keystore, rotation, zeroization, FFI e estudos continuam missing |

AEG0 manteve `Research=0` no snapshot até W-1459. O oracle aceita os seis casos correntes (dois paths de
crypto) e rejeita oito rotas contrafactuais. Mutation guards cobrem authority ambiental,
fallback, plaintext/serialization, codec inference e source/digest stale. A
máquina é host-only. Ela não afirma execução W nem readiness de provider.

### 1.40 SIMD1 — evidência para a baseline portátil

W-1459 fecha a superfície sem importar a semântica de outra linguagem. A
evidência primária orienta alternativas, mas a decisão normativa permanece em
`DESIGN.md`. `Simd<Element, lanes: usize>` mantém label required; a declaração
`SimdMask<_ lanes: usize>` torna o label da mask optional, por isso a aplicação
`SimdMask<16>` é corrente e não uma segunda identity:

- Rust documenta integer overflow e as APIs `wrapping`, `saturating` e
  `overflowing` em [primitive integer](https://doc.rust-lang.org/std/primitive.u8.html).
  A documentação de [portable SIMD](https://doc.rust-lang.org/stable/std/simd/index.html)
  mostra lanes e masks como uma superfície explícita, mas continua instável e
  não define a disponibilidade de W.
- Swift define operações de integer e overflow em
  [`FixedWidthInteger`](https://developer.apple.com/documentation/swift/fixedwidthinteger).
  W usa a separação de policies como evidência de nomenclatura, sem copiar
  overloads, ABI ou disponibilidade de Swift.
- Zig especifica vectors, lanes e operações por lane em
  [Vectors](https://ziglang.org/documentation/master/#Vectors). W preserva a
  sequência lógica e rejeita a promessa de vector width físico.
- LLVM descreve [vector predication](https://llvm.org/docs/LangRef.html#vector-predication-intrinsics)
  e masks como lowering. Isso apoia a separação entre lanes vivas, tail e
  backend. Não é autoridade para a safe memory boundary de W.

O caso concreto do Última Luz é uma varredura de delimitador em menus de
`16...32` bytes. O chunk completo usa `load`, o restante usa `loadPartial` com
fill igual ao delimitador e a mask impede que inactive lanes contem. O mesmo
caso cobre `|`, `&`, `countTrue` e um swizzle com índice duplicado. O oracle
host deriva todos os valores a partir dos bytes e das policies. Ele rejeita
expected caller-owned, leitura OOB, write antes do bounds failure, lane count
fora de `1...64`, índice de swizzle inválido, reduction fora da ordem e
`select` short-circuit. A propriedade de `storePartial` (borrow/inout,
preflight e inactive OOB) é verificada somente pelo host oracle; o witness
Last Light não alega uma execução de store.

As APIs de mask são `splat(Bool) -> SimdMask<N>`,
`fromArray([Bool; N]) -> SimdMask<N>` e `toArray() -> [Bool; N]`, todas sem
allocation; `all()`, `any()` e `none()` retornam `Bool`, e `countTrue()` retorna
`UInt`. Integer reductions usam os
nomes fechados `reduceAdd`, `wrappingReduceAdd`, `saturatingReduceAdd`,
`reduceMultiply`, `wrappingReduceMultiply`, `saturatingReduceMultiply`,
`reduceBitAnd`, `reduceBitOr` e `reduceBitXor`, sempre na ordem `0..N-1`.
Float reductions usam `reduceAdd(mode:)` e `reduceMultiply(mode:)` com
`ReductionMode` nominal obrigatório. Omissão, forma posicional, label desconhecido
ou aridade errada de `mode:` usa `W-LABEL-0005`; repetição de `mode:` usa
`W-LABEL-0006`. Strict é left fold, reproducible v1 usa árvore binária balanceada
target-independent e fast segue o float contract sem exigir bit equality entre
backends. Arithmetic, bitwise, shifts e policies só existem quando o
scalar Element admite a operação; floats não ganham bitwise, shifts ou
`overflowingX`.

**Amendamento de W-392:** o contrato de bits agora publica a matriz exata.
`add`, `subtract`, `multiply`, `negate` e `power` têm as quatro families
`checked`, `wrapping`, `saturating` e `overflowing`. `divide` e `remainder`
somente têm `checked`; divide rejeita `signed.min / -1`, mas remainder rejeita
somente divisor zero e `signed.min % -1` produz `0`. Left shift tem `checked`,
`wrapping` e `masked`.
Right shift tem `checked` e `masked`, com `logicalShiftRight` nomeado. Não há
wrapping/saturating/overflowing divide, remainder ou right shift. Rotations
reduzem count módulo width, `saturatingNegate` clampa unsigned para zero e
`overflowingX` devolve low wrapped bits mais flag. Isso substitui a frase aberta
“nas quais a policy tem significado” sem alterar os tokens correntes.

`euclideanDivide` e `euclideanRemainder` fecham a alternativa matemática sem
alterar `/` ou `%`. A escolha de `T` como retorno mantém a API associada ao
integer e evita criar uma tupla de quociente e remainder. A validação usa ambos
os sinais do divisor, mantém a falha `min / -1` somente para divide e conserva
`min % -1 == 0`.

SIMD1 é `oracle-backed-current` e não é implementação. Compiler, runtime,
provider, native acceleration, ABI, FFI, measurements e estudos humanos/modelos
continuam missing. No limite W-1459, `Research=0` permaneceu; as gates posteriores
W-1471, W-1473, W-1474 e W-1475 não pertencem a SIMD1.

## 2. Proveniência

A consolidação de 27 de julho de 2026 foi uma tentativa intermediária. Ela não
foi um design concluído. Esta tabela existe somente para explicar mudanças
rastreáveis.

| Tema | Forma histórica | Forma vigente |
|---|---|---|
| unit literal | `9.81[m/s^2]` | `9.81<m/s^2>` |
| namespace import | `import path [as alias]` | `import localName from modulePath` |
| refinement | `T where predicate`, com alternativas | `T<(predicate)>` |
| execution preference | superfície aberta | `async/spawn<.domain>` |
| entry | superfície aberta | forma curta + descriptor tipado |
| tensors | nested baseline, operadores abertos | nested + `@`, broadcast explícito |
| value generics | aberto | `name: Type` value parameters e labels declarados |
| unsafe | decisão sem grammar completa | `unsafe fn` e `unsafe {}` |
| async cleanup | lacuna | `defer async` |
| scalar literal | lacuna | `'x'` e `b'x'` |
| modules multi-file | header e builder concordam | mesmo nome explícito dentro do pacote |
| resolver/digest | ratificada, com docs divergentes | contrato consolidado |
| pointer tagging | mecanismo de memória candidato | otimização de representação com fallback |
| bootstrap | seed C e self-host cedo | profile W0 fechado antes de tasks |
| static contract | aplicações pontuais | envelope `<...>` fechado por head |
| ilha multilíngue | `fn<lang>` em pesquisa | adapter externo, façade C e static archive |
| callable | `CallbackType` e capture dispersos | `fn`, `some fn` e `any fn` separam pointer, ambiente, owner e drop |

Estas mudanças são experimentais. A fotografia da consolidação continua
acessível no
[arquivo histórico](history/archive/db1-2026-07-27)
e no histórico do Git.

## 3. Ledger

Esta tabela é o checklist de revisão humana. **Forma vigente** significa
“integrar e experimentar”, não “decisão irreversível”.

### Terminologia aposentada

As ocorrências históricas `T0`, `T1` e `T2` nesta seção são rótulos retirados.
Elas preservam proveniência de decisões antigas. A standard library atual usa a
policy plana por módulo, capability, target facts, provider e reachability.

| ID | Tema | Forma vigente | Alternativas preservadas |
|---|---|---|---|
| W-001 | função | `fn name(...): Return` | `func`; retorno `->`; sem keyword |
| W-002 | bindings | `const`/`let`/`var` | `let mut`; uma única keyword |
| W-003 | modifiers | ordem fixa antes de `fn` | ordem livre; effects após retorno |
| W-004 | labels (retired) | regra inicial: primeiro posicional e demais nomeados; W-1290 substitui por parâmetros comuns posicionais em qualquer índice e labels declaradas explicitamente | todos nomeados; inferir label pela posição |
| W-005 | closure | `(args) => body` | `fn(args) {}`; `{ args in }` |
| W-006 | capture | inferência + `<[mode name, ...]>` contextual, com migração pré-1.0 | `capture(...)`; `[capture]`; somente inferência |
| W-007 | visibility | módulo default; `export` individual ou coletivo; `package` não é access modifier | package visibility; `public/private` |
| W-008 | import seletivo | `{X} from module`; `as` somente dentro das braces | `X from module`; `path.{X}`; imports livres |
| W-009 | import de módulo | sem `from` achata; `name from origin` cria binding de módulo | namespace sempre; default export; `as` fora das braces |
| W-010 | módulos | filename default; header `module`; multi-file explícito; DAG | manifest como única origem; extensão entre packages |
| W-011 | runtime top-level | declarations/const somente | init global; ordem de inicializadores |
| W-012 | tipos nominais | `type X = T` | wrapper struct; `newtype` |
| W-013 | alias | `alias X = T` | `typealias`; context-dependent `type` |
| W-014 | refinement | `T<(.member predicate)>`; range como sugar | `value.member`; `T where (...)`; `T(where:)` |
| W-015 | value generics | value parameters e labels | positional only; contrato universal aberto |
| W-016 | existential | `any P` | `P` sozinho; `dyn P`; `Any` universal |
| W-017 | opaque type | `some P` em local, retorno e parâmetro generic anônimo | existential; generic nomeado |
| W-018 | reflection | `reflect.Reflectable` opt-in e alcançável | metadata universal; annotations |
| W-019 | Option | `T?` com some/none | null; sentinel; result-like |
| W-020 | conversão | total, única e sem perda | tudo explícito; promotions amplas |
| W-021 | owner | único/move-first | ARC universal; GC |
| W-022 | borrow | `ref` e `inout` | lifetime annotations públicas; pointers |
| W-023 | transfer | last-use + `take` obrigatório na API | move sempre explícito; move implícito amplo |
| W-024 | copy | implícito só para `Copy`; `copy value` explícito usa `Duplicable` | `.clone()` universal; COW como contrato |
| W-025 | shared | declaração `shared T` cria o primeiro owner em binding/field; expression/return criam binding local `shared` e usam `take`; `copy` é explícito | `share`/`try share` como caminho corrente; `tryShare` separado; ARC implícito; promotion por call/return/inference; block-region-only (retired) |
| W-026 | region block (retired) | syntax `region name(using:, limit:)` liderava e baixava para `Arena`; o bloco e a API histórica foram retirados antes de W 1.0, sem compatibilidade | lifetime annotations; heap por módulo; API sem bloco; açúcar lexical para Arena |
| W-027 | allocator | capability explícita, default fixado pelo product, system portátil e profile substituível | mimalloc universal; allocator por import; default thread-local mutável |
| W-028 | OOM | fallible explícito; geral aborta boundary | throws universal; abort de process sempre |
| W-029 | layout | W opaco; C/schema explícitos | layout W estável universal |
| W-030 | tagged values | otimização invisível com fallback | tagged address obrigatório; annotation |
| W-031 | property behavior | `var Behavior name` | prefix before var; `by`; wrapper type |
| W-032 | behavior composition | composite nomeado | lista ordenada; nesting arbitrário |
| W-033 | erro | `throws E` + `try` | exceptions abertas; Result em toda assinatura |
| W-034 | error widening | case único compatível | mapping sempre explícito; `From` livre |
| W-035 | panic | encerra a fault boundary física mais próxima | unwind recuperável; tratar toda isolation como fault boundary |
| W-036 | async cleanup | `defer async` | RAII sync only; `using`; cleanup solto |
| W-037 | concorrência | initializer `async` | Future/Promise; task API somente |
| W-038 | paralelismo (retired) | forma anterior; W-1161/W-1162/W-1172 fecham `spawn<domain>` como dispatch explícito | mesma keyword de async; parallel loop apenas |
| W-039 | execution domain (retired) | forma anterior `async/spawn<.domain>` sem label; W-1160/W-1162 aceitam também `<domain: .name>` no mesmo slot | alias duplo no mesmo slot; `on .name` (**Rejeitado por enquanto**); descriptor-only |
| W-040 | Task | linear, lexical, one-await | Future clonável; detached default |
| W-041 | grupos | lexical e bounded | queue ilimitada; thread pool exposto |
| W-042 | solicitação de cancelamento | `task.cancel(reason:)` intrínseco; reasons do caller são separados de deadline, budget e saída estrutural | statement `cancel` (**Rejeitado por enquanto**); budget como reason; async thread cancellation |
| W-043 | erro concorrente | primário lexical + anexos | primeiro a concluir; aggregate always |
| W-044 | atomics | `var atomic`, seq-cst comum e contratos estáticos de order; detalhes W-440–453 | C-like default; wrapper obrigatório; lock oculto em `var` comum |
| W-045 | nomes de mobilidade | `transferable`/`shareable` derivados; detalhes em W-424–429 | `Send`/`Sync` públicos; runtime checks |
| W-046 | service | keyword + protocol + closed turn | object+metadata; actor reentrant |
| W-047 | service call | ServiceRef sempre async | local sync/remoto async; RPC explícito |
| W-048 | mailbox | bounded por itens, bytes e trabalho em voo; detalhes em W-458–472 não mudam a call boundary | drop; unbounded; tratar como channel local |
| W-049 | entry curto | `entry { ... }` usa o default slot único | main mágico; manifest-only |
| W-050 | callbacks de host | product liga slots ABI estáticos; registries runtime tratam eventos mutáveis | body key/value; anonymous base; um mecanismo para todos os lifecycles |
| W-051 | units | `9.81<m/s^2>` | `[]`; `{}`; whitespace SI |
| W-052 | custom unit | `dimension`/`unit` declarations | wrapper types; runtime registry |
| W-053 | affine/log units | metaconstrutores distintos | scale universal; runtime-only |
| W-054 | range | quatro closures; unilateral em argumento/pattern; intervalo | dois ranges; producer universal |
| W-055 | membership | `value in (a, b)` | `.isOneOf`; equality chain |
| W-056 | exponent | `**`; `^` somente em unit grammar | `^` universal; `pow` only |
| W-057 | integer safety | checked, panic; APIs alternatives | wrapping default; Result operators |
| W-058 | float | IEEE strict default | fast default; build-mode semantics |
| W-059 | String | owned UTF-8 contíguo | tree/rope default; COW contract |
| W-060 | String indexing | access mode `view`, sem `string[i]` | scalar index; grapheme index default |
| W-061 | raw string | `#"..."#` | `r"..."`; backtick |
| W-062 | scalar/byte | `'x'` e `b'x'` | constructor only; char=grapheme |
| W-063 | arrays/maps | `[]` e `[key: value]` | braces para map/set |
| W-064 | matrix literal | nested arrays | semicolon/whitespace; constructor only |
| W-065 | matrix multiply | `@` | `*` + `.*`; `matmul` only |
| W-066 | broadcast | diferente shape explícito | Array API implicit; dotted operators |
| W-067 | device | transfer explícita | automatic placement |
| W-068 | SDK | stdlib plana por módulo e capability | packages somente |
| W-069 | prelude | pequena, edition-frozen | toda std implícita; nada implícito |
| W-070 | print | contextual ao host | intrinsic; `io.print` obrigatório |
| W-071 | C | `foreign c` + unsafe wrapper | C superset; generated bridge only |
| W-072 | inline language | `fn<C>` com adapter externo | `fn<lang: .c>`; library import; multi-language v0 |
| W-073 | parser | recursive-descent/Pratt + EBNF | generated parser; Tree-sitter compiler |
| W-074 | editor parser | Tree-sitter projection | compiler CST compartilhada |
| W-075 | IR | W/MLIR antes de lowering | C IR público; LLVM direto |
| W-076 | bootstrap | seed C conservador aceito em modo C11; self-host cedo | TypeScript/Bun; C++ compiler inteiro |
| W-077 | build tool | CMake/Ninja no seed | xmake; custom builder antes do self-host |
| W-078 | packages | unidade de compilação; config tipada e data-only; lock compartilhado | JSON-like; executable manifest; lock opcional |
| W-079 | resolver | determinístico, uma versão por identity em cada resolution realm | múltiplas versões no mesmo realm |
| W-080 | artifact | source-first, static preferred | binary-only; dynamic-only |
| W-081 | canonical bytes | deterministic CBOR | WLO imediato; JSON assinada |
| W-082 | digest | tagged SHA-256 inicial | hash fixo eterno; hash recebido sem metadata |
| W-083 | registry | metadata authority; mirrors por digest | registry hospeda tudo e define trust |
| W-084 | evidence | eixos separados | selo único; estrelas |
| W-085 | resource lens | facts/estimates/measurements | número exato universal; nada no import |
| W-086 | formatter | 120 colunas e uma forma | user-configurable style amplo |
| W-087 | tests | runner único com modos | ferramentas sem grafo comum |
| W-088 | AI | schemas/diagnostics comuns | dialeto AI; token count como objetivo único |
| W-089 | SQLite | durable adapter T2 | storage universal |
| W-090 | sandbox | capability + process/OS/Wasm | seccomp por módulo |
| W-091 | wRPC/wQL | packages após core | keywords W; protocolo universal |
| W-092 | WLO/tree strings | WLO é uma família de codecs data-only com perfil explícito; WLO1 fecha `wlo.string.v1` em CBOR determinístico bounded, enquanto `String` permanece UTF-8 contíguo e tree/rope/interning são especializados e Rejeitados como default | codec universal automático; header no payload; tree/rope/interning como representation authority |
| W-093 | GPU/HDL | lowerings posteriores | requisito da v0 |
| W-094 | custom operators | rejeitado | precedência e operators do usuário |
| W-095 | annotations/macros | rejeitado na v0 | `@annotations`; macro AST universal |
| W-096 | portal | gerar após design freeze; protótipo congelado | páginas manuais; escolher Astro agora |
| W-097 | aplicação `<...>` | contrato fechado por head e payload tipado | slots universais; mapa aberto |
| W-098 | campos | imutável sem prefixo; `var` para mutation | `let` obrigatório; `let` opcional |
| W-099 | collection dinâmica | `Array<T>`, `Map<K, V>` e `Set<T>` | `[T]`; braces para map/set |
| W-100 | tensor indexing | `tensor[i, j]`; prefixo retorna view | nesting obrigatório; método `at` |
| W-101 | recurso async | `defer async`, scope estruturado ou `take async fn`; sem obrigação linear universal | async destructor; task detached no drop; lint como semântica |
| W-102 | receiver | `fn` borrow, `mut fn` exclusivo, `take fn` owned, `static fn` sem receiver | `self`; inferir static; função livre |
| W-103 | camadas de memória | semântica separada de lowering, representação e host | tag ou allocator como semântica |
| W-104 | borrow suspenso | permitido somente com owner, frame e alias provados | proibir sempre; lifetime annotation |
| W-105 | pinning | interno sem annotation; `pin` explícito produz `Pinned<T>` público | annotation universal; raw pointer |
| W-106 | ciclos shared | `weak`, close, região ou lifecycle owner; sem collector default | cycle collector universal |
| W-107 | pointer provenance | `Address` observa index bits do mesmo space; `withAddress` preserva a origem do receiver; sem exposed provenance na v0 | pointer como integer; `Address.toPointer` |
| W-108 | origem de allocation | owner/control block/side table preserva deallocator, instance lifetime, mobility e adoption family | bits do pointer obrigatórios; `free` universal |
| W-109 | compactação | portátil → niche → low-bit; high-bit ausente da baseline e sempre com fallback | tagged address obrigatório; high-bit como carrier público |
| W-110 | hardening | sanitizer, PAC, MTE e capability têm precedência | compactação vence o profile |
| W-111 | subset self-host | profile `bootstrap.w0` fechado | compiler exige a linguagem inteira |
| W-112 | seed output | W0 para subset C conservador, backend normal W/MLIR | exigir features C11; MLIR completo no seed; C como backend público |
| W-113 | momento do self-host | depois de memória/FFI e antes de tasks | somente após o design completo |
| W-114 | cláusula estática | `<...>` no source e record tipado na HIR | `where`/`on`; modifier map |
| W-115 | slots angulares | schema declara posição, labels e slot primário | inferir slot pelo nome do enum case |
| W-116 | evolução self-host | gates SH0–SH7; cadeia seed C → A → B → C → D; oracle rejeita dependência de extended e drift fora de target metadata | marco único; compiler usa todo o design vigente; aceitar igualdade de payload sem comparar HIR |
| W-117 | eixos de execução | lifetime, intent, preference, isolation e affinity separados | thread group único |
| W-118 | início de child | initializers `async`/`spawn` iniciam no binding | lazy no primeiro await |
| W-119 | task longa | owner runtime explícito; sem detached sem owner | drop destaca; task global |
| W-120 | outcome de task | body settled, cleanup e só então success/error/canceled observável; panic encerra fault boundary | outcome antes do cleanup; cancel em `E`; panic como Result |
| W-121 | seleção de error | ordem lexical declarada | primeira completion sempre vence |
| W-122 | cancelamento | cooperativo, idempotente, snapshot bounded e sem rollback ou shield geral | matar thread; transação ou shield implícito |
| W-123 | resolução de domain | isolation/affinity vencem preference | contrato do caller substitui isolation |
| W-124 | grupos dinâmicos | concurrent/parallel map bounded e ordering explícito | queue ilimitada; intent oculto |
| W-125 | stream/channel | pull single-pass e MPSC bounded; detalhes em W-454–472 | generator unbounded; channel bidirecional universal |
| W-126 | memory model | safe W é data-race-free e SC sem orders fracas; orders explícitas seguem outcomes C++20 fechados pela superfície W | race definida em safe code; somente “thread-safe” nominal; chamar toda execução race-free de SC |
| W-127 | FFI concorrente | metadata conservadora e callback em executor conhecido | assumir non-blocking |
| W-128 | async lowering | invariantes W antes de MLIR Async/LLVM coroutine | backend define semantics |
| W-129 | lifecycle de instance | identity + generation; restart invalida state anterior | reuse de pointers/frames |
| W-130 | admission | quotas de itens, bytes e in-flight | unbounded; limite só por item |
| W-131 | falha de call | `E` e `ServiceFailure` são effects separados | transporte dentro de todo `E` |
| W-132 | call cycle | ancestry causal rejeita ciclo closed-turn conhecido | esperar somente deadline |
| W-133 | output durável | `PRC0-W-133-current` e `PRC0-W-133-adversarial` fecham a decisão de design com journal input/outcome, runtime closure, turn delivery e rejeição causal; autoridade é o caso PRC0 e evidence permanece host design-oracle | implementação/provider durable-recovery fica W-1442; não alegar compiler/runtime/provider |
| W-134 | scheduler de teste | clock/I/O/schedule injetáveis e replay | teste somente por timing real |
| W-135 | payload de service | value/`take`/capability; sem `ref`/`inout` do caller | borrow no fast path local |
| W-136 | paralelismo de service | instances keyed; mesma key serial | singleton longo; reentrância implícita |
| W-137 | RPC encadeado | expressão `pipeline` explícita é requisito; o bloco fecha um DAG estático | toda `ServiceRef` vira Promise lazy; builder runtime |
| W-138 | payload angular | `()`, `{}` e `[]` são expression, record e list | três operadores universais |
| W-139 | extensão de tipo | refinement, extension, struct, enum e C union separados | `T<{...}>` universal |
| W-140 | foreign artifact | unit agrupada, archive/object e façade C | archive por função; C source obrigatório |
| W-141 | foreign parser | body opaco entregue ao adapter da linguagem | parser W interpreta subset externo |
| W-142 | foreign delimiter | body braced com scanner do adapter | raw fence hash; parser W conhece strings externas |
| W-143 | language tag | `LanguageAdapterId` fixada no lock | enum eterno no compiler; string ou command livre |
| W-144 | referência contextual | `.member` usa subject ou enum esperado; HIR qualificada | somente `value.member`; `.case` apenas |
| W-145 | generic refinado | `Array<T><(predicate)>` separa aplicação e refinement | `Array<[T, predicate]>`; slot misto |
| W-146 | unit e bottom | `()` e `Never` | `Void`; `!`; retorno omitido dependente do contexto |
| W-147 | retorno fluente | `: self` explícito como reborrow | retorno `self` implícito; `Self` owned; builder externo |
| W-148 | associated member | `const`, `static fn` e `type` requerido | companion object; metatype runtime obrigatório |
| W-149 | associated type witness | `type Name` exige `alias Name = T` | `associatedtype`; `type Name = T` contextual |
| W-150 | mutable type storage | ausente; owner de `entry` ou service explícito | `static var`; módulo singleton |
| W-151 | object singleton | `object` permite várias instances; singleton é composição | object declaration singleton; module singleton |
| W-152 | construção | `Type(...)` baixa para `construct`; sem promessa de placement | `new Type`; literal `Type {...}` |
| W-153 | initializer sintetizado | struct usa menor nível; object fica no módulo | visibilidade do tipo sempre; sempre privado |
| W-154 | initializer customizado | vários `init` com formas disjuntas; `throws E`; factory nomeada | initializer único; `init?`; `async init` |
| W-155 | definite initialization | duas fases; sem uso de `self` parcial; cleanup por field | zero universal; runtime check; partial safe value |
| W-156 | computed property | `name: T { get }`; `var` exige write accessor | getter implícito; method obrigatório |
| W-157 | efeitos de property | property-safe, síncrona, local e sem `throws` | `async`/`throws` property; custo irrestrito |
| W-158 | mutation de property | `set(value)` e `modify` com `return inout` escopado | get-modify-set implícito; observers |
| W-159 | property requirement | `{ get [set] [modify] }`; stored field pode ser witness | protocol exige storage; reflection estrutural |
| W-160 | struct transparente | sem `init`: stored fields herdam visibilidade do tipo | `export` por field; todos os members herdam |
| W-161 | struct encapsulado | `init` explícito restaura default de módulo nos fields | keyword `opaque`; field sempre público |
| W-162 | object | storage e initializer sintetizado ficam no módulo | herdar visibilidade do object; constructor público |
| W-163 | enum e protocol | cases são membros do enum; protocol refinement agrega requirements; witness não repete modifier | `export` repetido; todos os members públicos; herança de implementação ou storage |
| W-164 | service | storage nunca cruza módulo; API usa protocol async | field público; computed property remota |
| W-165 | interface exportada | signature não expõe tipo menos visível; HIR normaliza | lint apenas; defaults preservados na HIR |
| W-166 | pattern de struct | `Type(field, field: pattern, ...)`; nominal e ordenado | `{field}`; tuple posicional |
| W-167 | evolução de pattern | `...` obrigatório fora do package | exaustivo externo; modifier no tipo |
| W-168 | ownership de pattern | modo uniforme owned, `ref` ou `inout` | qualifier por field; partial move |
| W-169 | limite de destructuring | struct visível; object e service rejeitados | destructuring estrutural universal |
| W-170 | evolução de struct | field com default é minor se a resolução não muda; field obrigatório é major | todo field novo é major |
| W-171 | evolução de enum | enum fechado; case novo é major | `nonexhaustive`; default case obrigatório |
| W-172 | source contra schema | source, ABI e wire evoluem por contratos separados | derivar schema do struct |
| W-173 | verificação SemVer | `w interface diff` classifica e sinaliza revisão | revisão manual; só major/minor binário |
| W-174 | consuming receiver | `take fn`; call usa `(take value).method()` | consumo implícito; `consuming fn`; free function |
| W-175 | saída consuming | success, error e cancellation consomem; owner pode ser retornado | restaurar no error; abortar sem drop |
| W-176 | authority de `deinit` | exclusivo e não consuming; mutation sem move | borrow read-only; consumir fields |
| W-177 | supressão de drop | ausente em safe W; wrapper mantém estado válido | `discard self`; `forget` geral |
| W-178 | limite de receiver | protocol exige mode exato; service e handles aliases não usam `take fn` | adaptação com copy; service consuming |
| W-179 | `deinit` e copy | tipo com cleanup customizado não atende a `Copy` | copiar e contar drops; lint |
| W-180 | identidade de overload | owner, nome e forma de call | tipos, return type ou constraints |
| W-181 | resolução de overload | forma antes do type-check; sem backtracking | ranking de melhor candidato |
| W-182 | defaults e overload | famílias de formas devem ser disjuntas | preferência por menos defaults |
| W-183 | ownership do overload set | um owner; imports não fundem sets | overload set aberto entre módulos |
| W-184 | overload como valor | closure explícita seleciona a forma | expected type; seletor de forma |
| W-185 | vários initializers | labels e formas disjuntas | ranking por tipos; initializer único |
| W-186 | delegação de initializer | `self = Type(...)` antes de qualquer field | `self.init`; delegação parcial |
| W-187 | falha de initializer | cleanup parcial; `deinit` após self completo | zero universal; leak parcial |
| W-188 | efeitos de initializer | síncrono; `throws E`; sem `init?` | `async init`; initializer failable |
| W-189 | evolução de overload | set existente: minor; primeiro overload: major; forma alterada: major | classificação somente por nome |
| W-190 | ordem de argumentos | ordem da declaração; labels não reordenam | named arguments livres |
| W-191 | parâmetros rest | `T...` homogêneo e final; `each` expande collection | somente collection; type pack; C varargs |
| W-192 | function type | source usa `fn(A): B`; labels e defaults ficam na declaração | labels no tipo; somente inference |
| W-193 | callable concreto | `some fn(A): B` preserva tipo, captures e specialization | generic nomeado; `fn` sempre apagado |
| W-194 | callable apagado | `any fn(A): B` guarda owner, invoke, drop e allocation origins; contextual erasure usa policy normal e `try erase(..., using:)` cobre recovery | `CallbackType`; box manual; allocation ambiental; operação explícita obrigatória para todo rvalue |
| W-195 | callable mode | `fn`, `mut fn` e `take fn` descrevem uso do ambiente | `Fn`/`FnMut`/`FnOnce`; inferência sem annotation |
| W-196 | call por valor | posicional, aridade completa e sem defaults | labels cosméticos; labels significativos |
| W-197 | capture e escape | HIR registra place, modo, lifetime, owner e drop | capture sempre weak; heap por default |
| W-198 | method reference | closure explícita mostra receiver e ownership | bound method implícito |
| W-199 | callback C | `unsafe fn<abi: .c>` fino + context/owner explícitos | converter closure W; callback universal |
| W-200 | static list | `StaticList<T>` compile-time, ordenada e apagada | named index runtime; set implícito |
| W-201 | operador `@` | família rank-1/rank-2 sem broadcast; APIs nomeadas para rank maior | contração geral implícita; `*` linalg |
| W-202 | exemplo normativo | cada contrato aponta para exemplo válido, erro ou cenário canônico | afirmação sem evidência local |
| W-203 | opaque parameter | `some P` é generic anônimo e especializado | exigir generic nomeado; existential |
| W-204 | switch | expressão exaustiva, sem fallthrough ou `break` | switch statement; fallthrough explícito |
| W-205 | ordem de case | ordem lexical, first-match e diagnostic de inalcançável | exigir patterns disjuntos; ranking |
| W-206 | múltiplos scrutinees | tuple subject e tuple pattern | `switch a, b`; matching relacional implícito |
| W-207 | custom pattern | dispatch de custom pattern é Rejeitado; conversão nomeada e guard permanecem vigentes | handler arbitrário; protocol de pattern na v0 |
| W-208 | callable transfer | `fn` é transferível/compartilhável; closure deriva predicates do ambiente | `Send`/`Sync` nominais; confiar no pointer |
| W-209 | compatibilidade callable | signature invariável; somente callable-mode possui lattice | variance; effect widening; ranking |
| W-210 | semântica de String | owner único, bytes UTF-8 contíguos e mutation exclusiva; static/SSO ficam internos | tree/rope default; UTF-16; COW baseline |
| W-211 | unidades e custos | sem `length`; bytes O(1), scalars/graphemes podem ser O(n) | grapheme default; cache obrigatório |
| W-212 | elementos de texto | `UnicodeScalar` Copy e grapheme como `view String` refinada; owned usa String refinado | Character/Grapheme nominal; scalar chamado Char |
| W-213 | índices de texto | origem borrowed, custo visível e uso terminal em edição | ordinal em subscript; índice universal |
| W-214 | slices de texto | byte slice é `view Bytes`; byte range para `view String` é fallible | arredondar boundary; slice sempre String |
| W-215 | Bytes | tipo binário owned distinto de `String` e `Array<u8>` | alias de Array; String aceita UTF-8 inválido |
| W-216 | conversão UTF-8 | strict, repair, borrow, copy e adoption explícitos; detalhes W-358–362 | replacement implícito; locale codec default |
| W-217 | construção de String | interpolation e Display escrevem num `String`; `+` consome left; reserve/append lideram loops | builder público; concat adjacente; String intermediário por campo |
| W-218 | raw/multiline | `#"..."#`, `${}`, multiline com dedent determinístico | hashes arbitrários; `r` prefix; três delimitadores equivalentes |
| W-219 | byte string | `b"..."` produz Bytes ASCII/escapes, sem interpolation | Unicode direto; Array literal somente |
| W-220 | igualdade Unicode | sequência exata; normalização e collation nomeadas | equivalência canônica em `==`; locale global |
| W-221 | bundle Unicode | edição, profile, tabelas e digests fixos; o seed pinou Unicode 17.0.0 para `XID_Start`/`XID_Continue` sem `Default_Ignorable_Code_Point`, com `_` adicionado ao início | versão do host; ICU obrigatório |
| W-222 | texto do host | `OsString`, `Path`, `Utf8Path` e `PackagePath` distintos; colisão NFC rejeitada | paths sempre String; bytes portáveis do OS |
| W-223 | C strings | `CString`/view separados, NUL verificado e inbound bounded | String sempre NUL; scan C ilimitado |
| W-224 | storage textual | refinement não fixa layout; reserva mínima é operação; W-1276 separa limite, resource gate e boundary | capacity pública; SSO observável |
| W-225 | estruturas textuais | rope, piece table, interning e tree string são especializadas | tree string geral; representation ABI única |
| W-226 | ordem de avaliação | esquerda para direita e sequenciada; formas condicionais short-circuit | ordem não especificada; optimizer escolhe |
| W-227 | resultados borrowed | `ref`/`inout` em tipos e retorno, provenance inferida e interface registrada | lifetime no source; lookup owned |
| W-228 | array dinâmico | `Array<T>` owned, contíguo, count/capacity O(1) e append amortizado O(1) | linked chunks default; `[T]` |
| W-229 | literais de array | `[a, b]`, `[]` contextual e `[value; count]` fixo com Copy | `[:]`; repeat clona move-only |
| W-230 | views de array | `view Array<T>` read-only Copy e `inout view Array<T>` exclusiva move-only | tipos Slice públicos; pointer público; resize pela view |
| W-231 | iteração | single-pass; borrow default, `ref`/`inout`/`copy` explícitos e `take` consome | copiar sempre; mutation estrutural durante loop |
| W-232 | pipelines | Array eager; `.lazy` e Iterator lazy; `collect()` materializa | tudo lazy; tudo eager |
| W-233 | `Map` | hashing keyed e ordem de inserção estável; full key confirma colisão | ordem de bucket; guardar somente hash |
| W-234 | `Set` | ordem de inserção; equality ignora ordem; sem literal próprio | set não ordenado; literal com chaves |
| W-235 | hashing | `Hashable: Equatable`; algoritmo/seed process-local e não persistente | XXH como ABI; hash como identity |
| W-236 | lookup borrowed | `EquivalentKey<K>` permite view com a mesma equality e hash feed | alocar key em todo lookup; equivalência ad hoc |
| W-237 | ordenação | `sort` stable por default; `sortUnstable` explícito; comparator `Ordering` | algoritmo fixo no contrato; Bool comparator |
| W-238 | maps ordenados | `SortedMap` por total order para range e key order | tornar todo Map tree; B-tree no ABI |
| W-239 | cleanup de collections | ordem inversa de índice/inserção; capacity e buckets invisíveis | drop order não especificada |
| W-240 | escopo da std | core em T0; Deque/PriorityQueue/BitSet em `std.collections`; concorrentes fora de T0 | todas as estruturas no prelude |
| W-241 | duplicação owned | `Copy` barato e implícito; `Duplicable` explícito via `copy value` | clone method; copiar owned implicitamente |
| W-242 | ausência | `Option<T>` com some/none; sem null/undefined universal | sentinela universal; pointer null por default |
| W-243 | estado de memória | definite init e move no compiler; `MaybeUninit<T>` unsafe | gravar none após move; uninitialized como valor comum |
| W-244 | controle Option | `?.`, lazy/right-associative `??` e postfix `?` só para none | force unwrap; postfix `?` para Result |
| W-245 | ownership Option | binding owned por default; `ref`/`inout`/`copy`; `take()` esvazia | copiar payload owned; mutation por optional chain |
| W-246 | Result | enum T0 success/error para storage e composição | Result implícito só em debug; exceptions abertas |
| W-247 | `try` | propaga `throws E` ou `Result<T,E>`; cada closure é outro effect scope | postfix `?` para ambos; propagação implícita |
| W-248 | error type | enum fechado e estruturado; `throws E` sempre tipado | throws sem tipo; string obrigatória |
| W-249 | effect polymorphism | generic `E: Error`; bottom `Never` é aceito e especializa como nonthrowing | keyword `rethrows`; erasure universal |
| W-250 | catch | ordem lexical, guard e exaustividade no contexto nonthrowing | ranking de catches; catch implícito |
| W-251 | uso de valores | todo valor non-unit/non-Never deve ser usado ou descartado com `let _` | annotation must-use; ignorar Result |
| W-252 | lowering de error | tagged result e cleanup edges; trace sidecar não observável | host exception unwind; sem trace estruturado |
| W-253 | fault boundary | process, Wasm instance ou compartment com teardown próprio | service lógico sempre recuperável; panic capturável |
| W-254 | panic | payload limitado, code estável e sem user cleanup garantido | payload alocável obrigatório; user recovery |
| W-255 | OOM | alocação normal pode panic; APIs `try*` retornam AllocationError | toda alocação fallible; emergency handler universal |
| W-256 | cleanup | saídas estruturadas e cancel executam LIFO; panic não garante user cleanup | panic unwind; defer que propaga error |
| W-257 | diagnostic | code estável, spans em bytes, facts e relação root/cascade | texto livre como API; reutilizar code |
| W-258 | fix e policy | edits com applicability/digest; ordem estável; error não suprimível | fix sem precondition; source suppression no design vigente |
| W-259 | `try?` | converte falha recuperável em Option e flatten; não captura panic/cancel | excluir o sugar; `try!`; preservar error oculto |
| W-260 | const context | `const`, value argument, contract, fixed size, unit e refinement exigem avaliação | confiar no optimizer; executar tudo em compile time |
| W-261 | const callable | `const fn` e `const init` explícitos; mesma semântica runtime | inferir API pelo body; função exclusiva da fase |
| W-262 | modifier const | depois de `static`; incompatível com unsafe/async; combina com mut/take | annotation; `comptime fn`; combinação irrestrita |
| W-263 | const-safe | local mutation, loops, recursion, dados e typed errors; sem capabilities/FFI | subset expression-only; executar host code |
| W-264 | fase | sem `isComptime`; mesmo input produz o mesmo valor nas duas fases | branch por fase; implementação separada |
| W-265 | const failure | error não tratado, panic e quota viram diagnostics W-CONST | fault boundary no compiler; AllocationError catchable |
| W-266 | ConstRepresentable | predicate derivado para valores estruturais sem identity/authority | protocol implementável; qualquer tipo serializável |
| W-267 | materialização | const sem owner; uso owned cria valor independente; borrow não escapa | singleton mutable; endereço estável público |
| W-268 | target | evaluator usa target e módulo `w.target`; nunca a máquina host | host semantics; target facts implícitos |
| W-269 | build input | módulo gerado e recipe declarada; sem env/file/clock no evaluator | `#define`; env intrinsic; acesso sandboxed ad hoc |
| W-270 | quotas | steps, heap, depth e result na recipe; wall clock não semântico | quota por source; sem limite; timeout como semântica |
| W-271 | cache const | chave inclui ConstIR, args, target, bundles, evaluator, quotas e generated modules | cache por source text; omitir target |
| W-272 | type builder | identidade declarada + const parse/refinement; sem função que retorna Type | `type(regex)`; type function arbitrária |
| W-273 | geração | ConstIR para ConstValue; codegen em tool target; WLO continua codec | stringify/reparse; macro AST universal |
| W-274 | feedback | PGO declarado só orienta otimização; nunca altera const/tipo/interface | substituir const com execução anterior |
| W-275 | implementação const | evaluator HIR antes de MLIR; folding MLIR não define correção | JIT host; canonicalizer como evaluator semântico |
| W-276 | bootstrap const | CE0 no seed C e core W0; ConstValue normalizado deve coincidir | excluir const fn do seed; evaluator só no compiler final |
| W-277 | force expression | sem `comptime expr` na baseline; binding const nomeia o resultado | keyword obrigatória; const block na v0 |
| W-278 | static argument | predicate estrutural sem float/dynamic collection; serialização canônica na identidade | qualquer ConstValue; somente integer |
| W-279 | const e overload | const não distingue call shape; elegibilidade não promete termination/quota | overload por fase; inferir const por call |
| W-280 | generic kinds | type e value; sem lifetime/effect/HKT/pack no source | kinds extensíveis; template sem kind |
| W-281 | generic labels (retired) | interpretação anterior: type positional; `_ name: Type` criava slot sem label; W-1160 substitui por label opcional `optional(name)` | todos posicionais; named type args |
| W-282 | generic scope | parâmetros entram em scope da esquerda para a direita | lista inteira em scope; forward reference |
| W-283 | protocol composition | `P & Q`, sem ordem e com normalização | `P, Q`; `T<[P, Q]>`; composite sempre nomeado |
| W-284 | generic body | verificado uma vez contra constraints; lookup fechado | template com lookup tardio; verificar só após instantiation |
| W-285 | generic inference | depois da forma de call; argumentos, receiver e expected result; solução única | ranking; busca por tipo conforme; body inference |
| W-286 | explicit generic args | type prefix e value labels podem compor com inference; sem `_` | placeholders; lista completa obrigatória |
| W-287 | primary associated type | protocol head declara projection de `Self`; aplicação restringe o witness | generic protocol por conformance; somente body |
| W-288 | associated witness | `alias` explícito; sem inference/default/GAT no design vigente | inferir por method; associated type default |
| W-289 | coherence | conformance no módulo do type ou protocol; escolha única por par | orphan livre; seleção por import |
| W-290 | conditional conformance | `extension<T: P> Nominal<T>: Q`; sem overlap ou specialization | blanket conformance; prioridade |
| W-291 | default witness | somente o módulo do protocol publica em `extension Protocol`; seleção gravada na conformance | body inline no protocol; extension importada muda witness |
| W-292 | existential compatibility | sem generic method, Self externo ou associated type não ligado | aceitar tudo com traps; banir existential |
| W-293 | existential opening | `any P` não conforma a P e não abre implicitamente | self-conformance; implicit opening |
| W-294 | opaque identity | `some P` preserva um tipo por instantiation; occurrence de parâmetro é independente | existential; união de returns |
| W-295 | generic lowering | monomorphization, shared body e witness são escolhas equivalentes | monomorphization universal; erasure universal |
| W-296 | generic interface | signature, witness requirements e HIR generic por digest/CAS | reparse de source; somente machine code |
| W-297 | generic termination | grafo finito, quotas de instance/depth e cache completo | expansão sem limite; timeout semântico |
| W-298 | generic variance | type constructors invariantes por default | variance inferida; covariance de Array |
| W-299 | bootstrap generics | constraints, primary associated types, coherence e monomorphization; sem any/some | seed sem protocols; runtime dictionaries |
| W-300 | enum subset | enum possui slot primário `cases`; `Enum<[.a, .b]>` | enum base + guard; anonymous union |
| W-301 | subset normalization | conjunto por ordem de declaração; duplicata/empty rejeitados; all vira base | StaticList ordenada na identity |
| W-302 | subset conversion | subset→superset/base implícito; base→subset checked | cast implícito nos dois sentidos |
| W-303 | subset flow | switch usa case-set e flow narrowing elimina checks | exhaustividade sempre pelo enum base |
| W-304 | subset payload/layout | payload preservado; layout público do enum base; tag interno pode sumir | wrapper/tag novo; payload subset |
| W-305 | subset evolution | retorno widening e parâmetro narrowing são major | qualquer mudança minor; variance automática |
| W-306 | subset de error | `throws Enum<[...]>`; throw e catch usam o case-set publicado | error enum inteiro; effect union separado |
| W-307 | planos de introspecção | interface/HIR para tooling; descriptor opt-in no runtime | runtime metadata universal; debug como API |
| W-308 | type identity | `reflect.TypeId` local ao build; sem persistência ou layout | ID estável global; nome como identidade |
| W-309 | metatype | sem `Type<T>`/`T.type`; generic, factory ou enum | metatype universal; dynamic construction |
| W-310 | reflection trigger | conformance explícita a `reflect.Reflectable`; sem annotation | inferir por uso; decorator; registro manual |
| W-311 | reflection visibility | somente interface exportada e properties lógicas | fields privados; backing storage; getter por string |
| W-312 | reflection reachability | witness alcançável mantém descriptor; sem registry global | todos os conformers como roots |
| W-313 | synthesis trigger | conformance no type head; protocol reconhecido por identidade | `@derive`; macro; nome textual |
| W-314 | synthesis scope | Equatable, Hashable, Duplicable e Reflectable em struct/enum; Reflectable em object; synthesis JSON fechada de W-900 e synthesis `data.Row` fechada de TAB0, ambas protocol-identity-triggered e all-or-none | qualquer protocol; synthesis genérica; Display e codecs automáticos fora de JSON e Row |
| W-315 | synthesis witness | all-or-none por protocol; constraints explícitas | completar witness parcial; inferir constraints |
| W-316 | rest syntax | último `T...`; zero ou mais; um label inicial | `params`; `*args`; overloads por aridade |
| W-317 | rest shape | conjunto infinito deve ser disjunto de todo overload | fixed vence rest; ranking por tipos |
| W-318 | rest binding | `Arguments<T>` não escapante; mode por elemento | Array alocado obrigatório; tuple runtime |
| W-319 | rest expansion | `each collection` somente no argumento final | `values...`; spread universal; expansão implícita |
| W-320 | rest ownership | value/ref/take; sem `inout`; cleanup por elemento | ownership apagado; inout dinâmico |
| W-321 | C varargs | adapter unsafe tipado ou `c.vaList`; rest W não cruza ABI | mapear rest diretamente; promotions implícitas |
| W-322 | formas type-level adiadas | typed property path é provável T2; GAT e heterogeneous packs ficam rejeitados por enquanto | incluir no W0; reflection por string |
| W-323 | resolução de enum case | `.case` exige expected enum; `Enum.case` resolve colisão | escolher por import, frequência ou ranking |
| W-324 | sequência e case-set | o head decide: `StagePath<[...]>` preserva ordem; `Enum<[...]>` normaliza conjunto | tratar toda static list como conjunto |
| W-325 | enum e flags | enum representa uma alternativa; simultaneidade usa Set ou tipo de flags separado | enum com semântica AND/OR contextual |
| W-326 | álgebra de case-set | somente na HIR; source nomeia a lista resultante | operadores públicos de union/intersection/difference no design vigente |
| W-327 | dois estados | enum em storage para runtime; value argument de enum para typestate local | typestate universal; enum runtime universal |
| W-328 | argumento de valor enum | slot primário aceita `.case`; slot normal usa `label: .case` | marker type vazio; string; annotation |
| W-329 | transição typestate | extension especializada + `take fn`; novo tipo no retorno | mudar tipo do binding no lugar; pre/post annotations |
| W-330 | falha consuming | outcome enum devolve cada novo owner; `throws` não restaura owner | rollback implícito; owner escondido no error |
| W-331 | path estático | `StaticList<Enum>` refinada por `const fn`; primeiro edge inválido vira diagnostic | lista sem validação; DSL obrigatória |
| W-332 | estado de service | enum persistido + snapshot revisionado; closed turn por call | `ServiceRef<State>` muda depois da call |
| W-333 | erasure de typestate | envelope enum explícito para collections mistas | `T<?>`; existential implícito; tag escondida |
| W-334 | DSL de transição | sem keywords novas; `StateGraph<E>` const é provável T2 para tooling | `state`/`transition` no design vigente; annotations |
| W-335 | validity e niche | HIR registra bit patterns válidos; niche só representa estados impossíveis | sentinel sem contrato; colapsar estados aninhados |
| W-336 | layout de enum | mapping determinístico; tag explícita é fallback; subset não promete tamanho público | niche obrigatório; wrapper por subset |
| W-337 | low-bit | somente storage interno com alignment real provado e canonicalização nas fronteiras | annotation de source; alignment nominal |
| W-338 | high-bit | profile experimental após negociação completa; ausente do portátil | inferir por CPU; requisito de linguagem |
| W-339 | metadata mutável | count, generation, allocator e deallocator ficam em owner/control block/side table | esconder tudo no pointer |
| W-340 | atomics tagged | operação cobre a palavra inteira; lock-free e ABA exigem provas separadas | atomicidade por associação; generation curta universal |
| W-341 | object header | nenhum header universal; cada ownership/runtime usa metadata necessária | header W em toda allocation |
| W-342 | NaN boxing | ausente da baseline; target optimizer pode testar container interno somente com fallback portátil | reduzir payload ou range do float; carrier público |
| W-343 | boundary de layout | FFI, persistência, address exposure e ABI usam forma canônica ou schema | tag interna cruza a fronteira |
| W-344 | fingerprint de representação | inclui somente validity, bytes, carrier e fatos ABI que mudam a entry; provider, options e patch ficam na recipe | fingerprint por target triple; allocator name sempre entra; compiler completo como proxy |
| W-345 | pointer compression | handle de arena/heap isolado é classe própria com base e bounds | tratar índice como pointer tagged |
| W-346 | início de async (retired) | forma anterior; W-1161 mantém `await` na task atual e children dos initializers `async`/`spawn<domain>` após staging lexical | Promise implícita; body parcial no parent |
| W-347 | contexto de child | cancellation, deadline, trace, budget e preference; user data/capability são explícitos | task-local map mutável herdado |
| W-348 | domains portáteis | somente `.main` é padrão; módulo e pacote declaram os outros IDs | lista global; enum manual; string |
| W-349 | domain schema | capabilities, capacity, fallback, affinity e trace identity | thread/pool como identidade semântica |
| W-350 | defaults de execução (retired) | forma anterior; W-1162 torna placement uma escolha do caller/profile no call site | default em módulo; `.compute` global; herdar domain serial e degradar `spawn` |
| W-351 | domain de módulo | contrato declara IDs; `import domain` importa requirement, não executor | enum manual; import cria queue/thread |
| W-352 | capacity aninhada | groups no mesmo domain compartilham budget; parent aguardando não retém permit | pool por group; produto dos limits |
| W-353 | liveness paralela | simultaneidade nunca é necessária para correção | spin wait entre children; thread por child |
| W-354 | fairness | eventual sob budgets bounded e jobs non-blocking; sem ordem entre siblings | FIFO scheduler como semântica; queue ilimitada |
| W-355 | priority e deadline | priority é policy; deadline monotônico vira cancellation; `Task.withTimeout` cria child lexical | `.background` como domain; priority garante prazo; deadline wall-clock |
| W-356 | executor dinâmico (retired) | direção anterior deixava `ExecutionDomainRef` provável; W-1175 fecha somente lane serial bounded e mantém executor custom em runtime unsafe | detached escondida; protocol comum substitui scheduler |
| W-357 | bytes de String | view read-only; mutação somente por operação que preserva UTF-8 | byte mutation com validação posterior; storage exposto |
| W-358 | conversão UTF-8 | view valida; String copia; adoption transfere carrier sem allocation e devolve o mesmo owner no erro | cópia implícita em todas; reuse opcional |
| W-359 | erro UTF-8 | offset, maximal-subpart length e reason estáveis | byte inválido apenas; mensagem livre; decoder-dependent |
| W-360 | reparo UTF-8 | um U+FFFD por maximal subpart; nunca implícito | um por byte; descartar bytes; replacement configurável global |
| W-361 | UTF-8 incremental | até três bytes pendentes; `finish` decide incomplete; offset do stream | validar cada chunk isolado; buffer sem limite |
| W-362 | BOM UTF-8 | core preserva U+FEFF; adapter nomeado aplica policy | remover sempre; preservar sempre em todo protocolo |
| W-363 | índices de texto | origem emprestada, custo visível e uso terminal em edição | integer offset universal; índice persistível |
| W-364 | grapheme owned | `String<(.graphemes.count == 1)>`; sem `Character` | tipo Character universal; Grapheme owned implícito |
| W-365 | interpolação | um `String` de destino + `Display.write`; `display()` é conveniência | builder público; String intermediário por campo; concatenação implícita |
| W-366 | edição Unicode | bundle e digests no semantic fingerprint; índices não persistem | versão do sistema; boundary congelada no valor |
| W-367 | PackagePath | NFC e colisão normalizada rejeitada | nomes distintos por bytes; escolher o primeiro |
| W-368 | semântica de performance | profiles preservam valor, panic, effects, ownership e numeric policy | release muda overflow/float; optimizer como semântica |
| W-369 | facts de otimização | `ProofFacts` na HIR para interval, case-set, length, shape, alignment e alias | annotations de usuário; confiar só no backend |
| W-370 | predicate opaco | invariant válido; optimizer usa somente fatos extraídos e verificados | SMT obrigatório; ignorar todo predicate |
| W-371 | largura interna | operation, accumulator, SIMD lane e storage são escolhas separadas | menor tipo único para tudo; carrier sempre obrigatório |
| W-372 | resultado refinado | expressão provada satisfaz expected refinement sem check; caso geral é fallible | `try` mesmo com prova; narrowing runtime implícito |
| W-373 | storage estreito | somente não escapante e após cost model; boundaries usam carrier | layout menor público por refinement; nunca comprimir |
| W-374 | custo de texto | complexidade por bytes e unidade explícita; caches/ASCII/SIMD invisíveis | `length` O(1) universal; cache obrigatório no layout |
| W-375 | integer `@` | checked semantics; widening fixo; `matmul<R>` muda redução/resultado | wrap; accumulator sempre igual ao elemento |
| W-376 | float `@` | strict default; fast e reproducible por mode explícito | fast global em release; operator dependente do target |
| W-377 | device e fusion | transfer explícita; fusion pode apagar intermediário, não mover device | auto-transfer; toda operação materializa |
| W-378 | PGO | input por digest só orienta optimization | muda const/interface; profile implícito da máquina |
| W-379 | explicação de performance | facts, decisions, estimates, measurements e missed reasons separados | assembly como única explicação; número exato universal |
| W-380 | proof budget | quotas determinísticas; interval/case-set/shape/alias baseline; SMT geral rejeitado | solver sem limite; timeout como resultado semântico |
| W-381 | gate de otimização | benchmark reproduzível + differential oracle + fallback | microbenchmark único; otimização sem profile portátil |
| W-382 | largura de `Int` | `Int`/`UInt` têm 64 bits; `isize`/`usize` seguem address width | Int segue target; literal default `i32`; BigInt default |
| W-383 | representação integer | widths fixas; signed two's complement; Bool distinto | signed dependente do target; Bool como integer |
| W-384 | token numérico | decimal/binário/octal/hex; exponent decimal; suffix após `_` | suffix colado; trailing dot; hex float |
| W-385 | valor do literal | magnitude/rational exato até expected type; uma materialização | truncar no lexer; converter decimal por f64 intermediário |
| W-386 | defaults de literal | integer `Int`; decimal `f64`; expected type prevalece quando válido | i32 default; BigInt/Decimal default |
| W-387 | tipagem binária | identidade ou uma conversão segura para um tipo operando; sem terceiro tipo | promoções C; ranking de common type |
| W-388 | conversão implícita | total, exata, única e sem authority oculta; refinement pode provar | cast implícito narrowing; exigir todo cast |
| W-389 | conversão explícita | `exactly`, `rounding`, `saturating`, `truncatingBits` e bits nomeados | um cast com policy dependente do par |
| W-390 | overflow integer | operators checked em todo profile; const vira diagnostic | wrap em release; undefined behavior |
| W-391 | divisão integer | zero e min/-1 causam panic; quotient toward zero; Euclidean nomeado | floor universal; resultado Option implícito |
| W-392 | shift e primitives portáveis de bits | count `UInt`; `add`/`subtract`/`multiply`/`negate`/`power` têm `checked`/`wrapping`/`saturating`/`overflowing`; divide/remainder somente `checked`; left shift `checked`/`wrapping`/`masked`; right shift `checked`/`masked` mais `logicalShiftRight`; count inválido e perda à esquerda causam panic; `rotated*` reduz módulo width; `saturatingNegate` clampa unsigned para zero; `overflowingX` devolve low wrapped bits + flag; bitWidth, counts, reverse de bits e bytes são APIs puras, const-evaluable, com zero definido pela largura | matrix aberta; mask do count como regra universal; regras C; wrap silencioso; wrapping/saturating/overflowing divide ou remainder; novos operators ou intrinsics target-specific no core |
| W-393 | float baseline | f32/f64 IEEE strict, nearest-even, subnormal e sem FMA implícito | fast-math em release; flush-to-zero default |
| W-394 | float equality | comparação IEEE parcial; `TotalFloat` para key e ordem total | float conforma aos protocols totais; bit equality como `==` |
| W-395 | modes float | strict default; fast e reproducible explícitos e versionados | flag global muda semântica; reproducible sem algoritmo |
| W-396 | numeric T2 | BigInt/UInt, FixedDecimal, Rational e Complex com custo explícito | número universal; Decimal como Money |
| W-397 | ML storage | f16/bf16 sem scalar operators e com tensor accumulator f32; Quantized separa storage/expressed | aritmética f16 implícita; float8 core |
| W-398 | range | intervalo; quatro closures; reversed vazio; stride para direção/step | range como collection; range descendente implícito |
| W-399 | superfície de pinning | `try pin take value`; `pin` é fallible e separado de `take` | `Pinned.make`; `take<.pin>`; modifier no binding |
| W-400 | saída de pinning | sem `unpin` ou `intoValue` público; drop in-place; boundary unsafe pode reconstruir sob contrato | unpin seguro irrestrito; proof token público |
| W-401 | endian numérico | valor independe de endian; bytes exigem `.little`, `.big` ou `.native` | ordem implícita de persistência; reinterpret seguro |
| W-402 | reals alternativos | Posit, Unum e decimal float rejeitados por enquanto; FixedDecimal, Rational e IEEE binary ficam baseline | número universal novo; trocar IEEE sem oracle/hardware |
| W-403 | construção de String | reserva e mutation pertencem a `String`; sem `StringBuilder` público | builder obrigatório; concatenação repetida |
| W-404 | view genérica | `view T` é access mode vigente; sem família pública `XView` | `Slice<T>`/`Span<T>`; `Readonly<T>` profundo; usar somente `ref` |
| W-405 | placement | sem annotation; local síncrono fixo que não escapa não usa allocator geral | annotation stack/heap; boxing por register pressure |
| W-406 | fato de alocação | HIR/interface registram obrigação; `w explain` e gate usam call graph | effect escrito em cada função; allocation invisível ao tooling |
| W-407 | alocação em region (retired) | somente call com `using: region`; bloco não captura todos os locais | placement lexical implícito; allocator global da região |
| W-408 | escape de arena | inline independente pode sair; storage dependente exige consuming `rehome` | copiar sempre no return; escape unchecked; adoção presumida |
| W-409 | arena e tasks | Arena move-only e não shareable; child paralelo recebe arena filha exclusiva | arena monotônica concorrente default; proibir todo child |
| W-410 | budget de arena | cobra span alinhado, padding, growth retido e drop metadata; host mede resident separado | cobrar somente live payload; usar resident bytes como semântica |
| W-411 | origem | owner preserva instance, lifetime, deallocator, mobility e adoption family; zero-size não aloca | `free` universal; origem em low bits; transfer sempre permitido |
| W-412 | allocator profiles | build profile fixa `.system`, `.none` ou runtime contract; plan fixa provider; mimalloc exige benchmark | override global obrigatório; allocator escolhido por import; path no manifest |
| W-413 | allocation failure | cases estáveis, strong guarantee em `try*` e budget distinto de OOM | tamanho livre global; falha parcial; uma exception universal |
| W-414 | inicialização de storage | safe typed allocation nunca expõe uninitialized; zero é operação/policy explícita | calloc semântico universal; bytes residuais legíveis |
| W-415 | criação shared (refinada) | W-1256 promove somente initializer de storage anotado `shared`; expression/return usam binding local e `take`; SHC0 fecha recovery custom declarativa com `try` fora do tipo | `share`/`try share` como caminho corrente; promotion em argumento/return/overload; wrapper nominal; shared universal |
| W-416 | cópia shared | handles são move-first; `copy` torna retain visível; optimizer pode elidir | shared atende a Copy implícito; retain escondido em assignment |
| W-417 | `ref` versus `view` | `ref` preserva place completo; `view` descreve projeção sem owner/capacity | tratar ambos como pointer + count; view nominal por tipo |
| W-418 | mutation de view | binding/parameter `inout view T`; extent fixo e sem resize; String/CString permanecem read-only | `MutableXView`; mutation por view read-only; copy-on-write |
| W-419 | materialização | `materialize()` normal e `tryMaterialize(using:)` fallible produzem `T` | constructor por família; adoção borrowed; conversão implícita |
| W-420 | escopo de view | core families e Tensor; custom type expõe core view ou borrow nominal | protocol inventa provenance; view automática de todo tipo |
| W-421 | ABI de view | descriptor W por família; C usa pointer + count somente quando contígua | descriptor universal; layout W cruza FFI |
| W-422 | lifetime de view | provenance inferida; `await` exige owner estável; child estruturado termina antes do owner | lifetime annotation; view mantém owner vivo; escape detached |
| W-423 | read-only e imutabilidade | `ref` é acesso read-only; `view` é projeção; imutabilidade profunda é fato inferido sem syntax | `Readonly<T>` universal; modifier `immutable`; `let` promete grafo congelado |
| W-424 | mobilidade pública | facts intrínsecos `transferable`/`shareable`; sem marker protocols | `Send`/`Sync`; `Sendable`; check runtime |
| W-425 | transferência | owner/acesso exclusivo, fields, allocator, cleanup e affinity; origem perde acesso | exigir synchronization para move único; copiar sempre |
| W-426 | sharing | storage vivo e reads sem race; interior mutation precisa de mecanismo verificado | exigir imutabilidade profunda; aceitar todo `ref` |
| W-427 | constraint de mobilidade | `T<(.transferable)>` e `T<(.shareable)>`; omitida é inferida | `T: Send`; `<mobility: ...>`; annotation na declaração |
| W-428 | views e mobilidade | descriptor não prova nada; owner, provenance e lifetime satisfazem o capture | view é Send/Sync por pointer + count; proibir toda view |
| W-429 | FFI mobility | local por default; somente provider/foreign interface publica fato trusted com target, adapter, signature, digest e fatos negativos | raw pointer deriva facts; assertion segura do usuário |
| W-430 | representação W0 de String | literal/static + buffer flat único com pointer/count/reserva/origin; Bytes usa carrier T0 compatível | SSO e COW no bootstrap; rope; runtime Unicode obrigatório |
| W-431 | COW de String | fora da baseline; optimizer exige efeitos de allocation e cleanup não observáveis | refcount em toda String; COW como contrato; proibir otimização |
| W-432 | reserva de String | mínimo total por bytes; exact capacity não é pública; `tryReserve` tem strong guarantee | bytes adicionais; growth fixo na linguagem; capacity property |
| W-433 | mutation de String | append/replace recebem view válida; source do mesmo owner é erro; índices são invalidados | mutable byte view; temporary de alias implícito; byte offsets unchecked |
| W-434 | esvaziar String | `clear` mantém storage; `reset` libera; `takeAll` transfere conteúdo | Boolean `keepingCapacity`; um método ambíguo; builder separado |
| W-435 | String e Bytes | carrier T0 compatível; adoption e `intoBytes` consomem sem allocation geral | layout público igual; cópia obrigatória; cast implícito |
| W-436 | caches de texto | reads não alocam nem mutam; summaries eager permitidos; índice alocante usa tipo próprio | cache lazy invisível; owner muta por read; grapheme ordinal O(1) |
| W-437 | String especializada (substituída) | W-1276 retira `InlineString`; Rope e texto indexado permanecem tipos próprios porque mudam operações | threshold público de SSO; tipo distinto apenas por storage |
| W-438 | ponteiro textual | somente borrow scoped; persistência usa CString ou buffer pinned sem relocation; pin do descriptor não basta | pointer estável de String; NUL obrigatório; raw pointer safe |
| W-439 | String no self-host | flat UTF-8, bytes, append/reserve, views, conversions e ownership; Unicode avançado não bloqueia SH0 | grapheme/locale antes do parser; C runtime de String permanente |
| W-440 | data race | bytes sobrepostos, concorrência, write e ausência de happens-before; atomics concorrentes precisam de extent idêntico; safe W rejeita | race com resultado definido; check somente em runtime; atomic parcial sobreposto |
| W-441 | happens-before | task start/join, channel em W-467, service turn, unlock/lock, release sequence e fence com reads-from atômica | thread start/join somente; cancel publica user state; duas fences sem atomic publicam |
| W-442 | storage atomic | `var atomic value: T` baixa para `Atomic<T>`; acesso comum seq-cst | `Atomic<T>` sempre explícito; behavior Atomic; todo var atomic |
| W-443 | atomic value | fato intrínseco fechado para Bool, integers e enum sem payload; pointer, owner, float, struct e palavra dupla ficam fora da safe std | protocol user-defined; qualquer Copy; `AtomicAddress`; atomic de shared; target raw ampliando `Atomic<T>` |
| W-444 | order | `<.order>` estática; load/store/update usam enum subsets; default `.sequential`; sequential participa de ordem total do scope | argumento runtime; suffix por método; relaxed default |
| W-445 | compare-exchange | result enum; success/failure usam matriz estática; success é RMW e failure é load; weak é explícita | Boolean; expected inout; combinação inválida em runtime; failure entra na modification order |
| W-446 | aritmética atômica | policy checked normal; wrapping/saturating/fetch nomeados | wrap do hardware implícito; closure update com retries ocultos |
| W-447 | borrow atômico | `ref` obtém Atomic; payload comum abre somente por `inout`/consumo exclusivo; edge não concede authority nem lifetime | `ref T` comum; misturar views atômicas e não atômicas; release prolonga owner |
| W-448 | lock-free | não é implícito; const `isLockFree` e contrato `lockFree: true` | garantir toda largura; runtime query sem target fixo |
| W-449 | ABI atômica | layout W opaco; operações concorrentes usam endereço + extent idênticos; C usa wrapper e metadata | layout igual a C `_Atomic`; layout estável universal; larguras parcialmente sobrepostas |
| W-450 | mutex síncrono (retired) | W-1257 preserva a critical section scoped como `lock` da linguagem e remove o wrapper | lock/unlock manual; guard; poisoning; wrapper nominal |
| W-451 | mutex assíncrono (retired) | W-1257 preserva aquisição suspensiva como `await lock`; domain/service lideram para task-owned state | guard cruza await; wrapper async; mutex síncrono no worker cooperativo |
| W-452 | RwLock e RCU (retired) | W-1259 rejeita RW lock baseline; W-1178 fecha SnapshotCell e mantém RCU safe em adapter | policy automática por property; RCU default universal; fairness do host como contrato |
| W-453 | contenção (retired) | W-1269–W-1272 retiram cache do tipo, permitem layout privado por evidence e mantêm partition explícita | prometer performance por `atomic`; padding universal; enfraquecer order sem prova |
| W-454 | stream assíncrono | `Stream<Item, Failure>` é protocol pull, single-pass e com cursor mutável | sequence + iterator obrigatórios; push callback; generator como semântica |
| W-455 | término de stream | `.none` ou primeiro error são terminais; `Failure = Never` remove `try` | continuar depois de throw; sentinel; close como item |
| W-456 | iteração assíncrona | `for try await` baixa para `next()`; `for await` quando nonthrowing | `await stream` lê tudo; callback; loop especial por tipo |
| W-457 | item borrowed de stream | `Stream<view T, E>` registra o stream como origem e impede outro `next` conflitante | família `TView`; view transferable; proibir todo item borrowed |
| W-458 | topologia de channel | MPSC bounded com endpoints separados | bidirecional copiável; MPMC default; unbounded |
| W-459 | endpoint de channel | `Channel<T><.send>` shareable move-first e `<.receive>` único move-only | `Sender<T>`/`Receiver<T>` nominais; direção dinâmica |
| W-460 | payload de channel | `T<(.transferable)>` owned; borrow e `view` são rejeitados | cópia implícita; raw pointer; lifetime runtime |
| W-461 | falha de envio | `ChannelSendError<T>` devolve owner; `send` usa subset `.closed` | panic; Boolean; perder item em erro |
| W-462 | capacity | obrigatória; zero é rendezvous; positiva limita itens + permits; sem unbounded no design vigente | default zero; hint elástico; fila ilimitada |
| W-463 | permit | reserva linear sem item; drop libera; close gracioso honra permit aceito | construir item antes de esperar sempre; reservation invisível |
| W-464 | cancellation de channel | commit linear; antes dele não envia, depois dele receiver possui; waiter sai da fila | resultado ambíguo; rollback do item recebido |
| W-465 | ordering de channel | FIFO de admission, ordem por sender e sem total order concorrente; `trySend` não ultrapassa | ordem global determinística; fairness não especificada |
| W-466 | close de channel | último sender ou receiver.close faz drain; drop do receiver aborta; sem close global no sender | close por qualquer producer; sentinel; panic em close duplicado |
| W-467 | happens-before de channel | send→receive, slot liberado→send admitido e close→fim observado | somente ownership; fence manual pelo usuário |
| W-468 | buffering de stream | nenhum prefetch default; adapter bounded com scope estruturado | watermark na assinatura; buffer ilimitado; producer detached |
| W-469 | `yield` | rejeitado por enquanto; Stream constructor nominal expõe borrow, cleanup, error, cancellation e capacity | generator define semântica; callback oculto |
| W-470 | outras topologias (retired) | candidatos anteriores foram substituídos por composição em W-1229–W-1232 | um `Channel<mode: ...>` muda loss e fan-out |
| W-471 | implementation de channel | target escolhe ring, segmentos, mutex ou atomics; lock-free não é contrato | algoritmo único no ABI; tagged pointer obrigatório |
| W-472 | accounting de channel | lens separa storage, itens, payload desconhecido, waiters, permits e watermark medido | capacity promete bytes transitivos; número único exato |
| W-473 | byte I/O | `ByteSource<Failure>` e `ByteSink<Failure>` async-first; cursor lógico no source | Reader/Writer nominal por backend; prefixo `Async`; interface sync única |
| W-474 | destino de read | append em `inout Bytes`; initialized count e spare privados; sem `ReadBuffer` público | `MaybeUninit` safe; `read(into: inout view Bytes)` genérico; allocation escondida inevitável |
| W-475 | resultado de read | `.data(positive)` ou `.end`; EOF é estável e progress vence EOF simultâneo | zero significa EOF; tuple count/error; EOF como error |
| W-476 | progress e error | progress retorna agora e error simultâneo fica latched para a próxima call | lançar depois de mutar sem informar; perder progress; outcome com estados impossíveis |
| W-477 | resultado de write | `.complete` ou `.partial(positive)`; `writeAll` informa prefixo já committed | Boolean; assumir write integral; rollback fictício |
| W-478 | cancellation de I/O | cancellation disputa com completion e só libera borrow depois do drain | liberar buffer no pedido; fingir zero progress; matar worker thread |
| W-479 | blocking I/O | adapter explícito, pool/queue bounded e drain após foreign entry; cancel só interrompe quando metadata prova | blocking invisível no worker cooperativo; pool ilimitado; destacar thread; uma interface condicional |
| W-480 | rights de arquivo | `File<[.read, ...]>` usa static list fechada e mantém checks dinâmicos do host | flags somente runtime; capability implica permissão de path; annotations |
| W-481 | offsets de arquivo | I/O seekable é posicional por default; `.end` observa o offset; shared File exige offset explícito | cursor compartilhado default; EOF latched no handle posicional; metadata.size + write |
| W-482 | cursor sequencial | adapter opaque `some ByteSource<IoError>` possui File + offset; sem classe utilitária pública | `FileReader` público; cursor dentro de todo File; offset global |
| W-483 | sockets | TCP pode virar halves únicos; UDP preserva datagrams em protocol separado | duas reads concorrentes; UDP como byte stream; message boundary implícita |
| W-484 | error de I/O | kind e operation portáteis; cause nativa opaca; task cancellation não é IoError | errno universal; wouldBlock em async; retriable Boolean |
| W-485 | finish e durability | protocols base não exigem close/flush; tipos concretos nomeiam finish, sync e half-close | async destructor; drop durável; flush universal |
| W-486 | adapters de stream | chunks borrowed/owned, lines e read-to-end exigem limites explícitos | buffer ilimitado; item borrowed transferable; framing invisível |
| W-487 | backend de I/O | target escolhe readiness, completion, blocking bounded ou immediate sem mudar source | backend na syntax; um algoritmo universal; thread por operação |
| W-488 | lifetime de buffer I/O | pinning é interno; handles, callbacks e borrows vivem até completion drain | `pin` obrigatório no caller; raw pointer escapa; cancellation encerra lifetime cedo |
| W-489 | famílias de I/O especializado | gather, scatter e transfer possuem decisões separadas; nenhuma otimização muda bytes ou progress | `readv`/`sendfile` invisível; mapa mutável universal; promessa sem target |
| W-490 | observabilidade de I/O | explanation record e trace mostram backend, progress, waits e cancellation race | backend opaco sem diagnóstico; log muda semântica; timestamps como ordering |
| W-491 | trabalho runtime-owned | `SupervisorRef` é owner explícito; `Task` permanece lexical | drop destaca; `spawn<owner: ...>`; task global |
| W-492 | operação supervisionada | descriptor fixa função e versão; key, input e bindings explícitos | closure arbitrária; capture de state; body trocado em work ativo |
| W-493 | admission de work | roots, running, admission queue e bytes são bounded; commit transfere input; unknown outcome reconcilia por key | fila ilimitada; input perdido; start fire-and-forget |
| W-494 | identity de work | supervisor + key completa + incarnation; attempt separado; hash nunca é identity | PID/pointer; hash persistente; nome solto |
| W-495 | observação de work | state, progress, cancellation e suspension separados; snapshot revisionado; retention bounded | event list ilimitada; ref para frame; polling sem revision |
| W-496 | rights de work | SupervisorRef → WorkKeyRef → WorkRef; observe, cancel e signal atenuam authority | Boolean runtime; todo observer controla; ID concede authority |
| W-497 | outcome de work | success, `E`, canceled e boundary separados | cancel em `E`; panic capturável como application error; ausência vira success |
| W-498 | restart de work | `.never` default; retry bounded exige step/effect ID/idempotência | retry eterno; reiniciar todo async body; retry mutante implícito |
| W-499 | workflow durável | replay desde o começo usa points e outcomes explícitos; sem persistir frame, pointer, borrow ou capability | serializar stack automaticamente; Durable Object universal |
| W-500 | service no source | `export service name: P { ... }` contém boundary e provider; `import service` adapta source comum | identity sem body; implementation separada; proxy global |
| W-501 | product runtime graph | `runtimeGraphs` fixa bindings default, providers, imports, exports, override policy e initializer arguments | manifest executável; reflection encontra implementação; limite só no host |
| W-502 | deployment e launch | planos data-only ligam units e services por digest; override alcança somente bindings `.startup` dentro do envelope | rebuild por ambiente; config invisível; live rebind; configuração cria authority |
| W-503 | rolling work | root fixa operation/schema; drain ou migration explícita | hot-swap do body ativo; retomar com versão ausente |
| W-504 | after-response | adapter host bounded para cleanup curto; trabalho confiável usa supervisor/queue/workflow | `waitUntil` sem prazo; Promise solta; resposta mantém process vivo |
| W-505 | identity keyed de service | `ServiceIdentity<K>` read-only entra como initializer argument; descriptor exige o mesmo key type | Context global; string key; inferir pelo primeiro argumento |
| W-506 | dedup de work | outcome e tombstone têm budgets separados; key só é reutilizada em nova incarnation | outcome eterno; expiração permite duplicação silenciosa; key global única |
| W-507 | completion versus cancellation | completion committed entrega o valor; cancellation fica pendente; unknown outcome permanece distinto | descartar valor committed; injetar cancel entre statements; rollback presumido |
| W-508 | entry anônimo | `entry { code }` ou `entry(handler)` fornece somente o descriptor `.default`; não cria base ou callback registry | body como mapa; `entry defaults`; herança entre entries |
| W-509 | entry nomeado | `entry Name(handler)` publica outro handler do slot default | escrever `process.main`; inferir pelo nome da função; body como tabela |
| W-510 | seleção de entry | product iniciado por host escolhe descriptor; library usa export e service-only unit usa provider no index | entry obrigatório para todo artifact; seleção runtime por nome; registry global |
| W-511 | aplicação multimodo | um `process.main` escolhe CLI/TUI/server; slots de host continuam distintos | vários mains no mesmo payload; OS chama `http.fetch` |
| W-512 | identidade de target | architecture-vendor-system-ABI + CPU/features/sysroot separados | string livre; target igual a OS; backend implica suporte |
| W-513 | host profile | slots, capabilities e lifecycle versionados; product usa `hostBindings` para slots além do default | assignment em `entry`; APIs por `#ifdef`; target concede capabilities |
| W-514 | product kind | `kind` seleciona schema fechado; executable, libraries, component, firmware, device bundle, test, benchmark e tool tipada | record de fields opcionais; executable universal; kind inferido pelo entry |
| W-515 | matriz de build | cada product/target/profile gera recipe e digest próprios; index agrega resultados | hash único entre architectures; matrix muda payload |
| W-516 | produto de referência | Última Luz é especificação executável, regressão e benchmark do W | exemplo descartável; snippets independentes como oracle principal |
| W-517 | nanoservice | service é fronteira lógica; runtime pode co-localizar sem apagar effects | processo por service; call remota transparente |
| W-518 | accelerator | device target gera kernels/objects ligados por host product | device como processo geral; offload implícito |
| W-519 | benchmark externo | sete source oracles, profile versionado e validators precedem medição; ranking não é semântica | otimizar para placar sem oracle; chamar source de resultado; prometer posição |
| W-520 | module set | filename define módulo por default; header e builder precisam concordar; lock grava files | manifest como única origem; descoberta livre do diretório |
| W-521 | std em W | contratos públicos são source W; handles e operações intrínsecas têm fronteira explícita | std toda no compiler; wrappers utilitários por operação |
| W-522 | fechamento do runtime graph | cada requirement recebe provider, supervisor, host capability ou import declarado | lookup por string; import implícito; provider descoberto por reflection |
| W-523 | interface de graph aberto | imports e exports tipados entram na interface do artifact; executable aberto exige deployment | esconder import no Context; rede global; executable presume provider |
| W-524 | packing | partição de providers em units ocorre no build e entra na recipe | extrair service de executable durante deploy; processo por módulo |
| W-525 | crossing de unit | somente service ABI cruza unit; preserva async, schema, quotas, failures e trace | call normal remota; borrow ou mutable state entre units |
| W-526 | deployment lock | source plan resolve products e releases para artifact, unit e adapter digests | tag mutável em production; build durante apply; secret dentro do lock |
| W-527 | WASI baseline | `wasm32-wasip3` usa Component Model native async; `wasm32-wasip2` é compatibilidade | polling 0.2 como semântica W; Wasm implica DOM |
| W-528 | unit root | entry unit é explícita; service-only unit publica provider no artifact index; toda unit possui root | unit vazia; initializer implícito; entry sintético pelo nome |
| W-529 | interface privada de unit | packer deriva endpoints privados; policy `.fixed` ou `.startup` controla override | tornar provider público; religar edge sem declaração; live rebind implícito |
| W-530 | tuple com labels | todos os elementos têm label ou nenhum; labels pertencem ao tipo; unitário exige vírgula | mistura de labels; labels decorativos; function arguments viram tuple; struct anônimo universal |
| W-531 | modelo HTTP | `std.http` representa semântica de mensagem, independente de HTTP/1.1, HTTP/2 ou HTTP/3 | frames no handler; Fetch API copiada integralmente; version decide domínio |
| W-532 | ownership de request | `Request` move-only transfere body e só permite um consumer | clone implícito; body copiável; stream ambiental |
| W-533 | materialização HTTP | decode, bytes e text são async e exigem limite; stream preserva backpressure | ler body inteiro sem limite; decode síncrono; buffer oculto |
| W-534 | headers HTTP | `Headers` segue Fetch: nomes ASCII case-insensitive, values String validados, guards, combinação padrão e `getSetCookie`; fields raw ficam no adapter de protocol | Map sem ordem; expor raw Bytes na API Web; juntar Set-Cookie; aceitar CR/LF |
| W-535 | response streaming | retorno transfere owner ao pump runtime-owned; entrega ao client não é confirmada | task solta; retorno confirma socket; segundo status após commit |
| W-536 | admission HTTP | product/call fixa requests, fila, bytes, connections e message limits | defaults ilimitados; task antes de admission; rate limit de negócio implícito |
| W-537 | host HTTP | native `serve` e worker slot usam o mesmo handler e oracle | API por protocolo de transporte; OS chama fetch; handler especial de benchmark |
| W-538 | `http.Context` | registries tipados expõem somente bindings e capabilities declaradas | mapa de env; singleton global; lookup livre por string |
| W-539 | SQL estático | descriptors const usam bind nomeado e identity derivada; sem `name` textual repetido | interpolação; SQL mutável; query sem dialect; identity manual |
| W-540 | row database | tuple tipado ou decoder explícito; schema bundle aumenta prova | preencher struct por reflection; `Any` row; column conversion silenciosa |
| W-541 | pool e pipeline | admission bounded; query/execute-many preservam um statement, outcome e ordem por input | pool ilimitado; combinar SELECTs; deduplicar queries; ordem de result variável |
| W-542 | transaction | closure recebe borrow não escapante; output sai após commit confirmado; unknown commit é distinto | transaction escapa; retry automático; perda de conexão significa rollback |
| W-543 | cache local | limita entries, loaders e fila; devolve owned duplicate; bytes ficam no envelope do product | Map global; fila livre; view sobre entry; cache como source of truth |
| W-544 | cache load | um loader por key; waiter cancela só a espera; admission e commit são explícitos | task detached; waiter cancela loader; errors cached por default; loaders iguais |
| W-545 | cache remoto | usa `ServiceRef` async separado; deployment não converte cache local em rede | API sync location-transparent; timeout invisível; remote fallback |
| W-546 | limits de product | host/capability e execution profile fixam envelopes máximos por unit; deployment somente reduz | config aumenta authority; limite só operacional; defaults sem artifact contract |
| W-547 | configuração de host | schema fechado do profile entra na recipe; deployment somente reduz policies permitidas | mapa de opções livre; config concede capability; proxy oculto corrige semântica |
| W-548 | parâmetro de chamada `const` | `name: const T` exige ConstRepresentable conhecido no call site; ABI pode apagar | `comptime` em cada call; runtime descriptor na API estática; monomorphization obrigatória |
| W-549 | gather write | `writeMany(_ sources: view Bytes...)` confirma prefixo da concatenação lógica; default usa um `write` | `IoSlice` público; concatenação; exigir backend vetorizado; erro por excesso de segments |
| W-550 | scatter read | provável T1 por owner `ReadBatch`; protocol comum continua com um `Bytes` owner | `inout view Bytes...`; mutable buffer universal |
| W-551 | transferência especializada | `io.transfer` provável T1 informa progress e fallback; zero-copy exige capability do host | lowering invisível de `write`; ausência de fallback; promessa universal de zero-copy |
| W-552 | ativação de workflow | usar `work.step`, `sleep` ou `wait` ativa análise replayable; sem annotation `durable` | keyword nova; replay sem análise; persistência automática de frame |
| W-553 | identidade de point | `WorkId` + kind + valor canônico fechado; duplicata ou input divergente é history mismatch | string livre; ordem de source; contador implícito |
| W-554 | identidade de efeito | `EffectId` é estável por point e operation; step attempt e root attempt são ordinais separados | key nova por retry; hash de payload como identity |
| W-555 | effect policy | `.repeatable`, `.idempotent`, `.transactional` ou `.atMostOnce`; at-most-once é default | exactly-once universal; retry automático de efeito desconhecido |
| W-556 | retry de step | policy const e bounded seleciona application errors, backoff e timeout | defaults infinitos; jitter oculto; Boolean retriable no error |
| W-557 | commit de step | input precede dispatch; outcome e progress precedem visibilidade ao workflow | devolver output antes do journal; converter falha de storage para application error |
| W-558 | timer durável | `sleep` registra duração e alarm do adapter; não mantém task frame nem serializa `Instant` | `Task.sleep` persistido; recalcular timeout em replay; wall clock implícito |
| W-559 | evento de workflow | binding tipado, `EventId`, inbox bounded e `send`/`trySend` com deduplication | event string sem schema; payload global; fila ilimitada |
| W-560 | corrida de wait | commit escolhe evento, timeout ou cancelamento; evento posterior permanece disponível | timestamp do sender decide; timeout descarta evento |
| W-561 | scheduling durável | points sequenciais; paralelismo estruturado pode ocorrer dentro do step | journal dependente do scheduler; fan-out implícito |
| W-562 | versão de workflow | root fixa operation, point/event schemas e adapter ABI; migration é explícita | hot-swap do history; worker antigo executa versão nova |
| W-563 | adapter de workflow | contrato portátil; SQLite profile é explícito e memory serve ao oracle volátil | SQLite universal; deployment reduz garantia; storage oculto |
| W-564 | confidencialidade de journal | artifact fixa mínimo e retention; payload não entra em diagnostics; adapter prova storage | plaintext implícito; secret handle serializado; deployment reduz proteção |
| W-565 | workspace | record `workspace` em `build.w` data-only lista members exatos; não cria identity publicável | discovery recursivo; workspace executável; package e workspace fundidos |
| W-566 | lock de workspace | um lock compartilhado com contexts por root, usage e target | lock por member sem visão global; um grafo único para todos os targets |
| W-567 | member local | identity + version compatíveis selecionam member e tree digest; mismatch falha | fallback silencioso para registry; import por path |
| W-568 | usage de dependência | `.product`, `.build`, `.test` e `.benchmark` fecham reachability e target role | uma lista universal; dev dependency entra no payload |
| W-569 | feature | seleção aditiva de grafo, sem default implícito ou conditional source na v0 | `if feature` livre; optional dependency cria feature; negation |
| W-570 | união de feature | união por resolution realm; realms distintos não vazam | união global do workspace; uma build por edge |
| W-571 | source de dependency | registry ou Git por commit; path somente em workspace; lock fixa tree externo e source set local | branch/tag no build; URL no import; binary URL como identity |
| W-572 | patch | somente workspace root, mesma identity/version, sempre visível e não publicável | dependency troca por fork invisível; patch transitivo; release patched |
| W-573 | build tool W | `.tool` usa `build-transform@1` e bindings tipados | install script; shell fragment; process com filesystem ambiental |
| W-574 | action hermética | tool, inputs, outputs, execution platform, budgets e capabilities formam a key; commit vai ao CAS | output no source tree; host implícito; cache por path/mtime |
| W-575 | nome de dependency | package import expõe modules; alias local muda somente o nome source | namespace canônico obrigatório; URL import; alias muda identity |
| W-576 | package identity | `authority` declarada + scoped ASCII name; revision, mirror e alias local não mudam identity | nome global sem authority; source concede identity; Unicode no path canônico |
| W-577 | source snapshot | `publish.files` allowlist usa modules e PackagePath; VCS ignore não altera release | publicar tudo; usar `.gitignore`; registry acrescenta arquivos |
| W-578 | licença de package | SPDX expression + files; proprietary e no-assertion são states distintos | string livre; ausência implica licença; metadata externa substitui texto |
| W-579 | lock, recipe e artifact | lock fixa resolução; recipe fixa inputs; artifact record liga outputs | lock contém resultados; recipe autorreferente; provenance decide resolução |
| W-580 | ativação de module set | nome e `.always`/`.selected` são explícitos; selector habilita somente `.selected` | todo arquivo declarado sempre ativo; inferir por referência textual; glob ambiental |
| W-581 | predicate de target | record positivo e fechado sobre architecture, vendor, system, ABI, endianness e pointer width | expressão Boolean geral; `cfg` no source; target set como predicate |
| W-582 | resolução de variante | um case concreto disjunto ou fallback explícito; ordem não cria priority | case mais específico vence; first match; merge de matches |
| W-583 | efeito de variante | seleção aditiva habilita dependency, module set, resource ou action | remover graph existente; trocar product/profile/host; ativar feature circular |
| W-584 | interface entre targets | `.uniform` compara interface normalizada; `.targetSpecific` marca a diferença | presumir paridade; esconder diferença no linker; uma API universal artificial |
| W-585 | role de target | package do payload usa target final; package do tool usa execution target; metadata cruza só como input | host ambiental; build dependency avaliada pelo target errado; um target global |
| W-586 | adaptação de plataforma | manifest escolhe module implementation; source mantém import normal e target facts não removem declarations | `#if`; filename condition; import ou annotation condicional |
| W-587 | mutation do package graph | `w add/remove` atualiza manifest e lock atomicamente, com dry-run e sem scripts | editar lock à mão; resolver depois; executar install hook |
| W-588 | inventário de source | package fixa todo source inventory; cada context fixa o active source set após selectors | um digest ambíguo; somente files ativos na release; discovery por checkout |
| W-589 | target spec | `TargetId` + CPU/features + platform contract; profile só impõe `cpuPolicy` | triple inclui SDK; CPU duplicada no profile; host completa campos |
| W-590 | execution platform | root ordena platform specs; cada phase fixa uma; CLI usa `--execution-platform`; executor compatível não muda a plan | `--execution` ambíguo; host global ou automático; endpoint na recipe |
| W-591 | requirement de toolchain | analyzer pede roles e capabilities; package não escolhe executable por path | compiler path no manifest; shell environment; toolchain monolítica obrigatória |
| W-592 | provider de toolchain | `runsOn`, `producesFor`, roles e artifacts por digest são independentes | backend implica SDK/linker; provider concede capabilities não declaradas |
| W-593 | resolução de provider | root ordena source/authority/provider; versão é comparada dentro de uma lineage no snapshot fixo; empate falha | comparar versões de providers distintos; registration order; primeiro executable em `PATH` |
| W-594 | toolchain plan | record data-only fixa target spec, execution platforms e providers; recipe referencia a row | toolchain na resolution; resolver novamente durante reprodução; output na plan |
| W-595 | system SDK | import explícito cria provider system-backed e closure por digest; build não faz discovery | SDK mais recente instalado; `xcrun`/`vcvarsall` em toda build; path na recipe |
| W-596 | ambiente de build | deny por default; projeção nomeada e valor entram na action recipe | herdar `PATH`, `SDKROOT`, `INCLUDE`, locale ou timezone |
| W-597 | sysroot | raiz lógica declarada e search paths por artifacts em ordem canônica | `/usr` do executor; library discovery do host; flags ambientais |
| W-598 | artifact composto | cada slice possui recipe/target/digest; composition recipe produz envelope sem assinatura | target universal fictício; concatenar bytes sem schema; um hash para várias recipes |
| W-599 | assinatura de plataforma | signing, timestamp e notarization produzem delivery record separado | assinatura dentro da payload recipe; JWT autorreferente; signed bytes como input |
| W-600 | claim de reprodução | bit-reproducible e publicly rebuildable são facts distintos | SDK fechado recebe selo público; um builder prova reprodução independente |
| W-601 | granularidade física | module é semântico; recipe pode usar object, archive, LTO ou incremental unit | static library obrigatória por módulo; granularidade física na linguagem |
| W-602 | provider inventory | snapshot associa provider records a execution platforms sem paths; row identity cobre só selections | consultar pools durante build; endpoint na plan; candidato não usado muda artifact |
| W-603 | materialização de provider | CAS ou sealed local são preferidos; system-backed exige verificação forte antes da action | confiar em mtime/path; copiar contra licença; rehash ambiental sem record |
| W-604 | platform availability | grafo alcançável precisa caber no `platformContract`; compiler não aumenta o minimum | SDK version vira runtime floor; link falha tarde; API nova eleva target em silêncio |
| W-605 | requirement transitiva | dependency pode pedir role; somente root autoriza language/source e escolhe provider | dependency fixa compiler; download precede policy; foreign adapter implícito |
| W-606 | ownership da toolchain policy | workspace, flag da root ou default seguro do consumer; package não concede autorização | policy transitiva; package autoriza próprio compiler; reprodução ignora deny local |
| W-607 | camadas de compatibilidade | API, interface, W exact, runtime, component, foreign e schema são contratos separados | uma ABI universal; source implica wire; component usa layout W |
| W-608 | interface semântica | `WInterface` separa semantic, documentation, diagnostic map e body chunks; `SemanticInterfaceKey` dirige compatibilidade | um digest mistura docs e type-check; AST serializada como autoridade; header textual |
| W-609 | cache de interface | AST/HIR interna usa toolchain key e não é artifact publicável | formato interno estável; package distribui cache; reparse obrigatório |
| W-610 | chave ABI W | `WAbiKey` cobre policies globais; `RepresentationMap` cobre tipos nas boundaries; requirements ficam separados | type set privado ou requirement set na key; target triple somente |
| W-611 | mismatch W | plan fixa artifact exato, source rebuild ou boundary explícita; candidate incompatível falha | fallback por disponibilidade; adaptar layout; escolher library por filename |
| W-612 | ABI semântica de call | signature registra ownership, effects, provenance, generic metadata e address space | somente tipos físicos; caller infere cleanup; throws por unwind |
| W-613 | ABI física de call | `WAbiKey` escolhe direct/indirect, registers, hidden context e error channel | calling convention fixa universal; source escolhe registers; C ABI interna |
| W-614 | generic binary-only | shared body, instâncias fechadas ou HIR aceita pela mesma distribuição; source-first é fallback | compiler futuro interpreta IR antiga; machine code generic universal |
| W-615 | symbol W | `SymbolId` semântico + mangling versionado e manifest completo; ordem e path não participam | nome source global; node ID; hash curto sem collision check |
| W-616 | runtime contract set | objects pedem operações versionadas; provider oferece; reachability produz `RuntimeClosureKey` fora da `WAbiKey` | `libwrt` monolítica; runtime implícito do host; requirement set dentro da `WAbiKey` |
| W-617 | inicialização de runtime | entry shim valida host e passa contexto bounded; sem module/library constructor implícito | globals exportados; init por import; scheduler descoberto no executable |
| W-618 | product de library | exports exatos + `.wExact` ou `.c`; container static/dynamic não muda semântica | todos exports do módulo; ABI inferida pelo suffix; library é module |
| W-619 | W dynamic library | artifact index e digest selecionam; loader valida ABI/runtime antes de entregar handle | `PATH`; soname sem digest; hot-swap in-place |
| W-620 | ABI W resiliente | rejeitada por enquanto; v0 recompila dependentes e usa C/component/service para evolução independente | estabilidade eterna; layout frozen por default; runtime permanente obrigatório |
| W-621 | função W com ABI C | `export unsafe fn<abi: .c>` possui body W; `fn<C>` continua ilha C | annotation; wrapper externo obrigatório; interpretar C como W |
| W-622 | export C | identifier é link name exato; header e export list são gerados; const inteira não cria storage; collision falha antes do link | mangling W; symbol alias ambiental; export de todos os symbols |
| W-623 | memória na façade C | owner cruza com destroy symbol ou context/drop; borrow termina na call | caller usa `free`; allocator global presumido; W collection direta |
| W-624 | falha na façade C | typed error vira carrier; panic é `.forbid` ou `.abortProcess` e nunca atravessa | exception unwind; panic como status automático; catch de panic em C |
| W-625 | plugin e version skew | process/Wasm/component usa schema; nova instance inicia e antiga drena | native library vira sandbox; copiar globals; trocar bytes sob instance ativa |
| W-626 | LTO e IR | HIR, MLIR e bitcode são artifacts exatos da recipe; LTO não define package ABI | bitcode público eterno; static library por module; LTO muda ownership |
| W-627 | diagnóstico de compatibilidade | `interface diff` e `abi show/diff` produzem resultados separados; symbol/runtime tools explicam a causa | um Boolean compatible; erro final do linker; versão como proxy |
| W-628 | runtime de façade C | `.none` não recebe contexto oculto; `.explicitContext` usa create/destroy e handle C | runtime global; lazy init; TLS implícita; context descoberto no host |
| W-629 | unload de library nativa | release fecha lookup e calls; v0 mantém mapeamento até o fim do runtime island | desmapear por refcount de handle; copiar globals; assumir que nenhum owner retém código |
| W-630 | metadata binária não confiável | readers de interface, ABI note e manifests são data-only, bounded e fuzzed | confiar em assinatura; deserialização sem limites; plugin durante decode |
| W-631 | header C por target | sidecar determinístico inclui calling convention, export macros e layout assertions; index liga header à slice | header universal por nome; path e timestamp; confiar só no linker |
| W-632 | resolução de symbol dinâmico | loader W usa handle + manifest e visibilidade local; calls não dependem de interposition | lookup process-global; primeiro symbol carregado; override por environment |
| W-633 | encoding de metadata | records de package/lock/recipe usam CBOR determinístico; `WMeta1` fica separado para `WInterface` e ABI pública; cache interno permanece recipe-exact | JSON como autoridade; misturar manifest com WMeta1; cache interno como contrato eterno; codec universal |
| W-634 | `Address` | `Copy` opaco por address space, equality local, hash local, hex e `Bits`; não é pointer, identity ou ordem | alias de `usize`; integer com dereference; object ID |
| W-635 | reconstrução de pointer | `withAddress` usa a provenance e o address space do receiver; v0 não possui exposed provenance ou `Address.toPointer` | round-trip integer; provenance global implícita; pointer forge safe |
| W-636 | cópia de pointer | cópia tipada preserva non-address bits e estado externo; Bytes não faz round-trip | mesmo size implica bitwise; serializar pointer; integer load/store equivalente |
| W-637 | `shared` e `weak` | leitura contextual em target normal adquire `shared T?` de `weak T?` de forma linearizável; value morre no strong zero e control block no weak zero | `.upgrade`, `.strong`, `.strong()`; weak aponta ao payload reutilizável; contador curto no pointer; ressurreição |
| W-638 | mobilidade de allocator | origem local impede `transferable`; parameter contract e `AllocationOriginMap` derivam a prova cross-domain | todo allocator cruza domain; `Allocator<(.crossDomain)>` source-visible; annotation por container; teste runtime tardio |
| W-639 | profile de memória | `memory.generalAllocator` e `memory.representation` são obrigatórios por build profile | default ambiental; allocator por dependency; compactação sem oracle |
| W-640 | low-bit lowering | alignment + address-space + provenance + tooling + canonical boundary; fallback quando qualquer prova falta | alignment nominal basta; ptr-int-ptr; tag cruza FFI |
| W-641 | entrada de representação | fingerprint descreve logical identity, validity e carrier físico; recipe retém provider e options | provider noise na ABI; fingerprint só por compiler; adaptação heurística |
| W-642 | carrier de `Bytes` | empty/static/inline/dynamic permitem conversão consuming com String sem layout público comum | sempre Array<u8>; cópia obrigatória; pointer tag define interpretação |
| W-643 | `CString` | payload sem NUL + terminador; count exclui NUL; UTF-8 só após decode explícito | CString sempre UTF-8; count inclui sentinela; scan inbound ilimitado |
| W-644 | address space | `Address` seleciona o space default; `Address<space: S>` preserva identidade e index width; casts exigem adapter `unsafe` | um integer universal; LLVM address-space ID no source; equality cross-space |
| W-645 | lifetime de task | scheduler state é ortogonal; body settled precede cleanup; outcome só fica observável depois do cleanup | running como lifetime; outcome antes do cleanup; cancel substitui resultado settled |
| W-646 | arbitragem de join | application error causa fail-fast; drain precede seleção lexical/input entre errors settled; completion order só quando declarado | esperar child anterior antes de cancelar; primeiro erro do scheduler como default |
| W-647 | snapshot de cancellation | reasons do caller, deadline, budgets, scope exit, sibling failure e ancestry formam valor fixed-size monotônico | um reason vencedor; lista alocada; budget como caller reason |
| W-648 | deadline | `Deadline` é monotônico e local; `TaskTimeout` é refinado; ausência usa `none`; boundary distingue propagation strict e approximate | wall clock; serializar Instant local; infinity como ausência; afirmar clocks distribuídos iguais |
| W-649 | admission de task | argumentos/captures avaliam uma vez em staging; reserva precede publish; falha limpa owners e produz handle canceled inline | pular efeitos sob carga; fila ilimitada; novo error effect em initializer `async` |
| W-650 | capabilities de domain | `.main` é serial; outros domains declaram serial, concurrent, parallel e limits | lista global de finalidade; domain implica thread |
| W-651 | execution profile | package fixa tasks, frames, timers, ready queues, pools, fallbacks e cleanup; product seleciona `executionProfile` | scheduler ambiental; limites somente no deployment; profile de build acumula runtime |
| W-652 | envelope por unit | packing aplica profile a cada unit com tasks; artifact index grava máximos; deployment reduz por unit | budget global impossível entre hosts; deployment aumenta ou troca pool |
| W-653 | blocking drain | cancel remove job não iniciado; foreign frame iniciado mantém owner até retorno ou fault boundary física | matar thread; liberar buffer cedo; task detached após deadline |
| W-654 | lifecycle de service call | envelope commit, admission, turn e outcome commit fixam ownership, cancellation e unknown outcome | cancellation presume ausência de efeito; retry mutante; abandono do turn |
| W-655 | deadline remoto | caller local mantém authority; remaining duration cruza; strict exige timebase/synchronization provada | resetar timeout em cada hop; garantia end-to-end sem clock proof |
| W-656 | device execution | ordinary `spawn` não faz offload; transfer, kernel artifact, launch e sync ficam explícitos em T2 | `.device` migra closure; auto-transfer; GPU como thread pool |
| W-657 | nomes de quantity | dimensão já dá identidade; nomes físicos locais usam `alias`; `type` exige distinção adicional de domínio | newtype para toda unit; conversão implícita entre newtypes |
| W-658 | duração operacional | `Duration` T1 é signed, exact e nanosecond; layout opaco; physical quantity converte com exactness ou rounding explícito | `f64`; infinity; alias de physical quantity; attosecond na baseline |
| W-659 | body de entry | body contém statements W; forma simples depende de adapter declarado pelo host profile | body como key/value; registro runtime; ignorar parâmetros sem adapter |
| W-660 | callback de host | product pode ligar symbol privado ou `export` a slot ABI estático | package visibility; pseudo-global `process`/`device`; convention por nome |
| W-661 | construção de service | declaration contém provider; initializer arguments são ligados pelo graph | identity sem body; field injection implícita; service locator no init |
| W-662 | APIs de host | `std.process` é T1 sem singleton; device, mobile e audio usam módulos SDK e contexts explícitos | pseudo-global ambiental; um Context universal; target implica authority |
| W-663 | signals de processo | `ctx.signals.register` instala handler runtime; adapter enfileira evento seguro; registration controla lifetime | `hostBindings`; executar W no raw handler; recuperar fault síncrona |
| W-664 | service resolution | slots são resolvidos antes do entry e ficam estáveis por process generation | live rebind; `ctx.services.get`; trocar provider durante call |
| W-665 | service bridge | `ServiceProtocol<P>` é gerado; `ServiceLink` materializa a boundary; `ServiceTransport` carrega somente frames wRPC | conformance manual; um SPI para local, RPC e bytes; wQL universal |
| W-666 | stack de service | source → `ServiceIR` → `ServiceLink`; o native distributed link usa wRPC → codec → transport | Cap'n Proto como runtime estrutural; wRPC fictício no local/component; um protocolo mistura tudo |
| W-667 | service IR | compiler deriva um record data-only de protocols W; não existe segundo IDL manual | `.proto`/`.capnp` como source authority; reflection runtime como schema |
| W-668 | identities de schema | `interface.lock` versionado guarda IDs, lineage e reservations sem annotation source | ordinals no source; declaration order; hash do nome |
| W-669 | evolução de service | compatibility é direcional; enum subset limita cases possíveis; unknown fields não ficam ocultos no valor W | SemVer sozinho; aceitar toda mudança aditiva; sidecar invisível em toda struct |
| W-670 | session wRPC | handshake negocia interface, codec, limits e features; `callId` é attempt e `effectId` é efeito lógico | schema arbitrário recebido; metadata map ilimitado; retry preserva call ID |
| W-671 | stream remoto | usa `some Stream<Item, Failure>` diretamente; direção vem da posição; drop envia reset e drain | quatro famílias RPC públicas; `RpcStream`; `view` remoto; `any Stream` na interface |
| W-672 | capability remota | table bidirecional, rights attenuated, release no último alias; resource cleanup pertence ao provider descriptor | URL livre; pointer; `capRelease` finge executar `close`; persistent ref default |
| W-673 | call dependente | wRPC inclui `pipeline` explícito; ancestry, intermediate ownership e orphan capability cleanup permanecem observáveis | toda task vira promise lazy; exigir mega-operation; pipeline implícito |
| W-674 | codec nativo | wWire é portátil, tipado e canônico; JSON é oracle; Cap'n Proto é foreign link e baseline | layout de memória como wire; codec único para todos os usos; freeze sem fuzzer |
| W-675 | input gate | closed turn bloqueia outro handler durante qualquer `await`; reentrância exige policy futura | interleaving default; storage library altera admission implicitamente |
| W-676 | output gate | turn fica `committing`; failure conhecido produz `commitFailed`, dúvida produz `unknownOutcome`; staging não entregue é descartado | próximo turn antes do commit; resposta prematura; rollback fictício |
| W-677 | seleção de service link | `servicePolicy.links` permite local, component, wRPC ou adapter; deployment lock fixa a escolha por edge | `transports` mistura níveis; escolha ambiental no startup; source seleciona IPC/rede |
| W-678 | consulta RestPC | QUERY representa somente operação provada safe e idempotent; content tipado exige `Content-Type`; cache key inclui content e metadata | GET com content; POST como fallback de toda query; método HTTP determinar efeitos do handler |
| W-679 | seleção de profile wWire | compatibilidade semântica ocorre primeiro; `WireSchemaDigest` por raiz seleciona `exact`, senão map compatível seleciona `compatible` | digest da interface inteira; profile por package; fallback ambiental |
| W-680 | layout wWire | `exact` usa bitmap, fixed values e length vector; `compatible` usa directory ordenada e blocks; nenhum usa offset ou pointer | offset directory baseline; TLV intercalado; raw struct |
| W-681 | domínio de wire | `WireValue` é fato derivado; refinements podem estreitar width; `usize` exige domínio portátil; clock local e borrow são rejeitados | conformance manual; serializar Instant; aceitar tipo conforme placement runtime |
| W-682 | decode canônico | decoder rejeita formas alternativas, valida estrutura antes de reservar e cobra bytes, items, depth, traversal e allocation | normalizar input; allocation por count antes de length; limite único de bytes |
| W-683 | unknown e capability | ordinary value descarta unknown; relay explícito preserva block canônico; capability usa ordinal e table fora do codec | unknown sidecar oculto; endpoint no payload; bytes com capability como content address |
| W-684 | suspensão observável | journal guarda alarm privado; `WorkSnapshot` publica duração restante com clamp em zero | expor wake `Instant`; usar snapshot como history; `sleep(until: Instant)` durável |
| W-685 | lifecycle de compatibilidade | pre-1.0 não preserva formas descartadas; pós-1.0 toda deprecation exige replacement, migration e milestone de remoção | shim pre-1.0; suporte indefinido; remoção sem aviso após 1.0 |
| W-686 | source de pipeline | `try await pipeline { let ...; return ... }`; body é DAG estático e não valor first-class | builder fluente; closure record-replay; promise lazy universal |
| W-687 | falha de pipeline | dependents bloqueiam, independentes cancelam e drenam; qualquer unknown outcome domina e leva todos os effect IDs | primeiro error esconde incerteza; rollback presumido; exatamente uma mutation |
| W-688 | source de transaction | `try await transaction<...> tx = provider { ...; commit value }`; contract pertence ao provider e commit fecha cada caminho de sucesso | method call como forma idiomática; provider implícito; `return` ambíguo; commit manual fora do scope |
| W-689 | boundary de transaction | um provider nominal, scope borrowed não escapante, output após confirmação e `unknownCommit(EffectId)` após dúvida | retry automático; cancellation significa rollback; devolver output incerto; restaurar owners consumidos |
| W-690 | composição transacional | somente effects derivados de `tx`; múltiplos providers usam workflow e compensação; nesting e pipeline ficam fora da baseline | transação distribuída implícita; 2PC default; chamada externa aparentemente atômica; savepoint como nested commit |
| W-691 | failure de service stream | `Failure` é `ServiceFailure` ou possui uma injeção total única; `Never` remoto é rejeitado; abertura e terminal são fases distintas | boundary error oculto; segundo error channel; `done()` posterior; disconnect vira fim normal |
| W-692 | créditos de service stream | grants absolutos para items e bytes wWire; reserva e token buckets aplicam limites por stream e agregados | crédito apenas por item; bytes compressed; buffer ambiental; limite cumulativo obrigatório para feed longo |
| W-693 | lifecycle de service stream | producer output vira root runtime-owned sem borrow da instance; input vira pump; reset cancela e drena; cross-route usa relay bounded | manter closed turn aberto; destructor async; producer detached; materializar stream no relay |
| W-694 | channel security wRPC | network usa TLS 1.3 mutual ou QUIC TLS 1.3 mutual; IPC prova peer e channel; deployment lock fixa identity e profile | criptografia própria; TLS oportunista; path de socket como identidade; TLS terminator invisível |
| W-695 | transcript de session | hellos canônicos, nonces CSPRNG, channel binding, seleção policy-bounded e `ready` bilateral formam um session ID tagged | confiar só em connection ID; negociar cipher no wRPC; fallback silencioso; MAC duplicado sobre TLS |
| W-696 | replay e admission wRPC | 0-RTT e frames antes de `ready` são proibidos; sequence é exata por lane; autenticação e quotas antecedem tables | mutation em early data; resume baseline; allocation antes do limite; capability entre sessions |
| W-697 | attenuation de service capability | protocol menor reduz o operation set; typed binding cria grant menor sem cast recuperável | lista paralela de rights; ACL por nome; generic covariance amplia authority |
| W-698 | root grant wRPC | deployment cria um grant por edge; call usa `targetCapability`; peer identity não concede root global | root por peer; `callerCapability`; index adivinhável; payload antes de authorization |
| W-699 | delegation de capability | path tipado delega; alias ou `take` ficam na assinatura; unknown carrier rejeita capability; three-party usa relay | URL ou bytes como referência; lookup ambiente; authority opaca; introdução direta implícita |
| W-700 | revocation de capability | exporter bloqueia admission; call admitida drena; resource cap é generation-exact; release não é revoke | rollback por revoke; reuse de ID; lease antigo rebind automático; cleanup de domínio por release |
| W-701 | envelope de release | records separados para payload, recipe, provenance, maintainer e platform; assinatura externa ao payload | JWT autorreferente; assinatura única para todos os papéis; assinatura muda payload digest |
| W-702 | reprodução independente | attestation referencia todos os inputs e outputs; default público exige dois builders independentes; source fechado não alega reprodução pública | CI duplicada como quorum; hash somente do executável; advisory reescreve evidence |
| W-703 | metadata e mirrors | root, targets, snapshot, timestamp, expiry, threshold e digest protegem registry; mirror é transporte | URL como trust; rollback/freeze aceitos; fallback para mirror não listado; bytes mutáveis |
| W-704 | transparency adapter | Sigstore/Rekor pode atestar identidade e registro; trust policy W continua local e separada | log como única autorização; OIDC como root universal; chave do builder acumula maintainer e platform |
| W-705 | ergonomia de transaction | contract default permite omitir `<...>`, mas `tx = provider`, body e `commit` continuam explícitos | `try await transaction;`; provider ambient; `tx` implícito; commit por `return` |
| W-706 | navegação do design | `DESIGN.md` continua canônico e integral; índice gerado publica bundles e métricas; leitor somente leitura recorta headings/IDs; check impede drift | capítulos com autoridades separadas; resumo manual duplicado; leitura integral por default |
| W-707 | gate de design freeze | cinco ciclos fecham ergonomia, kernel, execução, toolchain e contrato público; pesquisa com fallback não bloqueia | número de decisões prova completude; toda pesquisa bloqueia; implementação ampla antes dos spikes |
| W-708 | formatter canônico | CST lossless, trivia, 120 colunas, source order, comments anexados, semicolon removido quando a partition não muda e saída idempotente | remover semicolon que muda CST; style configurável amplo; import sorting; formatter dependente de HIR |
| W-709 | formatter e diagnostics | source com error fatal não é gravado; `--check` não modifica; spans continuam em bytes e a recovery fica no editor | saída parcial silenciosa; line/column como autoridade; formatter corrige sem reportar parser error |
| W-710 | representação por fronteira | low bit somente em storage interno provado; W exact publica niche; C, wire e persistência usam carriers explícitos; high bit fica experimental | tagged pointer universal; tag no source; pointer bits em wire; compactação que ignora hardening |
| W-711 | evolução da ABI W | v0 recompila consumers W; C, component, service schema e source rebuild atendem evolução independente | ABI resiliente implícita; layout congelado por default; runtime permanente sem protótipo |
| W-712 | registro de wire kinds v0 | IDs `1–25` são um registro core append-only; `0` é inválido; extensões exigem registro/negociação e kind local não é portátil | inferir ID pela ordem do enum; usar kind como semântica da aplicação; aceitar extensão desconhecida sem registry |
| W-713 | seed vectors wWire | `MenuKey` fixa quatro vetores hex para `exact` e `compatible`; os vetores orientam conformance sem prometer layout de memória | esperar o decoder para definir bytes; snapshots de implementação; usar JSON como wire nativo |
| W-714 | domínios de metadata | CBOR determinístico cobre records de build/distribuição; WMeta1 cobre interface e ABI públicas; wWire cobre payloads de service; AST/HIR permanece interno | um codec universal; inferir domínio pelo magic; payload usar bytes de recipe; cache interno virar ABI implícita |
| W-715 | lifecycle cross-boundary | oracle comum exige cleanup antes de outcome/join e commit terminal único; `unknownOutcome` não volta a confirmed | task outcome antecipado; service turn reentrante por default; commit uncertainty tratada como abort |
| W-716 | replay do scheduler | `scheduleId`, decisões lógicas, trace, outcome e owners fechados são comparados; packing físico diferente vira sidecar | comparar worker IDs; aceitar timing como semântica; esconder fault injection em logs; tratar panic como error recuperável |
| W-717 | schema do replay | eventos de task, service, commit e owner pertencem ao logical trace; worker, thread, queue e transport ficam no sidecar físico | trace bruto como contrato; worker ID como identidade W; ignorar owner closure; comparar somente payload final |
| W-718 | fault injection determinístico | `FaultSpec` usa `caseId`, boundary, point, action e occurrence bounded; combinações inválidas falham antes do runner; identidade entra no logical trace | relógio ou random como seletor; worker ID como selector; ocorrência ilimitada; colapsar cancel, error, commit uncertainty e panic |
| W-719 | lifecycle de stream | oracle distingue abertura, item, terminal, reset, drain e protocol failure; fault points de `open`, `decode` e `close` preservam outcomes próprios | `done()` posterior; reset como fim normal; item tardio entregue; error de boundary oculto; destructor assíncrono |
| W-720 | preflight do decoder wWire | budgets separados, soma checked de directory e blocks, e rejeição antes da reserva; oracle cobre excesso e overflow | limite único de bytes; reservar por count; soma unchecked; normalizar forma não canônica |
| W-721 | primeiro codec diferencial wWire | implementação Node limitada a `MenuKey` produz os quatro vetores e rejeita formas estritas; W0 e fuzzer continuam gates necessários | tomar o codec host como autoridade; congelar o formato com uma implementação; aceitar bytes não canônicos; omitir unknown scalar skip |
| W-722 | mutação reproduzível do codec | mutations com offsets e masks fixos; aceitação exige re-encode byte-for-byte; rejection usa erro de codec conhecido | random sem seed; aceitar valor diferente sem canonicalização; misturar mutation de structure com property de scalar |
| W-723 | segunda implementação wWire | C e Node concordam nos quatro vetores; cada implementação testa erros básicos; compilação usa diretório temporário e não cria artefato no repo | considerar GCC como target W; comparar somente o payload final; aceitar divergência de directory; exigir GCC em hosts sem toolchain |
| W-724 | evidência de reprodução | attestation completa compara todos os inputs declarados e payload/artifact digests; builder identity mede independência, mas não é input; oracle distingue input e artifact mismatch | hash somente do executável; comparar só recipe digest; usar builder identity como input; aceitar evidence incompleta; tratar bytes iguais com recipe diferente como reprodução |
| W-725 | resolução de execution domain (retired) | policy anterior de preference/default; W-1162 mantém requirements e profile, mas fecha domain no call site e não promete simultaneidade | thread group fixo no source; default de módulo; default implícito em todo `spawn`; capacity um invalida `.compute`; deployment aumenta budget; import cria queue ou thread |
| W-726 | separação de ServiceLink e transport | local usa mailbox/thunk, component usa component ABI, wRPC usa session/codec/transport e foreign usa adapter próprio; local/component não criam frames wRPC | transport universal; local serializado por aparência; component com wire implícito; foreign adapter sem digest; ServiceLink confundido com ServiceTransport |
| W-727 | quorum de reprodução | threshold só vale quando cada par prova builder, operator, credential e execution root distintos; contagem sem independência resulta em `rejectReproduction` | contar jobs da mesma CI; comparar somente `builderIdentity`; usar assinatura como prova de operador; aceitar root de execução compartilhado |
| W-728 | provenance e assinatura de platform | recipe, toolchain digest, artifact e platform target precisam apontar para os mesmos records; roles permanecem distintas; divergência resulta em `rejectReproduction` | assinatura platform como prova de source; toolchain implícito; comparar somente payload; um envelope para maintainer, builder e platform |
| W-729 | resource lens | `ResourceLensRecord` separa reachability, code, static data, instance, operation, peak, accounting, confidence e provenance; intervalos e `unknown` são válidos; medição não vira fact | um número de memória por import; annotation no source; soma sem target/profile; measurement como garantia; wildcard cobra tudo sempre |
| W-730 | kernel HIR de memória e ABI | owner/borrow/drop, storage estável, representação por boundary e `WAbiKey` são verificados antes do lowering; oracle pequeno cobre SH3/SH4 | backend decide ownership; borrow como ponteiro; pinning universal; niche em C/wire; link por nome ou target apenas |
| W-731 | fechamento de pesquisa | toda família possui baseline, tier ou rejeição; gates de implementação não voltam a ser pergunta sem decisão | manter itens vagos em Pesquisa; afirmar implementação sem oracle |
| W-732 | control flow aninhado | `label: for/while/repeat`, `break label` e `continue label`; sem `goto` ou nonlocal jump | `goto`; label em qualquer statement; força sair por exception |
| W-733 | assignment composta | arithmetic, power, shift e bitwise assignments avaliam place uma vez; sem logical, coalescing ou matrix assignment | increment/decrement; `&&=`; `??=`; `@=`; custom operator |
| W-734 | identidade e assertions | `isSameInstance(as:)` compara object nominal; `assert` executa em todo profile; `expect` é test-only | `===`; address como identity; debug-only assertion; safe assume |
| W-735 | catálogo da std | módulos por capability, target facts, provider e reachability; distribuição única | std monolítica no payload; package separado por tier; import implícito universal |
| W-736 | ciência e data parallel | `Complex<T>` T2 e `Simd<T, lanes: N>` T1 preservam numeric policy; scalar fallback não muda resultado | complex literal novo; vector width dependente do target; fast mode implícito |
| W-737 | contexto local (retired) | direção inicial separava task-local de TLS; W-1236 e W-1237 fecham inheritance, ownership, cleanup e limits | mapa task-local mutável; TLS como isolation; borrow TLS suspenso |
| W-738 | volatile e MMIO (retired) | direção inicial usava capability; W-1234 fecha schema, accessors e ordering | `var volatile`; integer cria pointer; MMIO safe sem host authority |
| W-739 | linker placement (retired) | direção inicial movia placement ao product; W-1238 fecha payload retention e recipe | annotation em source comum; import muda section; linker flag livre |
| W-740 | assembly (retired) | direção inicial exigia contract estático; W-1239 fecha adapter, effects e limits | asm safe; clobber implícito; naked function baseline; string passada ao backend sem scanner |
| W-741 | primitives de execução (retired) | direção anterior listava SnapshotCell como provável; W-1178 fecha sua superfície sem agrupar topology ou wait/notify | um Channel muda topologia por mode; safe RCU geral; uma API universal para toda topologia |
| W-742 | evolução de workflow (retired) | W-1241 substitui child/continue intrínsecos por roots, calls, effect IDs e outbox explícitos | persistir async frame; wall clock implícito; compaction definida pelo usuário |
| W-743 | metadata publicável | WMeta1 usa header/directory e chunks em subset CBOR determinístico; profiles separam interface e object ABI; cache interno continua recipe-exact | codec universal; JSON binário; HIR antiga vira ABI eterna |
| W-744 | extensões de service (parcialmente retired) | W-1242/1243 fecham resolver nominal e adapters lock-fixed; `PersistentRef` continua separado depois da v0 | authority por string no core; capability em unknown field; reconnect direto implícito |
| W-745 | ilhas externas posteriores (retired) | W-1233 fecha uma syntax inline e deixa source separado no build graph; providers continuam independentes | runtime externo implícito; staticlib por função; ABI W rica atravessa a ilha |
| W-746 | loop pós-condicional | `repeat { body } while condition` executa body ao menos uma vez; `continue` avalia a condição final | rejeitar post-test loop; `do ... while`; exigir `while true` e `break` negado |
| W-747 | block rotulado | `label: { ... }` aceita somente `break label`; saída lexical executa cleanup e não produz value | `continue` para block; label em qualquer statement; salto para dentro; break com value na baseline |
| W-748 | documentação de substituições | toda decisão que rejeita uma construção por outra recebe caso comparativo e cobertura gerada antes do freeze | mostrar somente a forma escolhida; contar apenas exemplos por seção; executar syntax rejeitada no corpus positivo |
| W-749 | fechamento do design freeze | famílias estão classificadas; grammar, semantics, diagnostics, std, targets, formats, execução, packages, W0 e substituições ainda exigem artefatos fechados | tratar classificação como spec completa; esperar backend para escrever contratos; congelar sem conformance |
| W-750 | newline e statement boundary | newline é whitespace; parser consome a maior expression; semicolon força boundary e permanece quando sua remoção mudaria statement partition | ASI; newline sempre termina; formatter remove todo semicolon mesmo com mudança de CST |
| W-751 | grammar normativa G0 | EBNF de block, binding, controle, labels e transfer statements pertence ao design; Tree-sitter é projeção | parser gerado como autoridade; prose sem grammar; aguardar frontend completo |
| W-752 | recovery sintático G0 | recovery insere somente delimiter/keyword exigida, preserva bytes em ERROR e usa MISSING zero-width; build rejeita árvore recuperada | inventar expression/identifier; compilar recovery tree; descartar bytes; formatter salvar reparo silencioso |
| W-753 | grammar normativa G1 | EBNF de raízes, imports e declarations pertence ao design; source de módulo e manifest são documentos disjuntos | package inline; parser gerado como autoridade; manifest misturado com source executável |
| W-754 | fase de imports | header precede imports e imports precedem declarations comuns; recovery não move imports | imports intercalados; import dentro de body; ordenação automática pelo formatter |
| W-755 | body de função | função comum e ilha `fn<Language>` exigem body; somente protocol requirement W pode omiti-lo; símbolo externo usa `foreign` | prototype solto no top-level, ilha sem body, body inferido, newline termina signature |
| W-756 | delimiters de configuração | `<...>` modifica contrato local nomeado; `{...}` define record completo e data-only de manifest | `package Name<...>`; body executável de manifest; delimiter escolhido somente por tamanho |
| W-757 | grammar normativa G2 | EBNF de types, generic parameters, contract payloads, tuples, arrays e function types pertence ao design | grammar gerada como autoridade; type syntax somente em prosa; um parser por head |
| W-758 | attachment de contrato | `<` toca o head em type, declaration e call; trivia antes do envelope é erro | `Array <T>`; whitespace muda apenas no generic call; formatter decide depois do parse |
| W-759 | payload calculado | expression const usa `<(...)>`; record e list mantêm `{}` e `[]`; head schema decide o kind | expression crua no envelope; delimiter define semântica universal; `<{...}>` cria extension |
| W-760 | precedência de type | um qualifier aplica-se à composição, `?` aplica-se ao type completo e não se repete; `Option<T?>` expressa duas ausências | qualifier stack aberto; `T??`; parentheses agrupam type; null universal |
| W-761 | nested contract close | em type/contract, closes adjacentes são tokens `>` distintos; em expression, `>>` é shift | exigir espaço entre closes; lexer sempre produz shift; regra dependente do type checker |
| W-762 | argumentos estáticos | type application e generic call aceitam as mesmas categorias estruturais, inclusive Bool, String, quantity e size | call aceita menos atoms; named value usa grammar própria; literal posicional depende do head conhecido pelo parser |
| W-763 | function type | qualifier, `unsafe`, callable mode, `async`, `fn`, contract, parameters, return e throws têm ordem fixa; parâmetros não têm labels/defaults | um callable apagado universal; labels no type; ABI inferida; callable externo opcional ambíguo |
| W-764 | grammar normativa G3 | EBNF de patterns, destructuring, match e exhaustividade pertence ao design; Tree-sitter projeta a mesma família de nodes | grammar gerada como autoridade; pattern definido somente por exemplos; árvore distinta por statement |
| W-765 | modalidades de pattern | bindings usam nomes diretos; match exige `let` para captures; `catch error` captura o error completo | todo nome captura em switch; `let let value`; named constant concorrendo com binder |
| W-766 | avaliação de match | scrutinee é avaliado uma vez; guard observa projeções read-only; ownership confirma somente após o guard | mover antes do guard; reavaliar scrutinee; protocol customizado com effects; guard suspende com capture provisória |
| W-767 | payload pattern labeled | enum payload labeled pode selecionar fields e terminar em `...`; forma posicional continua exata | ignorar labels declarados; misturar labeled e posicional; label desconhecido; rest intermediário |
| W-768 | grammar normativa G4 | EBNF de expressions, calls, literals, closures, conditional e forms restritas pertence ao design | tabela do parser como autoridade; precedência somente em prosa; expression definida por exemplos dispersos |
| W-769 | power e prefix | `**` associa à direita; power precede unary no lado esquerdo e aceita prefix no lado direito | `(-2) ** 2` implícito; power à esquerda; exigir parentheses para exponent negativo; `^` como power |
| W-770 | assignment | assignment exige place, avalia uma vez, produz `()` e não encadeia; compound preserva a operation correspondente | assignment devolve value; cadeia Copy-only; duplicação ou move implícito; logical assignment |
| W-771 | conditional expression | `if condition { value } else { value }`; statement sem else produz Unit; Bool sem truthiness | ternary `?:`; if nunca produz value; else implícito; condition numérica |
| W-772 | value block | último expression sem semicolon produz o result somente em contextos nomeados; semicolon preserva discard | todo block retorna tail; function com retorno implícito; formatter remove discard marker |
| W-773 | call e avaliação | receiver, argumentos, literals e operands avaliam da esquerda para a direita; labels não reordenam; caminhos condicionais são lazy | ordem não especificada; avaliação por parameter order; optimizer avalia caminho não selecionado |
| W-774 | prefix de effect e ownership | `try await` é canônico; prefix cobre seu subtree; consuming receiver usa `(take value).method()` | `await try`; effect inferido no caller; take receiver sem grouping; force unwrap |
| W-775 | expressions restritas | `unsafe` produz tail; pipeline usa `return`; transaction usa `commit`; panic produz Never | block geral como value; pipeline builder first-class; return em transaction; unsafe desliga checks |
| W-776 | grammar normativa G5 | tokens, owners, boundaries, commit points e recovery integram G0–G4 sem type information | type-directed parse; newline como boundary; parser gerado como autoridade |
| W-777 | statement partition | parser consome a maior expression no owner atual; semicolon força boundary e permanece quando partition ou tail muda | automatic semicolon insertion; newline encerra expression; remover todo semicolon |
| W-778 | leitura de contract | `<` imediato compromete type application, contract ou generic call; relation exige trivia antes de `<` | resolver pelo type checker; reinterpretar envelope incompleto como relation; exigir turbofish |
| W-779 | owner lexical | owner sintático resolve `>>`, delimiters, colon, `if`, `else`, `catch`, `case` e `while` final | lexer sempre produz `>>`; pontuação resolvida por name lookup; close atravessa owner |
| W-780 | commit point | recovery preserva a forma escolhida depois de token inequívoco e não inventa outra statement válida | `return if` vira return vazio mais if válido; generic incompleto vira relation; repair silencioso |
| W-781 | label estruturado | label nomeia loop ou block; continue avança loop; break sai do target; cleanup lexical sempre executa | goto; reinício no token do label; salto para dentro; block produz value por break |
| W-782 | formatter integrado | format preserva owners, statement partition e tail/discard; parse-format-parse e idempotência são invariantes | formatter type-directed; gravar recovery tree; alterar CST normalizada |
| W-783 | evidência de substituição | corpus mantém exemplo positivo escolhido e input negativo da forma substituída antes do freeze | documentar somente vencedor; contar prose como conformance; aceitar rejeitado no corpus positivo |
| W-784 | resultado semântico S0 | todo AST node produz result type, category, flow, owner delta, effects, facts e evaluation graph normalizados | checker retorna somente type; backend infere flow; facts ficam em diagnostics |
| W-785 | contexto do checker | scope, expected use, owners, effects, controls, facts e const mode entram explicitamente em cada check | estado global implícito; type-directed overload; capability por ambient context |
| W-786 | classes de effect | signature, control e operational effects permanecem distintos na HIR e no tooling | tudo em `throws`; annotations de custo em toda função; esconder capability no runtime |
| W-787 | join semântico | caminhos normais exigem type único, owner disponível em todos, borrow equivalente e facts garantidos | talvez-movido acessível; lifetime annotation pública; branch order muda o join |
| W-788 | análise de loop | entrada, continue e back edge formam fixed point seguro; breaks formam o estado posterior | análise de uma iteração; widening aceita move; loop tratado como call opaca |
| W-789 | initializer de child | initializers `async` e `spawn` exigem call root; parent prepara callable, argumentos e captures; child executa o body | mover expressão composta inteira implicitamente; lazy no await; avaliar argumentos no child |
| W-790 | disciplina de use | expression statement aceita Unit ou Never; outro value exige binding ou `let _` | `must_use` opt-in; descarte silencioso; warning sem erro |
| W-791 | matriz como interface | S0 é a interface normativa AST→HIR; seções de domínio acrescentam facts sem mudar o schema | regras dispersas sem normalização; MLIR define semântica; schema diferente por backend |
| W-792 | diagnostic D0 | record canônico separa code, phase, severity, primary, labels, facts, notes, fixes e root | texto livre como API; schema por renderer; LSP define semântica interna |
| W-793 | span D0 | source ID lógico e intervalo UTF-8 half-open; line e display column são projeções | host path; line/column canônicos; offset que corta code point |
| W-794 | phase e code | phase vem de enum fechado; code possui catálogo, meaning e lifecycle sem reutilização | phase livre; code escolhido pelo renderer; reutilizar ID retired |
| W-795 | facts e notes | facts tipados não carregam secrets; notes usam key e argumentos sem prose localizada | mensagem como único fato; copiar runtime payload; parser de texto para tooling |
| W-796 | fix D0 | machine exige prova e digest; review escolhe intenção; placeholder exige input; edits são simultâneos | aplicar fix stale; `copy` automático; edit sobreposto ou sequencial |
| W-797 | causalidade D0 | root identifica primeira falha; child exige causa independente útil; poison não cria cascade repetida | cada use de poison vira error; root por ordem de emissão; notes como diagnostics |
| W-798 | ordem D0 | source inventory, byte, phase, code e AST ordinal ordenam roots antes de atribuir instance | scheduler order; hash iteration; path absoluto; instance persistente |
| W-799 | truncation D0 | limite emite `W-DIAGNOSTIC-0001` com incomplete e mantém exit failure | truncar silenciosamente; success parcial; contagem dependente de threads |
| W-800 | policy D0 | policy promove warning, nunca reduz error; source suppression fica rejeitada | annotation de suppressão; adapter altera severity; warning ambiental |
| W-801 | conformance D0 | compile-fail compara JSONL byte-exact; corpus design-oracle não alega checker implementado | golden de prose; teste só de code; snapshot manual tratado como output real |
| W-802 | corpus F0 | cada caso possui input aceito, output canônico byte-exact, decisões e semicolons necessários; o status é design-oracle-input | tratar snapshot manual como formatter implementado; exemplo sem decisão; output apenas visual |
| W-803 | bytes F0 | output usa UTF-8 sem BOM, LF, dois espaços, até 120 colunas e exatamente um newline final | preservar newline do host; tabs; trailing whitespace; style configurável na v0 |
| W-804 | preservação F0 | input e output fazem parse sem recovery e possuem a mesma CST nomeada; implementação futura acrescenta AST normalizada e idempotência | comparar somente texto; aceitar recovery; exigir type-check do formatter |
| W-805 | semicolon F0 | cada semicolon retido possui role e mutation que muda a árvore ou produz recovery | preservar sem prova; remover por aparência; automatic semicolon insertion |
| W-806 | quebra de lista | forma plana quando cabe; forma quebrada usa um item por linha, trailing comma e close no nível do head | packing heurístico; trailing comma configurável; vários estilos canônicos |
| W-807 | quebra de chain | binary chain quebra antes do operator; postfix chain quebra antes do access; avaliação e owners não mudam | quebra depois do operator; reordenar operands; depender de types |
| W-808 | trivia F0 | comments permanecem com o owner, top-level declarations usam uma linha vazia e source order não muda | import sorting; mover comment para caber; agrupar por declaration kind |
| W-809 | diagnostic F0 | diferença canônica emite W-FMT-0001 com digests e replacement machine do source completo | diff sem precondition; diagnostic em prose; formatter corrige parser error |
| W-810 | outcome S0 | cada caso produz record JSONL com digest, focus UTF-8, status e resultado ou falha normalizados; design-oracle não alega checker | prose como output; record sem source identity; snapshot tratado como execução real |
| W-811 | baseline negativo S0 | cada caso negativo aponta para baseline positivo único da mesma decisão e nomeia um failureField de SemanticResult | negativo sem contraparte válida; múltiplos failures no mesmo caso; erro aceito por perda de outro campo |
| W-812 | SemanticResult executável | caso positivo materializa resultType, category, flow, ownerDelta, effectSummary, proofFacts e evaluationGraph | somente type e flow; campo opcional por backend; MLIR como autoridade semântica |
| W-813 | evaluation graph S0 | node IDs são locais ao record; edges referenciam nodes declarados e registram ordem, caminhos, transfer, suspension e cleanup | AST ID persistente; scheduler order; lista linear que perde edges condicionais |
| W-814 | normalização S0 | effects são sets byte-sorted, deltas seguem source order, targets de flow são explícitos e facts valem no caminho contínuo | hash order; target inferido no backend; fact de um branch promovido ao join |
| W-815 | namespace de diagnostic | code usa exatamente uma família uppercase e quatro dígitos; subdomínio pertence ao número, meaning e facts | famílias hierárquicas como W-TYPE-CONTRACT; code livre; reutilizar code por subfase |
| W-816 | fronteira lex/parse | literal ou comment não terminado usa W-LEX-0001 em source.lex; parser não reclassifica o mesmo erro | W-PARSE para token incompleto; emitir lex e parse roots para os mesmos bytes |
| W-817 | intenção não observável | newline e ausência de semicolon não geram diagnostic sobre uma partition hipotética; formatter e mutation corpus mostram a árvore real | warning que adivinha statement; ASI; parser escolhe pela aparência |
| W-818 | profile de catálogo | profile fatora phase, severity, facts, labels e fixes idênticos; entrada não pode sobrescrevê-lo e meaning permanece por code | copiar schema em cada entrada; inheritance de meaning; override local silencioso |
| W-819 | facts de parse | parse-syntax exige construct, actual e expected byte-sorted; owner label é opcional e não duplica primary | mensagem como fato; expected em ordem de hash; source text inteiro em facts |
| W-820 | cobertura D0 | índice gerado mede catalogados e referenciados por família e rejeita code fora do namespace | contagem global sem lacuna local; busca manual; ignorar code malformado |
| W-821 | diagnostics de contrato | W-CONTRACT-0001–0005 distinguem slot, kind, predicate, ordem/duplicação e aplicação de envelope | um type error genérico; parser resolve schema; mensagem sem facts estruturados |
| W-822 | corpus de contrato | cada code W-CONTRACT possui baseline positivo único, inversão syntax-valid, outcome S0 e snapshot D0 | apenas exemplo positivo; negative que também falha parse; snapshot sem resultado correspondente |
| W-823 | schema autocontido em fixture | head definido pelo usuário declara slots no próprio source; builtins usam schema normativo; label aponta para declaração quando disponível | ambiente implícito de teste; mock que aceita qualquer slot; diagnostic sem origem do requirement |
| W-824 | falha localizada de contrato | slot/kind/envelope impedem resultType; predicate inválido impede proofFacts; duplicação impede evaluationGraph normalizado | marcar todos os campos inválidos; continuar lowering com slot arbitrário; perder o type base já resolvido |
| W-825 | fronteira de ellipsis | `...` duplicado ou não final usa W-PARSE-0029 porque sua posição pertence à grammar | W-PATTERN sem AST válida; recovery tratado como type error; aceitar rest intermediário |
| W-826 | resultado interno de pattern | pattern focal produz Bool lógico, category value, captures em ownerDelta, narrowing em proofFacts e testes no evaluationGraph; Bool não é expression source | category especial por backend; pattern sem result; expor teste como value ao usuário |
| W-827 | captura por modalidade | binding aceita nome direto; switch/catch match exige `let`; W-PATTERN-0001 falha ownerDelta antes de criar capture | todo nome captura; lookup decide depois; capture implícita por ausência de symbol |
| W-828 | shape e opacidade de pattern | payload, field e category são verificados antes das projeções; erro impede graph ou fact correspondente | projetar por posição ignorando labels; destructuring estrutural de object; field desconhecido vira wildcard |
| W-829 | guard provisório | guard lê projection provisória; move, mutation, escape e suspension emitem W-PATTERN-0006 antes de confirmar ownership | mover e restaurar se false; clone implícito; guard pode suspender com borrow temporário |
| W-830 | range pattern | bounds const e comparáveis produzem ProofFact; incompatibilidade emite W-PATTERN-0007 sem conversão definida pelo usuário | protocol de comparação no match; unit apagada; range inválido tratado como nunca-match |
| W-831 | exhaustividade | somente case sem guard cobre domínio; W-MATCH-0001 registra missingCases do case-set provado | guard conta como cobertura; fallback implícito; warning para enum fechado incompleto |
| W-832 | case inalcançável | case totalmente coberto por predecessor sem guard é error W-MATCH-0002; overlap parcial permanece e entra em explain | warning ignorável; reordenar cases; last-match vence |
| W-833 | enum curto contextual | `.case` exige expected enum local; checker não busca types por nome de case nem lista candidates globais | inferir pelo único enum importado; ranking por proximidade; novo import muda type inference |
| W-834 | corpus G3/S0 | seis families PATTERN e três MATCH possuem baseline único, inversão syntax-valid, outcome e snapshot D0 | exemplo sem schema local; negativo com múltiplas falhas; exhaustividade testada só no runtime |
| W-835 | ownership de diagnostic G4 | parser possui chaining e range fora de posição; S0 possui condition Bool e branch join; G4 não duplica codes para a mesma causa | code EXPR para toda linha da tabela; emitir parser e semantic roots sobre os mesmos bytes; mensagem sem fase proprietária |
| W-836 | assignment em S0 | target exige exclusive place, place é resolvido uma vez e o result é `()`; uso como value falha em `resultType` | assignment devolve value; cadeia Copy-only; mutation de shared place; erro genérico sem category |
| W-837 | aplicabilidade postfix | head resolvido decide se o suffix é válido; optional member exige Option e falha antes do result type | aceitar `?.` em qualquer head como no-op; resolver member antes do head; diagnostic somente no lowering |
| W-838 | expansão `each` | collection é avaliada uma vez; `each` ocupa o argumento final de rest compatível; diagnostic usa índice zero-based em source order | spread universal; expansão intermediária; índice por parameter order; reavaliar collection por item |
| W-839 | ordem de effect prefix | ordem source canônica é `try await`; ausência, redundância e ordem compartilham W-EFFECT-0010 com facts distintos | `await try`; code separado para cada spelling; inferir effect no caller; fix sem schema |
| W-840 | operand de ownership prefix | `ref` e `inout` exigem place; W-1292 permite que o invocation plan possua um rvalue novo e satisfaça `ref T` sem aplicar `ref` ao rvalue; `take`, `copy` e `pin` verificam owner, mobility e origem antes de criar delta | `ref` sobre rvalue, annotation de lifetime ou operação sobre owner place inferida pelo callee |
| W-841 | corpus G4/S0 | sete families de expression, effect e ownership possuem baseline único, inversão syntax-valid, outcome e snapshot D0 | examples sem outcome; um fixture com várias falhas; duplicar errors de parser, type e expression |
| W-842 | ownership de diagnostic const | W-CONST possui sete meanings fechados; a primeira falha que impede ConstValue cria o root e a cadeia permanece estruturada | texto livre do evaluator; um code para toda falha; error por cada caller da mesma cadeia |
| W-843 | operação não const-safe | W-CONST-0001 cobre call, capability ou target semantic que ConstIR não reproduz; target é fact, não outro meaning | usar W-CONST-0007 para target; executar host semantic; fallback runtime num const obrigatório |
| W-844 | ciclo const | grafo falha antes de executar quando contém ciclo; diagnostic registra sequência fechada e todos os members | executar até quota; escolher member por hash; cortar ciclo sem mostrar edge de retorno |
| W-845 | quota const | steps, heap, call depth e result usam W-CONST-0003 com quota, consumed, limit e call chain | um code por resource; wall clock como resultado semântico; quota escondida no compiler |
| W-846 | predicate const false | predicate Bool que rejeita argumento estático usa W-CONST-0004, preserva head e argumento e publica uma `ConstRejectionSlice` bounded; `failure` serializa a causal boundary única quando suficiente e usa `predicate:false` no fallback | W-CONTRACT-0003; type mismatch genérico; aceitar type e inserir runtime check; inferir causa de domínio fora da slice |
| W-847 | failure durante const | typed error escapante e panic permanecem W-CONST-0005 e 0006; nenhum cria fault boundary ou cache entry | converter ambos em quota; panic do compiler; materializar Result oculto |
| W-848 | parâmetro de call const | W-CONST-0007 pertence somente ao argumento indisponível no call site e aponta call e requirement | code de operação target; monomorphization implícita; aceitar descriptor runtime |
| W-849 | corpus CE0/S0 | sete families CONST possuem baseline único do Última Luz, inversão syntax-valid, outcome e snapshot D0 | examples sem evaluator state; negativo com múltiplas causas; tratar snapshot manual como execução ConstIR |
| W-850 | placeholder de code | exemplos de causalidade usam `<root-code>` e `<child-code>`; placeholders não reservam IDs nem entram na cobertura | inventar W-TYPE sem meaning; catalogar exemplo fictício; reutilizar ID depois |
| W-851 | enum subset fora do domínio | W-TYPE-0121 registra base enum, subset normalizado, case e expected type; backend não insere guard | type error sem case-set; aceitar e panic runtime; tratar como match não exaustivo |
| W-852 | conversão implícita recusada | W-TYPE-0122 exige rota total, exata e única; ausência, perda ou ambiguidade preservam candidates e reason | cast genérico; ranking por custo; usar W-TYPE-0120 para expected/actual sem join |
| W-853 | inference genérica | W-GENERIC-0002 registra parâmetro, equation sources, candidates e motivo; constraint valida, mas não inventa solução | escolher conformer único no scope; inferir pelo body; voltar ao overload set |
| W-854 | convergência genérica | mesma instantiation recursiva é válida; crescimento estrutural estrito usa W-GENERIC-0005 antes de wall-clock quota | rejeitar toda recursion; timeout host; shared body esconde type expansion infinita |
| W-855 | corpus TYPE/GENERIC S0 | enum subset, conversão, inference e termination possuem baseline único, inversão syntax-valid, outcome e snapshot D0 | examples positivos somente; fixture com duas falhas; códigos sem facts de máquina |
| W-856 | wildcard de família | `W-DOC-*` e `W-FFI-*` selecionam famílias em policy; wildcard não reserva code nem cria meaning | inventar BUILD/DOC/FFI num exemplo; contar wildcard como entrada; manter ID sem condição normativa |
| W-857 | elegibilidade wire | W-WIRE-0001 pertence à fase interface e registra type path, domain reason, required profiles e alternatives quando boundary portátil alcança value local | falhar no decoder; permitir placement mudar tipo silenciosamente; mensagem sem path ou profile |
| W-858 | corpus wire D0 | Duration portátil e Instant local formam par único; o teste resolve spans e compara facts contra o catálogo | usar somente prose do erro; codec test como prova de type eligibility; snapshot manual como interface checker |
| W-859 | fechamento do catálogo citado | todos os codes com meaning citado possuem schema; o índice gera a contagem corrente; status permanece `projection-seed` até compiler e runners emitirem output real | declarar catálogo final pela contagem manual; reservar toda família; remover status antes do checker |
| W-860 | expansão F0 semântica | repeat rotulado, effect/ownership prefix, receiver consuming, spawn domain, parâmetro de valor, enum subset, capture e transaction possuem pares CST-equivalentes e snapshots D0 | formatar só declarations simples; usar HIR para layout; remover grouping de ownership; reescrever slot nomeado; reordenar constructs para legibilidade |
| W-861 | schema de substituição R0 | cada caso liga um requisito literal da seção 1 a IDs do ledger, tarefa, forma vigente, alternativas e quatro medidas | texto sem ligação; alternativa sem origem; decisão inferida pelo nome do caso |
| W-862 | cobertura progressiva R0 | check comum valida casos presentes e publica `estruturados/69`; `--require-complete` bloqueia o freeze enquanto faltar caso | tratar 69 bullets como auditoria completa do ledger; bloquear todo commit intermediário; declarar cobertura completa por prose |
| W-863 | source comparativo R0 | forma vigente é W corrente; alternativa declara W rejeitado, pseudocode ou outra linguagem e não entra no corpus positivo | parsear alternativa como W válido; omitir language; confundir estudo planejado com resultado executado |
| W-864 | fechamento de cobertura R0 | na denominação histórica, 70/70 requisitos possuíam caso estruturado; o gate do repository exigia completude e o índice distinguia input pronto de estudo executado | deixar o gate progressivo após completar o corpus; declarar ergonomia ratificada pela contagem; omitir formas ainda válidas como alternativas contextuais |
| W-865 | baseline estática R0S | digest do corpus fixa bytes, code points, non-whitespace, linhas e surface lexemes de tarefa e formas; snapshot é reproduzível | contar manualmente; snapshot sem digest; depender de tokenizer remoto para drift local |
| W-866 | limite de R0S | métrica de superfície é descritiva e não escolhe vencedor, não equivale a token de compiler/LLM e não substitui estudo humano ou de modelo | declarar forma menor como melhor; agregar snippets de escopos diferentes; chamar lexeme de token de modelo |
| W-867 | escala de estudo R1 | R1 fecha o protocolo comparativo: bundles Last Light mantêm source base, inputs, outcomes, tarefas e counterbalance; métricas são derivadas por escopo e não alegam preferência humana, modelo ou runtime | extrapolar preferência de snippet; remover contexto da alternativa; contar manualmente |
| W-868 | schema de bundle R1 | bundle fixa source base, casos R0, variantes distintas, inputs, outcomes, quatro tarefas, ordens, blinding, oracle, digests e estado de evidência | prompt solto; variante sem source; input implícito; ordem fixa; metadata revela a forma; resultado sem toolchain |
| W-869 | seed R1 de controle | scanner de carrier do Última Luz compara labels estruturados e flags mutáveis; duas variantes W fazem parse e dois inputs coincidem no oracle host; execução W permanece ausente | medir snippets R0 como programa; comparar W com C de escopo menor; chamar simulação host de runtime W |
| W-870 | máquina de memória M1 | bindings apontam para payloads; PlaceId usa root e projections; LoanId registra mode, origin, stability e parent; move/drop preservam payload e pin separa handle de payload | owner como Boolean; place sem root; loan sem token; pin copia valor; endereço pertence ao binding |
| W-871 | forma do corpus M1 | casos ligam owner, overlap, reborrow, origins, escapes, await, pin, FFI, representation, ABI e join ao Última Luz; snapshot guarda traces byte-exact; W-917 e W-918 fixam a revisão corrente | exemplos isolados sem state; caso sem source; apenas success; resultado sem trace |
| W-872 | limite de M1 | máquina tabelada e teste Node pequeno são oracles host distintos; modelam outcomes de allocation, shared/weak e Arena (historically region) sem executar allocator, atomics, destructor graph, panic, happens-before ou cancellation física | declarar verifier implementado; reduzir memória a M1; chamar outcome lógico de allocator real; apagar segundo oracle por duplicação aparente |
| W-873 | máquina de execução E0 | grafo separa lifecycle da task, sequência local e edges de publicação; cancelamento não cria happens-before | usar ordem do scheduler como semântica; publicar por cancel; observar outcome antes de cleanup |
| W-874 | corpus E0 | 73 sequências e 677 operações ligadas ao Última Luz cobrem lifecycle, cancelamento, fail-fast, drain, races, atomics, wait/notify, extent, subtrees de ticket e 10/10 origens happens-before | apenas casos aceitos; evento sem source; atomic acquire sem relação observada; trace completo repetitivo |
| W-875 | limite de E0 | oracle host recebe task, storage/extent, lifetime e reads-from resolvidos; não prova checker, scheduler, liveness, fairness, device scope, reclamation ou distribuição | declarar runtime implementado; inferir ausência de race por execução única; tratar E0 como memory model completo |
| W-876 | máquina de boundary effects B0 | service call, transaction e pipeline mantêm lifecycles separados; todos carregam effect identity e distinguem confirmação, falha conhecida e incerteza | um Boolean committed; cancel como rollback; transaction usada como pipeline; pipeline com rollback fictício |
| W-877 | corpus B0 | 39 sequências e 320 operações ligadas ao Última Luz cobrem 17 transições críticas, closed/output gates, commit, abort, retry policy, DAG, drain e capabilities | somente happy path; unknown sem effect ID; retry com call ID novo e effect ID novo; node sem cleanup |
| W-878 | limite de B0 | oracle host recebe admission e evidence resolvidos; não prova adapter, transport, queue, storage, codec, clock, deduplication, crash recovery ou distribuição | declarar exactly-once; chamar snapshot de fault injection real; inferir durabilidade de transição em memória |
| W-879 | identity de supply chain P0 | lock fixa estrutura; recipe fixa content e inputs; artifact fixa outputs; release liga records sem ciclo autorreferente | um hash universal; output dentro da própria recipe; builder identity como input semântico |
| W-880 | resolução P0 | maior versão compatível por realm, features aditivas e member local compatível tem precedência; member incompatível falha sem fallback silencioso | first match; várias versões na mesma realm; trocar member por registry; defaults de feature ocultos |
| W-881 | evidência de release P0 | comparação rejeita incompletude antes de inputs e artifacts; quorum exige builder, operator, credential e execution root independentes | contar jobs; comparar só payload; usar executor como input; combinar auditoria e reprodução numa nota |
| W-882 | corpus P0 | 44 casos e 379 operações ligados ao Última Luz cobrem resolver, lock, CAS, mirror, recipe, rebuild, roles e estados de release | exemplos sem state; digest manual; somente happy path; mirror tratado como authority |
| W-883 | limite de P0 | oracle host recebe facts de assinatura e metadata; não prova SemVer completo, TUF, Sigstore, download, archive, sandbox ou rebuild real | declarar registry implementado; tratar SHA-256 do oracle como algoritmo eterno; chamar duas simulações de builders independentes |
| W-884 | labels estruturados ratificados | label nomeia loop ou block lexical; `continue` avança o driver; `break` sai do owner; nenhuma forma reinicia no token do label | label solto; `goto`; salto para dentro; confundir `continue label` com task yield |
| W-885 | documentação de ausências | cada forma deliberadamente ausente mostra forma recusada, substituição W, diferença observável e caso comparativo | lista de nomes sem source; omitir motivo; apresentar alternativa recusada como syntax aceita |
| W-886 | corpus R1 ampliado | na denominação histórica, 25 bundles, 62 variantes e 100 tarefas cobriam os domínios R1 registrados com source base, inputs, digests e oracle; 41/70 casos R0 foram promovidos | extrapolar R0; variante sem contexto; outcome não fixado; chamar host oracle de execução W |
| W-887 | estudo R1 de units | `<unit-expression>` é a forma vigente; square units são rejeitadas como indexação, preservando cálculo, ownership, diagnostics e package facts | comparar snippets sem fórmula; tratar parse como type-check; escolher por contagem de caracteres |
| W-888 | estudo R1 de imports | flattening e module binding são formas vigentes; ambas preservam resolução, colisão, provenance, visibility e source migration, sem preferência normativa inventada | proibir uma forma antes do estudo; comparar conjuntos de imports diferentes; omitir colisão preparada |
| W-889 | estudo R1 de fail-fast | tuple-await e espera lexical preservam o error application e cleanup fail-fast; a variante lexical non-fail-fast é rejeitada, e observation tick/cancel são somente evidência host | mudar o error esperado; depender do scheduler host; confundir latência observada com ordem semântica universal |
| W-890 | cobertura total de ausências | cada alternativa do ledger declara se muda source; toda ausência comparável liga forma recusada, substituição W, diferença observável e caso R0 antes do freeze | tratar os 70 requisitos da denominação histórica como auditoria total; listar nome sem source; exigir caso de alternativa interna sem diferença visível |
| W-891 | catálogo std verificável | profiles cobrem 377 APIs em 29 módulos, 31/31 requisitos e oito carriers sem interface missing; declarations estão draft-ready, 92 superfícies são verificadas e 23/23 providers continuam missing | contar arquivo como cobertura, inferir API sem scan, tratar provider missing como execução ou duplicar o grafo de readiness |
| W-892 | context de host | context público é struct nominal encapsulado sobre provider interno versionado; entry fornece owner e interface lógica esconde RuntimeContext e storage; build Context e HTTP Context mantêm interfaces separadas | existential universal; object com identity; mapa ambiental; singleton; syntax especial por SDK |
| W-893 | build Context | read usa overloads `Input<String|Bytes>` const e limite efetivo; write usa overloads `Output<String|Bytes>`, consome value e possui effect linear por output; codecs são UTF-8 estrito ou bytes identity; `.codec` ocorre somente em `read(Input<String>)`; bounds menores do provider vêm do host profile/toolchain plan e entram na recipe key; operações concorrentes exigem bindings distintos; cancellation invalida a tentativa; o host publica um action-result/manifest atômico após success | filesystem sandbox como API; intrinsic genérico; codec universal; overwrite concorrente; output incremental implícito; Context apagado; commit/rollback ou transaction no handler; duplicate catchable que ainda publica |
| W-894 | superfície Web | client e server compartilham Headers ordenado, Request e Response move-only, URL tipada, BodySource fechado em seis cases, Body consuming e clone bounded; Blob compõe W e FormData separa lista de multipart; `Context` e `serve` são extensions; provider `std.http@1` continua missing | API HTTP paralela; copiar JavaScript/Web IDL; BodyInit universal; aliases `path`, `query` ou `decodeJson`; clone sem bound; Blob com authority; multipart parcial |
| W-895 | profile WinterTC | `web-common@2025` fixa e classifica o Minimum Common Web API como exact, adapted, extension, browser-only ou não aplicável; W não declara conformidade ECMAScript | alegar conformidade formal; copiar `globalThis`; seguir living surface sem snapshot; chamar extension de API portátil |
| W-896 | URL portátil | `URL` guarda record canônico opaco; getters textuais são views O(1); base é `ref URL`; mutation usa errors tipados; `searchParams()` devolve snapshot owned atual; edição `inout` fallible e scoped faz commit único e distingue query ausente e vazia; `URLSearchParams` preserva ordem, repetição, form encoding e sort UTF-16; WPT fecha o provider | doze Strings owned; params eager; cache interior; callback para leitura; property Web com allocation escondida; `SameObject`; parser parcial no contrato; aliases HTTP; silent no-op; alias mutável escapável; URLPattern no SDK0 |
| W-897 | intrinsic interno da std | `foreign intrinsic from "provider@major"` é primitive unsafe não exportável, restrita a módulos internos do SDK; manifest versionado fixa signatures, effects e gates; wrapper W safe contém a boundary; não existe ABI pública nem capability implícita; bootstrap usa allowlist e os mesmos digests | foreign symbol comum; `fn<C>`; annotation nova; provider ambiental; declaration por package; intrinsic público; authority por nome |
| W-898 | ReadableStream portátil | owner move-only atende diretamente a `Stream`; erasure interna pode usar box/indirection, SBO ou monomorfização com witness exato; `next` é o cursor único; `cancel` segue W-330, com handle inert antes de success, Failure ou task cancellation, sem owner restaurado e com drain estruturado; drop é idempotente e best-effort; BYOB é `ByteSource.read` sobre `Bytes` growable; sem prefetch, controller, reader object, `IncomingBody` público ou `any`; tee exige `Duplicable`; o genérico limita lag em itens e depende do allocation budget, sem promessa de memória transitiva; o overload de bytes limita lag exato e serve clone HTTP; COW preserva independência; branch drop não cancela a irmã; cleanup e pull upstream ocorrem uma vez; pipe fica direção até fechar writable/transform; provider continua missing | runtime de stream paralelo; façade declarada zero-cost; witness apagado sem prova; `getReader`; `IncomingBody`; rollback em catch; retry de cancel; resource owner dentro de error duplicável; lock dinâmico em safe W; tee Web unbounded; item count chamado de byte/memory bound; `sizeOf`, callback de custo ou medida transitiva; tee zero como rendezvous implícito; chunk copy obrigatório; BYOB por ArrayBuffer ou fixed-buffer identity; HWM oculto; pull reentrante; task detached; `pipeTo` antes de WritableStream; cancel como error de task; clone HTTP com pump próprio |
| W-899 | AbortSignal portátil | `std.abort` adapta o first-wins Web sem substituir a cancellation monotônica de W; `AbortReason` é `Error & Copy`, fechado e bounded; signal duplica handle O(1), devolve reason por valor, oferece `throwIfAborted` tipado, espera pelo reason sem perder wake, não concede authority e não é `WireValue`; controller é move-only, first-wins atômico e drop não aborta; timeout zero já está abortado, timeout positivo possui timer-resource independente do creator/root, continua ao escapar e cobra o execution domain sem manter task viva; falha de timer budget publica cancellation e solicita cancellation estrutural; `any` preserva o nome Web e valida antes de registrar: argumentos diretos e folhas pending únicas após flatten/dedup precisam caber no mesmo `maximumSources` por result, inputs abortados só vencem depois das duas validações e cada folha pending recebe uma registration; total vivo do execution domain depende do allocation/admission budget do provider, não do fan-in de uma call; o DAG não cria cycle; Request recebe signal interno dependente; error Web versus task cancellation segue settlement/commit e sempre drena I/O; RPC geral usa automatic call cancellation, Request usa control frames e live-control edge fica alternativa futura; provider continua missing | colocar em `std.runtime`; transformar cancellation de task em application error; reason dinâmico, message, error arbitrário ou resource owner; renomear `any` como `combining`; EventTarget e callbacks no SDK0; signal com authority; controller duplicável; abort implícito no drop; ligar timeout genérico ao creator/root; wall clock; timer ou observer sem bound; tratar fan-in de uma call como limite global de dependents; validar winner antes dos bounds; permitir que terminal direto ou `any` pending contorne limite; ordem total fictícia para races; handle como `WireValue`; AbortSignal remoto geral no SDK0; implementação safe W sem atomics e hooks provados |
| W-900 | JSON bounded SDK0 | `std.json` fornece `Encodable`, `Decodable`, `Codable`, `Limits` Copy/Equatable com defaults finitos, profiles `.interoperable`/`.rfc8259`, `ValueKind`/`ValueConstraint`, errors tipados com `Location`/`SyntaxKind`, `typeMismatch` e `invalidValue`, cursors `Writer`/`Reader` opacos e scoped com callbacks `some take fn`, `Number` nominal validado, `Value` sum type explícito, Object equality map-like com insertion order preservada no re-encode, synthesis somente por conformance JSON fechada (Array/fixed array/Option/Map<String,V>; sem tuple), unknown policy explícita e duplicate rejection; encoder compacto define escapes, shortest-round-trip e signed zero sem alegar canonical JSON; HTTP usa `json.*` e exige `maximumBytes` ou `json.Limits`; adapters direcionais de Command/AppResponse/WifiSession estão no produto de referência; provider `std.json@1` continua missing | serializer universal, reflection, `Any`, annotation, macro, metatype, cursor escapante, route unlimited, duplicate last-wins, NaN/Infinity, conformance Codable global de domain types ou codec automático para Display/outros schemas; chamar o output de JCS ou identity de signature/content |
| W-901 | HTTP SDK0 | `std.http@1` possui handles privados para Request, Response, body, Context e serve; BodySource aceita String, Bytes, URLSearchParams, Blob, FormData e ReadableStream; Blob usa shared Bytes sem provider; FormData é lista bounded e multipart fica no provider HTTP; RequestInit/Override separam defaults, inherit e none; policies são enums fechados; clone usa tee bounded; JSON compõe std.json; Context usa bindings tipados; serve depende de std.net/http missing; adapters usam errors tipados e RFC 9457 | BodyInit universal; clone sem bound; Context ambiental; intrinsics JSON genéricas; template irrestrito; mutation direta de Headers; Blob com authority; boundary multipart do caller; JSON lossy; claims de execução sem provider |
| W-902 | rede SDK0 | `std.net` é módulo com provider intrinsic único `std.net@1` missing; `Network` é capability nominal move-only sem initializer público; address values, resolve/connect, TCP split/lifecycle e UDP bounded possuem contracts explícitos; W-1252 adiciona halves UDP direcionais únicos sem esconder sharing ou alterar datagram semantics | socket global, constructor de capability, network String names, raw sockets, fd inheritance, socket-option escape hatch, transports sem contract próprio, reliability/ordering no UDP, cancelamento físico não provado ou protocol genérico de datagram sem segundo transporte |
| W-903 | Quantity/SI/IEC | `PRC0-W-903-current` e `PRC0-W-903-adversarial` fecham canonical duration, affine point/delta, IEC exact bytes e fixed JSON token; `Temperature` e `TemperatureDelta` são dimensões distintas com references `K`/`deltaK`; imports explícitos exigem `si.s`, `si.min`, `iec.KiB`, `iec.byte` e `degC`; API de conversão usa `iec.byte`, enquanto o JSON reference token permanece `bit`; wWire/schema/provider claims continuam missing | autoridade PRC0; implementação Quantity compiler/std JSON+wWire é W-1443; rejeitado registry ambient, alias `B`, tokens JSON alternativos e `<s>/<min>/<KiB>/<degC>` sem binding |
| W-904 | texto inteiro canônico | integers fixed, `Int` e `UInt` atendem `Display` decimal ASCII; parse decimal é estrito e typed; W-1253 fecha overloads radix `2...36` sem mudar Display ou schemas | locale numérico, whitespace/prefix/underscore implícito, formatar signed min por negação intermediária, radix alterar canonical decimal |
| W-905 | Duration exata | `Duration` continua signed i128 nanoseconds com layout opaco; getter read-only `nanoseconds: i128` e constructor total `Duration(nanoseconds: i128)` expõem o valor sem expor layout; refinements exigem narrowing checked em input runtime; JSON genérico de Duration fica rejeitado e cada endpoint escolhe schema; Wifi usa String decimal `remainingNanoseconds` | layout público, float/infinity, narrowing implícito, serializer Duration universal |
| W-906 | adapters direcionais de JSON | domain types não conformam diretamente `json.Codable`; cada endpoint possui adapter endpoint-owned/dedicated inbound somente `json.Decodable` e outbound somente `json.Encodable`; o módulo pode exportar a plumbing necessária ao host sem criar `json.Codable` no type de domínio ou um contrato global; schemas/versionamento ficam locais sem reflection, annotation ou conformance global; unknown members reject e duplicate sempre falha; IDs u64/u128, Money i128, Duration nanoseconds e completedOrders u64 usam decimal String canônica com parse/display no carrier explícito; u16/u32 usam JSON number; tokens são explícitos ASCII kebab-case; encoder põe `kind` primeiro e `notes` usa null; borrowed AppResponse adapters encodam sem mover/copiar o modelo; Quantity usa adapters nominais fixos para tokens de unit; Problem Details usam `code` estável e status derivado do code; Command, AppResponse e WifiSession têm source oracles provider-gated | conformance direta de domain type congelando um schema global, reflection/universal serializer, tokens derivados de source names, shape dependente de runtime value, envelope genérico que mistura decode e domain errors, status/code livres, unit String arbitrária |
| W-907 | kernel M1 de place loans | HIR usa PlaceId root estável + projections de field, tuple, enum payload, index, range normalizado `[start,endExclusive)`, dereference e view extent; LoanId registra place, mode, origin, emissão/fim, estabilidade e parent; move/drop do root observa descendants | contador global de borrow, place textual sem root, partial move, lifetime metadata no value |
| W-908 | overlap e mutation | paths iguais ou prefixos sobrepõem; fields conhecidos distintos, índices constantes distintos e ranges separados são disjuntos; ProofFacts atuais só refinam index, range e view e identificam os prefixos PlaceId exatos; active variant identifica enum place e case; enum variants distintos, dynamic index/range, deref opaco, union, packed, unaligned, opaque e foreign boundary sobrepõem; mutation estrutural conflita com todo storage | alias por nome, fact sem root/path, guardar ProofFacts no loan, aceitar `end` na HIR, prova limitada por budget, tratar deref ou union como disjunto |
| W-909 | reborrow | shared nasce de shared ou exclusive; exclusive nasce só de exclusive; child deve ser igual/subplace descendente, congela acesso direto do parent e o restaura no end; cópia shared de child preserva parent e conta outra obrigação; `accessLoan` lê shared e lê/escreve exclusive não congelado; children disjuntos coexistem | parent ativo consumível, cópia de child sem parent, exclusive de shared, sibling/widening reborrow, reborrow que usa annotation no source |
| W-910 | valores lifetime-dependent | cada payload possui edges individuais shared/exclusive para owner payload/place ou owner slot de interface; OriginSet é projeção deduplicada, mas duplicatas bloqueiam; composição inclui tuple, struct, object move-first, enum/payload, Option, Array, collection, closure e existential; erasure não apaga edges; stored fields são permitidos sem ARC ou referent ownership | origin sem owner identity, rejeitar stored field local, apagar provenance por erasure, metadata de lifetime em runtime |
| W-911 | containers de refs | Array<ref T> possui owner de descriptor/storage e edges para cada referent; insert/join adiciona edges; join lê source distinto e self-join exige snapshot; remove só reduz quando nenhuma duplicata resta; clear libera edges; o grafo simbólico não limita quantidade por proof budget | join sob loan exclusive, self-join implícito, contador fixo de origins, invalidar por budget, tratar descriptor como único owner |
| W-912 | escapes e await | `.lifetimeIndependent` é ausência de origin dinâmica; static/immortal passam somente esse gate; external escape rejeita edge dinâmica; channel/task exigem transferability, service exige WireValue + transferability mesmo local, persistence exige schema e foreign retention exige FFI; await resolve cada referent vivo e estável, com no-conflict e cleanup/cancel drain | origin immortal como authority de boundary, cópia implícita para escapar, task detached com borrow, usar estabilidade do aggregate para referent, annotation de lifetime como correção |
| W-913 | pin e self-reference | pin exige zero LoanId e zero dependency edge dirigida ao payload; payload pinned tem root estável distinto do handle; mover handle é permitido com obrigação ativa, drop de handle/payload falha, mover payload não; initializer self-referential safe é rejeitado | self-reference por initializer comum, unpin implícito, pin que apenas marca pointer |
| W-914 | provenance de interface | body infere mapping exato e separado para cada result dependency slot e slot ausente falha; sem body instance/member usa receiver compatível como origem autoritativa; static/free/protocol bodyless exigem exatamente uma entrada compatível e rejeitam duas ou mais com `W-BORROW-0011`; `init` com resultado borrowed/view é rejeitado; zero input só aceita result independent/static; import expectation e SemanticInterfaceKey do provider coincidem; oracle ignora inferredMapping bodyless; witness e lock detectam mudança | key opcional unilateral, igualar interfaces próprias de módulos distintos, `ref<sources: ...>` no source, colapsar result slots, mapping all-inputs, fallback implícito, witness divergente, docs no semantic key |
| W-915 | FFI de refs | safe ref/inout para C é call-scoped/noescape; retenção exige owner/lease pinned, destroy e unregister; opaque C return, packed, unaligned, union e opaque permanecem conservadores; fn<Language> passa lifetime somente com adapter W confiável | pointer persistente sem lease, free por caller, inferir lifetime de header ou body opaco |
| W-916 | cleanup e diagnostics M1 | deinit/cleanup preserva edges usadas pelos fields; NLL termina no último uso sem deinit observável; diagnostics distinguem overlap, dependency conflict, dependent escape, unstable referent, unstable suspension e frozen parent e sugerem materialize/copy/take, split/clear, reorder ou pin | hidden runtime lifetime, uma mensagem genérica, fix-it que inventa annotation |
| W-917 | endurecimento executável M1 | schema M1 fixa 185 casos e 606 operações; fecha subplace reborrow, child copies, owner access, ProofFacts ligados ao PlaceId, dependency authority, borrow/storage origins, Arena budget/close (formerly region), rehome, shared/weak lifecycle e ciclos, erasure inline/spill, alias borrows, failure consuming, boundary gates, interface mappings, referent await, pin, construção pinned, cleanup e adapter W; preserva owner, representation, allocator e WAbiKey | aceitar origin implícita, fact sem place, endereço do aggregate como prova, share reparar borrow, mobility declarada na call, self-proof estrangeira, duplicar check M0, chamar oracle de compiler/runtime |
| W-918 | authority de dependency edge | cada edge é obrigação de lifetime e capability; shared permite read; exclusive permite read/write; criação valida loans e edges de modo atômico; IDs são únicos; selector usa ID xor origin e a abreviação exige origin única | edge apenas como bloqueio; write por shared; origin first-match; dois selectors; conjunto parcialmente criado após conflito; operação source `accessDependency` |
| W-919 | estudo R1 de contratos sequenciais | `StagePath` usa contracts sequenciais e `StaticList<T><(predicate)>`; a forma fused é rejeitada, com source, validator, inputs, diagnostics e outcome preservados | snippet isolado; mudar o algoritmo; tratar static list como lista universal de constraints; chamar oracle host de evaluator W |
| W-920 | cobertura de promoção R1 | índice e checker contam IDs R0 únicos ligados a bundles; na denominação histórica, 41/70 media planejamento, não evidência humana, de modelo ou runtime | contar referências duplicadas; dividir bundles por requisitos; chamar promoção de ratificação; esconder o denominador |
| W-921 | inversão semântica de contrato fused | S0 compara `StaticList<T><(predicate)>` com `StaticList<[T, (predicate)]>`; a segunda forma faz parse e falha com W-CONTRACT-0002 no slot `T` antes de resolver o predicate | rejeição somente em prosa; W-CONTRACT-0005 no envelope errado; interpretar lista como constraints; emitir erro secundário de `.member` |
| W-922 | diagnostic de receiver consuming | place owned e movível em member `take fn` exige `(take receiver).member()`; call sem marker produz W-OWNERSHIP-0011 com place/type/category antes do move e não recebe fix automático; receiver não owned falha pela incompatibilidade anterior | inferir take pelo member; consumir e continuar checking; chamar todo receiver de binding; inserir fix que muda ownership; restaurar owner no error |
| W-923 | estudo R1 de receiver consuming | `CommandStream.finish()` exige marker consuming explícito; success, error, cancel, cleanup e owner indisponível permanecem iguais; inferência implicit é rejeitada | comparar APIs diferentes; omitir error; usar owner depois da call válida; chamar host oracle de runtime W |
| W-924 | formatter de receiver consuming | F0 preserva `(take stream).finish()` e prova CST equivalente; os parênteses pertencem ao operand de ownership e não são style opcional | remover grouping; formatar como `take stream.finish()`; mover `take` após member lookup; snapshot sem decisão |
| W-925 | ausência de domain default | header de módulo e execution profile não publicam `parallelDefault`; `spawn` e `parallelMap` exigem domain no call site; S0 preserva a rejeição do slot de módulo com W-CONTRACT-0001 | import escolhe executor; default em cada módulo; módulo concede budget; duplicar profile em source |
| W-926 | estudo R1 de domain (retired) | decisão anterior tratava `<domain: .compute>` como variante de schema; W-1160/W-1162/W-1172 tornam as duas formas equivalentes e preservam o dispatch sem prometer simultaneidade | tratar domain como thread; aceitar alias duplo no mesmo slot; capacity um invalida spawn; chamar oracle de scheduler |
| W-927 | formatter de domain (retired) | F0 preserva a forma positional ou named escrita e a HIR normaliza ambas; spacing e statement boundaries ficam canônicos | apagar label; inserir label; reescrever domain como frase `on`; inferir pool no formatter |
| W-928 | proveniências de borrow e storage | `OriginSet` mantém dependency edges; `AllocationOriginSet` mantém allocator instance, lifetime, mobility, deallocator e adoption family; move transfere ambos, mas nenhum substitui o outro | um origin set universal; allocator como borrow comum; metadata em pointer; lifetimeIndependent apagar storage origin |
| W-929 | operação interna de shared | a HIR pode usar uma operação conceitual de construção/retain que exige payload lifetime-independent, preserva origins internas e cria origin própria para control block; o source vigente expõe somente a declaração `shared T` | `share` como call W; share prolongar borrow; shareable reparar lifetime; ARC universal; control block sem allocator origin |
| W-930 | falha de operação consuming de storage | failure de `pin`, construction shared SHC0, `rehome` e `erase` consome e destrói o source uma vez, limpa destino parcial e não publica handle/address/existential; recovery custom agora usa `try` na construction expression | restaurar binding implicitamente; `share` current; source parcialmente válido; leak do destino; publicar pointer ou existential antes do success |
| W-931 | composição strong/weak | último strong executa deinit uma vez; weak mantém somente control block e allocator origin; leitura contextual após strong zero devolve none; último weak libera block; borrow shared fica ligado ao strong handle de origem; `inout` exige owner único; cross-domain exige payload shareable, contador thread-safe e todas origins móveis | weak acessa payload; `.upgrade`/`.strong`/`.strong()` current; borrow ligado ao contador global; alias sem relação bloqueia drop; ressurreição; contador local cruza domain; shareable ignora allocator mobility |
| W-932 | interface de storage owned | `AllocationOriginMap` liga paths de storage do result a allocator inputs, default do product ou runtime owner; ele é separado do borrow mapping e participa da SemanticInterfaceKey | esconder lifetime do allocator; colocar mapping somente em docs; tratar owned result como lifetime-independent por definição; expor mapping oculto na C ABI |
| W-933 | expansão de composição M1 | a tranche adiciona 21 casos e quatro testes independentes para budget/close, rehome, local versus cross-domain, share dependent, failure consuming, lifecycle strong/weak, borrows por handle e interface storage; W-938 estende o snapshot corrente | exemplo sem state; somente success; simular thread scheduler; chamar origin lógica de allocation física |
| W-934 | fronteira do design freeze | contratos, alternativas, exemplos, modelos host, vetores e spikes descartáveis fecham design; formatter, checker, HIR, scheduler, runtime, providers e compiler de produção começam depois e podem reabrir uma decisão por evidência | exigir implementação ampla para definir a linguagem; chamar oracle de produto; congelar sem modelo adversarial; impedir revisão após evidência real |
| W-935 | auditoria de decisões para freeze | R0 classifica o eixo source; F0/S0/M1/L0/E0/B0/P0/TAB0/DLPack podem ligar decisões ao eixo oracle; decisões mistas declaram todos os eixos obrigatórios; as demais exigem escolha interna, fallback provável, histórico, policy ou waiver; o índice publica a contagem corrente e `--require-complete` permanece desligado até o gate | tratar 68 casos como auditoria do ledger; classificar por keyword; ausência de entrada significar aprovação; somar eixos sobrepostos como decisões distintas; aceitar um único eixo para decisão mista; manter planilha manual fora do repository |
| W-936 | estudo R1 de callables | formas `fn`/`some fn`/`any fn`, callable modes, capture, erasure e function-value contracts permanecem vigentes; callable universal e protocols nominais são rejeitados, sem mudar ownership, effects, ABI ou recovery | snippet sem capture; comparar somente tokens; esconder segunda call; transportar default no valor; widening implícito; chamar host oracle de execução W |
| W-937 | storage de erasure | `any P` e `any fn` usam policy versionada de inline/spill; contextual erasure segue OOM normal; `try erase(take value, using:)` é consuming e fallible; box adiciona AllocationOriginMap; `some` e `ref any` não alocam só por opacity; M1 fixa inline, spill, failure, dependency e interface mapping | box universal; SBO ambiental; esconder allocator origin; restaurar source na falha; carrier existential em C/wire |
| W-938 | erasure executável M1 | oito casos e dois testes independentes derivam inline/spill pela policy, preservam payload origins e dependency edges, adicionam box origin, bloqueiam close prematuro, rejeitam spill proibido, convertem budget exhaustion em failure consuming e não publicam target parcial; o corpus M1 contém essa evidence | escolher storage por flag do caso; apagar origins; allocation em inline; source restaurado; target parcial; budget rejeita antes do consumo; chamar layout lógico de ABI física |
| W-939 | publicação atômica executável | release sequence inclui RMW contígua e termina em store; CAS success modifica e failure somente lê; fence exige reads-from; atomic order não concede borrow ou lifetime; E0 fixa modification order, seq-cst order, extent e exclusividade | release/acquire prolonga owner; CAS failure modifica; store relaxed continua sequence; duas fences publicam sem atomic; misturar width ou view |
| W-940 | fences atômicas | `std.atomic.fence` e `compilerFence` são T0, unsafe, estáticas e sem relaxed; default scope é system, compiler fence exige sameProcessor e scopes de device ficam T2 | fence T1 dependente do host; fence safe; argumento runtime; relaxed fence; compiler fence entre processadores |
| W-941 | interface esperada no link W exact | cada import registra providerInterfaceKey, semantic/physical signatures e fingerprints; compara essa expectativa com o provider, nunca o SemanticInterfaceKey próprio de módulos consumer/provider distintos | igualar keys próprios dos dois objects; confiar só no symbol name; usar API source como ABI |
| W-942 | igualdade física W exact | WAbiKey inteira coincide; call shape inclui convention, direct/indirect, register class, extension, sret/byval/byref/inalloca, alignment, hidden parameters e address spaces; somente representations alcançadas pela signature coincidem e diferenças privadas são livres | size/alignment como call ABI; comparar todo tipo privado; thunk por heurística; omitir hardening ABI |
| W-943 | carriers da façade C | `unsafe fn<abi: .c>` usa somente carriers C; borrow de parâmetro é call-scoped com extent, retorno pointer fica raw, owner leva destroy/context-drop, callback persistente leva context/pin/destroy/unregister, enum/refinement valida integer, safe borrow prova owner/lifetime/bounds/alignment/noescape e runtime context é explícito | String/owner W direto; caller free; pointer retornado ou retido como ref; C enum como enum W; hidden runtime; panic convertido implicitamente |
| W-944 | layout foreign v0 | W comum não possui `packed`, `aligned` ou union source; header import pode descrever packed/over-aligned, mas field unaligned só usa cópia; export C gera layout natural e casos especiais usam opaque + wrapper C | `struct<layout: .c, packing: 1>` vigente; borrow unaligned; offset manuscrito; foreign union safe; layout C universal |
| W-945 | pareamento de header C | header, library, target slice, digests e conformance assertions ficam ligados no artifact index; timestamp e local path não entram | header universal por filename; combinar slices; confiar em include guard; header não reproduzível |
| W-946 | reader lógico ABI L0 | schema e required features são fechados; fields opcionais podem ser ignorados; reader bounded rejeita missing/unknown required, duplicate, dangling reference, excess bytes/strings/count/depth; JSON é projeção, não bytes ABI | nota ausente compatível; feature desconhecida ignorada; decode ilimitado; snapshot JSON virar object ABI; confundir o reader lógico com o container físico |
| W-947 | recuperação de artifact ABI | ordem fixa é artifact exato, rebuild do source fixado, boundary canônica já declarada e erro; disponibilidade local não cria boundary | C/component fallback implícito; adaptar layout; escolher dynamic por filename; preferir boundary ao source |
| W-948 | envelope físico WMeta1 | header big-endian de 32 bytes, tamanho total exato, directory antes do payload, offsets por prefix sum e nenhum padding interno | offsets armazenados; gap ou overlap; trailing bytes; cast para struct nativa; alinhamento do object format entrar no container |
| W-949 | subset CBOR WMeta1 | core deterministic RFC 8949, lengths definidos, keys unsigned em ordem de bytes, uma raiz exata e representação numérica W por bytes tipados | float ou tag CBOR; duplicate key; indefinite item; map key textual; canonicalização dependente da library |
| W-950 | profiles e registro de chunks WMeta1 | profiles `interface` e `objectAbi` fecham os chunks críticos; kinds core são append-only; extensão opcional desconhecida pode ser pulada e crítica desconhecida falha | profile aberto; core opcional; kind privado em artifact público; extensão crítica silenciosa |
| W-951 | limites e publicação WMeta1 | ceilings W0 são validados antes de allocation; reader usa fases bounded e só publica uma view core immutable depois de validar todo o core | decode ilimitado; publicação parcial; overflow de soma; chunk vazio; ampliar limite por profile |
| W-952 | evolução WMeta1 | feature IDs são sorted, unique e append-only; a lista W0 é vazia; schema major incompatível falha e minor novo exige features para toda semântica obrigatória | reinterpretar field ID; feature desconhecida ignorada; minor novo tornar field obrigatório sem feature |
| W-953 | embedding e strip WMeta1 | sidecar, ELF note, Mach-O section, COFF section e Wasm custom section preservam bytes completos; wrapper e padding ficam fora; metadata fica retida por default | bytes específicos por target; depender do padding externo; strip silencioso; custom section definir validade Wasm |
| W-954 | integridade e open modes WMeta1 | digest SHA-256 tagged por chunk detecta corrupção; artifact record autentica o container; `directory`, `core` e `full` concedem autoridades distintas | digest conceder confiança; directory conceder interface; release ignorar chunk opcional; algoritmo crítico desconhecido |
| W-955 | corpus e readers WMeta1 | 42 casos byte-exact cobrem seed e mutations; readers Bun e C independentes precisam concordar; ambos são design oracles, não produto | fixture só positiva; dois wrappers sobre a mesma library; snapshot manual; chamar oracle de compiler |
| W-956 | domínios de metadata | WMeta1 contém interface e ABI públicas; build record CBOR, wWire runtime e cache AST/HIR continuam formatos separados | container universal; publicar HIR como compatibilidade; usar wire RPC para object metadata; colocar provenance mutável na interface |
| W-957 | receipt físico de allocation A0 | todo bloco não vazio carrega layout pedido, capacidade útil, alinhamento e token opaco de origem; zero bytes produz `noStorage` sem chamar o provider | pointer nu sem origem; sentinela alocada para zero; inferir provider pelo endereço; expor metadata mutável ao programa |
| W-958 | resize e relocation A0 | `resize` retorna success, unsupported ou failure preservando o receipt anterior; fallback pertence ao caller e só storage provadamente relocatable usa remap raw | `realloc` universal; perder o bloco em failure; executar move ou destructor dentro do provider; mover com loan, pin ou address lease ativo |
| W-959 | profiles físicos A0 | failure, progress, domains, mobility, resize, bulk release, limits e hardening são facts por provider e operação; target e recipe fixam provider e versão | escolher por nome; tratar progress como escala total; allocator geral em interrupt; confundir mimalloc v1/v2 com v3 |
| W-960 | retirement e reclamation A0 | retirement encerra acesso lógico; reclamation libera reuse físico somente depois de cleanup e do gate específico do owner | pointer atômico conceder lifetime; free executar destructor; reuse antes de quiescence; RCU genérico safe sem domain fechado |
| W-961 | papel de tagged pointers | tags de endereço são somente uma otimização ou hardening interno provado por target; ownership usa PlaceId, LoanId, origin e receipts independentes da representação | encoded owner universal; depender de high bits portáveis; misturar tag MTE com tag W; mudar ABI pública por allocator |
| W-962 | oracle físico A0 | corpus de layouts, failure, relocation, origem, domain, progress e reclamation usa uma máquina host pura e símbolos do Última Luz; não executa allocator real | benchmark como semântica; somente happy path; snapshot manual; chamar o modelo de runtime ou verifier implementado |
| W-963 | fases da runtime closure E1 | body, closure, observation e storage são eixos separados; outcome candidate fecha admission antes de child/wait drain, cleanup, typed drop, quiescence e outcome cell; `finishCleanup` fecha explicit cleanup mesmo vazio; cancel de Task após commit é registro tardio idempotente | uma máquina única de lifetime; publicar outcome antes de cleanup; usar join como sinônimo de reclaim; esconder storage retirement na task; alterar outcome por cancel tardio |
| W-964 | ordem de cleanup E1 | `defer` instala em body active/open; children e waits drenam antes de executar a stack LIFO mista; `finishCleanup` antecede typed drops inversos; registrations, queue links, timers e wakers criados pelo cleanup exigem node `defer async` ativo e drenam antes da outcome cell; nenhum código W segue o último typed drop | defer depois de drop; LIFO somente por categoria; callback pós-drop; allocation geral durante o drain; cleanup solto no parent closing; resource sem node |
| W-965 | budget e máscara de async cleanup E1 | bookkeeping é pré-reservado; user allocation segue profile; error capturado é evidence bounded, error não capturado é diagnostic; node `defer async` ativo possui child estruturado, herda deadline e registra cancel recebido sem reentrega; child mantém cancel local; grace vence a máscara; sem shield geral | allocation ilimitada; reentregar cancel automaticamente no node; registrar child no parent closing; async destructor universal; shield público; cleanup sem deadline |
| W-966 | completion e cancelamento de provider E1 | operation/wait usa registered→submitted→completing→terminal→drained, com cancel pré-submission podendo drenar localmente; cancel pós-submission é ortogonal e idempotente; `(OperationId, generation)` identifica slot; só completion escolhe outcome; late completion drena ownership sem retomar frame/callback; generation mismatch quita registro antigo | request de cancel ser completion; liberar `OVERLAPPED` antes de completion; assumir ordem CQE; callback W tardio; reutilizar slot sem generation; matar foreign frame por default |
| W-967 | reclamation de frame e outcome E1 | frame pode ser retired/reclaimable após closure quiescent e outcome movido; TCB/outcome cell vivem até join, observer ou retention expiry; reclaim exige zero runtime-owned child refs (retirados no drain), registrations, tickets, timers, wakers e runtime refs; handle não precisa join para liberar bytes | reter frame até join sempre; reclaim com child ref, waker ou queue vivo; outcome cell no frame; pointer atômico conceder quiescence; misturar A0 receipt com frame lifetime |
| W-968 | matriz de liveness E1 | progress é condicional a scheduler/consumer/host bounded; responsiveness só em cancel points/adapters; cleanup exige bounds finitos em user/runtime/provider; shutdown exige participantes cooperativos e finitos ou fault boundary fisicamente terminável; foreign/W arbitrário sem bound; ownership não promete detector de deadlock externo | eventualidade universal; fairness absoluta; cancelar qualquer foreign call; detector geral de deadlock; locks/channels considerados sempre progressivos |
| W-969 | escalada de shutdown e generation E1 | ready→admissionClosed→cancellationRequested→draining→quiescent→stopped; graceExpired→terminating→terminated; forced termination é boundary failure; registry do host e trace encerram roots; generations isoladas e completion velha não muta nova | shutdown como cancelamento normal; aceitar starts depois de admission close; cleanup interrompido virar success; completar slot na generation nova; restart sem incarnation |
| W-970 | oracle E1 e limites | `runtime-liveness-machine.mjs` é host-puro, adversarial e ligado a Última Luz; corpus cobre closure, completion/cancel races, generation, reclaim e shutdown; não prova scheduler/clock/OS I/O, fairness absoluta, advanced reclamation, device, recovery distribuído ou terminação de user code | snapshot manual; máquina runtime; declarar allocator/verifier implementado; ampliar E0/B0/A0; timing real; corpus sem símbolos ou decisões |
| W-971 | público Python inicial | Python é público inicial nas seções 0.1 e 0.4; scripts, automação, ciência, dados e AI entram no público; pessoa com Python realiza o Tour, workflow single-file e workflow científico básico antes de ownership baixo nível; ownership e effects continuam explícitos nas boundaries | adiar Python até depois de 1.0; tratar Python somente como documentação; prometer compatibilidade dinâmica; copiar o modelo baixo nível para o onboarding |
| W-972 | low-ceremony sem dynamic core | pipeline/loop, `json.Value`, `DynamicBatch` explícito e package tabular first-party são as fronteiras vigentes; dynamic core, duck typing, monkey patching, ambient imports e reflection unchecked são rejeitados; compiler/runtime/provider continuam gaps | `Any` universal; object model dinâmico; reflection unchecked; import ambiental; decidir todas as ergonomias como syntax vigente agora |
| W-973 | arquivo único hermético | `w run path/file.w -- <args>` usa módulo normal, imports explícitos, root package/workspace ou contexto efêmero, e entry explícito; não faz recursive/cwd/PATH/environment discovery, não baixa remote implicitamente e não deixa estado oculto | header standalone; body implícito; compact dependency constructor; metadata inline em comment; `--with`; scan recursivo; ambient package discovery; top-level execution arbitrário; download implícito; lock/deployment root |
| W-974 | session transacional e generational | `w repl` usa parser, checker e HIR normais; failed submission preserva a generation corrente; declaration aceita cria generation; dependents invalidados ficam indisponíveis; resubmission explícita recria e executa effects; redefinição/reset fecha admission e drena ownership E1; W-1246 usa token one-shot para consentimento e W-1247 exporta receipts sem persistir heap vivo | dynamic mode; replay automático; stale dependents; confirmação booleana; liberar estado vivo; restaurar tasks/resources/capabilities; notebook transcript como source de release |
| W-975 | Jupyter como tooling | PYN3 fecha o adapter Jupyter 5.5 sobre PYN2, `presentation.Presentable`, MIME/data bounded e export `.w`/package sem hidden replay; W-1248/1249 usam output append-only e history tail-only; W-1250 fixa os comandos iniciais | Jupyter como linguagem; notebook como artifact/release source default; MIME sem limite; update handle sem owner; history search ambiental; fingir kill de foreign code; replay oculto; segundo session model |
| W-976 | interop científico | Python Array API standard é checklist T2; DLPack e Arrow C Data são adapters first-party T2; Python buffer pertence à bridge; copy, device, stream, ownership, lifetime e release são explícitos e provados | Array API como semântica normativa W; copy implícito; lifetime ou release ambiental; buffer protocol no core; pandas clone na std |
| W-977 | dados exploratórios e tabulares | dados usam `json.Value`, schema, `data.Batch<Row>` ou `data.DynamicBatch` explícitos; DataFrame completo é package first-party antes de std estável; TAB0 fecha o carrier mínimo e TAB1 fecha CSV/Parquet/Arrow workflow; sem clone pandas | duck-typed rows; dataframe universal na std agora; object global dinâmico; schema inferido sem limites; tratar ecossistema como syntax |
| W-978 | ergonomia R1 | R1 compara comprehensions com pipelines/loops, checked broadcasting com broadcast explícito, negative/end-relative indices, unpacking/destructuring, display e labels reordenáveis; labels permanecem em ordem até evidence de ganho | escolher syntax final sem R1; broadcast implícito; labels reordenáveis por default; mudar lookup/reproducibility por conveniência |
| W-979 | gates de latency | `time-to-first-result` e `steady-state` são gates separados; first-result cobre hello cold/warm, edit-run e transaction/redefinition/invalidation de 10 cells; steady-state cobre collection transforms, CSV throughput, tensor elementwise/broadcast/matmul CPU e zero-copy DLPack/Arrow overhead; output/semantics precedem tempo e registram compiler version/target/hardware | número fixo de vitória; benchmark isolado; comparar somente tempo; um runtime obrigatório; backend rápido como autoridade semântica |
| W-980 | Python bridge boundary | W-from-Python e Python-from-W usam stable C/Python APIs e data interchange como bridge; CPython ordinário usa bridge, service ou fault boundary; um adapter AOT resolvido pelo manifest pode expor `fn<Python>` somente com artifact hermético, C façade tipada, runtime/deps fixos e diagnostics, effects e provenance de 19.2; generic adapter permanece sem promessa e sem proibição; lifecycle, GIL e interpreter aparecem no adapter | static lib Python previsível sem adapter; Python runtime embutido no core; lifecycle oculto; foreign code sem fault boundary; proibir todo adapter AOT; converter `fn<Python>` em forma core |
| W-981 | fronteira de evidência R1P0 | R1P0 fecha como `oracle-backed-current` somente para protocolo host: parse, oracle, source/input/result e digests são correntes; compile, run, provider, OOM e estudos humano/modelo permanecem missing | chamar parse de conformance; tratar host oracle como runtime; promover forma por digest; registrar participante inexistente |
| W-982 | transformação Python em W | pipeline lazy é a Forma vigente para transformação sem side effect; loop explícito é a Forma vigente para controle e side effects; comprehension é rejeitada por enquanto e o baseline GEN2 stream é preservado; limit zero e no-match preservam `[]`; oracle sidecar registra inspeção bounded | adicionar comprehension à grammar; usar pipeline para side effect; remover limit; comparar inputs diferentes; usar oracle eager |
| W-983 | broadcast de shape | scalar expansion e broadcast explícito checked são as formas vigentes; broadcast implícito é rejeitado por enquanto; Julia dotted broadcast é Alternativa documental; NumPy evidencia conveniência, intermediários, memória ineficiente e menor legibilidade em dimensões maiores, mas não é autoridade W; R1 mede a troca | broadcast universal oculto; scalar exige annotation; Julia syntax no parser; ignorar mismatch e axis change |
| W-984 | acesso relativo ao fim | `.last` é Forma vigente, retorna `ref String?` e absorve empty sem guard; arithmetic explícito é alternativa com guard; sintaxe relativa especial é rejeitada por enquanto, mantendo `last`, suffix e get APIs nominais; C# `^1` é alternativa documental | index negativo sem guard; usar `^1` como syntax W; underflow unsigned; converter empty em panic |
| W-985 | ordem de labels de call | a call é sequência ordenada de labels; overload e initializer selecionam essa forma antes de tipos; ordem de declaração é Forma vigente; default em `currency` cria `majorUnits:,currency:` e `majorUnits:`; overload `currency:,majorUnits:` cria terceira sequência; labels unordered são rejeitados por enquanto | ranking por tipos; dizer que fixed-order é ambíguo; colapsar formas por default ou reordering; alterar resolver no estudo |
| W-986 | tuple destructuring fixo | binding de tuple/struct de shape fixo é Forma vigente; projections `.0`/`.1` preservam uma avaliação e exigem `copy` ou borrow explícito para componente move-only; starred unpacking é Rejeitado por enquanto por ownership, aridade dinâmica e partial moves; `each collection` continua call-rest | reavaliar `word()`; starred na grammar; tratar `each` como destructuring; mover tuple parcial |
| W-987 | corpus R1, contagens e limites | corpus R1 é derivado por script: base/pre-closure 51/148/204/69/75 e aggregate 52/150/208/69/75; R1E0 5/14/20, R1H0 4/11/16 e R1S1 8/22/32 com denominador global 75; bundles fixam inputs, digests e evidence missing | contar manualmente; promover por referência duplicada; omitir adversarial; declarar participante ou modelo executado |
| W-988 | carrier Batch mínimo | `data.Batch<Row>` é finito, owned, columnar, imutável após publicação, schema fechado, row count comum e vazio válido; schema sem fields exige row count explícito; payload publica somente depois da validação | `Table<Row>` estável; DataFrame no core; colunas com counts diferentes; batch vazio como erro; mutação depois da publicação |
| W-989 | DynamicBatch e Array<Row> | `data.DynamicBatch` pode publicar schema runtime; binding tipado explícito valida antes de publicar o `data.Batch<Row>`; `Array<Row>` continua válido para algoritmos row-centric e é rejeitado como carrier universal; DataFrame completo é package first-party | duck typing; `Any` carrier; Array como coluna universal; DataFrame estável na std |
| W-990 | trigger de data.Row | `struct X: data.Row` ativa synthesis por identidade do protocol, struct-only, all-or-none e stored instance fields em declaration order; witness manual e DTO dedicado continuam | annotation genérica; macro; nome textual; synthesis parcial; reflection como trigger |
| W-991 | limites da synthesis Row | schema identity inclui field name, order, logical type, nullability, refinement e semantic extension; unsupported resources e top-level nullable rejeitam; conformance não cria codecs ou ABI schema | aceitar `Any`, service/object identity, task/channel/lock, pointer/ref/view, closure ou foreign handle ilimitado; JSON/CSV/Arrow automáticos |
| W-992 | nullability e names do carrier | `Option` define null; NaN é valor; row index não é especial; field names são UTF-8, nonempty e unique; duplicate/empty externo exige mapping ou rejeição | sentinel row; NaN como null; last-wins; rename silencioso; lookup por nome duplicado |
| W-993 | seleção typed de fields | descriptor gerado e enum-like shorthand selecionam field estático em O(1); dynamic usa nome e binding explícito; read checked de valor Copy devolve valor; W-1251 fecha fields non-Copy sob W-420; reflection e String lookup não entram no hot path | reflection unchecked; String lookup estático; acesso sem bounds; `XView` automático; materialização oculta |
| W-994 | acesso e encoding | typed Batch valida upfront, oferece random value O(1), selection O(1) e scan O(rows); run-end é materializado para `plain` com provenance antes da publicação ou falha; layout físico é opaco | scan para cada access; publicar run-end como O(1); ABI derivada do layout; copy silencioso |
| W-995 | copy, device e conversion | `.never`, `.ifNeeded` e `.always` governam payload copy; target device é explícito quando há transferência; sem target fica no device atual; `.never` falha quando target exige transferência; source usa `copyPolicy:` porque `copy` é keyword; `CopyPolicy` é o contrato lógico; conversion lossy, narrowing, unit/timezone, missing/extra/reorder/type change exige mapping explícita; default é exact schema | policy escolher device; transfer implícita; narrowing silencioso; reorder automático; converter por heurística; device mismatch aceito em `.never`; `copy:` como label |
| W-996 | schema estável em stream | `Stream<Batch<Row>, E>` mantém schema identity em todos os chunks; schema change é typed error e nunca union ou promotion silenciosa | union midstream; promotion de nullable/type; schema por chunk sem identidade |
| W-997 | owner e release foreign | import possui um owner; release ocorre exatamente uma vez após views, waits e children drenarem; owned export transfere responsabilidade e torna o owner local `transferred`; borrowed export é scoped; novo owner completa release; C Data exige trusted producer e validação estrutural | release duplicada; release local após transfer; view após release; owner global; borrowed export escapante; raw pointer sem owner |
| W-998 | trust boundary e sanitização | untrusted input valida counts, buffers, offsets, lengths e nesting/bounds antes da publicação; UTF-8 é validado quando a column declara essa codificação; validity + physical values exigem bytes zero/initialized nos nulls antes de boundary | decode ilimitado; publicação parcial; UTF-8 obrigatório em numeric/binary; null físico não inicializado atravessando boundary; confiança por endereço |
| W-999 | limits e arithmetic | limits cobrem rows, columns, fields, buffers, total bytes, allocation bytes, nesting, metadata bytes, string bytes e chunks; overflow e counts reificados falham antes de allocation/publicação | limits somente de rows; overflow depois de allocation; quota implícita; chunks ilimitados |
| W-1000 | Schema identity e extensions | `data.Schema` separa identity semântica de metadata bounded; names/order, type/nullability/refinement/extensions formam identity; cada extension nominal possui ID estável, versão e parâmetros canônicos bounded sem registry ambiental; extension sem adapter é opaque/dynamic e não bind nominal; physical layout não entra | metadata alterar identity; registry ambiental; extension universal; adapter implícito |
| W-1001 | fronteira TAB1 de adapters | TAB0 definiu o boundary; TAB1 fechou declarations, contracts, oracles e host evidence para CSV, Parquet e Arrow como design; DLPack é adapter tensorial em bundle próprio, com classificação tensor vs tabular explícita e nunca carrier tabular; providers e `w-compile`/`w-run` seguem missing sem mover semântica para formats | CSV/Parquet/Arrow definir Row; DataFrame como carrier; DLPack tabular; signature final neste bundle |
| W-1002 | máquina host TAB0 | `tabular-carrier-machine.mjs`, 64 cases, 155 operations, 22 accepted e 42 rejected, snapshot e host tests modelam invariantes de publication, binding, chunks, owner, trust, sanitização e limits sem executar W; casos positivos e negativos evitam tautologia | chamar host oracle de compiler/runtime; snapshot manual; modelo sem adversariais; testar somente happy path |
| W-1003 | estudo R1 tabular | `typed-batch` é a direção oracle-backed-current; TAB0 fixa schema, ownership, limits, null/NaN, adversariais, tasks, target/package receipts e evidence missing; provider/compiler/runtime continuam gaps W-1002/W-1004/W-1005 | study de snippet; input implícito; variante caricata; declarar participante/modelo inexistente |
| W-1004 | corpus e superfície TAB0 | três casos R0 ligam carrier, synthesis e copy aos novos requisitos; surface snapshot e índices são derivados por script; promoção mede planejamento e não ratificação | editar snapshot manual; contar forma por texto; duplicar caso; chamar digest de evidência humana |
| W-1005 | fechamento do bundle TAB0 | checks scoped incluem machine/cases/host, study bundle/oracle/parse, substitutions/surface, examples, links, freeze audit, index e diff-check; evidência durável mantém `w-compile`, `w-run`, estudo humano e estudo de modelo como missing; declarations, oracles e host evidence dos adapters TAB1 estão fechados somente como design, enquanto providers e execução W continuam missing; sem compiler, runtime, provider ou DataFrame de produção | promover host/parse a execução W; alterar grammar gerada; criar std/data/contracts.w; prometer Forma antes de promoção |
| W-1006 | SnapshotByteSource | fonte finite, positional e content-stable com `byteCount: u64`, offset `u64`, `SnapshotReadStep`, short reads e acesso paralelo; não possui cursor; Parquet e Arrow file exigem este owner | usar ByteSource cursor, reread de path ou content stability ambiental |
| W-1007 | surface data TAB1 | `std.data` declara Row, Schema, nominal SchemaIdentity, opacos Batch/DynamicBatch/Column, FieldDescriptor com Owner, LogicalType indireto e descriptors fechados, constructors `take` para storage consumido, CopyPolicy, BindingPolicy, MappingPolicy, Limits, BindError e EncodeProgress; provider `std.data@1` é missing | Any, reflection, DataFrame universal, stringly logical type ou provider implementado no draft |
| W-1008 | descriptors e selection | `batch.column(.field)` usa descriptor gerado; dynamic selection recebe nome e binding explícito; selection é O(1) e não usa String lookup no hot path | reflection unchecked, String lookup estático ou field rename silencioso |
| W-1009 | borrowed e copy column | Copy fields devolvem valor; `batch.column` devolve uma loan `view StringColumn`/`BytesColumn` ligada ao Batch; `copy` materializa owner; não existe `view Value` ou XView; nested/custom exigem projection ou materialização | view universal, copy implícito ou escape de borrow |
| W-1010 | generic conditional limitation | grammar atual não representa declaration genérica condicional; declarations/oracles separados modelam Copy e non-Copy sem alterar grammar | grammar ad hoc, macro ou syntax condicional nova neste bundle |
| W-1011 | binding e schema policy | `.exact` é default; `.project` é explícito e exige mapping; rename, reorder, narrow, unit, timezone, missing e extra não são heurísticos | union/promotion silenciosa, cast implícito ou schema inference runtime |
| W-1012 | decode common surface | typed `decode` retorna `some Stream<data.Batch<Row>, DecodeError<SourceFailure>>`; source é consumido/owned; batch publica depois da validação | devolver rows soltas, source borrowed escapante ou publicar antes de preflight |
| W-1013 | decodeAll | `decodeAll` é async, tem limits finitos explícitos e materializa um único Batch; não é caminho ilimitado ou cache implícito | unbounded collect, sync-only convenience ou materialização oculta |
| W-1014 | encode progress | overload batch recebe ref Batch e inout Sink; stream consome source e usa error fechado com BatchFailure/SinkFailure; progress u64 é checked e informa bytes committed, complete records e partial-record fact | transaction implícita, retry automático, progresso booleano somente ou source failure perdido |
| W-1015 | publication and error | cada batch só publica após schema, offsets, nulls, nesting e limits; error após publicação não revoga batches anteriores | rollback de batches já publicados ou publication parcial |
| W-1016 | cancellation TAB1 | cancellation é control outcome, drena waits/source/sink/owners conforme E1, e trace/snapshot retém progress; não há detach ou retry | cancelar sem drain, liberar borrow cedo ou transformar progress em zero |
| W-1017 | CSV names | typed overload é `decode`; dynamic overload é `decodeDynamic` e exige `data.Schema`; `decodeAll` e dois encode overloads completam a surface | `decode` dynamic ambíguo, schema opcional ou codec de Row universal |
| W-1018 | CSV portable profile | UTF-8, header required, comma, DQUOTE, CRLF/LF, no CR nu, whitespace preserved, no null token, exact fields, case-sensitive Bool, locale-free numbers e nonfinite disabled | ambient charset/locale, header inferido ou whitespace trim oculto |
| W-1019 | CSV RFC profile | `.rfc4180` usa CRLF e regras do RFC 4180; header nunca é inferido; profile é opção explícita | tratar RFC como default parcial ou inferir header |
| W-1020 | CSV null policy | decode usa `.none`, `.empty` ou bounded tokens; encode separa `.unavailable`, `.empty` e token; empty String não é null e colisões são rejeitadas antes do source | sentinel universal, null token ambiental ou empty/null conflados |
| W-1021 | CSV errors | duplicate/empty header, row width, invalid UTF-8, field conversion, limits e byte/record/field/header location são typed errors | string error, location perdida ou error sem bound |
| W-1022 | CSV writer | `WriterProfile.canonical` emite header e CRLF, quote mínimo, DQUOTE duplicado, shortest-roundtrip e numbers locale-free; formula escaping é policy de apresentação separada; encode parcial preserva progress | spreadsheet escaping lossless default, locale output ou quote indiscriminado |
| W-1023 | CSV tooling | `w data inspect <path> --format csv --sample-rows <N>` e `w data schema ... --emit <file>` produzem candidate/report bounded sem alterar compilação | schema inference runtime, mutation silenciosa ou extensão magic |
| W-1024 | Parquet source | decode recebe `source: take SnapshotByteSource`; positional footer/row-groups/column-chunks/page access não usa cursor | ByteSource, ambient file seek ou dataset directory discovery |
| W-1025 | Parquet binding | default `.exact`; `.project` nomeia target DTO e mapping; semantic mismatch rejeita | reorder/rename/narrow/unit/timezone/missing heurístico |
| W-1026 | Parquet limits | limits finitos cobrem encoded/decoded bytes, allocations, footer, Thrift strings/containers/nesting, row groups, chunks, pages, dictionaries, indexes, bloom e compression ratio | `.unlimited`, row-only limits ou overflow pós-allocation |
| W-1027 | Parquet preflight | magic, footer, offsets e sizes validam antes de read/allocation; malformed stats/index/bloom não muda results | trust por offset, parse tardio ou hint alterando dados |
| W-1028 | Parquet logical matrix | integer widths/sign, f16/f32/f64, decimal precision/scale, UTF-8, calendar date, time-of-day, instant UTC, civil localDateTime, UUID, list/map/nested/null têm matrix; zoned/named timezone só por extension nominal e mismatch é typed | physical type redefine semantic type ou conversion implícita |
| W-1029 | Parquet checksum and encryption | page checksum `.whenPresent` é default; `KeyResolverCapability` só chega por binding explícito de entry/context e `KeyResolver.from` a consome, scoped e bounded para footer/page; unsupported/encrypted sem resolver gera error typed; não há plaintext fallback | capability ambient ou decrypt ambiental |
| W-1030 | Parquet writer profile | modern writer emite LogicalType e ConvertedType legacy quando exigido; LIST/MAP usa três níveis; legacy reader compatibility é profile | writer legacy universal ou schema W definido por ConvertedType |
| W-1031 | Parquet writer plan and commit | `WriterPlan` fixa compression, row-group/page, dictionary/statistics/checksum/encryption e digests; writer só commits footer/file válido ao sucesso; failure deixa artifact incompleto; deterministic bytes sem digests falham | partial artifact apresentado como válido ou determinism default |
| W-1032 | Arrow API split | IPC stream/file têm nomes distintos e source contracts; dynamic variants usam `decodeIpcStreamDynamic` e `decodeIpcFileDynamic` | um decode universal, cursor para file ou overload ambíguo |
| W-1033 | Arrow IPC validation | untrusted IPC valida FlatBuffers, messages, buffers, offsets, nesting, body sizes, compression ratio, schema/footer identity, endian e alignment antes de publish; checksum só pertence a envelope externo | confiar metadata, body size tardio, checksum core inventado ou untrusted C bridge |
| W-1034 | Arrow dictionaries | stream exige dictionary before use salvo all-null permitido; file não aceita replacement; deltas/replacements seguem profile e error typed | aceitar replacement em file ou dictionary forward reference |
| W-1035 | Arrow copy policy | CopyPolicy só governa zero-copy/copy quando escolha real; `.never` falha em alignment, compression, endian, layout, target ou lifetime incompatível; baseline CPU | policy escolher device, copy silencioso ou transfer implícito |
| W-1036 | trusted C handles | C Data/C Stream usam opaque move-only handles de producer trusted; input serialized untrusted nunca usa bridge; structural validation continua | raw pointer safe, handle fabricado ou C bridge para bytes hostis |
| W-1037 | C lifecycle | release exatamente uma vez; get_next serializado por bridge bounded com `BlockingQuota` de concorrência/fila/jobs, cancel/drain; results independentes; last_error copiado antes do callback seguinte; export C Stream é deferido e export array transfere release callback | double release, callback concorrente, worker ilimitado oculto ou owner local após transfer |
| W-1038 | device classification | C Device/DLPack ficam bundle tensorial; foreign device handle é classificado/rejeitado e nunca dereferenced como CPU ou transferido implicitamente | tratar device pointer como CPU ou escolher transfer implicitamente |
| W-1039 | cross-format mapping | matrix cobre Bool, widths, floats/NaN/signed zero, FixedDecimal, UTF-8, Bytes, UUID, Date/Time, Instant UTC, civil LocalDateTime, Option, list/map, nested, enum, Quantity e custom; timezone exige extension nominal | format redefine Schema identity ou mapping não total heurístico |
| W-1040 | provenance and security | provenance registra format/profile/provider version+digest/options/schema identity/copy/materialization; no extension magic, ambient charset/locale/env/path recursion/compression inference; null physical slots sanitizados | metadata incompleta, ambient lookup ou null bytes não inicializados |
| W-1041 | Last Light TAB1 route | `reference/last-light/data_formats.w` liga CSV typed upload, Parquet snapshot archive, Arrow IPC handoff de todos os batches e C trusted import ao mesmo rows/schema/outcome; schema é gerado, identity é nominal, view fica scoped e copy é explícito | UI nova, descriptors manuais, rows divergentes, primeiro-batch truncado, view escapante ou execução W alegada |
| W-1042 | TAB1 adversarial ledger | adversariais incluem CSV chunk quote/bare CR/token/partial encode, duplicate header, empty/null, invalid UTF-8, width, negative/NaN, Parquet footer/offset/size/bomb/Thrift/logical/legacy/checksum/encryption/source/footer commit/digest, Arrow schema/dictionary/endian/copy/alignment/C/quota/double-release/device/cancel | happy path only, parser binário fingido ou adversarial sem ID |
| W-1043 | host evidence TAB1 | `tooling/tabular-adapter-machine.mjs`, 86 cases/193 operations (36 accepted, 50 rejected), checker, JSONL snapshot e host tests derivam state/identity/progress/provenance/tokenizer/footer/page/IPC/ownership/quota; cada case liga símbolo real do Last Light; não compilam ou executam W | expected echo, snapshot manual, boolean rule echo, provider/reader de produção |
| W-1044 | R1 adapter study | adapters TAB1 preservam schema, batches/rows, async effects, empty/negative/NaN/multiline, cleanup, package and target receipts; direção current é host-oracle only e provider/compiler/runtime gap W-1045 permanece | comparar formatos como equivalentes, input implícito, `expect true` ou participante/modelo inventado |
| W-1045 | SDK0/TAB1 projections | std-api contracts e snapshot catalogam std.data/csv/parquet/arrow e SnapshotByteSource como draft; providers permanecem missing; design-index, profiles, requirements e surface são derivados por scripts | alegar provider disponível, editar snapshot manual ou alterar generated tree-sitter src |
| W-1046 | header contextual PYN1 (superseded) | Forma histórica arquivada em `history/archive/pyn1-workflow`; não é surface corrente | header `script` em module source, metadata inline ou parser gate corrente |
| W-1047 | fields e requirements PYN1 (superseded) | Forma histórica arquivada em `history/archive/pyn1-workflow`; records correntes pertencem a package/workspace | lock standalone, fields de header e requirements inline como surface corrente |
| W-1048 | context standalone PYN1 (superseded) | Forma histórica arquivada em `history/archive/pyn1-workflow`; RU0 seleciona package/workspace ou contexto efêmero | header que vence workspace, merge de locks ou discovery ambiental |
| W-1049 | entry de script PYN1 (superseded) | Forma histórica arquivada em `history/archive/pyn1-workflow`; RU0 exige descriptor explícito `.default` ou named | `entryForm` implícito, top-level execution ou args implícitos |
| W-1050 | roots e imports explícitos | grafo usa somente imports explícitos; input físico é opaque e provider retorna canonical token/owner/containment; logical imports são relativos; same-drive/UNC/Unicode equivalentes podem passar e escape/traversal/symlink/different owner falham | replace lexical como canonicalização, recursive scan, cwd/PATH/environment scan, symlink sem provider facts |
| W-1051 | dependency record PYN1 (superseded) | Proveniência arquivada em `history/archive/pyn1-workflow`; dependencies correntes pertencem a package/workspace | dependency inline em source module, resolver paralelo |
| W-1052 | sources rejeitados em script shareable (superseded) | Proveniência arquivada em `history/archive/pyn1-workflow`; source authority corrente pertence a package/workspace | override local oculto ou registry inferido do módulo |
| W-1053 | lock root e virtual selection (superseded) | Proveniência arquivada em `history/archive/pyn1-workflow`; RU0 usa resolution aninhada e não aceita lock root | flatten PYN1 custom, constraint sem resolution, root `package.lock` |
| W-1054 | run sem re-resolução | `w run` compila source normal, mas não resolve constraint, atualiza lock, instala package ou executa install/build action; action output necessário deve estar no lock/CAS/policy | resolver implícito, update no run, callback de package ou shell oculto |
| W-1055 | fetch pinned e offline | fetch usa candidate real quando root/artifact não está no CAS; mirror pode servir lock content-addressed; `--offline` exige root e todos os digests de metadata/content/artifacts/action outputs; cache é explícito | network antes de resolution, fallback registry, expectativa como bytes, alias textual no CAS |
| W-1056 | integridade de artifact | digest sempre é verificado antes de publication/build; authority e signature exigem evidence/policy explícitas, mismatch aposenta bytes | executar bytes antes de verificar, defaults `true`, retry em authority não listada, aceitar assinatura divergente |
| W-1057 | requirements de capability PYN1 (superseded) | Proveniência arquivada em `history/archive/pyn1-workflow`; requirements correntes pertencem ao módulo/package conforme W-1412 | header `requires` como surface corrente, source grant ou secret inline |
| W-1058 | deployment grant PYN1 (superseded) | Proveniência arquivada em `history/archive/pyn1-workflow`; deployment corrente é record nomeado aninhado no workspace | deployment root independente ou grant automático do host |
| W-1059 | capability transitiva | dependency transitiva não herda authority; effects exigem handles/bindings recebidos e contrato próprio | root grant global, boolean escalation, relay opaco, capability por import |
| W-1060 | identity efêmera (superseded) | RU0 deriva identity de source, imports locais, context, resolution, target, profile e toolchain, sem path físico | identity por path textual/cwd ou header histórico |
| W-1061 | cleanup sem estado oculto | product temporário e failed run não deixam manifest/lock oculto; cleanup limpa artifacts transitórios e conserva provenance observável | lock temporário no cwd, manifest oculto, falha publica output parcial |
| W-1062 | commands atômicos PYN1 (superseded) | Proveniência arquivada em `history/archive/pyn1-workflow`; RU0 usa operações de package/workspace | editar header, lock parcial ou `w script` como surface corrente |
| W-1063 | promotion equivalente PYN1 (superseded) | Proveniência arquivada em `history/archive/pyn1-workflow`; RU0 não promove source para package | promotion implícita ou troca de root |
| W-1064 | evidência PEP 723 (superseded) | PEP 723 permanece referência histórica; W corrente não usa metadata inline | copiar comment syntax ou dependency inference |
| W-1065 | operações do oracle PYN1 (superseded) | Oracle corrente é `tooling/module-run-machine.mjs` e deriva parseModule/context/roots/imports/resolution/build/entry/cleanup | máquina de header ou promotion como gate corrente |
| W-1066 | corpus PYN1 (superseded) | Corpus histórico está em `history/archive/pyn1-workflow`; RU0 usa 12 casos/58 operações | tratar snapshot histórico como gate corrente |
| W-1067 | Last Light script oracle (superseded) | `horizon_tool.w` é o fixture corrente de módulo normal com entry explícito | `horizon_script.w`, header ou body implícito |
| W-1068 | grammar e projeções PYN1 (superseded) | Grammar corrente não possui header contextual ou body implícito | reservar `script` global ou editar parser gerado |
| W-1069 | formatter PYN1 (superseded) | Formatter corrente preserva module header opcional e entry declaration | formatar header `script` histórico |
| W-1070 | diagnostics PYN1 (superseded) | Diagnostics correntes usam `source.entry`, `source.context` e `source.roots` RU0 | usar fases W-SCRIPT para header histórico |
| W-1071 | host tests e snapshot RU0 | checker deriva state/trace de module-run, host test verifica entry, roots, resolution, identity e cleanup; snapshot é regenerável | snapshot manual, teste só de expected, chamar host oracle de runtime |
| W-1072 | origens rejeitadas | URL, stdin e shebang não entram na baseline por path, environment e portability | executar source remoto, stdin root implícita, shebang com resolver ambiental |
| W-1073 | edition e target | lock/resolution e recipe falham em edition ou target mismatch antes de build | ignorar edition, escolher target do host, compartilhar lock incompatível |
| W-1074 | context explanation RU0 | `w context` mostra mode, roots, selected context, source/resolution digests e recipe antes do entry | saída resumida sem razão ou merge silencioso |
| W-1075 | status do bundle RU0 | `PRC0-W-1075-current` e `PRC0-W-1075-adversarial` ligam entry explícito, package resolution e cleanup a uma rota current/rejected; RU0 continua design-oracle-input e não execução W | autoridade PRC0; CLI/resolver/runtime/provider gap é W-1444 |
| W-1076 | identidades da sessão | `SessionId`, `SessionIncarnation`, `ExecutionOrdinal` e `GenerationId` são campos separados; reset/restart muda incarnation; publish muda generation | usar ordinal como generation, prompt por generation, reset sem incarnation |
| W-1077 | ordinal e prompt | submission completa que guarda history incrementa ordinal, inclusive error; incomplete/completion/inspect e queued cancel não incrementam; active cancel tem ordinal; prompt é `w[n]` | incrementar em cada tecla, usar `gN` no prompt, omitir erros do history |
| W-1078 | generation opaque/display | ID interno é opaco e display `gN` é projeção; receipt carrega base/final | expor display como ID, derivar ID de path ou ordenar por frontend |
| W-1079 | contexto default hermético | `w repl` abre `ephemeral-std-only`, fixa lock/capabilities e não varre cwd/PATH/environment | import ambient, resolver dentro da sessão, lock mutável |
| W-1080 | parser/checker/HIR normais | sessão usa os mesmos profiles normais; facts do parser e checker separam parse/semantic e wrapper classifica expression, declaration, statement, loop, call, await e defer | dynamic mode, parser especial, regex como checker, wrapper que cria syntax |
| W-1081 | command position | `:status`, `:why`, `:history`, `:cancel`, `:reset` e `:quit` só são command quando `:` é o primeiro token não espaço de uma entrada nova sem source acumulado | command no meio do source, token contextual global, top-level arbitrário legalizado |
| W-1082 | read-only snapshot | completion, inspect e status leem snapshot committed e não veem staged visibility ou effects | completion executar code, inspect ler staged, status mutar |
| W-1083 | fases transacionais | submission percorre collected→parsed→checked→staged→preflight→executing→settling→publish→draining-old→committed/ready; steps permitem reader/cancel interleave | commit em parse, effects antes de preflight, fase omitida, receipt booleano sem trace |
| W-1084 | falha sem publication | parse/type falha sem effect/generation; runtime pré-publish limpa staged E1 e preserva effects externos observados | publicar binding parcial, rollback externo geral, apagar print observado |
| W-1085 | transaction explícita | receipt distingue invocation observed de provider outcome; `rolledBack` só vem de capability/provider `transaction`, `unknown` permanece unknown | transaction universal, inferir rollback de runtime failure/kind, replay compensatório oculto |
| W-1086 | receipt machine-readable | receipt informa request, session, incarnation, ordinal, prompt, base/final generations, outcome, phases, effects, diagnostics, invalidation e cleanup | terminal text como contrato, receipt sem base, omitir outcome degraded |
| W-1087 | cross-generation mutation | Copy staging automático para value Copy; snapshot, adapter transaction e deferred-no-fail são positivos; take, inout, escaping ref/borrow/view têm rejeições separadas; borrow lexical nonescaping passa | liberar old value, borrow que escapa, exigir ceremony para Copy, aceitar take por conveniência |
| W-1088 | binding graph fingerprints | versão guarda HIR/type/layout/effect fingerprint; hard edges carregam BindingId/version/incarnation e kind lookup, type/layout, const, witness, owner/import | grafo por nome atual, fingerprint omitido, edge só textual |
| W-1089 | snapshot provenance | valor avaliado guarda soft provenance; `let snapshot = limit * 2` retém 6 após rebind | snapshot como dependent compilado, rerun implícito, stale value silencioso |
| W-1090 | compiled dependent | função `doubled` guarda hard edge para `limit`; redefine invalida transitivamente com reason e closure | usar `let doubled`, preservar function stale, recompilar automaticamente |
| W-1091 | redefinição explícita | nova versão invalida dependents sem recompilação/rerun; resubmission é explícita; old version não é current lookup | replay de cell, lookup old version, rebind que muda effects sem receipt |
| W-1092 | type invalidation | mudança de type/layout invalida values, code e witnesses pertinentes | manter value vivo com layout antigo, invalidar somente name |
| W-1093 | drain preflight | closure separa symbol graph de owner scopes e deriva replaceability/retention/deadline de provider events; W-1246 substitui `allowDrain` público por token estruturado validado | booleans de conclusão no caso, iniciar drop durante preflight, drenar sibling |
| W-1094 | preflight rejection | known unreplaceable, quota, retention ou confirmação ausente rejeita antes de executing/effects e mantém generation antiga, staged scope e outputs removidos | publicar e depois rejeitar, liberar old resource, rebase silencioso |
| W-1095 | drain pós-publication | falha/deadline após publish mantém nova generation committed e sessão degraded, bloqueando mutações | rollback da publication, marcar ready, aceitar mutation em degraded |
| W-1096 | reset boundary | reset/restart incrementa incarnation, faz preflight/publish/drain e registra force boundary | reset sem drain, incarnation reutilizada, prometer cleanup foreign |
| W-1097 | structured lifetime | child/wait/defer local settle antes do commit; persistent task/resource tem owner scope por binding, child da sessão e atravessa gerações não superseded | detached task, reparent retroativo, wait fora do owner, drenar sibling |
| W-1098 | lexical loan boundary | ref/borrow/view lexical não cruza submission; safe view exige owner e hard dependency | view armazenada sem owner, borrow trans-generation implícito |
| W-1099 | output identity | output carrega RequestId, ExecutionOrdinal e candidate generation; external print sobrevive falha | output sem request, display staged após reject, misturar output de generation |
| W-1100 | single writer | uma submission mutating é admitida por vez; frontends enfileiram por FIFO ticket/admission | writers concorrentes, ordem por completion, rebase implícito |
| W-1101 | stale base | base generation stale rejeita ou reanalisa somente com opção explícita; não há silent rebase | aceitar base antigo, escolher generation por último writer |
| W-1102 | history bounded | history in-memory é limitado por count e bytes; raw source pode existir em memória ou ser redigido por policy, com reservation antes de effects; guarda metadata/digests, não live values | `~/.w_history`, log ilimitado, segredo implicitamente persistido, valor vivo serializado |
| W-1103 | security e contexto | startup script, cwd/PATH/env import, segredo implícito e resolver/network na sessão são proibidos | descobrir ambiente, importar segredo, atualizar lock em submit |
| W-1104 | quotas | source, total bindings, hard edges, HIR/artifacts, history reservation, tasks/resources, output, diagnostics, invalidation e drain deadline possuem limites derivados | quota só em output/declarations novas, limite não observável, executar e expulsar próprio receipt |
| W-1105 | diagnostics de sessão | arquitetura usa fatos/fases para parser, semantic, context/lock, stale opaque base, invalidated binding, ownership, quota, cancellation/close e post-publish degraded | error genérico, phase falsa, diagnóstico sem fix boundary |
| W-1106 | transcript canônico | transcript começa em g0, termina g4 após quatro publishes, mostra w[5] unavailable e w[6] type error preservando g4, além de snapshot 6 e closure limit→doubled→menu com edges imediatos | transcript usa somente `w[n]`, function como let, failure avançando generation |
| W-1107 | fronteira PYN3 | PYN3 separa `std.presentation`, adapter Jupyter e export; todos usam PYN2 como session autoritativa | segundo REPL/runtime, MIME ou ZeroMQ como semântica da linguagem, notebook mode dinâmico |
| W-1108 | presentation protocol | `presentation.Presentable` escreve em `presentation.Writer` opaco T2 com limits; `Display` continua textual e independente; provider `std.presentation@1` fica missing | `Bundle<String,Any>`, substituir Display, writer público sem bounds, utility object global |
| W-1109 | effect mask e fallback | presentation permite borrow, allocation bounded e writer writes; rejeita suspend/I/O/state/capability; fallback usa Presentable, Display elegível ou resumo compiler-owned sem user code | renderer effectful, debug dump privado, synthesis de Display/Presentable, presentation alterar outcome |
| W-1110 | media segura | baseline suporta plain text, bounded JSON, PNG/JPEG e vendor `+json`; HTML/SVG exigem sanitizer provider; JavaScript, widget, remote asset e active content são rejeitados | MIME arbitrary pass-through, confiar HTML do user, kernel marcar notebook trusted, payload untyped |
| W-1111 | previews sem trabalho oculto | table preview limita rows/columns/bytes e não coleta stream; tensor em device mostra metadata e não copia para host; full snapshot/copy é explícito | collect implícito, device copy implícita, render entrar na value identity, calcular todas as MIME eager |
| W-1112 | baseline Jupyter | adapter implementa messaging 5.5, kernelspec determinístico e interrupt por message; `language_info` é exato e `supported_features` fica vazio no baseline; debugger/subshell/comms/widget ficam ausentes | protocolo próprio, feature advertisement falso, completion/history em `supported_features`, startup handshake novo como requisito baseline |
| W-1113 | transporte e secrets | connection file é startup capability user-only; HMAC antecede JSON use, frames/replay são bounded; CurveZMQ é usado em todos sockets quando oferecido e pode ser policy-required | HMAC como encryption, secret em log/receipt, remote auth no kernel, aceitar replay ou frames unlimited |
| W-1114 | lifecycle de request | request observa authenticate→busy→process→reply→related outputs→idle com parent correlation; execute usa FIFO PYN2 e control só solicita cancel/reset/close | idle prematuro, output após idle, control mutar graph, ordenar por client clock |
| W-1115 | counter e silent | execution_count é ExecutionOrdinal; silent força no-history/output e só aceita submission read-only/non-suspending/effect-free sem publication; mutation silenciosa falha no preflight | counter como GenerationId, silent mutation, ordinal para completion/inspect, output oculto ainda executado |
| W-1116 | user expressions e stdin | user expressions são read-only/effect-free por key após success; stdin respeita allow_stdin, um request bounded e routing original; password nunca persiste; input bloqueia export até parametrização | user expression mutar state, input ambiental, password em history, mais de um waiter sem bound |
| W-1117 | status e cancellation | outcomes PYN2 mapeiam para ok/error, sem status aborted; interrupt confirma admission e execute reply confirma drain; shutdown ok só após safe close | traceback Python falso, interrupt afirmar termination, shutdown success antes de drain, rollback de committed degraded |
| W-1118 | requests read-only | completion/inspect/is_complete usam snapshot committed e Unicode codepoint offsets; inspect inclui plain text; W-1249 fecha history como tail-only | executar completion, ler staging, byte offset como codepoint, history raw ilimitada |
| W-1119 | metadata e identidades | metadata `w` versionada liga request/incarnation/generation/ordinal/outcome/digests/exportability; msg_id/cell ID/counter nunca substituem identities W; secrets/live values não entram | identity por frontend/timestamp, capability em metadata, cell ID como BindingId, client clock ordenar sessão |
| W-1120 | notebook como exploração | nbformat cell IDs são validados; outputs/trust não são source nem prova W; export reproduzível exige notebook mais receipt manifest explícito | `.ipynb` como release source, notebook signature como build proof, output codegen, hidden sidecar ambiental |
| W-1121 | prova de export | export valida source digest, committed chain, binding versions/edges, lock/context/target, effects e ausência de stdin/secret/degraded/live resource; não executa | replay oculto, export com unknown effect, capturar live value, aceitar cell invalidada ou silent mutation |
| W-1122 | ordem e resultado do export | pure declarations usam topological order com ordinal como tie break; effects preservam execution order; conflito/redefinition não-lossless falha; resultado é module-run/package mais audit manifest | renomear/remover binding, reexecutar, inserir value literal, ordem do documento como autoridade, comentário gerado de prose |
| W-1123 | output transitório | tail expression summary não cria `_`/`ans`; W-1248 fecha output como append-only e não reserva live update API | binding implícito, handle frontend string cru, update atravessar reset, lifetime não bounded |
| W-1124 | status do bundle PYN3 | `PRC0-W-1124-current` e `PRC0-W-1124-adversarial` fecham preview bounded e compiler-summary fallback; PYN3 permanece host design-oracle e separado de DLPack | autoridade PRC0; kernel/presentation/export providers, compiler/runtime e bridge continuam W-1445 |
| W-1125 | baseline DLPack PYN4 | DLPack 1.3 versioned é a baseline documental; `DLManagedTensor` legacy é rejeitado e provider/compiler/runtime ficam missing | tratar legacy como vigente, executar o provider a partir do oracle, declarar compatibilidade sem release proof |
| W-1126 | major/minor version mismatch | major mismatch chama somente deleter uma vez sem dereference; minor mismatch aceita somente fields/enums conhecidos | ler fields após major mismatch, aceitar unknown minor fields, liberar duas vezes |
| W-1127 | DLPack flags | somente read-only, producer-copied e subbyte-padded são conhecidos; unknown flags rejeitam | ignorar flags desconhecidas, reinterpretar subbyte, usar producer-copied como zero-copy |
| W-1128 | tensor Device identity | `tensor.Device` vem de resolução do provider e equality inclui provider instance; raw `(type,id)` não é identidade W | comparar somente números, tratar handle foreign como CPU pointer, colapsar kind desconhecido em `other`, transferir implicitamente |
| W-1129 | tensor Queue capability | Queue é opaca, provider-minted e ligada a Device; receipt de bindQueue/producerWait prova happens-before; mismatch falha e CPU rejeita queue extra | aceitar stream integer ou `ready: true`, inferir queue, misturar queues de devices/providers |
| W-1130 | managed tensor carrier | `dlpack.ManagedTensor` é opaque move-only e não forgeable; raw pointer não entra em safe API, receipt ou diagnostic | expor pointer, serializar carrier, construir managed tensor a partir de bytes untrusted |
| W-1131 | zero-copy open | `open` rejeita incompatibilidade e producer-copied e não copia payload; metadata bounded é permitida e registrada | esconder copy, chamar metadata allocation de payload copy, aceitar producer-copied |
| W-1132 | dynamic bind | `openDynamic` devolve owner consuming e `bind<Element,shape>` valida dtype/rank/shape/layout exatos | fazer cast parcial, reinterpretar dtype, conservar owner após bind |
| W-1133 | explicit materialization | `materialize` sempre é operação explícita e receipt separa producerCopied de wMaterialized | usar CopyPolicy condicional, copiar no open, omitir target ou queue |
| W-1134 | imported owner and view | ImportedTensor é move-only read-only; `withView` é lexical, não escapa, não produz inout e drena work | retornar view, guardar borrow, mutar foreign storage, cruzar await sem drain |
| W-1135 | close and quarantine | `close` drena views/queues/jobs e libera exatamente uma vez; failure vai para quarantine/fault boundary | UAF, skip release em cancel, callback após close, restaurar owner depois de failure |
| W-1136 | DLPack layout validation | rank, dimensions, product e span `byte_offset + Σ((shape_i-1)*stride_i)*elementBytes + elementBytes`, shape/stride length, element strides, rank-zero null rules, alignment em data+offset, overflow e overlap proof são checked | usar pointer arithmetic unchecked, aceitar negative stride, tratar stride bytes como elements, aceitar overlap sem proof, assumir extent do DLPack |
| W-1137 | dtype matrix | typed baseline aceita mapping nativo do provider, lanes 1 e mapping W byte-aligned; Bool/Complex exigem storage profile provado; DLPack não expõe Endian público; subbyte/opaque ficam dynamic/rejected | converter endian, aceitar lane diferente, usar booleano do caller como storage proof, alterar NaN/signed zero, reinterpretar unsupported |
| W-1138 | producer provenance | allocation extent exige trusted producer/provider provenance; DLPack não prova extent | confiar em raw shape, aceitar provenance ausente, ler bytes untrusted |
| W-1139 | synchronization receipt | producer estabelece happens-before no consumer Queue por receipt de bindQueue/producerWait; CPU não exige queue e rejeita queue extra; receipt não aceita `ready: true` | usar stream integer, inferir cross-provider sameness, publicar sem provider receipt |
| W-1140 | capsule one-shot | names versioned são `dltensor_versioned`→`used_dltensor_versioned`; destructor libera somente antes do consumo e static names outlive capsule | consumir duas vezes, chamar deleter no destructor consumed, usar nome legacy |
| W-1141 | Python lease boundary | lease é child do interpreter scope, exige GIL/attached thread state e drena antes de finalization; sem prova copia ou rejeita | callback depois da finalization, free/UAF na boundary, deadlock deliberado |
| W-1142 | limits and arithmetic | rank, dimensions, elements, span, metadata/control, leases, release jobs, wait e deadline são bounded; overflow precede allocation/publication | quota somente em payload, aritmética wraparound, alocar antes de validar |
| W-1143 | consuming export | export consome owner W, transfere release obligation e só marca writable com uniqueness real; borrowed escaping rejeita | exportar borrow, copiar payload oculto, liberar owner W após transferência |
| W-1144 | cancellation and close order | cancellation mantém owner, drena consumer work e chama deleter depois; close failure registra quarantine | abandonar owner, pular drain, tratar cancel como sucesso sem receipt |
| W-1145 | receipts and redaction | receipts distinguem copy classes, device/queue/provenance e release state e redigem pointer, capsule address, secret, GIL e interpreter pointer | vazar endereço/secret, receipt booleano sem phase, chamar diagnostic de trace raw |
| W-1146 | untrusted and network boundary | DLPack é trusted in-process e não é serialization, network ou bytes untrusted; buffer protocol não entra no core | desserializar ManagedTensor, aceitar pointer de rede, adicionar buffer protocol |
| W-1147 | PYN4 status and evidence | `PRC0-W-1147-current` e `PRC0-W-1147-adversarial` ligam DLPack release/deleter, lease e view-escape a evidência host; fixture/machine/checker não executam W | autoridade PRC0; DLPack/Python/device bridge é W-1446; C Exchange não é semântica do core |
| W-1148 | R1E0 evidence boundary | cinco bundles R1E0 fecham o protocolo de parse/oracle host e source migration; compile, run, provider, OOM e estudos humano/modelo permanecem missing | chamar parse de ratificação, chamar oracle de execução W, inventar participantes ou modelos |
| W-1149 | R1E0 post-test loop | bundle compara `repeat` selecionado com `while true` válido e mede body, predicate, guard extra da alternativa, `continue`, `break`, cleanup lexical, zero, 9 e multidigit | negar body inicial, reavaliar predicate após break, ou tratar `do/catch` como post-test |
| W-1150 | R1E0 conditional and function return | bundle cobre `if` value, Unit sem else, named value block, join, selected effects, discard/tail, `return` explícito em function e o negativo de implicit function tail | aceitar non-Unit sem else, unir branch types por union implícita, ou tornar function body value block |
| W-1151 | R1E0 assignment | bundle cobre place/RHS one-shot, ledger de ownership, commit após RHS success, preservação após failure, Unit, move-only, compound one-place e rejeição por context/AST | devolver value, encadear, duplicar owner, dropar antes do RHS ou ler/escrever place duas vezes |
| W-1152 | R1E0 power boundary | bundle cobre `**` right-associative, unary base/exponent, `^` XOR e a separação de unit grammar | trocar `^` por power, associar à esquerda, ou exigir parentheses para exponent prefix |
| W-1153 | R1E0 fluent self | bundle seleciona `: self` com fallthrough e compara `return self`; validator separa receiver mode, return contract, exit e storage; omitted type é Unit, `Self` é owned e `take fn` rejeita | tornar `: self` `Self` owned, exigir `return self`, copiar/mover/alocar receiver ou aceitar `take fn` |
| W-1154 | R1E0 corpus metrics and status | R1E0 deriva 5/14/20 com denominador global 75 e status `design-oracle-input`; nenhuma contagem promove host evidence a runtime, e compile/run/provider/human/model claims ficam missing | escrever contagens manuais, promover host evidence a runtime, ou declarar estudo humano/modelo executado |
| W-1155 | generic value parameters | parâmetros de valor usam `name: Type` ou `_ name: Type` (`optional(name)`), são compile-time imutáveis, resolvidos após name resolution e participam de identidade, ConstIR e monomorphization sem storage runtime | `const name: Type` no envelope, classificação por casing, inheritance/base-class constraint, storage implícito |
| W-1156 | generic labels e contract values (retired) | interpretação anterior: `_` removia o label externo; W-1160 substitui por label opcional, com as duas formas no mesmo slot; values mantêm associated exposure estática | primary-only, field de instance automático, reorder de labels, member callable implícito |
| W-1157 | implicit script entry (superseded) | Forma histórica arquivada em `history/archive/pyn1-workflow`; RU0 não baixa statements finais e exige entry declaration | wrapper `.default` privado, entry+implicit misturados, args/ctx implícitos, script importável |
| W-1158 | script bootstrap e latency | `w run file.w` usa parser/checker/HIR comuns, mede first-result separado de steady-state e migra tooling para W somente após self-host C | runtime script separado, vitória sem benchmark, remover seed C, Bun como dependência do produto |
| W-1159 | proof-backed memory recommendations | diagnostics de race e ownership usam proof facts, e alternativas de partition, channel/service, lock ou atomic permanecem condicionais | naming heuristics, shared como mutation/sync, atomic como lifetime/ownership, arquitetura automática |
| W-1160 | labels opcionais | `_` dá label opcional no mesmo slot; formas positional e `name:` normalizam para uma HIR; colisão torna declaration/overload inválido | `_` elimina label, alias arbitrário, resolver por tipo |
| W-1161 | suspensão inferida | body/HIR infere `maySuspend`; `async fn` fixa o contrato quando necessário; bare call de `maySuspend` é erro | propagação nominal obrigatória, warning para bare call, call-site `sync` |
| W-1162 | placement no call site | domain é escolha explícita do caller; network usa await; declaration-side placement existe somente por correctness | spawn como hint de performance, domain silencioso, worker dedicado para I/O |
| W-1163 | lowering resumable | ordinary ABI para `neverSuspend`; frame somente para values live across suspension; backend pode usar MLIR, LLVM coro ou CPS | stackful obrigatório, stackless obrigatório, libmill/libdill como dependency |
| W-1164 | standard library plana | availability deriva de target facts, capabilities, effects, provider status/digest e reachability; não há tier field | distribuição por tier, source shipping que força link, availability global |
| W-1165 | caminho de memória | value/ref/inout/take/copy e scopes estruturados formam o caminho normal; `region` sai da Forma vigente; Arena é API avançada | region syntax vigente, promoção unique→shared, Arena em tutorial normal |
| W-1166 | contrato atômico | atomics são operations/order/extent; lowering LLVM direto ou fallback declarado; `lockFree: true` rejeita fallback | atomics como interrupt/block, weakening silencioso, atomic para ownership |
| W-1167 | projections de process | `process.args` e `process.context` são projections intrínsecas read-only limitadas ao native-process root e ao host profile | hidden args/ctx, singleton universal, Context global |
| W-1168 | exemplos de documentação | `///` e `/** ... */` suportam `@example` com terminal único; runner gera teste hermético e omite release payload | doctest ambient, múltiplos terminals, measurement universal |
| W-1169 | terminologia retirada | labels numéricos são históricos e não aparecem em catálogo, snapshot ou docs atuais | renomear tiers como levels, manter enforcement tier |
| W-1170 | comparativos de execução | Swift, GCD, Go, Java, P2300, Koka, libdill, LLVM e MLIR formam gates comparativos sem definir W ou virar dependency | backend externo como autoridade, comparação sem diferença observável |
| W-1171 | evidence de execution ergonomics | máquina, checker e snapshot derivam forms, suspension/SCC/call, placement, effects, FIFO/lifecycle/budgets e frame bytes; a direção é current host protocol, sem alegar compile/run/provider/human/model | checker que ecoa strings do JSON, snapshot manual, host test tautológico, alegar compiler/runtime |
| W-1172 | dispatch para domain serial | `spawn<domain>` cria child estruturado no domain explícito; serial aceita dispatch, executa um segmento runnable por vez, preserva FIFO no primeiro start e libera o permit durante await/join; `.parallel` é capability do domain, não da keyword | rejeitar serial, tratar spawn como thread paralela, launcher fora de binding `let`, wait síncrono no mesmo domain, `parallelDefault` oculto |
| W-1173 | dispatch de barreira | `spawn<domain, .barrier>` cria um ticket estruturado; prior jobs drenam, a barreira `neverSuspend` executa sozinha e libera jobs posteriores depois do cleanup; o checker pode ordenar `ref`/`inout` somente para um grafo fechado no mesmo domain | shared ownership desnecessário, lock oculto, barrier que suspende, placement como isolation universal, `dispatch_barrier_async` sem queue privada, read/write por convenção não verificada |
| W-1174 | leitura tolerante a staleness | `load<.relaxed>()` continua atômica; storage comum só participa quando happens-before ou barreira prova a ordem; o modifier `atomic` nunca expõe uma view comum dos mesmos bytes | read não atômica concorrente porque o valor pode ser antigo, relaxed como non-atomic, weakening silencioso, `volatile` como synchronization |
| W-1175 | lane serial dinâmica | `ExecutionAuthority.openSerial` cria owner lexical bounded sobre pool existente; primeiro start é FIFO, só um segmento runnable usa o permit, suspension o libera, rejeição devolve o input em `TaskAdmissionError<Input>`, close drena e refs não estendem lifetime | copiar GCD inteiro, reter worker durante suspension, restaurar binding movido, perder input na admission, fila global, sync dispatch, QoS no call site, target queues, fire-and-forget, thread por lane, executor custom safe, usar lane local no lugar de service keyed |
| W-1176 | claim de memória | gerência automática exige prova real de owner/borrow/drop/reclamation, placement semanticamente neutro e contratos explícitos para shared/pin/FFI/OOM | alegar memória resolvida por existir um borrow checker, exigir GC/ARC universal, usar resultado de oracle host como implementação |
| W-1177 | criação shared ergonômica (refinada) | W-1256 seleciona storage `shared` explícito no caminho comum; expression/return usam binding local e `take`; SHC0 seleciona `try` na construction expression e mantém recovery consuming | `share`/`try share`, wrapper nominal, promotion em argumento/return/overload, retain implícito |
| W-1178 | snapshot publicado | `SnapshotCell<T>` é move-only/shareable; `read` scoped vê uma versão, `snapshot` duplica e `publish` consome uma versão completa | guard público, ref escapante, mutation in-place, update closure escondida, safe RCU geral |
| W-1179 | reclamation de snapshot | publicação retira a versão anterior; cada versão executa drop uma vez depois do último reader, sem esperar no publish | liberar no swap, manter tudo até drop do cell, expor grace period, `Atomic<shared T>` |
| W-1180 | oracle SP0 | máquina host pura cobre publication order, staleness, error drain, retirement, close, OOM pré-publicação e estratégias equivalentes | chamar oracle de provider/runtime, snapshot manual, caso sem símbolo Última Luz |
| W-1181 | wrappers de lock (retired) | W-1257 substitui Mutex/AsyncMutex/RWLock por uma construção scoped sobre `shared T` | preservar wrappers por compatibilidade pré-1.0; guard escapante; recursive lock |
| W-1182 | FIFO de lock (retired) | W-1258 mantém mutual exclusion, cancellation e edges, mas não promete FIFO/fairness para permitir lowering eficiente | fairness host como semântica; poisoning; unlock antes de cleanup |
| W-1183 | conjunto mínimo (refinado) | W-1259 usa owner/domain/service/atomic/snapshot/channel antes do único fallback `lock` | copiar primitives host; condition sem ownership; RCU safe; RW lock universal |
| W-1184 | oracle LM0 (retired) | W-1260 substitui o corpus por LM1 da construção da linguagem e alternativas lock-avoiding | manter expected antigo após mudar semântica; chamar host model de runtime |
| W-1185 | plano cross-axis de ownership | as quatro formas de execução usam PlaceId, LoanId, OriginSet, owner delta e drop obligations comuns; placement não cria outra memória | ownership por scheduler, copy/share implícito, ARC para reparar capture inválida |
| W-1186 | staging e fechamento cross-axis | capture passa parent→staging→child; rejection limpa uma vez; cleanup precede outcome e join restaura somente a autoridade permitida | rollback de take, outcome antes de cleanup, loan lexical encerrado por evento runtime invisível |
| W-1187 | publicação e ownership | happens-before publica mutation autorizada sem conceder owner, ampliar loan ou ressuscitar binding; scope exit cancela, drena e faz join | cancellation como rollback, join como share, binding movido volta por error |
| W-1188 | oracle MX0 | máquina host pura compõe staging, mobility, loans, admission, cancellation, cleanup, outcome, join e drop sem substituir M1/E0/E1 | colar snapshots independentes, expected echo, chamar modelo de compiler/runtime |
| W-1189 | read/write síncrono (retired) | W-1259 substitui por SnapshotCell, domain barrier ou lock exclusivo conforme o problema | manter API só porque hosts oferecem; tratar lock e domain como aliases |
| W-1190 | admission read/write (retired) | fases e tickets permanecem evidência histórica; nenhuma fairness RW entra na linguagem | fairness do host, barging, upgrade/downgrade ou recursive read como contrato |
| W-1191 | provider read/write (retired) | adapter unsafe pode fixar provider/target sem criar primitive safe universal | poisoning, liberar payload vivo, benchmark como semântica |
| W-1192 | evidence read/write LM0 (retired) | casos anteriores preservam a alternativa em history/rationale; LM1 testa o contrato vigente | chamar oracle antigo de runtime ou de decisão corrente |
| W-1193 | estado lógico Lazy | `var Lazy` possui initializer armazenado, winner único e uma publicação completa do initializer; contenders não repetem a execução | inicialização duplicada, retorno parcial, `Once` raw, carrier separado para o caso comum |
| W-1194 | lowering Lazy por prova | local, isolation serial e estado atômico com parking preservam a mesma semântica; interface publica `blockingWhenContended` | nome distinto por lowering, wait oculto em domain non-blocking, sincronização sempre ativa |
| W-1195 | failure e lifetime Lazy | initializer é sync/nonthrowing; reentrada dinâmica e panic falham boundary; mutation exige exclusividade; cada capture path e valor retido termina uma vez | poisoning recuperável, retry implícito, cancellation durante initializer, setter concorrente, self-owning closure |
| W-1196 | evidence LZ0 | `PRC0-W-1196-current` e `PRC0-W-1196-adversarial` derivam winner, waiter happens-before, publication e reentry rejection no oracle host; provider continua missing | autoridade PRC0; lazy compiler/runtime/synchronization provider é W-1447 |
| W-1197 | construção pinned direta | `try pin Type(...)` avalia argumentos, reserva storage estável e executa o initializer no destination final; não cria `T` completo intermediário | construir e mover sempre, `pin init`, `pin let`, annotation de field, carrier parcial safe |
| W-1198 | failure de construção pinned | falha de argumento limpa staging anterior; falha de allocation limpa staging; falha de initializer limpa fields em ordem inversa e libera storage; nenhum caminho publica endereço parcial | leak de staging, `deinit` sobre aggregate incompleto, binding parcial, rollback do source consumido |
| W-1199 | limite de self-reference safe | `self` permanece initialization place até commit; endereço, borrow e escape do aggregate parcial falham; source safe não armazena self-reference; adapter unsafe publica somente wrapper completo | self-reference pelo initializer comum, `MaybeUninit` safe, pointer para bytes parcialmente válidos |
| W-1200 | evidence de construção pinned M1 | M1 deriva destination root, zero move intermediário, delegação, cleanup, publication e rejeições; Last Light mantém casos direto e consuming | oracle isolado duplicando M1, snapshot manual, chamar modelo de compiler ou allocator |
| W-1201 | espera atômica suspensiva | `await Atomic.wait<order>(whileEqual:)` devolve somente uma representação diferente; notify one/all é explícito e store não acorda implicitamente | blocking wait público, polling oculto, retorno por wake espúria, notify automático em toda store |
| W-1202 | admission e cancellation de atomic wait | tickets monotônicos, notifyOne seleciona o elegível mais antigo, notifyAll seleciona todos; cancel pré-notify remove e pós-notify permanece pendente | wake perdida, barging, waiter órfão, cancel como rollback de notification committed |
| W-1203 | lifetime, memória e provider de atomic wait | frame estável automático, owner drena waiters, parking key inclui generation; acquire observa release, notify não é fence; provider não bloqueia worker cooperativo | pin manual no caminho comum, owner destruído com waiter, ABA tratado como evento, spin ilimitado, endereço reutilizado, provider como autoridade |
| W-1204 | boundary blocking coerente | foreign blocking usa `spawn<.blocking>` bounded ou fault boundary física da seção 12.11 e nunca ganha fix-it `sync` | boundary ainda Pesquisa, bloquear worker cooperativo, interromper foreign frame sem contrato, reintroduzir call-site sync |
| W-1205 | evidence atomic wait E0 | E0 deriva fast path, check/register, FIFO, notify, cancellation, drain, ABA e release/acquire; source Última Luz fixa a superfície | chamar oracle de scheduler/runtime/provider, expected echo, caso sem símbolo consumidor |
| W-1206 | capture sem retain implícito | closure escapante de owner move-first escolhe `take`, `copy` ou `weak`; inference fica limitada a Copy e borrows provadamente bounded | ARC implícito em closure, retain escondido, capture `unowned`, fix-it que escolhe ownership |
| W-1207 | classificação de ciclo forte | HIR rejeita SCC fechado cujas edges fortes só terminariam pelo `deinit` interno; weak, close e lifecycle drain permanecem distinções observáveis | rejeitar todo grafo cíclico, aceitar self-cycle imutável, converter strong em weak, confiar em nome de field |
| W-1208 | censo sem coletor | profile debug/test registra control-block edges e reporta SCC que nenhum root alcança somente depois de admission close e drain; não coleta nem muda drop | coletor default, relatório antes do drain, ciclo alcançável chamado de leak, deinit executado pelo detector |
| W-1209 | evidence de ciclos e captures M1/S0 | S0 fixa capture explícita e diagnostics; M1 deriva SCC forte, edge rompível, roots e residual pós-drain; Last Light fornece consumer | checker por substring, graph fornecendo resposta esperada, chamar oracle de runtime/compiler ou leak sanitizer real |
| W-1210 | claim de concorrência e paralelismo | quatro formas de execução compartilham ownership/lifetime; children drenam; synchronization forma um happens-before explicável; schedulers e providers reais precisam provar equivalência, liveness e cleanup | declarar problema resolvido por syntax, thread por task, lock-free universal, copy/share oculto, oracle host chamado de runtime |
| W-1211 | descriptor fechado de kernel | `accelerator.module<{...}>()` sintetiza module identity, manifest e launch stubs tipados a partir de um static record; a função original permanece chamável no host | lista heterogênea runtime, lookup por string, reflection ou interface sem nome estável |
| W-1212 | scope de launch owned | `Launch<Module>` é move-only, pertence a module/queue/device/provider generation e fecha admission antes de drenar e liberar uma vez | deinit assíncrono, scope copiável, owner sem close ou cleanup fire-and-forget |
| W-1213 | ownership e transfer de device | staging preserva take/copy/ref/inout; tensor borrowed reside no device ou mapping provado; transfer e host read são explícitos | copy ou materialization escondida, ref escapante, queue concedendo owner |
| W-1214 | submission estruturada | invocation passa por staging, submit, execução, drain, cleanup, commit e join; cancellation pós-submit não presume preemption | cancel como rollback, output antes de drain, task detached ou provider sem join |
| W-1215 | queue e happens-before | provider cunha submit/completion receipts; dependencies vêm de owners/loans/results/waits; cross-queue exige handoff explícito | caller `ready`, ordem global implícita, queue order como ownership ou host visibility |
| W-1216 | fault e generation | device loss fecha admission; owners drenam ou ficam em quarantine; completion stale é suprimida depois do drain | liberar storage vivo, reutilizar generation, continuar admission após protocol fault |
| W-1217 | limits e equivalência | limits cobrem work e retenção; CPU fallback exige module/numeric/layout/effect/memory proof e nunca insere transfer | ordinal físico no source, fallback silencioso, tolerância não declarada |
| W-1218 | evidence KM0/DEV0 | KM0 fecha synthesis/identities/instances/artifact; DEV0 consome esse resultado e fecha scope/queue/submission sem executar W ou provider | um oracle misturando compiler/runtime, snapshot manual, driver fictício |
| W-1219 | composição de recovery | recovery de service compõe call B0, closure E1, gates e supervisor sem criar syntax, annotation ou quinta forma de execução | actor/runtime paralelo, handler durable, retry escondido no source |
| W-1220 | mailbox e admission durável | quotas precedem enqueue, FIFO vale por sender/instance e input commit fixa a admission que sobrevive a process fault | FIFO global, priority com starvation, reservation volátil chamada de commit, queue ilimitada |
| W-1221 | turn e output frontier | uma instance executa um turn de aplicação; runtime closure precede outcome; output retido captura frontier e só abre depois do commit | reentrância default, delivery antes de cleanup, output gate como transaction remota |
| W-1222 | effect identity e dedup | `callId` identifica attempt; `effectId` e digests identificam o efeito; duplicate aguarda ou replaya; conflito e retention são failures explícitas | connection ID como effect ID, payload divergente aceito, tombstone ou outcome sem budget |
| W-1223 | matriz de faults | posição do fault e `StepEffect` determinam replay, retry com mesma key, decisão transactional ou `unknownOutcome`; outcome committed nunca reexecuta | exactly-once universal, retry at-most-once, compensação automática após dúvida |
| W-1224 | generation e ownership em recovery | process fault fecha admission; nova generation reconstrói somente values do journal; frame, pointer, loan e capability não persistem; completion stale é suprimida | serializar stack, reviver borrow, liberar resource foreign antes de drain, reuse de generation |
| W-1225 | disconnect e dependent calls | disconnect revoga capability da conexão sem apagar outcome; retry resolve novo handle; pipeline preserva DAG e propaga incerteza sem criar atomicidade | remote igual a local, capability persistente implícita, pipeline como distributed transaction |
| W-1226 | journal e commit authority | prefixo confirmado, schema/version/digest e uma authority definem recovery; multi-provider usa step ou outbox e SQLite é apenas adapter/profile | aceitar suffix corrupto, WAL em network filesystem, transaction implícita entre providers |
| W-1227 | retention, restart e shutdown | queues, outputs, journal, dedup, tombstones e restart window são bounded; drain fecha admission e aguarda owners/commit/quarantine | restart infinito, compaction que perde record vivo, shutdown com output ou turn pendente |
| W-1228 | evidence SR0 | fixture Última Luz e machine/checker/snapshot/test host cobrem 48 casos, 392 operações e 17 testes sem executar W, database, network ou provider | expected echo, fault booleano sem record real, completion não registrada, chamar model de runtime ou exactly-once proof |
| W-1229 | accounting de channel | capacity mede itens; payload refinado pode limitar bytes carregados; quotas de itens/bytes/trabalho pertencem à mailbox com authority | `WeightedChannel`, callback de peso, peso do caller chamado de memória real |
| W-1230 | distribuição de trabalho | collections usam TaskGroup; streams usam concurrentMap/parallelMap bounded; services cobrem admission contínua e distribuição entre instances | `WorkQueue` MPMC, receiver compartilhado, child detached, fila ilimitada |
| W-1231 | fan-out explícito | tee fecha duas branches estáticas; service fecha subscribers dinâmicos e nomeia loss/replay/close/failure/duplication | `Broadcast` universal, policy de lag implícita, cópia escondida por channel mode |
| W-1232 | latest state separado de eventos | SnapshotCell mantém versão corrente; adapter específico publica revisão depois do snapshot e prova wake sem perda | `Watch` universal, conflation/equality/lifecycle escondidos, state tratado como event history |
| W-1233 | uma syntax de ilha | `fn<Language>` contém body inline; source separado é nó foreign-unit do build graph; builder agrupa units internas | syntax de unit nomeada, arquivo externo como module W, staticlib por função |
| W-1234 | MMIO por capability | DeviceContext e target manifest cunham register tipado; accessors respeitam access mode e volatile não sincroniza | `var volatile`, integer-to-register safe, RMW genérico, barrier implícita entre resources |
| W-1235 | interrupt verificado | host slot fixa ABI, effects, budget, acknowledgement, preemption, overflow e fault policy; trabalho suspensivo usa event bounded | annotation livre, raw frame suspensivo, dois owners de EOI, signal/cancel como interrupt |
| W-1236 | task-local estruturado | associated const descriptor recebe identity nominal; binding immutable/shareable acompanha children, drena antes do pop e não cruza boundaries | mapa ambiental mutável, runtime registry, authority escondida, inheritance por boundary, retain/copy oculto |
| W-1237 | TLS estreito | associated const descriptor recebe identity nominal; safe ThreadLocal aceita initial const, Copy sem drop e closures neverSuspend; target sem TLS rejeita | destructor best-effort, runtime registry, TLS como task/domain isolation, borrow suspenso, emulação por fiber |
| W-1238 | placement data-only | product target variant fixa section/address/alignment/visibility/retain e valida o symbol no payload final | annotation comum, object `used` como prova final, import ou flag livre alterando placement |
| W-1239 | assembly fechada | unsafe fn<Asm> usa signature tipada e contract de target/clobber/memory/stack/unwind/volatility verificado pelo adapter | naked function, clobber/call/unwind escondido, correctness inferida do body opaco |
| W-1240 | forwarding de suspensão | scope higher-order não escapante publica dependência simbólica da callable; call direta e await continuam distintos sem duplicar API | annotation manual, overload sync/async com a mesma forma, task escondida, blocking wait |
| W-1241 | roots duráveis explícitos | outra execução durável começa por service ou SupervisorRef com WorkId/effect próprios; transaction/outbox pode ligar dois roots | child workflow/continueAsNew intrínseco, inheritance oculta, journal ordenado pelo scheduler |
| W-1242 | resolução nominal | imports, requirements e bindings formam o grafo; plugin runtime exige registry capability fora do core | lookup de ServiceRef por string, runtime registry global, nome concedendo authority |
| W-1243 | adapters lock-fixed | toolchain, deployment ou product fixa adapter, conformance e digest; v0 não publica registry/SPI de package | adapter baixado ou registrado durante startup, source shape reservado sem corpus |
| W-1244 | commit provider fechado | um provider por turn, frontier bounded e terminal receipt estável determinam committed/aborted/unknown e owner drain | 2PC implícito, cancellation substituindo decisão, receipt stale publicando state |
| W-1245 | dependency de script explícita (superseded) | Proveniência arquivada em `history/archive/pyn1-workflow`; W-1412/W-1415 usam records package/workspace | compact constructor, metadata inline, sibling manifest, `--with` |
| W-1246 | confirmação de drain | token opaco one-shot liga sessão, generation, closure, ação e deadline; `:drain` consome o token sem repetir source | boolean de caller, consentimento ambiental, replay automático, token reutilizável |
| W-1247 | sessão efêmera | `:receipts` exporta manifest bounded/redacted; heap, bindings vivos, tasks, resources, handles e capabilities não são restaurados | imagem do heap, startup restore, serialização de capability, transcript como source |
| W-1248 | apresentação append-only | itens ricos são imutáveis e ordenados; progresso cria item novo bounded | `display_id`, update, clear, live handle sem owner/drain/quota |
| W-1249 | history tail-only | Jupyter aceita somente tail bounded e redacted; range/search retornam diagnostic explícito | índice persistente de raw source, busca ambiental, resposta parcial |
| W-1250 | CLI inicial de notebook | `w notebook check`, `w notebook export` e `:receipts` usam paths explícitos e não executam cells ocultamente | label abstrato, descoberta ambiental de sessão, hidden replay |
| W-1251 | acesso tabular sem cópia | String e Bytes usam views concretas ligadas ao Batch; copy materializa owner; nested/custom usa projeção tipada, adapter semântico ou materialização explícita | view universal, XView automático, retain escondido, copy implícito, borrow escapante |
| W-1252 | UDP direcional | socket inteiro serializa; split consuming cria receive/send halves únicos, permite uma operação por direção e libera o control block após ambos drenarem | Arc/shared socket no source, múltiplos receivers, protocol genérico sem segundo transporte, drop de um half fechar o sibling |
| W-1253 | radix explícito | parse/format usam radix refinado 2...36, ASCII, lowercase default e uppercase opcional sem mudar Display | locale, prefixo ou whitespace implícito, radix runtime inválido, canonical decimal variável |
| W-1254 | C Exchange N0 | bridge Python pode usar table estática como fast path call-scoped, non-owning, não suspensivo e no current producer stream; metadata morre no retorno e uma lease mantém o producer até o work receipt drenar; N1 usa carrier versioned | expor na std, reter DLTensor temporário, liberar producer antes do drain, atravessar suspension, esconder fallback ou tratar otimização como semântica |
| W-1255 | barreira cíclica | safe std não inclui primitive genérica; phase local usa TaskGroup, epochs usam domain barrier e participantes duráveis usam service/workflow | participant loss implícita, reset de generation silencioso, cancellation sem outcome, barreira universal |
| W-1256 | primeiro shared declarativo | initializer de binding/stored field anotado `shared T` cria o primeiro owner; expression/return usam binding local e `take`; SHC0 fecha allocator custom com `try` fora do tipo e paths de origin do control block | `share`/`try share` current; promotion por overload, argumento ou return; ARC/retain implícito; wrapper nominal obrigatório |
| W-1257 | lock da linguagem | `lock`, `await lock` e `try lock` abrem body scoped sobre `shared T`; body não suspende, não lança application error e não escapa dependency | Mutex/AsyncMutex wrapper, guard, unlock manual, await no body, body repetido |
| W-1258 | gate e lifecycle | HIR/interface preservam allocation/place/access; overlapping concurrent access usa a mesma gate; unlock publica release/acquire; cancellation e drop drenam; sem poisoning ou cross-boundary | análise textual global, gate cruzar process, plain read disputar write, FIFO host como prova |
| W-1259 | seleção lock-avoiding | owner/domain/service/atomic/barrier/SnapshotCell/channel precedem lock; RW lock e wrappers saem da safe std; adapter especializado exige target e benchmark | lock-first, RW universal, async mutex para task-owned state, read comum de atomic stale |
| W-1260 | evidence LM1 | 39 casos/86 operações e 11 testes host derivam as três formas, busy sem body, cancellation, edges, reentry, boundary, drain e substituições; não executam W/provider | reciclar LM0 read/write, expected echo, chamar modelo de runtime/compiler |
| W-1261 | body estrangeiro opaco | `foreign_body_content` preserva os bytes entre as chaves e nunca cria statements W; formatter copia o range | interpretar C como W, normalizar whitespace, AST foreign no CST W, fence paralelo |
| W-1262 | scanner hermético | adapter/lock fixam scanner ABI, profile e digest; recipe liga scanner e body digest; scanner recebe bytes e limits | scanner ambiental, shell, filesystem, adapter sem lock, confiar no Tree-sitter para build |
| W-1263 | inline C baseline | `c-inline-1` cobre tokens que afetam braces e restringe preprocessing; `const` é pointee C em `c.ptr` ou requisito de chamada W-548, nunca readonly type geral | preprocessor completo inline, macro condicionado como delimiter, `const T` nativo, C subset W |
| W-1264 | fallback editorial | fallback editorial preserva bytes/range e retorna `preservedUnvalidated`; WLO/editor tree não vira authority, e W-1267 permanece implementation gap de FB0 tooling | fallback virar build evidence, parser dinâmico universal, body W-shaped, reescrita do editor |
| W-1265 | source map estrangeiro | offsets do adapter são relativos ao content e validados antes de mapear ao arquivo W | span fora do body, line/column sem bytes, diagnostic sem adapter/body digest |
| W-1266 | failure bounded | encoding, delimiters, lexical terminal, nesting, bytes e digests falham antes de codegen; recovery não engole suffix W | scan unbounded, truncation silenciosa, fix-it dentro do body sem adapter, aceitar NUL |
| W-1267 | evidence FB0 | 45 casos/90 operações, 9 testes host, corpus Tree-sitter e um par F0 byte-preserving cobrem scanner/body/recipe sem executar adapter ou formatter | snapshot como implementação, expected echo, alegar frontend C ou formatter prontos |
| W-1268 | evidence CTX0 | identidade nominal, default, binding LIFO, snapshot por child, drain, boundaries, TLS físico, migration e availability são derivados por machine e teste host independentes | mapa ambiental, cópia por child, TLS por task, destructor best-effort, chamar oracle de provider/runtime |
| W-1269 | cache fora do tipo atômico | Atomic descreve value, extent, order e progress; cache isolation não entra no schema do tipo | `cache: .isolated`, granule universal, ordem fraca como mitigação |
| W-1270 | layout privado por evidence | compiler pode separar places disjuntos somente com layout opaco, target/evidence pinados e footprint admitido; ausência de fact mantém fallback | padding obrigatório, erro em target desconhecido, profile ambiental, claim de exclusividade física |
| W-1271 | partition explícita | counters locais e redução aparecem no source porque mudam overflow, publicação e snapshot; atomic global nunca é sharded silenciosamente | auto-sharding, per-thread oculto, soma com overflow diferente |
| W-1272 | layout físico na boundary | adapter ou target record garante offsets/alignment publicados, sem prometer throughput ou cache exclusiva | wrapper safe universal, padding implícito em ABI, benchmark como semântica |
| W-1273 | evidence IL0 | machine/corpus/test host derivam layout aplicado/não aplicado, partition e boundaries; não medem cache nem executam W | snapshot como compiler, target inventado pelo caller, performance claim sem measurement |
| W-1274 | grafia de shared ownership | `shared T` e `weak T` são prefixos de ownership; `shared T?` é Option do handle e `shared Option<T>` possui payload opcional | `Shared<T>`, `shared<T>`, wrapper de std, optionalidade ambígua |
| W-1275 | allocator fora de `shared T` | product profile escolhe o default; construction contract pode receber `allocator:` e preserva origin; allocator não entra no tipo | allocator como generic slot, call `share`, case de módulo criando instance, `try` no tipo |
| W-1276 | texto bounded por composição | refinement limita valor, resource gate limita allocation e carrier de adapter publica layout | `InlineString<N>` no core, capacity como property de String, threshold source |
| W-1277 | placement textual privado | byte bound fornece extent máximo; escape, target, profile e cost model escolhem inline, static, flat ou arena | storage source obrigatório, general allocation escondida sob gate, truncation |
| W-1278 | mutation refinada | cada mutation prova o predicate na saída; unknown usa staging base e reconstrução fallible | check oculto, truncation, panic implícito, valor refinado temporariamente inválido |
| W-1279 | boundary textual física | adapter declara count, fixed bytes, encoding, offsets, padding, bytes não usados e overflow | refinement como ABI, layout W privado atravessando boundary, capacity inferida publicada |
| W-1280 | Blob por composição | `std.blob.Blob` usa `shared Bytes` imutável, faixa checked, type normalizado, duplicação explícita e cursores independentes | provider de Blob, file authority, registry URL, part list dinâmica, clamp silencioso |
| W-1281 | FormData lógico | lista W ordered de String ou Blob+filename; append/set/delete preservam o standard e falha não publica mutation parcial | object dinâmico, DOM form, filename como Path, unordered map, mutação parcial |
| W-1282 | multipart bounded | FormDataLimits + MessageLimits precedem attachment; host escolhe boundary; encoder streams Blob com backpressure | boundary do caller, collect completo, tamanho unchecked, Content-Type divergente |
| W-1283 | Body Web completo | BodySource aceita String, Bytes, URLSearchParams, Blob, FormData e stream; blob/formData consuming exigem limits | BodyInit apagado, stubs parciais, materialização sem limit, provider extra para Blob |
| W-1284 | head de síntese de kernel | `accelerator.module` é compiler-owned, recebe static record em const de module scope e produz a única conformance KernelModule sem runtime call/registry/authority | conformance manual, função runtime, macro user-defined, descriptor local ou reflection |
| W-1285 | identidade de module | interface cobre fields/signatures; implementation acrescenta callable privado, HIR/call graph; paths, tempo e ordinal físico ficam fora | hash de arquivo, rename privado quebrar interface, identity ambiental ou uma identity única |
| W-1286 | especialização finita | identidades canônicas de tipo e ConstIR derivam KernelInstanceId; bundle genérico é source-backed e binary-only contém conjunto finito fechado | JIT implícito, lookup por string, evaluator duplicado no linker ou binary incompleto |
| W-1287 | stub e artifact | stub preserva labels/ownership e acrescenta Launch; artifact liga instances, target, numeric mode, features e provider ABI; open só valida/abre | transfer escondida, open compilar, artifact sem target facts ou launch sem failure typed |
| W-1288 | subject explícito de refinement | `value` é binding contextual imutável do candidate dentro do predicate e baixa para a mesma ConstIR de `.member`; lookup lexical exige qualificação | `value` ambiental, shadow dependente de imports, storage sintético ou HIR diferente para a forma longa |
| W-1289 | members associados diretos | `const` e `static fn` pertencem ao namespace compile-time do tipo; protocol só é necessário para requisito generic; mutable type storage continua ausente | companion obrigatório, metatype runtime, `static var`, módulo singleton ou witness sem consumidor polimórfico |
| W-1290 | labels callable uniformes | `name: T` é posicional em qualquer índice; `named name: T` exige label homônimo; `external internal: T` exige label distinto e `_ name: T` torna o label opcional; initializers e enum payloads permanecem record-like | labels inferidas pela posição, todos nomeados, reorder, ranking por tipo |
| W-1291 | posição de ownership em parâmetro | labels e binding ficam antes de `:`; `ref`, `inout`, `take` e `const` iniciam o contrato à direita; `copy` fica somente no call site | `take value: T`, modifier como label, `copy` na assinatura ou duas ordens canônicas |
| W-1292 | operação de ownership no call site | marker aparece quando a call opera sobre place existente; borrow já tipado e rvalue owned novo passam diretamente; receiver read-only permanece implícito | omitir sempre `ref`, marcar todo rvalue, borrow implícito de owner lvalue ou marker sem type/value category |
| W-1293 | categorias das formas de memória | `shared T` e `weak T` são tipos de handle; `ref T`, `inout T` e `view T` são tipos dependentes; `take T` e `const T` são contratos; `atomic` modifica storage e baixa para `Atomic<T>` | `Shared<T>` público, `atomic T`, allocator no tipo shared ou tratar toda keyword como modifier equivalente |
| W-1294 | uma abstração de allocator | ASC0 usa um `Allocator` owner/capability com plan lexical; origin preserva instance, lifetime, mobility e deallocator; `Arena` é apenas lowering interno | `Allocator<(.arena)>` source-visible, `Allocator<(.crossDomain)>`, provider enum fechado, API Arena pública |
| W-1295 | payload de budget de allocation | `BudgetExceeded` publica limit, committed e requested bytes; overflow usa `.sizeOverflow`, e identidade física fica no diagnostic sidecar | erro Boolean, bytes disponíveis globais, provider identity no valor ou payload truncado após overflow |
| W-1296 | root de processo único | host cria Arguments e Context; handler explícito recebe owners e entry curto empresta via `process.args`/`process.context`; `process.clock()` preserva identity/origin/authority/lifetime da projection longa, e `process.deadline` preserva value identity/origin/lifetime sem ampliar authority | descartar argv, injetar args/ctx, singleton process, lookup global ou duplicar projections |
| W-1297 | argumentos nativos | Arguments preserva OsString ordenado, empresta por get/iteração e oferece comparação textual exata sem lossy decode | Array<String>, locale, normalização, cópia por projection ou acesso sem bound |
| W-1298 | Context por capability | getters retornam owners retidos root-scoped; reachability exige stdio/network/clock/signals/services individualmente | mapa universal, capability opcional runtime, Context serializable ou getter ambiental |
| W-1299 | stdio de processo | Input usa um cursor ByteSource e stream de linhas UTF-8 bounded; Output usa progress e calls sem byte interleaving | readline global, decode lossy, newline implícito, collect ou concorrência sem admission |
| W-1300 | signals geracionais | registration owned fecha admission; replace publica generation; callbacks aceitos são children estruturados fora do raw handler | callback W no signal handler, hostBinding estático, contagem exata, handler detached |
| W-1301 | status e drain separados | ExitCode cobre término normal portátil; Services.drain fecha admission até deadline sem prometer rollback ou matar o processo | raw status truncado, drain como transaction, timeout igual cancel ou exit ambiental |
| W-1302 | raiz de filesystem | FileSystem é capability root-scoped; resolução prova containment e rejeita absolute, traversal e symlink escape | cwd, root global, canonicalização lexical ou path concede authority |
| W-1303 | paths nativos | Path preserva OsString; Utf8Path é fallible e display lossy não participa de lookup, equality ou identity | String universal, locale, normalização ou case folding implícitos |
| W-1304 | snapshot de arquivo | snapshot materializa cópia bounded, valida versão e publica SnapshotByteSource estável | cast de File, mmap invisível, limite após allocation ou aceitar mudança durante leitura |
| W-1305 | mutation de namespace | create/remove/rename ficam na mesma authority; rename no-replace default, replace regular explícito e unknown outcome tipado | recursive helper core, delete+rename, cross-root ou retry cego |
| W-1306 | durability explícita | sync data/all, finish data/all e syncNamespace são as únicas solicitações; none, drop, rename e exit não inserem persistência | flush universal, async deinit, durability por default ou promessa sobre cache externa |
| W-1307 | interferência de arquivo | shared File usa I/O posicional; ranges disjuntos e read/read são paralelos, overlap unordered é provider-ordered com warning e append atomiza somente offset; sem ordering ele também interfere com I/O posicional | cursor compartilhado, lock implícito, tratar recurso externo como data race W ou prometer ordem de append |
| W-1308 | IoError portátil | kind fechado, operação lógica W e cause opaco bounded; adapter de domínio pode promover o snapshot | errno público, syscall como operação, cause serializável ou enum non-exhaustive |
| W-1309 | controle de I/O | wouldBlock suspende, interrupção sem progress repete, EOF usa ReadStep e cancellation usa TaskOutcome | transformar controle em IoError ou oferecer retryable Boolean |
| W-1310 | Duration operacional | total signed exato de nanoseconds em i128, layout opaco, arithmetic checked e conversão de unit exata; quantities físicas ficam em Pesquisa | unsigned duration, float, infinity, wraparound, picoseconds no baseline ou layout público |
| W-1311 | Clock por capability | `.clock` projeta owner monotônico root-scoped; `process.clock()`/`ctx.clock()` são default nonthrowing quando capability está disponível; `hostSuspend:` é seleção ativa com slot estreito included/excluded; runtime interno não concede leitura à aplicação | global clock, constructor público, lookup ambiental, `time.clock()` sem Context, `Clock.current` ou `.monotonicClock` paralelo |
| W-1312 | origem temporal local | Instant e Deadline são opacos, dependem do root e não cruzam service/wire/storage/foreign; Duration cruza | raw ticks públicos, serializar Instant, comparar roots ou identity generic na syntax |
| W-1313 | profile monotônico honesto | now não regride; resolução positiva e `HostSuspendPolicy` descreve somente suspensão do HOST/SO; included, excluded e unspecified afetam deadline de modo explícito; exemplo de sleep/hibernate/VM pause no restaurante | frequência constante universal, bool do caller, inferir política unspecified, tratar await/coroutine como suspensão do host ou timing como prova |
| W-1314 | expiration estruturada | deadline relativo nonnegative, zero imediato, nunca early; retorno pode atrasar e expiration cancela com cleanup drain | matar thread, alarme exato, rollback, error da aplicação ou liberar recursos cedo |
| W-1315 | tempo civil separado | `.clock` não fornece data, timezone ou calendário; contrato civil futuro terá capability e values próprios e nunca dirige deadline | `now()` global de parede, timestamp em Instant ou calendário implícito no runtime |
| W-1316 | evidence TIME0 | source W, 47 casos e oito testes host cobrem Duration, Clock, origin, profile, suspensão do HOST/SO, deadline, boundaries e clock virtual sem alegar provider | timing real como oracle, expected echo, chamar modelo de scheduler ou provider |
| W-1317 | fronteira de evidência R1H0 | quatro bundles independentes fecham parse/oracle host, ownership/effects/cancel/cleanup e formas reservadas explícitas; compile, run, provider e estudos humano/modelo permanecem missing | chamar parse de ratificação, chamar oracle de execução W, ou inventar participantes |
| W-1318 | R1 nomenclatura de suspensão (retired) | estudo histórico preserva `SuspendAccounting`; decisão ASC0 escolhe `HostSuspendPolicy` e separa inspection passiva de aquisição ativa | booleano, inferência de `unspecified`, ou tratar suspensão da task como HOST/SO |
| W-1319 | R1 aquisição de owner fraco | target normal lê `weak T?` como `shared T?`; `.upgrade`, property `strong` e method `strong()` passam a alternativas retiradas/rejeitadas; live, expired e último strong release permanecem no oracle | acesso de payload por weak, aquisição não linearizável, ressurreição ou shared implícito |
| W-1320 | R1 escopo de Arena (retired) | estudo histórico compara Arena; ASC0 escolhe bloco lexical allocator, ledger de drops, `rehome` antes da fronteira, escape proibido e cleanup automático | Arena como source surface, escape unchecked, bulk release antes de drop W, ou alocação OS |
| W-1321 | R1 slot runtime de allocator | `Array<String>(allocator: memory)` preserva capability e origin; `using:` permanece label local livre; `allocator:` é control argument reservado em construction expressions quando o contrato publica allocation sites; mobility é derivada | capability no type identity ou comptime, inferência por texto `using:`, `Allocator<(.crossDomain)>` source-visible, failure tardia ou origin omitida |
| W-1322 | métricas e fechamento R1H0 | variantes preservam casos e traces quando a forma é genuína ou explicitamente modelada como candidata; selected continua baseline current; métricas não afirmam estudo humano ou de modelo | contar bundles como implementação, promover oracle a runtime, ou declarar ratificação |
| W-1323 | SDM0 | derive S0/D0; pairs W-785/W-788; meta W-791..820 | echo, pair, backend, global, secret |
| W-1324 | phases | DESIGN/catalog/machine share closed ordered set | missing phase, alias, drift, lifecycle |
| W-1325 | ASC0 memory transition evidence | M1 covers contextual weak transitions; four R1 host oracles cover weak acquisition, Arena problem matrix, allocator control-label reservation/mobility and suspend deadline outcomes; none executes compiler, runtime or provider | expected echo, provider execution, inferred label semantics, weak payload access |
| W-1326 | allocator control argument | `allocator:` is reserved only in construction expressions, appears before ordinary arguments, stays outside overload/initializer signature, and governs published allocation sites only; `using:` remains free elsewhere | global reservation of `allocator:`, propagation to arbitrary initializer allocations, user-defined allocator meaning, inference by label text |
| W-1327 | declaração lexical de allocator (estendido por W-1348) | `allocator name: plan { ... }` e `allocator plan { ... }` criam owner/lease/scope; construction direta usa a stack corrente; nome anônimo não cria binding observável | `Arena.fixed`, scope sem owner/lease, região implícita ambiental, contexto ambiental transitivo, regra especial de shadowing para allocator |
| W-1328 | plans fixed, bounded e custom | `PRC0-W-1328-current` e `PRC0-W-1328-adversarial` fecham admission, custom lease, typed drops e close ordering no ASC0 oracle; `.fixed`/custom contract continuam design, e `.bounded` permanece Research | autoridade PRC0; provider/lowering gap reutiliza W-1333; não alegar O(1), raw provider ou fallback oculto |
| W-1329 | lifecycle, escape e rehome | close drena children/waits/loans/dependents, executa drops tipados e só então reclaim; unwind é uniforme; origin local sobrevive a `await` na mesma task com owner/lifetime estáveis, mas exige `rehome` antes de spawn/service/channel | reset comum, detached work, escape unchecked, drop após bulk release |
| W-1330 | parâmetro contextual de allocator (estendido por W-1349/W-1350) | primeiro e único slot `allocator name: ref Allocator` entra signature/resource facts/ABI/HIR, publica conclusão contextual de call e preserva function type/callable/lifecycle facts | parâmetro oculto em toda função, slot não primeiro/duplicado, propagação sem slot, ABI foreign escondida |
| W-1331 | aquisição ativa de clock | `process.clock()`/`ctx.clock()` selecionam default nonthrowing quando capability está disponível; `hostSuspend:` seleciona policy com `HostSuspendPolicy<[.included, .excluded]>`; `Clock.hostSuspendPolicy` é passive fact e `.unspecified` é diagnostic | `SuspendAccounting`, `suspendAccounting()`, `time.clock()` ambiental, inferência provider |
| W-1332 | binding explícito de units | `250<ms>` exige import seletivo/flattened de `std.si`; `import si from std` exige `250<si.ms>`; registry ambient não existe | ms global, qualificação inconsistente, source sem binding |
| W-1333 | evidence ASC0 | Last Light, corpus, parser, HIR, ASC0 e `PRC0-W-1328-current`/`PRC0-W-1328-adversarial` cobrem allocator scope, admission, lease, typed drops e units; nenhum declara compiler/runtime/provider implementado | manter `implementation-evidence-gap`; expected echo, check como execução, compatibility pre-1.0 |
| W-1334 | construction contract SHC0 | `let root: shared T = try T(allocator: memory, ...)` mantém `shared T` como prefixo, exige binding/field explícito, cobre payload e `result.$controlBlock`, separa `initializerThrows` do `failure` do site, colapsa edges com o mesmo error type e exige error set exato/único quando são distintos, rejeita `share`/`try share`/container público | try no tipo, promotion em call/return/inference, wrapper nominal, allocation site arbitrário, união implícita ou error set com extras/duplicatas |
| W-1335 | lifecycle SHC0 | uma fronteira atômica segue a prova de facts; ordem física não é promessa; strong zero deinit payload e libera block se weak zero; weak zero libera block somente com strong zero; acquisition live cria owner e expired não ressuscita | publicação parcial, ressurreição weak, control block sem origin, layout/count fixos |
| W-1336 | provider profile e origin map SHC0 | admission/open é separado da construção; `AllocationOriginMap` inclui `$storage`, `$controlBlock` e record com origin/deallocator/mobility/lifetime/adoption/bulk; payload shareability vem do tipo/HIR, contador do plano de control block e mobility da travessia do map; profile/recipe fecha progress e limits | descriptor sozinho como prova, allocator no tipo, origem escondida no pointer, plan open dentro do initializer, caller flags |
| W-1337 | failure e boundary SHC0 | falha consuming limpa prepublication exatamente uma vez; `rehome` unique precede shared cross-domain e altera origin/mobility sem provar shareability; boundary exige payload shareable, contador thread-safe e todas origins móveis; shared não é rehomable; nested calls não herdam allocator | restauração implícita, shared rehome, propagação transitive, boundary local, flags caller divergentes |
| W-1338 | evidence SHC0 e FFI | oracle independente e fixture do restaurante cobrem default/custom/try, weak, rehome, nested origins e cycles; `memory.w::watchClosingBell`/`BellLease` fornece a fonte FFI, enquanto os casos exigem unregister para fechar admission, drain in-flight, destroy/unpin/reclaim em ordem; `BellLease` não prova drain pelo header; lease externa fecha e drena separadamente no ASC0; métricas não alegam runtime | M1 interno chamado source, API FFI inventada no fixture SHC0, callback local escapante, drain antes de unregister, facts FFI incompletos, oracle como compiler/provider |
| W-1339 | fronteira de evidência R1S1 | oito bundles em sete famílias fecham source-boundary, contracts, declarations, patterns, callable/property, phases e delimited values; parse/oracle são host evidence e compile/run/provider/human/model claims permanecem missing | chamar parse ou oracle host de execução W, declarar participante inexistente, ocultar witness |
| W-1340 | bundle R1S1 de fronteiras source | `r1-source-boundaries` separa operações de newline, forced semicolon, discard e formatter; a canonização deriva da sequência de source items e usa `formatting.w` como source base | automatic semicolon insertion, remoção por aparência, policy de formatter ambiental ou fonte sem relação |
| W-1341 | bundle R1S1 de contratos syntax | `r1-static-contract-syntax` compara envelope attached, close nested e contrato local no fixture `generics.w`; `build.w` é source reference e o manifest angular fica em witness textual | reparsing por whitespace, close obrigatório inventado ou package record tratado como contrato local |
| W-1342 | família R1S1 de declarations | `r1-manifest-surface` cobre manifest data-only e `r1-data-declaration-surface` cobre `billing.w::MenuItem` e `kitchen.w::StockReservation`; field export e projection segura são alternativas, enquanto storage público é rejeitado | package inline, export total implícito ou constructor público sintetizado |
| W-1343 | bundle R1S1 de patterns | `r1-pattern-surface` compara identity nominal, tuple scrutinee, cases fechados e field-set/open-rest derivado de `newField`; structural, multi-subject, implicit-open e custom dispatch são witnesses | pattern estrutural sem nominalidade, exaustividade aberta implícita, route error escolhido por dado ou handler que esconde effects |
| W-1344 | bundle R1S1 de callable e property | `r1-callable-property-surface` usa `kitchen.w::isIdle`, `hardware.w::legacyProbeStatus` e `callables.w` para a closure; separa property/method, effectful property, `fn<C>`/slot nomeado e closure/anonymous-fn | suspension ou failure escondidas em member access, capture omitido, status runtime usado para rejeitar sintaxe ou linguagem estrangeira reparseada |
| W-1345 | bundle R1S1 de fases source | `r1-source-phase-surface` deriva first declaration, import-after-declaration e body presence de uma sequência de source items; interleaved import e prototype são operations adversariais explícitas | descoberta por scan de arrays já classificadas, prototype solto, empty import como prova ou ordem de import ambiental |
| W-1346 | bundle R1S1 de values delimitados | `r1-delimited-value-surface` separa matrix nested, tuple singleton, grouped scalar, ragged adversarial e semicolon rejection causal; owner consumption fica explícito | semicolon com significado de row, grouping tratado como tuple, ragged ad hoc ou owner consumido duas vezes |
| W-1347 | métricas e fechamento R1S1 | scripts derivam 8/22/32 com denominador global 75 e snapshot mutation checks; nenhuma métrica afirma compilação, execução, provider ou estudo humano/modelo | contagem manual, promoção por digest ou tratar design-oracle-input como ratificação |
| W-1348 | formas e stack corrente ASC0 | named/anonymous cria owner, lease e scope; root, parâmetro e lexical formam stack com prioridade explícita; open failure não publica contexto/binding | binding sintético, ambient lookup, fallback de acquisition ou herança lexical entre funções |
| W-1349 | conclusão contextual de call | slot standard primeiro recebe `ref currentAllocator`; cadeia entra no callee e sem slot reinicia no root; W-ALLOCATOR-0010 cobre somente slot contextual sem current | inferência por nome/tipo, parâmetro comum ou propagação sem slot |
| W-1350 | interface, callable, lifecycle e evidência | signature/HIR/ABI preservam slot; overload usa W-LABEL-0004, initializer usa W-ALLOCATOR-0011; callable/capture/lifecycle/explainability e status permanecem explícitos | default parameter, ABI oculto, capture ou rehome implícito, claims de implementação |
| W-1351 | expressividade de borrow de ordem superior | BRX0 fecha receiver, body-derived mapping e bodyless com origem única (uma entrada compatível para free/static/protocol); duas ou mais entradas independentes sem autoridade rejeitam `W-BORROW-0011`; callable cria loan por invocation, stream item fica preso ao receiver/storage e adapters preservam OriginSet; relation owned BRX2 e carrier nominal ficam separados, sem syntax de lifetime, GAT ou metadata runtime | copiar lifetime names de Rust, promover relation syntax, tratar aggregate como mesmo resultado, esconder mapping em `any fn`, ou alegar compiler/runtime/provider |
| W-1352 | sucessor ATOM2 do gate ATOM0-G1 | ATOM2 supersede ATOM1: carrier canônico compiler-synthesized promove records fechados value-only de 1–128 bits dentro de `Atomic<T>`/`var atomic`; handle `{slot,generation}` usa owner table e checked exhaustion sem wrap; SnapshotCell/domain continuam safe; adapter de reclamation especializado é permitido somente `unsafe` como implementation-evidence gap; pointer/tagged pointer e RCU universal são rejeitados | pointer/owner safe por atomicidade, acoplar padding/layout raw de T ao carrier, protocol user-defined para qualquer record, generation wrap ou owner table ausente, RCU universal safe, reclamation sem quiescence/drop, ou claims de compiler/runtime/provider |
| W-1353 | método e invariantes de GEN1 | o oracle compara as mesmas traces em duas máquinas independentes (`switched-resume-frame` com slots/PC e `returned-continuation-state-loop` com estado/token); owner graph, commit/HB, resultado, cancelamento e cleanup/drop/drain são invariantes. A/B/C permanecem composáveis no escopo observado. Ver W-454–469, W-1161/W-1163, W-1185/W-1186 e W-1240 para contratos existentes. | frame de usuário como ABI, lowering que altera ownership, metadata de runtime, caller echo ou tratar trace físico como semântica |
| W-1354 | dispositions de evidência e ergonomia de GEN1 | métricas estruturais de símbolos source e slices do mesmo cenário deixam a pergunta ergonômica aberta (`observedStructuralDifference` + `humanDecisionPending`); o builder bounded é current-candidate somente para diálogo; frame/resume público é intencionalmente rejeitado; bloco Stream compiler-owned é Research-candidate sob captures, capacity/prefetch, `Result` item, cancelamento, cleanup, effects e ausência de identidade/ABI pública. O oracle informa/estreita `GEN0-R1`; compile, run, provider e estudos humano/modelo continuam ausentes. | `yield` ambiental com frame público, lifetime/effect/ABI ocultos, LOC como decisão, promover bloco compiler-owned sem prova, promover D por obrigação ou declarar fechamento por oracle |
| W-1355 | contratos atuais de adapters IPC A/B | A immutable mapped snapshot e B bounded mapped byte channel/log tornam-se contratos atuais de adapter/provider somente com receipts explícitos de layout/schema/generation/lease, atomics+wake address-free e lifecycle/security; não adicionam syntax/API W; sem receipt/profile há fallback para SnapshotByteSource/wire/Arrow/service; B é volatile e C universal `Mapped<T>`/`shared`/raw pointer permanece rejeitado | `Mapped<T>` universal, `shared T` como IPC, mmap invisível, raw pointer, receipt ausente tratado como sucesso ou estudo host tratado como comportamento implementado |
| W-1356 | snapshot mapped relocatable, generation objects e selector durability | cada generation é um objeto/extent imutável separado; leases ligam `objectIdentity+generation` e impedem reuso enquanto vivas; layout usa offsets/índices relativos e payload pointer-free; header valida magic/version/schema/schemaDigest/layoutDigest/length/alignment/endian/generation; durable request ordena stage/hash → request → flush data+metadata → release selector → flush selector/namespace → receipt; visibility-only não inventa receipt; crash antes selector preserva a generation anterior, crash pós-selector sem receipt deixa visibility viva mas recovered current desconhecida, e crash pós-receipt é sucesso; stale remap, resize e view escape são explícitos | address equality, cast de struct nativa, publish antes de validation/flush, inferir flush físico, reusar generation com lease, resize com view viva ou access pós-unmap |
| W-1357 | channel mapped wire carrier, commit/materialization e crash | carrier bounded de bytes (não `Channel<T>` raw) exige cap0 sem slots com rendezvous pareado e capN com ocupação derivada do header validado; header.length iguala o extent e `slotCount*slotSize` cabe em `slots`; owner local retorna antes de commit, commit publica bytes wire canônicos e a generation recebe bytes depois; receiver valida length/schema/checksum e cria owner novo, provando no máximo um owner por slot (não exactly-once distribuído); cancelamento mantém regra existente; provider prova atomic width/order/alignment/lock-free progress/wait-wake; crash de writing faulta generation, full committed sobrevive ao producer e pode ser materializado pelo reader, reading faulta generation e supervisor ordena fault→stop-access→drain→drop-view→unmap→close→reopen sem repair oculto | capacity caller, ordinary atomic como prova process-shared, String/owner no mapping, lock/allocator/scheduler oculto, repair in-place ou worker cooperativo bloqueado invisivelmente |
| W-1358 | providers POSIX/Windows, backing e reducers | cada case escolhe binding authority: POSIX/Windows file-backed para snapshot durável com data+metadata receipt, POSIX `shm_open`/Windows pagefile para channel volátil; apenas `allowedLayouts[]`/`allowedSchemas[]` e seus digests são autoridade; reducers independentes derivam eventos, lifecycle físico e compact outcome comum; divergência de reducer/provider, facts caller, FFI close fora de ordem e fallback unsupported são rejeitados ou explicitamente normalizados; Windows não finge `unlink` e immediate withdraw retorna unsupported | selecionar resultado pelo expected/flags, equivalência de nome/handle, callback após unmap, `FlushViewOfFile` como durability total ou provider fact inventado |
| W-1359 | evidence IPC2, CAP0 e documentação | provider-authoritative publication e durable selector receipt tornam-se contrato atual de adapter/provider condicionado a receipts; o corpus state/event-derived, POSIX probe digest-backed, fallback SnapshotByteSource/wire/Arrow/service e `unknownDurability` permanecem boundaries explícitas; B é volatile e journal durable usa A/service; probe Windows, provider, crash-recovery, durability, w-compile, w-run, FFI, stress e estudos humano/modelo permanecem missing; C universal continua rejeitado | chamar probe/oracle host de execução W, fechar gate por LOC ou fixture, alegar provider Windows/crash/durability sem evidência, tratar `unknownDurability` como sucesso, ou promover C universal |
| W-1360 | problema-first SYN1/SYN2 | SYN1 separa A composition atual, B transform de dados, C generated module artifact, C2 recipe/IR e D mutation dinâmica; SYN2 fecha o C estreito como module set `.w` content-addressed, com C2 e D rejeitados. O host oracle não é implementação | classificar o problema inteiro como macro gap, medir maturidade ou promover o oracle a implementation |
| W-1361 | A synthesis fechada | `Hashable`/`Reflectable`, generic/protocol composition, JSON, `data.Row`, kernel synthesis finita e declarations manuais preservam identity nominal e constraints fechadas; `Display`, codec genérico e synthesis universal continuam fora | annotation universal, protocol por nome, synthesis parcial ou reflection como trigger de declarations |
| W-1362 | B artifact typed atual | `final.menu` continua no build transform W0 e publica bytes typed (`MenuBytecode`/resource); quando declarations não são necessárias, runtime lookup ou data artifact é suficiente | fazer o menu compiler emitir declarations ou chamar o output de módulo W sem frontend normal |
| W-1363 | C módulo gerado separado | SYN2 promove como contrato de design o module set content-addressed de files `.w`, provenance e source maps; cada unit reabre antes de freeze e a publicação exige parse/name/type/ownership/effect/ConstIR no contrato, sem alegar compiler executável | phase in-process, current-module injection, HIR splice, source que não é W ou compiler claim |
| W-1364 | C2 recipe/IR | typed declaration recipe/IR é rejeitada quando duplica parse/name/type/ownership/effect checking e não resolve nada além do artifact C; só uma prova futura de ganho adicional poderia reabrir a comparação | segundo type checker, recipe sem provenance ou recipe que escolhe resultado pelo expected |
| W-1365 | D mutation dinâmica | proc macro, annotation, decorator, metaclass, eval/exec, textual AST mutation e current-module injection são intentionally-rejected por fase/authority/identity não fechadas | copiar proc-macro expansion, `cfg` textual, metaclass runtime ou eval como contrato W |
| W-1366 | DAG de geração | action events terminam em tool-finish; `observedTrace` alcança somente staged output e parse/source-shape host; `requiredPhaseTrace` registra parse → name → type → ownership → effect → ConstIR → interface diff → freeze → publicação candidata → consumer sem alegar execução de compiler; graph receipt exige dependencies, `tool produces output` e consumers que importam output | phase caller-owned, produces ausente, direção de import inválida, ciclo/reachability quebrada ou tratar required trace como evidence |
| W-1367 | action/result identity | action recipe key inclui tool artifact/profile/host, entry, execution platform, typed input digest/schema, dependency receipts, output descriptor/source profile, graph receipt, declared target+ABI receipt, capabilities, quotas e version; output digest e paths físicos ficam fora, no result/module ou adapter | incluir output digest/path físico na action key, omitir descriptor/dependency/graph/ABI fact, mtime/random/time/env/network ou cache que escolhe outputs conflitantes |
| W-1368 | interfaces e docs/maps | public inventory e schema facts derivam `SemanticInterfaceKey`; docs e source-map manifest derivam `DocumentationKey`/`DiagnosticMapKey`; field/enum drift recompila consumers, docs/map/private body-only não | um digest misto, map como API, body privado na key ou fake fix sem editable origin |
| W-1369 | dois targets | duas projections independentes compartilham logical generated module, interface e diagnostics; target-neutral output mantém semantic/action keys, enquanto physical artifact e WAbiKey podem mudar; facts explícitos produzem variants targetSpecific completas | host target implícito, backend escolher resultado, base singular para targetSpecific ou target triple sozinho como semantic identity |
| W-1370 | duas publicações, failure e cancellation | SYN2 fecha a fronteira semântica: action-result/CAS ficam separados de interface/compiler cache; parse/receipt/map fault preserva somente o result; error/cancel/quota/OOM pré-result descarta staging com bookkeeping host cleanup→drain→discard uma vez e panic não promete cleanup de usuário | cache/interface colapsados, partial action result, compiler cache host inventado, cleanup success, state residual ou cleanup duplo |
| W-1371 | diagnostics e provenance | generated span só produz fix quando logical SourceId, source digest, byte span editável e unique generated coverage são exatos; source overlap permite many-generated-to-one-source, endpoints respeitam UTF-8 e path editável fica no adapter; generated-only não inventa fix | checkout path em DiagnosticMapKey, overlap source rejeitado, offset mid-sequence, first mapping ou fake fix |
| W-1372 | hermeticidade e capability | capabilities e authority requests são vazias; handles read-only devem resolver ao source path selecionado ou `module://identity`, cobrem exatamente inputs e ficam fora da action key; I/O ou suspend permanece build transform; undeclared FS, environment, network, clock e random são negados | authority ambient, handle para arquivo alheio, path físico na key, secret access, shell callback ou capability implícita |
| W-1373 | evidence SYN1/SYN2 | estudo SYN2/DYN2, cases, machine, manifest, snapshot e testes são source-backed host evidence com refs oficiais; o boundary é current oracle-backed, enquanto compiler/name/type/ownership/effect/ConstIR, CAS, target compiler/provider, run e human/model permanecem implementation gaps | chamar Tree-sitter/receipt/registry host de semantic frontend/provider, escolher result por expected, ou fechar gate por snapshot |
| W-1374 | digests e source inputs SYN1 | todo digest de source/tool/input/output/provenance aceita somente `sha256:` hexadecimal lowercase real; source bytes de `reference/last-light/menus/final.menu` e typed descriptors derivam seus digests, handles read-only cobrem exatamente os inputs declarados e source refs exigem símbolo único | pseudo digest, sourceRef forged/stale, typed input sem schema/digest ou handle ambient |
| W-1375 | expected, route e eventos SYN1/SYN2 | eventos strict até finish ou cleanup→drain→discard derivam route/status e publication; expected, status, route e failure selector não são authority do caller; o checker fixa a fronteira event-derived current | expected echo que escolhe outcome, malformed C marcado intentionally-rejected, phases caller-owned ou failure selector |
| W-1376 | output source-shape SYN1 | UTF-8 e syntax usam o Tree-sitter W real; depois um scanner bounded mascara comments/strings, ignora nested scope, agrupa headers multilinha e deriva somente imports/declarations do profile fechado; frontendReceipt liga facts sem alegar full type-check | regra acidental one-line, entry/service/foreign/test/top-level statement, proxy chamado parser, booleans caller ou compiler claim |
| W-1377 | module/interface identity SYN1 | `moduleCodeDigest`/result cobre source bytes e private body; `SemanticInterfaceKey` normaliza visibility, generics/statics, effects, throws, ownership, origin/allocation, const, conformance e resource facts; docs/map keys ficam separados | incluir provenance/docs/source map na module key, interface superficial ou private body que força consumer rebuild |
| W-1378 | target projection SYN1 | target-neutral compartilha module/interface/diagnostic/action identities e deriva WAbi por registry-backed ABI facts; targetSpecific não possui base singular e inclui target identity, semantic+ABI facts e registry digest/revision em action/WAbi receipts por variant | physical artifact como ABI authority, ABI omitida da action key, targetEquivalent caller boolean, base singular ou host target implícito |
| W-1379 | source-map diagnostics SYN1 | mapping verifica byte bounds/boundaries, logical SourceId/digest, duplicate e overlap no generated axis; source spans podem sobrepor; fix usa a única mapping que cobre o diagnostic, inclusive a segunda; generated-only não inventa fix | first mapping, rejeitar many-to-one source, stale/duplicate map, UTF-8 mid-sequence ou fake fix |
| W-1380 | manifest evidence SYN1/SYN2 | manifest exige roles/artifacts/sourceRefs/officialRefs exactos, target registry separado, HTTPS allowlist, closure digest chain, reused reducer refs e current/missing evidence; provider-ready forged e refs extras são mutations rejeitadas | provider-ready forged, registry host tratado como provider, extra/troca de ref, role Research para C2, source symbol repetido ou URL primária trocada |
| W-1381 | BRX3 problem-first source clause | Last Light `selectPrimary` fecha requirement/interface bodyless com `borrows(...)`; body/default continua prova; source spelling é contextual, sem lifetime names/GAT/runtime metadata | apresentar schema sem cláusula como comportamento implementado, trocar o problema por feature estrangeira ou usar source sem símbolo real |
| W-1382 | relation authority and witness proof | requirement/interface possui a relação; provider, implementation e witness verificam slots, modes, digest, interface lock e provider expectation; caller/call-site nunca escolhe; generic/open divergence rejeita | caller claim, witness-only authority, stale/missing/duplicate/forged slot, mode ilegal, digest drift ou witness divergence |
| W-1383 | BRX3 host oracle and composition | máquina deriva clause resolution, ordinals, relation/edges/OriginSet/SemanticInterfaceKey/WAbi invariants e invocation boundaries; snapshot é host design evidence e implementation gaps ficam explícitos | expected echo, booleans caller, runtime relation table, WAbi carrier ou tratar oracle/parser como compiler/provider |
| W-1384 | BRX3 promotion gate and stop condition | a forma source é vigente antes de W 1.0; compiler/HIR/separate compilation/provider/linker/FFI/runtime permanecem implementation-evidence-gap e não mudam o contrato | continuar se relation exigir caller authority, hidden escape, runtime state, witness divergence ou WAbi change |
| W-1385 | CYC1 Restaurante e fronteira do problema | ciclo explícito começa por `MenuSection` parent weak/children shared, observer hub, service/plugin/listener graph, caches, linked structures, actor refs, FFI registrations e recursos; CYC0 continua uma composição por owner, weak e close/drain | substituir o problema por uma primitive estrangeira, tratar callback/service edge como ownership implícito ou omitir file/socket shutdown |
| W-1386 | CYC1 máquina event-derived | corpus e oracle derivam admission, strong/weak edge, close/unlink, unregister, cancel, callback enter/exit, drain, quiesce, drop, destroy/unpin/reclaim, SCC Tarjan, reachability, unknown boundary, drop order e census; expected não escolhe outcome | expected echo, caller outcome booleans/flags, SCC mutável durante census, census antes de drain/quiescence ou collector side effect |
| W-1387 | CYC1 static/dynamic cycle boundary | SCC strong fechado e somente `deinitOnly` deriva `W-OWNERSHIP-0014`; SCC criado em runtime exige close/drain e residual pós-drain deriva `W-MEMORY-0001`; root vivo permanece live-root e root/edge foreign-hidden sem adapter permanece `unknown` | rejeitar todo grafo dinâmico no compile, esconder residual atrás de root não relacionado ou marcar foreign edge conhecido sem metadata |
| W-1388 | CYC1 lifecycle, FFI e concorrência | explicitClose exige owner declarado e close/unlink com a mesma autoridade; lifecycleDrain associa owner a node/resource/registration ou registry fechado; unregister → callback drain → destroy → unpin → reclaim, resource finish antes de census, `service.callCycle` metadata limitado a call-cycle (`metadata`) ou deadline externo (`external`), panic/fault boundary, cross-domain counter/origin, lock para mutation, weak-zero e ABA/reuse são casos independentes | close sem autoridade, drain targeted que rompe outro owner, destroy fora de ordem, callback in-flight tratado como quiescent, deinit como deadline remoto, callCycle arbitrário, weak resurrection ou address reuse antes do último weak |
| W-1389 | CYC1 self-reference e lowering | self-weak só depois de publish em método de owner; constructor witness que expõe `self` em partial init é rejeitado; long chain registra requisito de implementação de lowering iterativo como preocupação inconclusiva, sem claim de compiler/runtime | constructor self escape, resurrection, finalizer effect ordering, declarar lowering pronto ou usar collector para corrigir stack/recursion de drop |
| W-1390 | CYC2 conditional-liveness closure | generation/ID detached, owner-scoped lease com invalidation/close e detached value sem back edge fecham a baseline; weak-key e ephemeron são Rejeitado por enquanto; transparent collector/finalizer são Rejeitado | promover primitive weak-key/ephemeron, collector ou finalizer sem falha das três composições sob critérios bounded |
| W-1391 | CYC2 evidence gap e stop condition | CYC1 permanece QA current; CYC2 registra 3 composições baseline, 4 rejeições, 1 implementation gap e dois estados de reopen; compile/run/provider/stress/OOM/FFI/human/model são implementation gaps | chamar oracle/snapshot de compiler/runtime/provider, ativar Research sem caso bounded ou reabrir por contagem de casos |
| W-1392 | problema-first DYN1 | DYN1 modela hot change no Restaurante: REPL snapshot, typed service/plugin generation, split/local projection, export/import, callback/FFI e fault boundary; DYN0 permanece Componível e o problema inteiro não é trocado por eval; o resultado é host design-oracle | promover feature estrangeira por ergonomia, chamar comportamento host de implementação ou classificar todo dynamic behavior como rejected |
| W-1393 | fatos e eventos DYN1 | recipe, artifact/index/lock, source, interface/WAbi/runtime closure, schema, target, capability/effect, isolation, quotas e receipts são facts estruturados; prepare/validate/preflight/ready/switch/close/drain/publication e stale events derivam o resultado; caller não escolhe status/route/compatible/published/drained/rollback/authority | booleans de outcome, expected echo, route caller-owned, registry/name/PATH lookup ou status copiado do evento |
| W-1394 | switch, drain e recuperação | switch atômico publica nova generation; admission antiga fecha; cancel/drain cobre children, waits, loans, streams, callbacks/resources e ordena unregister → inFlight drain → destroy → unpin → release; process/Wasm/component acrescentam unmap, native exact-WAbi retém mapping; pre-switch failure preserva old, post-switch drain failure é degraded, rollback só provider receipt pré-publicação, crash pré-publicação é fault-boundary ou unknown-effect e crash pós-publicação mantém new committed | rollback depois de publish, completion antiga aceita, cleanup fora de ordem, cancel igual rollback, crash escondido ou drain sem quota |
| W-1395 | identities, schema e targets | `SemanticInterfaceKey`, `ServiceIRKey`, `WAbiKey`, `RuntimeClosureKey`, artifact/recipe/source-map/documentation ficam separados; old/candidate schema e interface receipts derivam exact/compatible, e compatible exige novas SemanticInterfaceKey/ServiceIRKey com compatibility-map digest derivado e decisão receipt; reducers local/split exigem equivalência lógica ampla e permitem trace físico distinto; target A/B tem registry/WAbi/artifact facts distintos | uma hash mista, ABI por nome/arquivo, target implícito, base singular para target-specific ou um handler compartilhado que ecoa state |
| W-1396 | export/import bounded | export deriva o receipt set validado da publicação e inclui source/package/workspace resolution/recipe/artifact/interface/map, provenance, redactions e bound; import executa reopen → parse → check → resolveReceipts e nunca restaura heap/task/loan/capability/ServiceRef/provider handle; stale, missing, duplicate, forged receipt e nested digest são adversariais | snapshot de heap, live handle persistente, redaction parcial, receipt sem digest ou import que restaura runtime closure |
| W-1397 | capability, effects, FFI e segurança | grants são subset attenuation dos declared e vinculam interface/generation/artifact; effects exigem right, declared effect, generation ativa e provider outcome whitelist; process/Wasm/component drenam antes de unload, native exact-WAbi mantém mapping no runtime island, callback tardio é rejeitado e nome/string não concede authority | capability oculta, ambient lookup, in-process-native-as-sandbox, dlclose callback vivo, retry de effect sem receipt ou revocation booleana |
| W-1398 | rotas A/B/C/D e lacuna estreita | A inspector de snapshot e REPL export é composição; B service/plugin generation é composição; C é o schema de tooling/artifact `GenerationReference` read-only de identity/migration, sem authority ou API nominal; D mantém eval/exec, frame/debugger mutation e outros mecanismos rejeitados | criar reflection write, record que carrega heap/task/loan/capability, converter para `ServiceRef`, adicionar source spelling ou rebaixar DYN0 por provider gap |
| W-1399 | evidence e stop condition DYN1/SYN2-DYN2 | cases, métricas derivadas, local/split reducer refs digest-pinned, mutation divergence, refs oficiais, checker e snapshot são oracle-backed current evidence; compiler/runtime/provider/std-provider, isolamento real, stress, OOM/FFI receipts e estudos humano/modelo continuam implementation gaps; promoção de implementação exige fault oracles e receipts independentes sem leak/stale publish/unbounded resource | chamar Tree-sitter/oracle de compiler/runtime, declarar std/provider ready, encerrar por contagem de casos ou omitir queue de documentação |
| W-1400 | problema-first HUM0 | oito slices do Restaurante cobrem diagnostics/`w explain`, ownership/borrow/shared/weak, allocator, execution, tasks/channels, services/generations, package/build/REPL e FFI callback lease; cada slice preserva source refs e problema/outcome comuns | estudar snippet isolado, trocar o problema por feature estrangeira ou inferir ergonomia de preferência |
| W-1401 | stimulus source-derived | cada input extrai uma janela bounded de source UTF-8 real por `sourceRefId`, símbolo único, `beforeLines`/`afterLines`/`maxBytes` e digest derivado; a janela começa e termina em limites de linha, e o adversarial aplica uma mutation find/replace única na mesma janela, mantendo mutation/repair observer-only | input textual inventado, digest manual, find ambíguo, janela divergente, limite mid-codepoint ou mostrar mutation/expected ao participante |
| W-1402 | tasks e contrabalanceamento HUM0 | cada slice fixa exatamente `explain`, `recall`, `repair` e `change`; primary alimenta explain/recall, adversarial alimenta repair/change, e duas ordens counterbalanced e blinding escondem identidade de source/variant | tarefa sem recall, outcome diferente por variante, ordem fixa, role/path/digest/oracle visível |
| W-1403 | fatos ocultos e `w explain` | IDs D0/Place/Loan/OriginSet, GenerationId real, worker/thread/queue, endereço/PID/clock/locale, segredo/payload/ponteiro e implementação host ficam ocultos; facts determinísticos de owner/borrow/drop, effects, allocator, trace/receipts e fact-estimate-measurement-unknown são explainable | expor identidade física, confundir estimate com fact, usar host metadata como resposta ou vazar oracle |
| W-1404 | contrato humano HUM0 | registro futuro exige `participantIdHash` sha256, background não-vazio em C/Rust/Python/W, tempo e queries não negativos, confiança obrigatória 1..5, outcomes exatos semantic/repair/change e `observerReceiptDigest`; PII não entra | participant ID livre/email, background implícito, confidence opcional, outcome manual ou record com PII |
| W-1405 | contrato de modelo HUM0 | registro futuro exige provider/model/version/tokenizer não vazios, params JSON fechado, input/observer digests sha256, tokens input/output/total com soma e os mesmos outcomes verificados | tokenizer/modelo ausente, params aberto, token total inconsistente, digest inventado ou provider sem provenance |
| W-1406 | guards anti-echo e anti-leak | schema fechado rejeita unknown/result-like fields; participant input não aceita expected/status/route/role/path/digest/oracle; checker rejeita stale/missing/duplicate refs, forged outcomes, divergência de problema/outcome e mutation leakage | caller-owned result, status/route/expected echo, metadata escondido no prompt ou source/oracle stale |
| W-1407 | métricas sem resultados fictícios | prontidão deriva somente oito slices, 32 tasks e zero records; métricas futuras usam semântica, reparo, mudança, tempo, queries, confiança e provenance de modelo; score/preference/ergonomic win e LOC não escolhem design | inventar participantes/modelos, calcular score antes de records, usar preferência como evidência ou declarar snapshot implementação |
| W-1408 | stop condition e promoção | primeiro protocolo violation ou oracle disagreement para coleta, mantém slice em Research e exige caso independente; preference só depois das tasks objetivas e não há promoção automática | continuar com digest stale, identity leak, outcome divergente, duplicata, métrica manual ou promover forma por contagem |
| W-1409 | fidelidade mutation ↔ prompt | cada adversarial descreve a falha exercitada pela mutation real: endpoint ownership no channel, ordem state/event no service, autoridade de package, optional registration/guarded unsubscribe no FFI e consuming ownership nos slices de owner/execution; `capacity: 1` → `capacity: 0` não é usado como bug | prompt de generation para mutation sem generation, cap de buffer tratado como falha sem oracle, repair que não restaura o texto mutado ou BellLease apresentado como prova de drain completo |
| W-1410 | fronteira FFI e drain externo | `BellLease` prova registration optional e unsubscribe guardado; callback in-flight drain, destroy, unpin e reclaim permanecem obrigação externa do oracle/`w explain`, com receipt independente | inferir drain completo de um `deinit`, publicar endereço/thread/payload, destruir antes de drain ou aceitar callback após reclaim |
| W-1411 | renderer participant-only | renderer fechado entrega somente `scenario`, `task`, `instruction`, `source` e `blindedLabel`; source é o stimulus derivado, e forbidden fields/words são rejeitados antes da entrega | incluir id/path/digest/mutation/expected/oracle/role/status/route, retornar objeto aberto ou esconder metadata no label |
| W-1412 | root executável uniforme | source executável é módulo normal com `entry` explícito; `.default` é o descriptor sem nome, `--entry` seleciona named e contexto efêmero aceita somente std/imports locais | header `script`, body implícito, execução arbitrária de módulo, solve/update/fetch oculto ou dependency externa em contexto efêmero |
| W-1413 | reexport explícito | `export * from path` e `export { A, B as C } from path`; `export { A }` permanece export coletivo local | `export import`, facade com keyword import ou alias implícito fora de braces |
| W-1414 | input tipado de behavior | behavior declara `input name: Type`; callable `input initialValue: fn(): Value` é slot explícito, sem hidden capture/inference, com type/effect checks normais | identifier solto como input, storage que substitui slot, captura ambiental ou callable sem type |
| W-1415 | roots físicos unificados | somente package/workspace são roots; `resolution` e `deployments` são fields aninhados, owner único é package isolado ou workspace, identities/digests permanecem separados e publication exclui esses fields | `lock`/`deployment` root independente, package member com resolution duplicada, deployment dentro de SemanticInterfaceKey ou resolver que reescreve metadata |
| W-1416 | pipeline e labels observáveis | pipeline de atlas usa duas calls dependentes e labels demonstram `continue` para loop externo, preservando DAG/driver/cleanup vigentes | pipeline com somente return local, continue que reinicia token do label, goto, label em statement arbitrário ou salto não lexical |
| W-1417 | identidade split e transação do root | owner basis exclui `resolution`/`deployments` e deriva `ownerDigest`; resolution e deployments têm digests próprios; `w resolve` altera somente resolution; add/remove/update fazem compare-and-replace com validation, temp sibling, cleanup e reducers POSIX/Windows independentes; `atomicVisible` e `crashDurable` são outcomes separados e durability exige provider receipt | `workspaceDigest` misturado com resolution, sidecar obrigatório, merge automático, last-write-wins, patch in-place, temp path exposto, receipt caller-owned, durability inferida de flag, resolver/fetch oculto ou alegação de compiler/runtime/provider |
| W-1418 | default de protocol sem herança | protocol contém somente requirements; o módulo do protocol publica defaults em `extension Protocol`; conformance registra default ou witness próprio; implementação nominal vence; overlap e ambiguidade exigem member explícito; defaults não têm storage nem ampliam ownership/effects; body fica fora do SemanticInterfaceKey | body inline no protocol, herança de fields/initializer/deinit, `super`, protected, linearização, prioridade por import, extension externa trocando witness ou dispatch runtime oculto |
| W-1419 | três camadas de feature | package feature é graph estático aditivo; availability é proof de target/provider; runtime feature é policy tipada dentro do programa já autorizado | uma única flag que seleciona dependency, prova API e muda comportamento runtime |
| W-1420 | binding typed de availability | direct authenticated availability facts e typed provider binding tornam-se contrato atual de toolchain/provider sem keyword ou runtime API nova; binding é fail-closed e fixa target/domain/generation/providerDigest; capability/effect/ABI/interface checks continuam obrigatórios e não mudam por flag | Boolean, OS version string, deployment field, runtime flag, raw OS/deploy/ambient authority ou binding que altera capability/effect/ABI/interface |
| W-1421 | runtime feature tipada | chave nominal fixa type/values/fallback/context fields/owner/expiry; schema identity é separada da config generation/digest | string key global, valor Any, fallback ausente, context aberto ou config digest tratado como interface |
| W-1422 | rollout e exposure | snapshot imutável, prioridade sem empate, bucket determinístico e exposição explícita posterior à decisão | RNG/process hash, prioridade ambígua, evaluate-and-log oculto ou side effect em avaliação pura |
| W-1423 | stale/missing e composição | config stale/missing usa fallback; availability precede runtime policy; todos os branches seguem no graph/effects/runtime closure | stale fail-open, flag estreitando availability, branch não compilado ou policy carregando código |
| W-1424 | authority amplification rejeitada | flag não habilita dependency/módulo, capability/effect, ABI/interface ou foreign symbol; attempts são diagnostics | remote config como command/eval, source `#if`, macro/annotation ambiental ou provider como root authority |
| W-1425 | composição runtime tipada de AVF0 | runtime feature tipada permanece composição package/runtime atual e não recebe keyword ou nova runtime authority; provider binding fail-closed usa target/domain/generation/providerDigest typed; capability/effect/ABI/interface não podem mudar por flag; raw OS/deployment/boolean/ambient routes continuam rejeitados | promover por precedência externa, chamar oracle host de compiler/provider, introduzir keyword/runtime API, ou permitir runtime feature conceder capability/effect/ABI/interface |
| W-1426 | problema-first SEC0 | segurança inclui invariantes safe, capability/effect/API mediation, input/resource, secrets, audit, supply chain, isolation, deployment, FFI, tenants e patch attestation | reduzir segurança a memory/paralelismo ou importar uma sandbox externa como contrato W |
| W-1427 | safe W e substituição de checks | memory/type/effect/capability/input/resource proofs são irredutíveis; check provado pode ser elidido; check não provado permanece, falha o build ou atravessa `unsafe` explícito completo | unchecked UB por optimização, booleano de segurança, `unsafe` implícito ou proof caller-owned |
| W-1428 | capability e API mediation | API exige capability explícita, effect declarado, provider mediation e attenuation não-ampliadora; ambient lookup, string authority e unknown API falham | lookup ambiental, peer autenticado como root, capability wider ou effect oculto |
| W-1429 | input, resource, secret e supply-chain | traversal/input/resource budgets, secret lease sem serialization/logging, audit trail, source/lock/artifact digests, signer, reproducibility e attestation compõem a admissão | limite ausente, secret em wire/log, provenance inventada, build não reproduzível ou signer implícito |
| W-1430 | perfis físicos e defense-in-depth | trusted CPU, native process, Wasm component, isolate, freestanding e FPGA/ASIC declaram threat model, residual risk, product minimum e deployment controls | perfil único, isolamento universal, deployment que aumenta capacidade ou runtime flag que troca profile |
| W-1431 | substituição de runtime protection | todos os perfis mantêm `memory-safety`, `effect-capability-checks`, `input-bounds` e `supply-chain`; `runtimeEnforcement: present` exige basis `runtime-enforcement`/issuer `runtime-provider`/stage `runtime`; `omitted` exige static proof, hardware enforcement, external mediation ou exceção revisada, com receipt fechado e profile/target/artifactDigest/proofDigest compatíveis; `threat-model-not-applicable` fica em `threatExclusions`, somente para isolation/tenant/side-channel e exige policy-review receipt | `disableChecks`, feature flag, ambient config, “high performance”, exceção para memory/input/supply-chain, target/artifact não vinculados ou omission sem receipt |
| W-1432 | identity física e interface | target físico pode mudar `WAbiKey`, runtime closure e hardening receipt; mudança privada preserva `SemanticInterfaceKey`; mudança pública exige nova key | misturar ABI com semantic key, target triple como authority, mudar contrato sem interface digest ou esconder receipt |
| W-1433 | side-channel e residual risk | timing, cache, scheduler, concurrency e resource use exigem threat model, clock/scheduler/concurrency policy, mitigation e residual risk; não há solução universal | claim universal, residual vazio, clock implícito ou tratar isolation como eliminação de side channel |
| W-1434 | FFI, unsafe, multi-tenant e patch | FFI explicita ABI/provenance/bounds/cleanup/effect/allocator; tenant capability é bound e mediada; patch receipt ordena source→lock→recipe→artifact→signature→attestation→admission com digests SHA-256, signer e rollback policy fechados | UB, raw pointer safe sem boundary, cross-tenant capability, debugger bypass, patch reorder, signer/policy arbitrários ou receipt caller-owned |
| W-1435 | contratos de evidence/admission SEC0 | seis profile schemas, side-channel residual budget, patch attestation e ordered deployment/hardening receipts tornam-se contratos atuais de evidence/admission, não alegação de security conformance; profile selection usa package/target/recipe receipts e nunca runtime feature; accepted routes permanecem safe composition e rejected authority/caller-echo routes permanecem rejeitadas; compiler/runtime/provider/hardware/sandbox/attestation verifier/secret lifecycle/FFI/fault/stress/local-split continuam missing | chamar oracle/snapshot de security conformance ou compiler/runtime/provider/hardware, promover por Cloudflare, runtime feature selecionando profile, caller echo ou omitir receipts e evidence faltantes |
| W-1436 | fechamento BRX0/W-1351 e ponte BRX3 | BRX0 mantém origem única e `W-BORROW-0011`; BRX3 promove `borrows(...)` requirement/interface/function type para relação aberta sem mudar WAbi/runtime; carrier nominal owned permanece alternativa | inventar receiver/body ausente, manter fallback morto por all-inputs, usar caller relation ou chamar parse/oracle/snapshot de compiler/runtime/provider |
| W-1437 | forma estreita GEN2 | `stream <[capture_item, ...]> { ... }` é expressão compiler-owned que retorna `some Stream<Item, Failure>`; capture list explícita com `copy`/`take`/`ref`/`weak` é avaliada na construção; cada emissão exige `yield take value` ou `yield copy value`; `take` move/invalida, `copy` exige `Duplicable` e preserva o original; pull capacity zero, cursor exclusivo, await/try explícitos, terminal bare return, defer cleanup e cancel/drop seguem `Stream`; frame, token, scheduler, push, buffer oculto, yield-from, view/borrow/inout, send/throw/close, retorno de valor, falha sem tipo, reentrada e FFI resume não entram | generator geral, `stream fn`, bare `yield value`, `yield copy` de não-`Duplicable`, frame/resume público, scheduler yield, prefetch ambiental, item borrowed, protocolo bidirecional ou lowering que muda owner graph/HB/result/cancel/cleanup |
| W-1438 | evidência e lacuna GEN2 | GEN2 contém 20 casos, dois reducers independentes e snapshot determinístico; cinco ganhos ergonômicos e duas perdas de cerimônia de capture fecham a decisão de design, com `yield take`/`yield copy` (copy exige `Duplicable`), mas compile/run/runtime/provider, stress, estudos humano/modelo e debug/ABI/reflection continuam faltando | chamar machine/parser/snapshot de compiler/runtime/provider, escolher por LOC, declarar implementação pronta ou promover metadata/frame por conveniência |
| W-1439 | migração de keywords GEN2 | `stream` e `yield` são keywords literais/reservadas; bindings antigos migram para `source`/`cursor`, `Stream` nominal permanece, e o negativo `lock state { state }` conserva o CST anterior; `W-STREAM-0001` torna a quebra pré-1.0 explícita | identifier wildcard contextual, alterar silenciosamente CST/diagnóstico não relacionado, `stream fn`, keyword apenas em highlights ou compatibilidade implícita |
| W-1440 | capture explícita GEN2 | `stream <[capture_item, ...]> { ... }` exige lista explícita (inclusive `stream <[]>`); `copy`/`take`/`ref`/`weak` são avaliados da esquerda para a direita na construção, antes do retorno de `Stream`; `take` indisponibiliza o binding do parent, `inout` não é mode e `next` não decide capture | capture ambiental implícita, mover `source` somente no primeiro `next`, lista tratada como valor runtime, `inout`/borrow escapante ou `ref`/`weak` sem prova de estabilidade/liveness |
| W-1441 | provider de codec/profile WLO | produção exige codec e provider de profile WLO para receipts de compile, run, interop, limits, OOM, fuzz, target e package; WLO1 prova somente o design-oracle host de `wlo.string.v1`, não implementação | tratar os bytes WLO como W ABI; alegar provider/runtime/compile por digest do oracle; reutilizar parser/formatter como prova de codec de produção |
| W-1442 | durable recovery provider gap | PRC0 fecha somente o contrato SR0 de journal, runtime closure, recovery e delivery; produção ainda exige provider durable de storage/alarms, receipts independentes, compiler/runtime/run e fault recovery | **implementation-evidence-gap**; missing compiler, runtime, provider, w-compile, w-run, human-study e model-study |
| W-1443 | Quantity compiler/std JSON+wWire provider gap | PRC0 fecha source contract de Quantity/SI/IEC, point/delta e tokens fixos; produção ainda exige compiler typechecking/lowering, std.json/wWire codecs, byte-exact adapters e provider receipts | **implementation-evidence-gap**; missing compiler, runtime, provider, w-compile, w-run, human-study e model-study |
| W-1444 | module-run CLI/resolver/runtime gap | PRC0 fecha route de module entry, package roots, resolution e cleanup no RU0 oracle; produção ainda exige CLI, compiler, resolver, runtime, target/run receipts e provider | **implementation-evidence-gap**; missing compiler, runtime, provider, w-compile, w-run, human-study e model-study |
| W-1445 | PYN3 kernel/presentation/export provider gap | PRC0 fecha preview bounded, typed presentation e compiler-summary fallback; produção ainda exige kernel process, ZeroMQ/session, frontend/export providers, compiler/runtime e sanitizer/fault receipts | **implementation-evidence-gap**; missing compiler, runtime, provider, w-compile, w-run, human-study e model-study |
| W-1446 | PYN4 DLPack/Python/device bridge gap | PRC0 fecha DLPack versioned release/deleter, lease e view-escape rejection; produção ainda exige Python capsule/GIL bridge, device/queue providers, compiler/runtime e interop receipts | **implementation-evidence-gap**; missing compiler, runtime, provider, w-compile, w-run, human-study e model-study |
| W-1447 | lazy compiler/runtime/synchronization provider gap | PRC0 fecha winner/waiter publication, happens-before e known reentry rejection; produção ainda exige lazy lowering, scheduler/runtime synchronization, parking provider e stress/fault receipts | **implementation-evidence-gap**; missing compiler, runtime, provider, w-compile, w-run, human-study e model-study |
| W-1448 | IPC1 A/B implementation evidence gap | ASIC0 torna A immutable mapped snapshot e B bounded mapped byte channel/log contratos atuais de adapter/provider condicionados a receipts, mas produção ainda exige Windows two-process, provider receipts, crash/recovery, durability, w-compile, w-run, FFI e stress evidence; C universal permanece rejeitado e não há nova syntax/API W | **implementation-evidence-gap**; missing Windows two-process, provider receipts, crash-recovery, durability, w-compile, w-run, FFI e stress |
| W-1449 | AVF0 typed binding implementation evidence gap | ASIC0 fecha direct availability facts, typed provider binding e runtime composition como design/oracle contract; produção ainda exige compiler/diagnostics, provider publication, local-split, fault e stress evidence sem alterar capability/effect/ABI/interface por flag | **implementation-evidence-gap**; missing compiler, diagnostics, provider publication, local-split e fault/stress |
| W-1450 | SEC0 evidence/admission implementation evidence gap | ASIC0 fecha profile, side-channel, patch e ordered deployment/hardening receipts como evidence/admission contracts, não security conformance; produção ainda exige compiler/runtime/provider, hardware/sandbox, attestation verifier, secret lifecycle, FFI, fault, stress e local-split evidence | **implementation-evidence-gap**; missing compiler, runtime, provider, hardware, sandbox, attestation verifier, secret lifecycle, FFI, fault/stress e local-split |
| W-1451 | manifest de projeto unificado | `build.w` direto e data-only por diretório contém um ou dois records top-level em qualquer ordem, pelo menos um e no máximo um `package` e um `workspace`, sem wrapper físico `build.w {}`; package-only selecionado em contexto standalone possui `resolution`/`deployments`; package-only membro de workspace omite esses fields e o workspace declarado é o owner; workspace-only e package+workspace atribuem esses fields ao workspace e o package os omite; `workspace.members` aponta para diretórios cujo `build.w` contém `package`; schemas `w.package/1` e `w.workspace/1` permanecem; package inline, nested workspace member, glob, scan ambiental, source executável, arquivo vazio/duplicado e owners duplicados são rejeitados; `package.w`/`workspace.w` são removidos sem shim pré-1.0 | oracle-backed-current; PFU0-W-1451-current fornece o controle host-oracle e o candidate de manifest é aceito |
| W-1452 | streaming de saída de service | APIs de service retornam explicitamente `some Stream<Item, Failure>`; a chamada via `ServiceRef` sempre acrescenta `ServiceFailure` na fase de abertura/admission; o erro da função chamadora deve ser `ServiceFailure` ou ter exatamente uma conversão total de `ServiceFailure`; separadamente, o `Failure` terminal permanece no stream e deve admitir `ServiceFailure`; `Channel` exige capacity, endpoints, ownership, backpressure e `close` explícitos; mailbox e `Stream` mantêm lifecycle distinto; `stream fn` é rejeitada por capturas, lifecycle e erro ambíguos; client-stream, bidi, channel implícito, capacity implícita, `ServiceRef` sem await e closed-turn change ficam fora | oracle-backed-current; PFU0-W-1452-current fecha o retorno explícito e rejeita o candidate `stream fn` |
| W-1453 | lifecycle de property | `get`/`set`/`modify` permanecem vigentes em property stored, computed e behavior; `init` bypassa accessors; assignment simples usa `set`/replacement e nunca `modify`; mutation compound usa `modify` uma vez, sem get-copy-set; `return inout` é pre-borrow e `defer` retoma pós-borrow; old value e backing storage drop ocorrem uma vez pelas regras explícitas; cleanup customizado fica no backing type sem `deinit` oculto; notificação externa é método/service/channel nomeado; acesso à mesma property em accessor faz dispatch normal e sobreposição no borrow exclusivo falha; `willSet`/`didSet` e observers implícitos são rejeitados | oracle-backed-current; PFU0-W-1453-current fecha o lifecycle e rejeita o candidate observer |
| W-1454 | arquitetura de capability comum | capability nominal não forjável, sem initializer público; provider/profile/digest são explícitos; owner ou lease liga root+generation; cada operação declara effect/error/ownership/bounds/complexity/cancellation; values portáveis são separados de handles locais; child task recebe borrow/move explícito; service/process faz host rebind; test provider é explícito; não há lookup ambiental | oracle-backed-current; AEG0-W-1454-current fecha a fronteira comum e rejeita authority ambiental, handle wire e child implícito |
| W-1455 | tempo civil explícito | `std.time` operacional não muda. `UtcTimestamp` é `WireValue` portable; `Instant` e `Deadline` continuam locais. Civil date, local datetime, timezone e calendar são values explícitos. Conversão exige provider/database profile com version/digest e zone/calendar/profile explícitos. DST gap/fold rejeita por default ou exige policy. Locale/calendar/timezone não são ambientais. Wall clock não dirige deadline. Leap-second/smear policy fica no profile sem conversão automática e não há conversão implícita | oracle-backed-current; AEG0-W-1455-current fecha package/profile civil separado e rejeita wall clock global, deadline civil e timezone implícito |
| W-1456 | perfis de random | package/profile geral separa secure provider-backed de deterministic explicit-seed. Secure não aceita seed, fallback ou downgrade e exige bytes bounded, integer uniforme checked e erro tipado. Deterministic é replayable e não satisfaz secure. Draw order é owner-local, sem inheritance entre task/service/process. Context HTTP projeta o mesmo contrato. Somente seed/profile determinístico pode entrar em test receipt; secure seed/draw/bytes não entram em receipt/log/diagnostic. Handles não são WireValue | oracle-backed-current; AEG0-W-1456-current fecha secure/deterministic e rejeita seeded secure, fallback e inheritance implícita |
| W-1457 | codecs e compression explícitos | packages específicos declaram ByteSource/Sink, profile+digest, streaming e quotas separadas para encoded, logical, allocation, depth e ratio. Offset/progress errors são tipados. Cancellation não desfaz bytes committed. Dictionary/state tem owner explícito. Codec/schema e compression transform têm identity/limits separados | oracle-backed-current; AEG0-W-1457-current fecha requisitos operacionais e rejeita `Codec<T>` universal, reflection, inference ambiental e quota colapsada |
| W-1458 | crypto e secrets scoped | app crypto passa por package/provider capability ligada pelo deployment. Algorithm/profile são typed e pinned, sem string/fallback/downgrade. Secret/key handle é opaque, nonextractable por default, purpose/audience/generation scoped e move-only. Lifecycle tem dois caminhos: acquire→active→revoking→revoked→released para revoke/rotation, ou acquire→active→expired→released para expiry. Revoke fecha nova admission e drena operações admitidas. Host controla rotation/expiry/zeroization. Secret não entra em wire/storage/log/diagnostic/receipt | oracle-backed-current; AEG0-W-1458-current fecha lifecycle e rejeita plaintext/env lookup, secret wire e downgrade |
| W-1459 | baseline portátil de `std.simd` | `Simd<Element, lanes: usize>` e `SimdMask<_ lanes: usize>`, lanes `1...64`, label required somente em Simd e optional em mask com aplicação `SimdMask<16>`, Element escalar fechado com `Bool` em mask, sequence target-independent, layout opaco, scalar fallback obrigatório, mask `splat(Bool) -> SimdMask<N>`/`fromArray([Bool; N]) -> SimdMask<N>`/`toArray() -> [Bool; N]` sem allocation, `all`/`any`/`none` retornam `Bool` e `countTrue` retorna `UInt`, load borrow source e store destination `inout` com partial tail total e preflight, arithmetic lane-wise condicionado ao scalar Element, floats sem bitwise/shift/overflow APIs, integer overflow mask por lane, masks com bitwise operators, reductions nomeadas em ordem/policy (`reduceAdd`, `wrappingReduceAdd`, `saturatingReduceAdd`, `reduceMultiply`, `wrappingReduceMultiply`, `saturatingReduceMultiply`, `reduceBitAnd`, `reduceBitOr`, `reduceBitXor`) e float `ReductionMode` obrigatório (`strict` left fold, `reproducible` árvore balanceada v1, `fast` sem igualdade de bits cross-backend; omission/positional/wrong-label/wrong-arity de mode: usa W-LABEL-0005 e repetição usa W-LABEL-0006), static swizzle com count-first em `1...64`, duplicata e primeiro OOB em source order, e `w explain performance` com lowering facts | oracle-backed-current; SIMD1-W-1459-current fecha o contrato host-only e rejeita width/layout nativo, Bool lane, dynamic shuffle, alignment flag, write antes de bounds failure, short-circuit e performance universal |
| W-1460 | fingerprint semântico pós-validação de generic D1 | evidence interna versionada `w-seed-generic-fingerprint-1`: preimage canônico independente de spans/indices/source spelling, validação/preflight antes da avaliação, `VERIFIED` + `AVAILABLE` somente após todos os predicates true, `VERIFIED` fora do subconjunto + `UNSUPPORTED`, demais estados + `NOT_AVAILABLE`/bytes zero; witness `restaurant` com standard duplicado, cancelled, vazio, salto e duplicata; C e Bun reconstrutores independentes | oracle-backed-current; `GPF0-W-1460-current` usa fragments reais de Last Light, seed C e oráculo Bun independente; usar spans/indices/source spelling, chamar digest de `TypeId`/cache key/identidade, emitir antes de `VERIFIED` ou confiar somente no C; fingerprint-1 sozinho ainda não contém o preimage completo de declaration/substitution/witness de W-1467 e não é a identidade semântica; target, profile, edition, toolchain, compiler, bundle e ABI pertencem à recipe física; digests diferentes implicam preimages diferentes, mas digest igual isolado sem preimage não prova igualdade nem identidade collision-safe; a proveniência source-backed de W-1460 e seu gate permanecem preservados |
| W-1461 | evidência D2 String source-backed em generic predicates | D2 source-backed bounded de `String` em predicates genéricos: literal simples até 4.096 bytes, `==`/`!=`, preflight canônico, `VERIFIED`/`REJECTED`/`UNSUPPORTED`/`INVALID` e fingerprint Bun independente | oracle-backed-current; `GPF0-W-1461-current` liga diretamente os markers reais de `generics.w`, `isFinalCallLabel`, positivos duplicados, rejeitados, empty, over-limit, corrupção e digests Bun ao gate independente `tooling/check-seed-generic-validation.mjs`; o caso não afirma String completa, compiler, runtime ou self-host |
| W-1462 | expressão const tipada escalar em generic value | D3 source-backed bounded de expressão parentetizada com literais, grouping, unary e binary operators escalares, resultado `Bool` ou integer explícito, função ConstIR sintética com origem explícita, receipts `CONST_ARGUMENT`/`PREDICATE` ordenados e fingerprint normalizado | oracle-backed-current; `GPF0-W-1462-current` liga os markers reais de `generics.w`, prova immediate `42`, computed `(6 * 7)`, duplicate, rejected `(6 * 6)`, quota cumulativa, overflow, unsupported call e corrupção com seed C e reconstrução Bun independente; identifiers/named const, graph dependencies/cycles, imported heads/predicates, String computed result, identity final, compiler/runtime e self-host permanecem limites |
| W-1463 | module named const no generic value | D4 source-backed bounded de `const name: Type = expression` local, relation explícita, forward reference, lowering ConstIR sintético com dependency `CALL`, preflight causal de graph/cycles, receipts e fingerprint normalizado igual ao immediate/D3 | oracle-backed-current; `GPF0-W-1463-current` liga markers reais de `generics.w`, prova named/duplicate `42`, forward chain, rejected, cycles self/2/3 com paths fechados, ciclo inalcançável, type mismatch, unresolved, unsupported, corruption, zero capacity, quota, `dependencyLimit` (257 declarations, `UNSUPPORTED` + failure `dependency-limit`) e `arithmeticOverflow` (`W-CONST-0006`, receipt `CONST_ARGUMENT` sem predicate) com seed C e oráculo Bun independente; dependency fora do subset mantém failure `function`; imports, associated const, initializer inference, cache compartilhável/cross-argument/session, identity final, compiler/runtime e self-host permanecem limites |
| W-1464 | memoização local determinística de DAG de module const | D5 source-backed bounded para module const local `Bool`/integer já lowerable por D4: tabela fixa por invocation, chave por declaration identity, estados `ACTIVE`/`READY`, counters append-only em evaluation result/receipts, hits que omitem body work e preservam o step do `CALL`, reset e quota observáveis, sem alterar preflight causal ou fingerprint | oracle-backed-current; `GPF0-W-1464-current` liga os markers reais de `generics.w`, prova diamond 4 misses/1 hit/7 steps, reconstrução Bun independente de source order, repeated invocation, D3/D4 linear com zero hits, quota 7/6, arithmetic failure não cacheada, cycles/zero capacity/dependency-limit/corruption com counters zero e receipt causal de ciclo somente quando há capacidade e fingerprint idêntico ao immediate/D3/D4; cache compartilhável de §3.6.5, cross-argument/session, imports, associated const, inference, identity final, compiler/runtime e self-host permanecem limites |
| W-1465 | sessão privada de avaliação por aplicação | D6 source-backed bounded para duas arguments `TYPED_PENDING_CONST` da mesma aplicação: sessão vazia por run, tabela fixa de 256 compartilhada somente durante o loop de argumentos, READY reutilizável entre irmãos, predicates com evaluation nova, counters/quotas/receipts/fingerprint preservados e sem API pública | oracle-backed-current; `GPF0-W-1465-current` liga `AnswerPair.agrees`, as aliases equivalentes e o teste `restaurantGenericContractHolds` do Restaurante, prova primeiro argument 7 steps/4 misses/1 hit, segundo irmão 1 step/0 misses/1 hit, quota total 8, quota 7 com falha antes do lookup no segundo, novo run 7/1, failure-first, cycle/corruption/dependency-limit preflight zero e preimage Bun independente de dois i64; cache compartilhável, outro run/application, imports, associated const, inference, identity final, compiler/runtime e self-host permanecem limites |
| W-1466 | inferência scalar append-only de module const | D7 source-backed bounded para `const name = initializer` e `export const` local: solver de grafo acíclico independente de source order, forward references, `declared_type` source-only, `effective_type` append-only, default `Int`/`i64`, Bool, suffix e propagation por identifier; cycles causais e barreiras D4 preservados; ConstIR-6/fingerprint-1 estáveis | oracle-backed-current; `GPF0-W-1466-current` remove somente as quatro annotations do diamond Last Light, preserva `ultimateAnswer: i64`, prova quatro records explicit=false/declared=NONE/effective=i64, symbol exportado i64, integer default/Bool/suffix/propagation, graph forward/reordered, equivalência explicit/inferred, ciclos anchor/unanchored com zero evidence e negativos D4 completos via C e oracle Bun independente; imports, associated const, identity final, cache compartilhável, compiler/runtime e self-host permanecem limites |
| W-1467 | identidade semântica collision-safe de specialization | D8 separa a identidade semântica da specialization, a recipe física de materialização/cache e `reflect.TypeId`; preimage completo com declaração nominal local struct, substitution normalizado, refinements e witness vector count zero; digest somente accelerator com full-byte compare; schema `w-seed-generic-specialization-1`; API caller-owned com measure/write, lifecycle explícito `NOT_AVAILABLE`/`AVAILABLE`/`UNSUPPORTED`/`CAPACITY`, bytes required/written e digest | oracle-backed-current; `GPF0-W-1467-current` liga fragments reais do Restaurante para immediate/computed/named/diamond/AnswerPair e `StaticValue<Bool,true>`/`StaticValue<String,"The final seating">`; head/module/refinement adversaries continuam fixtures C sintéticos; o gate Bun compara bytes C, length e SHA e cobre rejected/quota/overflow/cycle/invalid/corrupt/unsupported, capacity exact/zero/short-by-one, sentinels, NULL input e digest-forced collision; recipe física, receipts autoritativos package/interface, witness selection geral, TypeId runtime e compiler completo permanecem gaps |
| W-1468 | origem nominal collision-safe e specialization-2 | D9 separa `NominalDeclarationOrigin`, `SemanticTypeConstructor`, contrato/interface agregados, recipe física e `TypeId`; receipt completo de authority/package/module path/kind/owner/name; builder caller-owned measure/write com schema `w-seed-nominal-origin-1`, SHA accelerator, full-byte equality e validação de digest/framing/relação; specialization sobe para `w-seed-generic-specialization-2` e validation para `w-seed-generic-validation-8`, preservando fingerprint-1 | oracle-backed-current; `GPF0-W-1468-current` liga markers reais de `build.w`, `domain.w` e `generics.w` ao package `last-light/restaurant`, consome o AuthorityOrigin completo de `AUL0-W-1469-current`, separa módulos `domain`/`generics`, compara bytes completos C e Bun independentes e cobre authorities/packages/modules/kinds/owners, aliases/version/revision/path/target/profile ausentes, missing origin, corrupção/truncated/trailing/digest/relation, capacidade e digest collision; o resolver completo de registry e a Git repository authority permanecem gaps, assim como NFC completo, `.local` build-local nonportable e nunca publicável, witness selection geral, recipe física, TypeId runtime e compiler completo |
| W-1469 | authority origin e continuidade registry bounded | `AuthorityOrigin` usa bytes públicos completos da gênese sem assinaturas; `trustedGenesis` é o payload público completo fornecido out-of-band e define o origin; `trustedCheckpoint` é resolver-owned, persistido entre chamadas e não é um novo trust input out-of-band; ele ancora a root corrente; cada update N+1 satisfaz separadamente o threshold da root anterior e o threshold da root nova; lock separa origin, evidence e record; AUL0-W-1469-current liga o Last Light Restaurante | oracle-backed-current; AUL0-W-1469-current prova trusted anchor, rotação, checkpoint, alias, mirror, corrupção, rollback, gap, thresholds, key IDs, package identity e full-byte equality; persistence/CAS real, expiry/freshness/timestamp, targets/snapshot, Git authority, `.local` origin, NFC e compiler completo permanecem gaps |

| W-1470 | posição do launcher de child no initializer | Forma vigente: `let x = async ...`, `let x = spawn<.compute> ...` e `let x = spawn<domain: .compute> ...`; `async` usa o domain atual e `spawn<domain>` usa o domain explícito; initializer produz `Task<T, E>`; staging de callee/args/captures ocorre uma vez no parent; somente raiz callable única de initializer de binding `let` é aceita; `var`, return, escape, launcher nested, expression statement e expressão composta são rejeitados; await direto permanece na task atual e await de task faz join | current; grammar, parser seed, formatter, semantic/tooling machine, corpus, atlas e Last Light migram para a posição do initializer sem shim. A posição anterior vinha do modelo declaration-like de Swift, mas W preserva o mesmo Task lexical. Sources: [Swift SE-0317](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0317-async-let.md), acessado em 2026-08-25 |
| W-1471 | bridge blocking call-site `sync` | Forma vigente de design: bare `maySuspend` continua error. `sync f()` só é válido quando o callee escreve `async fn` (`sourceSpelling: explicit`), preserva scope estruturado, cancellation, deadline, cleanup, join e drain, não cria detached task, bloqueia a thread e acrescenta `blocksThread`; exige blocking authority, quota bounded, provider bridge e checks de target/deadlock/fairness/cancellation; rejeita cooperative/serial/signal/freestanding/nonblocking, progresso dependente do mesmo permit, callable inferida, callable `fn` sem async explícito e `neverSuspend` | oracle-backed-current por `DRC0-W-1471-current`; frontend grammar, lowering, runtime bridge, provider receipt e liveness/deadlock validation continuam missing. Kotlin [`runBlocking`](https://kotlinlang.org/api/kotlinx.coroutines/kotlinx-coroutines-core/kotlinx.coroutines/run-blocking.html), acessado em 2026-08-25, é precedente de bridge em main/tests/callbacks, não prova universal |
| W-1472 | view, ref e interface projection | Forma vigente: `ref Array<T>` observa owner e metadata completos; `view Array<T>` observa janela lógica sem capacity; `view String` exige substring UTF-8 válida; `view Tensor` pode ser strided; tipos nominais podem expor views de famílias core por método e aggregates owned podem guardar `ref`/`view` com origins; properties suprimidas formam interface projection, não storage view; não há `Viewable`, protocol universal ou `view Object` automático | current; clarificação normativa em DESIGN §16.2 e examples no cheatsheet preservam a semântica existente de `ref`, `view` e `inout view` |
| W-1473 | virtual memory e data movement performance | Direção vigente: MEM0 separa file-backed immutable mapping, anonymous reserve/commit/decommit, private COW, shared/MMIO boundary, protection, advice/prefault/discard, huge pages, NUMA, pinned host, device/unified transfer, vectored I/O, sendfile-style zero-copy, alignment/cache/prefetch/non-temporal operations e composição allocator/Arena/fixed/IPC1. Cada item exige owner move-only, extent bounded, permissions, address-space/provenance, drop determinístico, live-view exclusion, external interference e target evidence; `Mapped<T>` universal é rejeitado | oracle-backed-current por `DRC0-W-1473-current`; [`MEM0`](tooling/studies/mem0-virtual-memory-data-movement/) fecha a classificação, não uma API/provider/compiler/benchmark. Esses itens continuam implementation gaps. Fontes primárias registradas no estudo foram verificadas em 2026-08-25 |
| W-1474 | efeitos simulados, aprovação posterior e test infrastructure | Direção vigente: state machine bounded `proposed -> simulated -> awaitingApproval -> revalidating -> committing -> committed|rejected|conflict|unknown`, proposals com effect/input/authority/provider+generation/result/dependencies/approval/limits/expiry, DAG causal, bulk approval topológica, revalidation externa e `unknownOutcome(effectId)` após dispatch; simulated e committed values ficam separados; não há rollback, compensation ou exactly-once | oracle-backed-current por `DRC0-W-1474-current`; [`SEA0`](tooling/studies/sea0-simulated-effects-approval/) fecha a máquina compartilhada e as quatro lanes de teste. Carrier, provider real, fault multi-process/hardware e performance continuam gaps. Fontes primárias registradas no estudo foram verificadas em 2026-08-25 |
| W-1475 | readiness de training e inference | Direção vigente: LLM0 inventaria a cobertura existente de Tensor/shape/value parameters, broadcast/reduction/numeric mode, f16/bf16/quant direction, views/strides, Device/Queue/Launch, DLPack, ownership, streams/backpressure/services, deterministic RNG/profile e packages/receipts; gaps de training e inference são classificados como core, std/API, typed IR/compiler, runtime/provider, tooling/evidence ou application framework, com default de não inflar o core | oracle-backed-current por `DRC0-W-1475-current`; [`LLM0`](tooling/studies/llm0-training-inference/) fecha o ownership map e os dois workloads, não framework, kernel, provider ou performance. Fontes primárias registradas no estudo foram verificadas em 2026-08-25 |
| W-1476 | teste de tipo e recuperação borrowed | `is` testa somente enum tag ou tipo nominal exato de existential com `reflect.Reflectable`, retorna Bool e não faz narrowing; `reflect.downcast<T>(ref existential)` retorna `ref T?`, exige `T` compatível com toda a composição, herda origin/lifetime e não copia, move, retém ou aloca; downcast owned, `as`/`as?`/`as!`, cast por string, type pattern e smart cast ficam fora | current design contract em DESIGN §8.8.1; compiler typing, existential runtime identity, borrow lowering, diagnostics e execução continuam implementation-evidence gaps |
| W-1477 | scatter read e transferência posicional | `io.ReadBatch` é owner move-only de segments com capacity fixa e initialized counts privados; `io.readMany` preenche a concatenação em ordem e retorna `data`, `end` ou `full`, com fallback de uma leitura. `io.TransferPlan` possui intervalo, progresso e scratch reservado; `io.transfer` liga `SnapshotByteSource` a `ByteSink`, diferencia source end de limit, preserva sufixo não committed e pode selecionar operação nativa somente por capability interna. `IoSliceMut`, `inout view Bytes...`, probe `isVectored`, syscall pública e promessa universal de zero-copy ficam fora | current design contract em DESIGN §14.2.11, std.io draft e Última Luz; compiler/runtime, provider SPI, cross-target fault tests, receipts de estratégia e benchmarks continuam implementation-evidence gaps. Fontes primárias de `readv`, `WSARecv`, `sendfile`, `TransmitFile` e Rust vectored I/O foram verificadas em 2026-08-25 |
| W-1478 | aplicação fechada e hook pós-borrow de behavior | behavior aceita zero ou um input, exatamente `initialValue: fn(): Value`; cada parâmetro generic deve ser inferido unicamente pelo tipo lógico depois de `for`; a aplicação usa somente o nome nominal, sem argumentos, generic arguments, composição ou backing access. `modify` pode usar `defer` uma vez após o borrow e observa mutation admission, inclusive término com error, sem copiar `oldValue`. Policy estática pertence ao tipo; dependência runtime usa owner, método, service ou channel nomeado | current design contract em DESIGN §10 e witness `Versioned` no Última Luz; parser aceita a declaration existente, mas checker/HIR, diagnostics e execução do lifecycle continuam implementation-evidence gaps. Swift SE-0258 foi verificado como alternativa de argumentos, backing e projection em 2026-08-25 |
| W-1479 | projeção nominal borrowed de aggregate | Uma API que suprime properties declara um aggregate nominal lifetime-dependent: `ref Field` para place completo, `view Carrier` para extent verificável e valor owned para snapshot/cópia; constructor ou método nomeado escolhe fields e o HIR preserva origins por field. Protocol menor limita methods, aggregate borrowed expõe dados sem copiar e DTO owned permite escape. `view Object`, `view Nominal`, field mask, derivação estrutural, `Viewable` universal e recuperação de authority omitida ficam fora | current design contract em DESIGN §16.2 e witness `PublicCourse` em `views.w`; parser de aggregates e borrows existe como seed, mas checker de origins por field, diagnostics e execução do borrow continuam implementation-evidence gaps |
| W-1480 | direções de service stream sem Channel implícito | Service operation usa o mesmo carrier em posições diretas: `take some Stream<Item, Failure>` no parâmetro forma client-streaming, `some Stream<Item, Failure>` no resultado forma server-streaming, ambos formam bidirectional e nenhum forma unary; input transfere o readable owner, output permanece opaque, items são owned/transferable/WireValue, Failure admite ServiceFailure e cada edge preserva créditos, ordem, terminal, cancellation e drain. Channel.receive pode fornecer input após Channel.open com capacity explícita. Stream nested, input sem take, item borrowed/non-wire, any Stream publicado, stream fn, Channel/capacity implícito e open sem await ficam fora; W-1480 substitui somente a rejeição client/bidi de W-1452 | current design contract em DESIGN §23.1.5, estudo host SVC0 e witness `service_streaming.w`; semantic checker, ServiceIR lowering, runtime pumps, providers, cross-route faults, performance e estudos humano/modelo continuam implementation-evidence gaps. Fontes primárias de gRPC, WebAssembly Component Model e Cloudflare RPC foram verificadas em 2026-08-25 |
W-1412–W-1416 substituem W-1046–W-1049, W-1051–W-1053, W-1057–W-1058,
W-1060, W-1062–W-1070, W-1157 e W-1245. W-973, W-1050, W-1054–W-1056,
W-1059, W-1061, W-1071–W-1075 e W-1158 continuam válidos e recebem a nova
classificação de módulo e root. Os IDs permanecem no ledger como proveniência.
A regra corrente não preserva header, body implícito, lock/deployment root ou
`export import` por compatibilidade pré-1.0.

#### 1.3.21.7 Identidade semântica collision-safe de specialization (W-1467)

**Motivação:** W-1460–W-1466 provaram um fingerprint pós-validação. Eles não
fecharam a igualdade collision-safe de uma specialization. A decisão D8 separa
essa igualdade semântica da recipe física de materialização/cache e de
`reflect.TypeId`.

A identidade semântica é o preimage canônico completo da declaração nominal,
do substitution environment normalizado e das conformance/witness semantic
identities em requirement order. O digest SHA-256 é apenas um accelerator.
Igualdade exige length, digest e full-byte compare. A camada seed não possui
receipts autoritativos de package/interface, target/profile/toolchain/ABI,
materialization ou witness selection geral. O witness vector D8 tem count zero
e qualquer constraint fora do subset é `UNSUPPORTED`.

O schema append-only `w-seed-generic-specialization-1` usa prefixo domain-
separated, root tag distinto, declaration schema explícito, substitution vector
ordinal e witness count. O declaration schema codifica local struct, module id,
head name, parameter kind, domain concrete/dependent, refinement kind e
predicate body digest. Type/value canonical encoding compartilha a projeção
`fingerprint-1`. Labels, spans, source indices, annotation presence, counters,
quota e session ficam fora. Target, profile, compiler, lowering plan e runtime
facts pertencem à recipe física futura, não ao encoder semântico seed. `WAbiKey`,
`RepresentationMap` e a futura `SemanticInterfaceKey`/module-package
declaration receipt são nomes da camada física/interface, não salts da
igualdade semântica. Edition não é salt arbitrário: seus efeitos semânticos
entram na declaração/interface e normalização; a recipe registra a edition.

A API C é append-only. O caller fornece buffer e capacidade. O resultado separa
`NOT_AVAILABLE`, `AVAILABLE`, `UNSUPPORTED` e `CAPACITY`, além de bytes written,
bytes required e digest. Não-`VERIFIED` publica `NOT_AVAILABLE` e zeros;
`VERIFIED` não encodable publica `UNSUPPORTED` e zeros sem alterar o estado
principal; buffer curto, inclusive zero, publica `CAPACITY`, required exato,
written zero, digest zero e não toca o buffer; somente capacidade suficiente
publica `AVAILABLE` com bytes e digest. `NULL` com capacidade não-zero é
`INVALID` antes de evaluation; `{nonnull,0}` é o caso `CAPACITY`. O caller
mantém buffer e inputs disjuntos e imutáveis entre measure/write. A função de
comparação testa views vazios/NULL, corrupção e um adversário que força digest
igual com preimages diferentes.

As fontes primárias usadas como evidência comparativa, sem copiar a semântica
de W, são [rustc `Instance`](https://doc.rust-lang.org/nightly/nightly-rustc/rustc_middle/ty/struct.Instance.html),
que expõe `def` e `args` e descreve instanciação sob demanda em codegen/const
eval; [Swift ABI Mangling](https://github.com/swiftlang/swift/blob/main/docs/ABI/Mangling.rst)
e [Swift ABI TypeMetadata](https://github.com/swiftlang/swift/blob/main/docs/ABI/TypeMetadata.rst),
que distinguem nomes/metadata e vetores de argumentos/witnesses dos caminhos
físicos de instanciação; e [Rust `TypeId`](https://doc.rust-lang.org/std/any/struct.TypeId.html),
que é um handle opaco cujo hash/ordenação não é estável entre releases. A
inferência limitada é apenas que uma identidade semântica deve ser separável
da materialização/cache e de um handle local; estas fontes não são autoridade
para a codificação ou igualdade de W.

O gate usa fragments reais de `domain.w` e `generics.w` para os witnesses
source-backed. Immediate `42`,
computed `6 * 7`, named const, diamond e aliases equivalentes publicam a mesma
identity quando declaration, module e refinement são iguais. Head, module e
predicate body diferentes publicam identities diferentes por fixtures C
sintéticos; eles não são alegados como fragments compiláveis do Restaurante.
`StaticValue<Bool,true>` e `StaticValue<String,"The final seating">` exercitam
TYPE/dependent domain e permanecem diferentes. Rejected `41`, quota,
overflow, cycle, invalid, corruption e unsupported não publicam identity.
Capacity exact, zero e short-by-one preservam sentinels. Bun reconstrói o
preimage independentemente, compara os bytes escritos pelo C e repete o gate
duas vezes. O caso permanece
oracle-backed-current. Recipe física, TypeId runtime, compiler completo e
cache persistente continuam implementation-evidence gaps.

Alternativas rejeitadas:

- chamar `fingerprint-1` de identidade, pois ele não contém declaration schema
  completo nem witness vector e o contrato antigo deve permanecer estável;
- usar somente digest ou TypeId truncado, pois colisões e corrupção poderiam
  produzir falso positivo;
- incluir target, profile, compiler, lowering ou counters no preimage, pois
  esses fatos pertencem à recipe física ou à evidence de execução;
- inventar witness IDs sem receipt autoritativo, pois o subset D8 não possui
  seleção geral de witnesses;
- implementar cache físico ou TypeId runtime no seed, pois isso ampliaria a
  superfície além da prova semântica caller-owned.

Uma revisão pode responder por ID. Uma mudança deve atualizar o exemplo, a
grammar, o formatter, o corpus e a seção semântica correspondente.

#### 1.3.21.8 Origem nominal e specialization-2 (W-1468)

**Motivação:** D8 ainda colocava `module_id` e o nome do head na raiz local da
specialization. Esse texto não identifica uma declaração collision-safe quando
dois authorities, packages ou módulos usam os mesmos nomes, e não distingue a
origem de um contrato público que muda por interface. D9 fecha esse boundary
sem fingir que o seed é um resolver.

`NominalDeclarationOrigin` é a preimage de uma declaração: authority canônica
com domínio próprio, scoped package name, caminho de módulo canônico, kind,
owner chain semântica e declared name. O `SemanticTypeConstructor` acrescenta
somente o schema identity-defining do head. A specialization acrescenta
substitutions normalizadas e semantic witness identities. `DeclarationContractKey`
e `SemanticInterfaceKey` continuam camadas de contrato/interface; recipe física
inclui bodies, ABI, target, toolchain e implementação; `TypeId` continua local.
Docs, spans, source spelling, file/checkout path, version/revision, aliases de
dependência, workspace, features, target, profile e interface digest ficam fora
da origem nominal.

`DeclarationContractKey` é keyed pela origin e inclui os facts públicos da
declaration e de seus children; exclui docs, source maps, private bodies e
declarations não relacionadas. `SemanticInterfaceKey` agrega a lista ordenada
desses contratos e os imports, reexports e facts do módulo. Field/member drift
muda o contrato, não origin/construtor; docs mudam `DocumentationKey`, private
body muda body/artifact, e outra declaration pode mudar somente a interface
agregada. Version fica fora da origin; `SemanticInterfaceKey`/`WAbiKey`
impedem misturar contrato ou ABI incompatível.

Os kinds D9 são somente `STRUCT=1`, `TYPE=2`, `OBJECT=3`, `ENUM=4`,
`PROTOCOL=5` e `SERVICE=6`; `alias` preserva a origem reexportada. Callable
origin, function overload e const declaration permanecem fora desta fatia de
type constructors e formam gap separado.

Package usa exatamente duas partes ASCII de 1--63 bytes, iniciadas por
`[a-z]`, continuadas por `[a-z0-9-]` e limitadas a 127 bytes no total.
Module segments, owners e declared name usam
`[A-Za-z_][A-Za-z0-9_]*`, sem NUL. UTF-8 inválido ou ASCII fora da gramática é
`INVALID`; UTF-8 válido não-ASCII e facts que excedem somente ceiling são
`UNSUPPORTED` até NFC.

O receipt usa bytes com tags explícitas, lengths/counts big-endian e sem NUL
terminator. A authority é uma preimage inteira autenticada antes do seed. O
builder não cria autorização; ele valida framing, limites e digest, mede e
escreve sem publicação parcial. Input/output/result, arrays de texto/authority
e origin view são disjuntos e imutáveis entre measure/write. A view liga receipt,
module e declaration head.
Malformed, digest divergente, truncação, trailing bytes e relação module/head/
kind/owner inválida falham antes de avaliação. Unicode/NFC não é inventado no
seed: a entrada ASCII canônica é encodable, UTF-8 inválido é `INVALID` e
Unicode válido fora do subset é `UNSUPPORTED` até o resolver fornecer a
normalização executável. O builder publica no máximo 16.384 bytes de preimage;
o parser impõe um envelope hard de framing de 65.536 bytes. Uma view acima
desse envelope é `INVALID`; dentro dele, framing completo acima de um ceiling
de feature pode ser `UNSUPPORTED`, mas framing parseado como `AVAILABLE` ou
`UNSUPPORTED` exige SHA-256 correspondente. Somente framing `INVALID` evita o
hash.

`w-seed-generic-validation-8` e `w-seed-generic-specialization-2` são versões
novas. `fingerprint-1` permanece byte-for-byte e não recebe autoridade nova.
Sem receipt, a validação ainda pode ser `VERIFIED`, mas a identidade publica
`IDENTITY_REQUIRED` com `0/0` e digest zero. A specialization-2 escreve o
preimage nominal uma vez, seguido pelo schema D8; não duplica module/head.

O digest do predicate body ConstIR é somente um proxy bounded do lowering
observado pelo seed. Ele não é receipt semântico autoritativo universal do
predicate/construtor; o receipt do compiler completo continua gap. `.local`
nunca é publicável: sua origem é build-local nonportable.

As fontes comparativas primárias são [Rust incremental compilation in detail](https://rustc-dev-guide.rust-lang.org/queries/incremental-compilation-in-detail.html),
[rustc HIR definitions](https://doc.rust-lang.org/nightly/nightly-rustc/rustc_hir/definitions/index.html),
[Swift ABI Mangling](https://github.com/swiftlang/swift/blob/main/docs/ABI/Mangling.rst)
e [Swift Modules](https://github.com/swiftlang/swift/blob/main/docs/Modules.md).
Elas sustentam somente a separação comparativa entre origem nominal, argumentos,
namespaces e materialização. Não são autoridade para tags, receipts ou
igualdade de W.

O gate source-backed lê markers reais de `reference/last-light/build.w`,
`domain.w` e `generics.w`. O package marker é `last-light/restaurant`; o gate
consome o `AuthorityOrigin` completo e aceito de `AUL0-W-1469-current`. O
verifier D10 recebe trust input out-of-band e não é um resolver completo.
StagePath é ligado a `domain`;
UltimateAnswer, AnswerPair, StaticValue e FinalCallValue são ligados a
`generics`. A matriz Bun/C cobre same-origin equivalence, authorities/packages/
modules/kinds/owners divergentes, body/refinement divergente, fatos físicos
ausentes, missing/corrupt/trailing receipt, sentinels e forced digest collision.

Alternativas rejeitadas:

- continuar usando `module_id` textual, pois alias, package e authority não
  ficam ligados à declaração;
- usar `SemanticInterfaceKey` como origem, pois qualquer mudança pública do
  módulo alteraria todas as declarações mesmo quando a origem não mudou;
- derivar authority de alias `.registry("w")`, pois alias não é autorização;
- incluir version, checkout, target, profile, feature, body ou counters, pois
  esses fatos pertencem a recipe, contrato/interface ou evidence;
- aceitar digest sozinho, pois uma colisão forçada ainda teria de comparar o
  preimage completo.

#### 1.3.22 Bundle W-1470--W-1475: launchers, views e research gates

W-1470 fecha uma migração de superfície antes do 1.0. A posição
declaration-like de Swift explica a grafia anterior, mas não exige que W
preserve essa posição: nas duas grafias o Task continua lexical, staged pelo
parent e owned pelo scope. O initializer é a posição comum para call direta,
`await`, `async` e `spawn`. A regra de uma raiz callable única evita esconder
uma expression composta, e a forma corrente não fornece shim para código
antigo.

W-1471 fecha a pergunta sintática: a bridge `sync` não pode ser inferida de
`maySuspend`; somente `async fn` explícito dá uma precondição source-visible
estável. Uma callable may-suspend por inferência continua válida em `await`,
`async` e `spawn`, mas `sync` nela é error. A bridge blocking exige authority,
quota, provider, deadlock/fairness e cancellation evidence. Ela mantém o scope
estruturado e bloqueia a thread. `runBlocking` de Kotlin é precedente de uso
localizado, não evidência de uma policy portátil. DRC0 fecha o design; frontend
e runtime continuam gaps.

W-1472 não cria outro tipo de view. `ref` observa o place completo e
`view` observa uma projeção da família do carrier. A diferença de metadata,
capacity, UTF-8 e strides é normativa. Um protocol ou aggregate com properties
suprimidas é interface projection e não storage view. A ausência de `Viewable`
universal evita um protocolo que não teria como provar provenance.

W-1479 torna a alternativa de aggregate concreta. Uma projeção nominal pode
combinar `ref` para properties completas, `view` para extents de carriers e
valores owned copiados. O Restaurante usa `PublicCourse` para mostrar título e
allergens sem expor `supplierContract`. O tipo do resultado, e não uma field
mask dinâmica, define a autoridade disponível. Suas dependency edges impedem
mutation incompatível e não mantêm o owner vivo. Um protocol menor continua a
forma para reduzir methods; um DTO owned continua a forma para obter snapshot e
escape. Uma derivação universal seria curta, mas teria de inventar descriptor,
origins e regras de authority para qualquer layout nominal.

W-1480 resolve uma contradição interna encontrada pela leitura adversarial. A
introdução de 23.1.5 rejeitava input e bidirectional streams, mas o lifecycle
normativo já definia input pump e duas direções, o pipeline já movia um feed
para `archive.ingest`, e o corpus de performance já exigia client, server e
bidirectional streaming. SVC0 preserva essa semântica com uma superfície menor:
posição de parâmetro ou resultado decide a direção do mesmo `Stream`.

O modelo é familiar sem copiar a taxonomia RPC para o core. A documentação
oficial do [gRPC](https://grpc.io/docs/what-is-grpc/core-concepts/) confirma as
quatro topologias e a independência das duas direções. O
[WebAssembly Component Model](https://github.com/WebAssembly/component-model/blob/main/design/mvp/Concurrency.md)
modela stream em parâmetro e resultado com ownership único do readable end. A
[Cloudflare RPC](https://developers.cloudflare.com/workers/runtime-apis/rpc/)
confirma que transferir streams com flow control deve retirar o uso do sender.
Essas fontes sustentam o problema e a interoperabilidade, não provam um runtime
W.

No Restaurante, `summarize` consome sinais incrementais e `exchange` move o
input para o producer de output. Um caller push usa `Channel.open(capacity:)`,
mantém o sender e transfere o receiver, que já atende a `Stream`. Assim, humanos
veem `take`, `await` e a direção na assinatura; compiler e runtime obtêm uma
edge canônica com owner, failure, créditos e drain, sem inferir buffer ou
transport.

W-1473 fecha uma fronteira de performance, não uma API. File mapping,
anonymous virtual memory e device memory possuem owners, permissions,
provenance, receipts e interferências diferentes. O estudo MEM0 exige bounds,
drop determinístico e live-view exclusion e inclui fallback de IPC1. Linux,
Windows, CUDA e LLVM fornecem vocabulário primário de target, mas não um
provider W. MEM0 atingiu sua stop condition; a classificação em camadas é a
direção corrente e a implementação por target permanece separada.

W-1474 separa uma proposta simulada de um efeito committed. A máquina bounded
revalida generation/state antes de dispatch, invalida dependents causalmente e
mantém `unknownOutcome(effectId)` após a boundary real. Não há rollback,
compensation ou exactly-once. SEA0 cruza essa máquina com infraestrutura de
teste determinística. Cloudflare OS fornece o precedente direto de simular o
resultado e aprovar depois; FoundationDB e CHESS justificam explorar falhas e
schedules previsíveis. Nenhuma dessas fontes transforma simulation em
conformance de provider real.

W-1475 fecha o inventário de training e inference em torno de gaps. W já tem
Tensor, Device/Queue/Launch, DLPack, ownership, streams, deterministic RNG e
receipts. Autodiff typed, layouts dinâmicos, formatos subbyte, sharding,
checkpoint, KV cache, batching, verification e disaggregation exigem IR,
runtime/provider, tooling ou application evidence antes de qualquer mudança no
core. LLM0 usa workloads source-shaped e só valida contracts e gaps. O mapa de
ownership é corrente; frameworks, kernels, providers e benchmarks não são.

As três pastas de estudo têm dados estruturados, casos positivos e adversariais,
oracles independentes e checkers no package root. DRC0 acrescenta um caso de
fechamento independente para W-1471, W-1473, W-1474 e W-1475. Cada README marca
o limite entre design-oracle-input e implementação. Somente W-1471 seleciona
nova syntax; os outros estudos fecham direções sem promover uma API universal.
