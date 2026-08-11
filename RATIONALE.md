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
- `share(value)` normal e `tryShare(value, using:)` recuperável contra `try share` universal e promotion contextual;
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
- header `script` data-only contextual contra comment metadata PEP 723 e dependency inference;
- root standalone com lock por digest contra package context, ambient registry e `--with`;
- imports explícitos e root canônica contra scan recursivo, cwd, `PATH`, environment e symlink escape;
- capability requirements com deployment grant contra source grants e escalation transitiva;
- entry explícito ou body implícito final, com cleanup explícito, contra execução arbitrária de módulo e estado oculto;
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
os 69 requisitos. Essa contagem fecha o input dos estudos; ela não afirma que
os estudos foram executados. Ela também não substitui a auditoria do ledger
definida na [seção 24.2](DESIGN.md#242-recursos-deliberadamente-ausentes).

O source vigente de um caso deve ser W aceito pelo contrato corrente. Uma forma
substituída pode ser W rejeitado, pseudocode ou outra linguagem. O campo
`language` declara essa origem. O corpus não afirma que o parser W aceita a
alternativa. Estudos humanos e de modelos usam o mesmo `task` e o mesmo input;
eles registram resultados, mas não mudam a decisão sem nova entrada no ledger.

O kernel executável de memória usa a baseline M1. O corpus possui 165 casos e
580 operações, com 70 outcomes aceitos e 95 rejeitados. Cada caso liga
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
mede as 169 formas derivadas do corpus pelo runner nesta revisão. O runner
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
spawn<.compute> let port = mix(left)
spawn<domain: .compute> let starboard = mix(right)
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
`R0-erasure-storage`.

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

#### 1.3.16 Workflow single-file PYN1

**Exemplo:** `horizon_script.w` usa header standalone e depois de promotion;
um source sem dependency externa pode usar package context sem header. A
resolução e o entry permanecem explícitos ou implícitos conforme o source.

[`tooling/script-workflow-cases.json`](tooling/script-workflow-cases.json)
contém casos positivos e negativos para o header, root, context, imports,
virtual selection, lock root, fetch, requirements, identity, entry, cleanup,
mutation e promotion. São 95 casos e 546 operações (23 aceitos e 72
rejeitados). O corpus declara símbolos de
[`horizon_script.w`](reference/last-light/horizon_script.w) e os IDs PYN1 do
ledger como decisões que os casos exercitam. Os casos cobrem:

- no-header std hello, header locked e header standalone em workspace;
- no-header package context, context explanation e identifier `script`;
- missing ou mismatched lock, duplicate alias/field/header e schema/field inválido;
- imported script, path/mutable/ambient/override dependency e root/import traversal ou symlink escape;
- recursive, cwd, `PATH` e environment scan, URL, stdin e shebang;
- fetch antes de lock, network policy, offline root/closure hit/miss e CAS explícito;
- digest, authority e signature mismatch, requirement/grant/secret e handle transitivo;
- `entryForm` explicit/implicit/missing, body/effect evidence, declaration-after-statement,
  import de root implícito, error escapante, cleanup de failure e ausência de hidden state;
- add/remove/resolve atomic, promotion equivalente e promotion com dependency/local graph,
  entry, requirements ou provenance divergente;
- edition, target e Windows case/drive/UNC mismatch;
- payload P0 `contexts`/`packages`, package IDs content-derived, closure transitiva,
  dangling/unreachable/missing nodes e alias collision local;
- root physical opaque com same-drive, same-UNC e Unicode canonical-equivalent
  accepted, different owner rejected.

[`tooling/script-workflow-machine.mjs`](tooling/script-workflow-machine.mjs)
deriva state, trace e digests. O teste host verifica standalone em workspace,
retirement de bytes no mismatch, default capability e atomicidade. O snapshot
JSONL é gerado pelo checker e não repete o campo `expected` como semântica.
Parse Tree-sitter, host oracle e cases são evidência de design. CLI, compiler,
resolver, provider, runtime, estudo humano e estudo de modelo permanecem
missing.

O oracle mantém separadas estas projeções operacionais:

| Operação | Evidência derivada |
|---|---|
| `parseHeader` | posição, edition, fields, aliases, source kind, entry e digest do body implícito |
| `selectContext` | standalone, package ou ephemeral sem merge implícito |
| `resolveRoots` | discovery físico, containment do target e policy de busca |
| `validateImports` | edges explícitos, path→digest e script root-only |
| `validateResolution` | payload P0, root recomposto, seleção virtual e closure alcançável |
| `admitFetch` e `verifyArtifact` | policy, CAS, candidate real, authority, signature e retirement |
| `admitCapabilities` | requirements, grants efetivos e handles transitivos |
| `buildEphemeral` e `runEntry` | identity sem path físico e entry explícito ou implícito |
| `cleanup` e `contextExplanation` | ausência de estado oculto e explicação completa da seleção |
| `scriptAdd`, `scriptRemove`, `scriptResolve` | troca atômica de header e lock após nova prova de parse |
| `promote` | package equivalente, graph e entry preservados, provenance emitida |

O fixture conserva também os detalhes do lock que não precisam ser repetidos no
contrato de scripts: payload P0 não achatado, context virtual com `rootEdges`,
IDs de package derivados de conteúdo, aliases locais ao parent, traversal da
closure e seleção exata por use, target role e target. Artifact records, recipes
e outputs ficam fora do payload do lock e entram na identity somente quando são
selecionados. CAS ambiental nunca entra na identity.

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

O corte corrente tem 67 casos e 287 operações: 53 programas aceitos e 14
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
DLPack permanece um adapter T2 separado. PYN2 registra somente output bounded
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

PYN4 fecha dois módulos T2 draft e mantém `std.tensor@1` e `std.dlpack@1`
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

**Exemplo:** `share(MenuSection(...))` cria o primeiro owner com a policy normal;
`try tryShare(take draft, using: memory)` escolhe recovery e allocator.

O bundle
[`r1-shared-construction`](tooling/studies/r1-shared-construction) compara três
formas parseáveis: operação explícita com policies separadas, `try share`
universal e promotion pelo expected type. Os inputs cobrem temporary, binding
existente, allocator bounded, payload lifetime-dependent e falha antes da
publicação do handle. O oracle host verifica consumo, cleanup e failure policy;
ele não aloca um control block W.

Rust separa `Arc::new`/`Rc::new` das variantes `try_new`. Swift ARC mantém
reference counting automático para instances de class. O working draft de C++
define factories `make_shared` e `allocate_shared`. W não copia essas
superfícies: o qualifier `shared` continua sendo o tipo e um verbo no value
expression torna allocation e mudança de ownership observáveis.

A forma selecionada reduz ceremony no caso normal sem introduzir promotion
contextual. `tryShare` permanece distinto porque o caller escolhe uma failure
boundary recuperável. Tree-sitter e o oracle host são a evidência atual;
`w-compile`, `w-run`, estudo humano e estudo de modelos permanecem missing.

### 1.4 Concorrência, paralelismo e execução

Esta seção preserva comparação, precedentes e alternativas. A seção 12 de
[`DESIGN.md`](DESIGN.md) define o contrato corrente de W.

#### 1.4.1 Modelo de execução

As fontes primárias usadas no gate W-1170 são:

- [Swift SE-0296](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0296-async-await.md),
  [SE-0304](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0304-structured-concurrency.md)
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
| criação de child | `async let` ou `spawn<domain> let`, estruturados | `async let` e task groups | `go f()` cria goroutine | `StructuredTaskScope.fork` | algorithms compõem operation states |
| handle | `Task` linear; join produz value ou outcome | binding ou scope | `go` não devolve handle | `Subtask` | operation state |
| lifetime | scope cancela, drena e faz join | scope espera children | goroutine pode sobreviver ao caller | scope faz join e shutdown | operation state vive até completion/stop |
| erro e cancel | typed errors, outcome e cancel bounded | error/cancel de child | valores e APIs | scope policy | error/stopped completion channels |
| placement | `spawn` escolhe domain tipado | actor/executor preference | scheduler runtime | executor ou virtual thread | scheduler sender |
| ordering | domain serial ou paralelo; barrier explícita | executor e GCD | scheduler/queue | executor | scheduler/algorithm |
| ownership | provas de transfer, share e loans | exclusivity e isolation | disciplina do programa | JMM e synchronization | lifetime/data-race rules de C++ |
| admission | budgets do domain/profile | policy do executor | runtime policy | policy do executor/scope | sender/scheduler contract |
| efeito | `maySuspend` inferido; `async fn` recomenda call | `async` nominal | sem efeito async nominal | sem efeito async nominal | completion signatures |

Koka é somente uma referência para
[inferência de effects](https://koka-lang.github.io/koka/doc/book.html).
libdill e libmill são referências de runtime para coroutines e channels
([libdill](https://sustrik.github.io/libdill/),
[libmill](https://libmill.org/)). Nenhum deles define W ou é uma dependency.

O gate W-1170 compara estrutura, placement, ordering, ownership, admission,
cancellation e lowering. Ele passa somente quando `async let` preserva
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

Release sequences e compare-exchange foram comparados com o
[LLVM LangRef](https://llvm.org/docs/LangRef.html#cmpxchg-instruction). Fences,
compiler fences e a separação entre success e failure foram comparadas com as
APIs de Rust para
[`fence`](https://doc.rust-lang.org/std/sync/atomic/fn.fence.html),
[`compiler_fence`](https://doc.rust-lang.org/std/sync/atomic/fn.compiler_fence.html)
e [`Atomic`](https://doc.rust-lang.org/std/sync/atomic/struct.Atomic.html).

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

A documentação do kernel Linux sobre
[RCU](https://docs.kernel.org/RCU/whatisRCU.html) separa removal de reclamation.
Ela também exige que readers anteriores terminem antes do reuse. W preserva
essa divisão em `SnapshotCell`, mas não expõe grace periods no safe source.

#### 1.4.5 Locks de leitura e escrita

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
Logo, nenhum primitive do host fornece sozinho a policy lógica de W.

Domains cobrem o caminho task-owned com placement, tickets e cancellation.
`SnapshotCell` cobre publicação imutável; `Mutex` e `AsyncMutex` cobrem critical
sections. Code freestanding e adapters sem task runtime ainda precisam de
reads simultâneos sobre storage mutável. W-1189 a W-1192 fecham esse espaço com
`ReadWriteLock<T>`, closures scoped e admission por fases. O provider e o
benchmark continuam ausentes; isso limita claims de performance, não a
semântica da safe std.

Condition variables foram consideradas pelo mesmo critério. Separar predicate,
lock e notification cria uma protocol surface em que lost wakeup e lifetime do
waiter pertencem ao caller. Channel, task outcome e service carregam state,
ownership, cancellation e close no mesmo contrato. O runtime continua livre
para usar condition, futex ou parking internamente.

`Once` raw também não entra. Const/module initialization resolve o caso
estático. `var Lazy` cobre o caso tardio sem publicar uma primitive de estado.
Barreiras cíclicas e atomic wait/notify permanecem pesquisas separadas porque
possuem participantes e suspension diferentes.

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

Rust separa criação normal e recuperável em `Arc::new`/`Arc::try_new` e
`Rc::new`/`Rc::try_new`. O
[ARC de Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/automaticreferencecounting/)
torna a alocação de class instance contextual, mas também torna toda class uma
reference type. O
[working draft de C++](https://eel.is/c++draft/util.sharedptr) preserva factories
de shared ownership e recomenda uma única allocation. W escolhe um verbo
explícito para a mudança de ownership, separa policy normal de recovery e deixa
co-allocation para o optimizer.

#### 1.5.2 Allocators e arenas

[`Allocator` de Rust](https://doc.rust-lang.org/std/alloc/trait.Allocator.html)
e o
[`remap` de Zig](https://ziglang.org/download/0.14.0/release-notes.html#Allocator-API-Changes-remap)
informam strong failure, resize in-place e relocation. W deixa fallback e
commit no caller e registra origem por receipt.

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
| ergonomia com evidência | 65–72% | R0 cobre 69/69, R0S mede a superfície derivada por script e R1 possui 21 bundles contrabalanceados do Última Luz que promovem 32/69 casos R0; participantes e modelos ainda não foram executados |
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
| `Arena` baseline com scope explícito | **Provável** | M1 fecha lifetime, budget e rehome lógicos; async e destruição física exigem protótipo |
| allocator explícito por `using` | **Possível agora** | origem e deallocator acompanham o owner |
| mobilidade derivada da origem | **Provável** | M1 separa origem local/cross-domain; FFI e matriz de providers exigem protótipo |
| allocator geral por build profile | **Possível agora** | profile gera runtime requirement e plan fixa provider exato |
| `shared` + `weak` sem cycle collector | **Provável** | M1 fecha lifecycle lógico; atomics, overflow e tooling de ciclos exigem avaliação |
| `async let`/`spawn<domain> let` estruturados | **Possível agora** | state machine e runtime mínimo delimitados |
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
| `InlineString` com capacity no tipo | **Provável** | storage explícito atende embedded e ABI; overflow e boundary permanecem visíveis |
| strict numerics e overflow verificado | **Possível agora** | backend oferece operações adequadas |
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
| output gate por commit dependency | **Provável** | closed turn e staging existem; multi-provider e abort exigem fault tests |
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
| WorkQueue, broadcast, watch e weighted channel | **Provável** | cada tipo possui loss, fan-out, lag e accounting próprios |
| `ByteSource`/`ByteSink` async-first | **Possível agora** | short progress, EOF e errors possuem resultados fechados |
| read por append em reserva privada de `Bytes` | **Possível agora** | initialized count e commit ocultam storage ainda não inicializado |
| cancellation de I/O com completion drain | **Provável** | backends possuem completion; runtime e borrow checker precisam de oracle |
| filesystem com rights estáticos e offset posicional | **Provável** | handles e syscalls existem; profiles e diagnostics exigem protótipo |
| adapters blocking com quota | **Provável** | pool bounded preserva semântica; cancellation física depende da API |
| backends readiness/completion equivalentes | **Provável** | contrato comum está fechado; matriz de targets deve provar os mesmos traces |
| gather write com segments borrowed | **Possível agora** | rest homogêneo, prefix progress e fallback sem allocation fecham a superfície |
| scatter read por `ReadBatch` | **Provável** | owner único fecha aliases, initialized counts e rollback parcial |
| file/device transfer especializada | **Provável** | operação informa progress e fallback; zero-copy exige capability do adapter |
| `transferable`/`shareable` estruturais | **Possível agora** | fields, captures, borrows, cleanup e interface compilada fornecem facts fechados |
| data-race freedom e happens-before | **Possível agora** | ownership, tasks, channels, services, locks e atomics fornecem edges fechados |
| `var atomic` e orders estáticas | **Possível agora** | superfície baixa diretamente para atomic load/store/RMW/cmpxchg |
| fallback atomic não lock-free | **Provável** | runtime striped lock preserva semântica; signals e freestanding exigem profile |
| `Atomic<T, lockFree: true>` | **Possível agora** | target e alignment resolvem o contrato em compile time |
| `Mutex.withLock` scoped | **Possível agora** | closure não escapa e cleanup síncrono fecha unlock |
| closure de `AsyncMutex.withLock` sem suspension | **Possível agora** | tickets FIFO, cancellation e unlock possuem contrato LM0 |
| `ReadWriteLock<T>` na safe std | **Possível agora** | closures scoped e fases writer-aware fecham a semântica; provider e benchmark continuam gates de implementação |
| condition variable na safe std | **Rejeitado** | channel, task outcome e service unem evento, ownership e cancellation |
| lazy concorrente, barreira cíclica e atomic wait/notify | **Pesquisa** | cada forma ainda precisa de failure, cancellation, generation e provider contract próprios |
| `SnapshotCell<T>` | **Possível agora** | `read`, `snapshot` e `publish` fecham versões, edges e reclamation sem API RCU no caller |
| RCU genérico safe | **Rejeitado** | reclamation, ABA e leitura longa exigem adapter `unsafe` especializado |
| facts trusted para FFI e synchronization customizada | **Provável** | somente provider ou foreign interface fixa target, digest e negative facts |
| domain default por módulo | **Rejeitado** | import não possui instance, lifecycle ou executor |
| QoS na syntax de `spawn` | **Rejeitado** | policy no profile ou group não parece garantia de ordering ou deadline |
| `bootstrap.w0` e self-host antes de tasks | **Provável** | subset fechado; seed C e adapter MLIR precisam de prova |
| mimalloc como profile | **Provável** | API e build são conhecidos; versão, targets e foreign mix exigem benchmark |
| mimalloc universal | **Rejeitado por enquanto** | origem estrangeira, versão e targets impedem um default sem evidência |
| SQLite como durability universal | **Rejeitado** | adapter oficial é útil; semântica universal não é portátil |
| seccomp por módulo importado | **Rejeitado** | import não é uma security boundary |
| sandbox portátil por process/Wasm | **Provável** | depende do host, mas preserva o contrato |
| `fn<C>` com static archive | **Provável** | depende primeiro da façade C e do build hermético |
| `fn<Rust>`/`fn<Swift>` | **Provável após C** | adapter hermético agrupa runtime e usa façade C tipada |
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
| Arquivo único com argumentos e execução repetida | `w run <product>` e lock de package | `w run path/file.w -- <args>` com header standalone ou package/ephemeral context hermético | CLI e resolver | **Direção** |
| REPL com edição, history e completion | parser, checker e HIR normais | session transacional, generations e custo de invalidação | tooling e HIR | **Direção** |
| Notebook para code, prose, data e rich output | LSP, diagnostics e outputs estruturados | kernel Jupyter, protocol tipado e export canônico | tooling e produto | **Direção** |
| Reexecução offline e provenance | lock e artifact digests de package; PYN1 fecha header, virtual selection, lock root e CAS para arquivo único | gates de registry/provider e execução real | resolver e release | **Direção** |

`w run path/file.w -- <args>` é a direção para arquivo único. Com header, o
source é sempre root standalone mesmo dentro de package. Sem header, dentro de
um package o comando usa o entry default explícito ou implícito, contexto e lock
desse package; fora de package, cria contexto efêmero hermético com std e
módulos locais. O
resolver não pesquisa ambiente ou path e não baixa remote implicitamente.

O source graph do arquivo contém somente imports explícitos. Sem package
context, a root local é o diretório do script. Com package context, a root é o
package.w selecionado pela regra de workspace vigente. `w context` mostra a
seleção discoverable, o manifest, o workspace, o lock e as roots antes da
execução. Não há recursive scan, cwd scan, `PATH` scan ou environment
discovery. `w run path/file.w` exige o unnamed/default entry vigente. Ele não
cria execução arbitrária de módulo: a forma curta permitida é somente o
`implicit_entry_body` final do root, baixado para `.default`.

Uma falha do package/product efêmero não deixa manifest ou estado oculto.

PYN1 adota metadata inline data-only no header `script`, com lock por digest e
provenance. Manifest sibling, comment metadata e `--with` não são formas
correntes:

```text
w run path/file.w -- input.csv
w script add path/file.w package@constraint --as package_alias
```

O comando final exige lock por digest, provenance e reexecução offline. A
implementação de resolver, CLI e provider permanece missing.

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
hidden replay de effects. Nomes de comandos para check/export continuam
**Pesquisa**. A implementação do kernel continua pós-freeze.

##### Standard library e ecossistema

| Motivo de adoção Python | Cobertura W atual | Gap real | Camada correta | Estado |
|---|---|---|---|---|
| NumPy e ciência numérica | `std.math`, `std.tensor`, shapes, `@` e device transfer | adapters Python e corpus de interoperabilidade | módulos std e adapter first-party | **Direção** |
| DataFrames e dados colunares | `std.data`, `std.csv`, `std.parquet`, `std.arrow`, schemas e database rows tipadas; TAB0 e TAB1 fecham adapters, contracts e host evidence como design | DataFrame completo fica package first-party; seguem dependency form e explicit import-root de single-file, session transacional com resource/drain semantics e rich display, bundle próprio de DLPack tensorial e evidence dos gates de latency; providers ficam pós-freeze | módulos std concretos, package first-party e codecs | **Direção** |
| Plotting e rich display | protocols de display e tooling estruturado | renderer, limits e backends de plot | first-party package ou third-party | **Pesquisa** |
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
semântica do target para source comum. SDK0 usa types fechados e não aceita raw
sockets, descriptors herdados ou socket-option escape hatch.

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

**Alternativa:** um raw body com fence hash permite recovery sem adapter. O
corpus deve comparar a forma braced com `#{...}#` antes do design freeze.

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
- split direcional ou protocol genérico para datagram;
- APIs de radix adicionais;
- `slice`, `span`, `borrow` e `readonly` no lugar de `view`;
- matrix literal `[1 2; 3 4]`;
- “A última linguagem que você vai precisar aprender” como slogan.
- ordinals no source, hash do nome e ordem de declaração no lugar de
  `interface.lock`;
- metadata PEP 723 em comment, sibling manifest obrigatório, package-only e
  CLI `--with` no workflow PYN1.

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

### 1.14 Resultado das pesquisas consolidadas

Esta tabela preserva o snapshot que precedeu o fechamento das formas correntes.
Ela não cria uma segunda lista normativa de features.

**Exemplo:** `ReadBatch` fica provável. `inout T...` fica rejeitado. Os dois
resultados tratam a mesma necessidade sem deixar uma decisão ambígua.

Todos os itens antes classificados como **Pesquisa** possuem agora uma saída:

| Grupo | Provável ou possível | Rejeitado ou adiado |
|---|---|---|
| tipos | typed property path e `StateGraph` const | anonymous sum/record, constraint list, GAT, packs e existential opening |
| compile time | `WMeta1` com chunks CBOR | callable const indireto, SMT geral e autotuning no build |
| memória | `InlineString`, trusted foreign facts e cache isolation | public unpin, high-bit baseline e async-close universal |
| execução | dynamic execution-domain selection, topology types, advanced atomics, fences e sync | QoS em `spawn`, permit type rule, `yield`, safe RCU e service reentrant |
| workflow | child workflow, fan-out e `continueAsNew` | durable race, absolute core sleep, user compaction e 2PC implícito |
| I/O | `ReadBatch`, `io.transfer` e commit-provider SPI | zero-copy implícito, `flush` universal e transaction multi-provider |
| services | wWire, custom adapter SPI, plugin lookup e `PersistentRef` | 0-RTT, opaque capability relay, direct introduction e distributed ref equality |
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
`async let`, domínio explícito de `spawn`, FIFO serial, gate `.parallel` e
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
domain para trabalho estruturado. `ReadWriteLock<T>` não é alias dessas formas.
Ele usa tickets e fases para storage síncrono; benchmark posterior decide quando
recomendá-lo em vez de `Mutex`, `SnapshotCell` ou domain barrier.

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
| M1 memory transition | 165 casos, 580 operações | 70 aceitos, 95 rejeitados | checker puro | não executa W |
| A0 physical allocation | 48 casos, 123 operações | 15 aceitos, 33 rejeitados | 13 testes | não mede allocator real |
| L0 layout e ABI | 78 casos, 96 operações | 27 aceitos, 51 rejeitados | 10 testes | não implementa linker, importer ou backend |
| execution ergonomics | 61 casos | 23 positivos, 36 negativos, 2 informações | 15 testes | não compila nem agenda W |
| E0 concurrency | 57 casos, 527 operações | 31 aceitos, 26 rejeitados; 10/10 origens HB | checker puro | valida witness; não enumera execuções |
| E1 liveness | 41 casos, 473 operações | 19 aceitos, 22 rejeitados | 7 testes | não prova clock, OS I/O ou terminação de user code |
| MX0 ownership + execution | 46 casos, 274 operações | 23 aceitos, 23 rejeitados | 14 testes | compõe modelos; não executa checker, scheduler ou runtime W |
| CH0 bounded channel | 47 casos, 333 operações | 28 aceitos, 19 rejeitados | 12 testes | não implementa scheduler, runtime ou provider W |
| LM0 scoped locks | 42 casos, 171 operações | 25 aceitos, 16 rejeitados, 1 fault | 11 testes | não implementa `std.sync@1` |
| SP0 snapshot cell | 27 casos, 82 operações | 14 aceitos, 12 rejeitados, 1 fault | 7 testes | não implementa reclamation físico |

E0 cobre lifecycle, cancellation, fail-fast, as dez origens de happens-before,
races, modification order, fences, RMW, extents e tickets de barreira. Ele não
prova liveness, fairness, preemption, oversubscription, task-frame allocation,
reentrância de service, device scope, reclamation, ABA ou execução distribuída.

E1 separa scheduler, clock e provider do contrato de closure. MX0 usa um único
witness para testar owner graph e task lifecycle juntos; ele impede que duas
provas isoladas escondam copy, share, rollback ou drop divergente. LM0 cobre
loans, FIFO, `tryWithLock`, cancellation, unlock, drop e fault boundary. SP0
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
prematura. B0 recebe admission, evidence e effect IDs resolvidos. Ele não prova
adapter, transport, fila bounded, clock, storage, wWire, deduplication, database,
crash recovery ou execução distribuída.

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
| W-004 | labels | primeiro posicional, demais nomeados | todos nomeados; todos posicionais |
| W-005 | closure | `(args) => body` | `fn(args) {}`; `{ args in }` |
| W-006 | capture | inferência + `capture(...)` | `[capture]`; somente inferência |
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
| W-025 | shared | `share(value)` normal, `tryShare(value, using:)` recuperável, `copy` para novo owner e `weak()` | ARC implícito; promotion por expected type; block-region-only (retired) |
| W-026 | region block (retired) | syntax `region name(using:, limit:)` liderava e baixava para `Arena`; a API foi mantida, mas o bloco foi retirado antes de W 1.0 | lifetime annotations; heap por módulo; API sem bloco |
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
| W-037 | concorrência | `async let` | Future/Promise; task API somente |
| W-038 | paralelismo (retired) | forma anterior `spawn let`; W-1161/W-1162/W-1172 fecham `spawn<domain> let` como dispatch explícito | mesma keyword de async; parallel loop apenas |
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
| W-092 | WLO/tree strings | pesquisa com fallback | formato/representação default |
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
| W-118 | início de child | `async let`/`spawn let` iniciam na declaração | lazy no primeiro await |
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
| W-133 | output durável | outcome de step só aparece depois do commit; outbox para mensagem; gate geral em pesquisa | gate geral inferido na v0 |
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
| W-163 | enum e protocol | cases e requirements herdam; witness não repete modifier | `export` repetido; todos os members públicos |
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
| W-207 | custom pattern | pesquisa; conversão nomeada ou guard no design vigente | handler arbitrário; protocol de pattern na v0 |
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
| W-221 | bundle Unicode | edição, tabelas e digests fixos para UAX #15/#29/#31 e UTS #39 | versão do host; ICU obrigatório |
| W-222 | texto do host | `OsString`, `Path`, `Utf8Path` e `PackagePath` distintos; colisão NFC rejeitada | paths sempre String; bytes portáveis do OS |
| W-223 | C strings | `CString`/view separados, NUL verificado e inbound bounded | String sempre NUL; scan C ilimitado |
| W-224 | storage textual | refinement não fixa layout; reserva mínima é operação; capacity/SSO exatos não são properties | capacity pública; SSO observável |
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
| W-291 | default witness | somente o módulo do protocol publica; seleção gravada na conformance | extension importada muda witness |
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
| W-346 | início de async (retired) | forma anterior; W-1161 mantém `await` na task atual e children `async let`/`spawn<domain> let` após staging lexical | Promise implícita; body parcial no parent |
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
| W-392 | shift | count `UInt`; bound e perda à esquerda causam panic; bit policies nomeadas | mask do count; regras C; wrap silencioso |
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
| W-415 | criação shared | `share` usa policy normal; `tryShare` é fallible e aceita allocator explícito; não há promotion implícita | constructor wrapper nominal; expected type aloca; shared universal |
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
| W-429 | FFI mobility | local por default; fato trusted provável somente em provider/foreign interface com target e digest | raw pointer deriva facts; assertion segura do usuário |
| W-430 | representação W0 de String | literal/static + buffer flat único com pointer/count/reserva/origin; Bytes usa carrier T0 compatível | SSO e COW no bootstrap; rope; runtime Unicode obrigatório |
| W-431 | COW de String | fora da baseline; optimizer exige efeitos de allocation e cleanup não observáveis | refcount em toda String; COW como contrato; proibir otimização |
| W-432 | reserva de String | mínimo total por bytes; exact capacity não é pública; `tryReserve` tem strong guarantee | bytes adicionais; growth fixo na linguagem; capacity property |
| W-433 | mutation de String | append/replace recebem view válida; source do mesmo owner é erro; índices são invalidados | mutable byte view; temporary de alias implícito; byte offsets unchecked |
| W-434 | esvaziar String | `clear` mantém storage; `reset` libera; `takeAll` transfere conteúdo | Boolean `keepingCapacity`; um método ambíguo; builder separado |
| W-435 | String e Bytes | carrier T0 compatível; adoption e `intoBytes` consomem sem allocation geral | layout público igual; cópia obrigatória; cast implícito |
| W-436 | caches de texto | reads não alocam nem mutam; summaries eager permitidos; índice alocante usa tipo próprio | cache lazy invisível; owner muta por read; grapheme ordinal O(1) |
| W-437 | String especializada | SSO invisível medido; `InlineString`, Rope, IndexedText e tree string são tipos próprios | threshold público de SSO; uma String universal adaptativa |
| W-438 | ponteiro textual | somente borrow scoped; persistência usa CString ou buffer pinned sem relocation; pin do descriptor não basta | pointer estável de String; NUL obrigatório; raw pointer safe |
| W-439 | String no self-host | flat UTF-8, bytes, append/reserve, views, conversions e ownership; Unicode avançado não bloqueia SH0 | grapheme/locale antes do parser; C runtime de String permanente |
| W-440 | data race | bytes sobrepostos, concorrência, write e ausência de happens-before; atomics concorrentes precisam de extent idêntico; safe W rejeita | race com resultado definido; check somente em runtime; atomic parcial sobreposto |
| W-441 | happens-before | task start/join, channel em W-467, service turn, unlock/lock, release sequence e fence com reads-from atômica | thread start/join somente; cancel publica user state; duas fences sem atomic publicam |
| W-442 | storage atomic | `var atomic value: T` baixa para `Atomic<T>`; acesso comum seq-cst | `Atomic<T>` sempre explícito; behavior Atomic; todo var atomic |
| W-443 | atomic value | fato intrínseco fechado para Bool, integers e enum sem payload | protocol user-defined; qualquer Copy; floats e structs na baseline |
| W-444 | order | `<.order>` estática; load/store/update usam enum subsets; default `.sequential`; sequential participa de ordem total do scope | argumento runtime; suffix por método; relaxed default |
| W-445 | compare-exchange | result enum; success/failure usam matriz estática; success é RMW e failure é load; weak é explícita | Boolean; expected inout; combinação inválida em runtime; failure entra na modification order |
| W-446 | aritmética atômica | policy checked normal; wrapping/saturating/fetch nomeados | wrap do hardware implícito; closure update com retries ocultos |
| W-447 | borrow atômico | `ref` obtém Atomic; payload comum abre somente por `inout`/consumo exclusivo; edge não concede authority nem lifetime | `ref T` comum; misturar views atômicas e não atômicas; release prolonga owner |
| W-448 | lock-free | não é implícito; const `isLockFree` e contrato `lockFree: true` | garantir toda largura; runtime query sem target fixo |
| W-449 | ABI atômica | layout W opaco; operações concorrentes usam endereço + extent idênticos; C usa wrapper e metadata | layout igual a C `_Atomic`; layout estável universal; larguras parcialmente sobrepostas |
| W-450 | mutex síncrono | `Mutex.withLock` scoped e marcado blocking; sem guard público na baseline | lock/unlock manual; behavior Locked; poisoning |
| W-451 | mutex assíncrono | aquisição suspende; closure protegida é sync e cancel-safe | guard cruza await; mutex síncrono no worker cooperativo |
| W-452 | RwLock e RCU (retired) | direção anterior agrupava primitives distintas; W-1178 fecha SnapshotCell e mantém RCU safe rejeitado | policy automática por property; RCU default universal |
| W-453 | contenção | explanation record mostra lowering, lock-free, waits e de-atomicization provada; cache isolation é provável com target contract | prometer performance por `atomic`; padding universal; enfraquecer order sem prova |
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
| W-470 | outras topologias | `WorkQueue`, `Broadcast`, `Watch` e weighted channel são prováveis T1 com contracts distintos | um `Channel<mode: ...>` muda loss e fan-out |
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
| W-565 | workspace | `workspace.w` data-only lista members exatos; não cria identity publicável | discovery recursivo; workspace executável; package e workspace fundidos |
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
| W-594 | toolchain plan | record data-only fixa target spec, execution platforms e providers; recipe referencia a row | toolchain no package lock; resolver novamente durante reprodução; output na plan |
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
| W-637 | `shared` e `weak` | `upgrade` linearizável; value morre no strong zero e control block no weak zero | weak aponta ao payload reutilizável; contador curto no pointer; ressurreição |
| W-638 | mobilidade de allocator | origem local impede `transferable`; `Allocator<(.crossDomain)>` publica a prova necessária | todo allocator cruza domain; annotation por container; teste runtime tardio |
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
| W-649 | admission de task | argumentos/captures avaliam uma vez em staging; reserva precede publish; falha limpa owners e produz handle canceled inline | pular efeitos sob carga; fila ilimitada; novo error effect em `async let` |
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
| W-736 | ciência e data parallel | `Complex<T>` T2 e `Simd<T, lanes>` T1 preservam numeric policy; scalar fallback não muda resultado | complex literal novo; vector width dependente do target; fast mode implícito |
| W-737 | contexto local | `TaskLocal<T>` tem scope estruturado; `ThreadLocal<T>` usa accessor sync e não cruza `await` | mapa task-local mutável; TLS como isolation; borrow TLS suspenso |
| W-738 | volatile e MMIO | platform SDK cria `MmioRegister<T, access>` por capability; volatile não sincroniza e não é modifier geral | `var volatile`; integer cria pointer; MMIO safe sem host authority |
| W-739 | linker placement | product/target variant liga symbol a section/address/retain com overlap e alignment verificados | annotation em source comum; import muda section; linker flag livre |
| W-740 | assembly | `unsafe fn<Asm>` T2 declara target, operands, clobbers, memory, unwind e volatility; adapter gera provenance | asm safe; clobber implícito; naked function baseline; string passada ao backend sem scanner |
| W-741 | primitives de execução (retired) | direção anterior listava SnapshotCell como provável; W-1178 fecha sua superfície sem agrupar topology ou wait/notify | um Channel muda topologia por mode; safe RCU geral; uma API universal para toda topologia |
| W-742 | evolução de workflow | child workflows, fan-out e continue-as-new T2 usam IDs determinísticos; race durável e absolute core sleep ficam rejeitados | persistir async frame; wall clock implícito; compaction definida pelo usuário |
| W-743 | metadata publicável | WMeta1 usa header/directory e chunks em subset CBOR determinístico; profiles separam interface e object ABI; cache interno continua recipe-exact | codec universal; JSON binário; HIR antiga vira ABI eterna |
| W-744 | extensões de service | custom adapter, plugin lookup e PersistentRef são T2; 0-RTT, opaque relay, direct introduction e distributed equality ficam fora | authority por string; capability em unknown field; reconnect direto implícito |
| W-745 | ilhas externas posteriores | source units e adapters Rust/Swift/Zig/C++/Fortran são prováveis após C e sempre usam façade tipada | runtime externo implícito; staticlib por função; ABI W rica atravessa a ilha |
| W-746 | loop pós-condicional | `repeat { body } while condition` executa body ao menos uma vez; `continue` avalia a condição final | rejeitar post-test loop; `do ... while`; exigir `while true` e `break` negado |
| W-747 | block rotulado | `label: { ... }` aceita somente `break label`; saída lexical executa cleanup e não produz value | `continue` para block; label em qualquer statement; salto para dentro; break com value na baseline |
| W-748 | documentação de substituições | toda decisão que rejeita uma construção por outra recebe caso comparativo e cobertura gerada antes do freeze | mostrar somente a forma escolhida; contar apenas exemplos por seção; executar syntax rejeitada no corpus positivo |
| W-749 | fechamento do design freeze | famílias estão classificadas; grammar, semantics, diagnostics, std, targets, formats, execução, packages, W0 e substituições ainda exigem artefatos fechados | tratar classificação como spec completa; esperar backend para escrever contratos; congelar sem conformance |
| W-750 | newline e statement boundary | newline é whitespace; parser consome a maior expression; semicolon força boundary e permanece quando sua remoção mudaria statement partition | ASI; newline sempre termina; formatter remove todo semicolon mesmo com mudança de CST |
| W-751 | grammar normativa G0 | EBNF de block, binding, controle, labels e transfer statements pertence ao design; Tree-sitter é projeção | parser gerado como autoridade; prose sem grammar; aguardar frontend completo |
| W-752 | recovery sintático G0 | recovery insere somente delimiter/keyword exigida, preserva bytes em ERROR e usa MISSING zero-width; build rejeita árvore recuperada | inventar expression/identifier; compilar recovery tree; descartar bytes; formatter salvar reparo silencioso |
| W-753 | grammar normativa G1 | EBNF de raízes, imports e declarations pertence ao design; source de módulo e manifest são documentos disjuntos | package inline; parser gerado como autoridade; manifest misturado com source executável |
| W-754 | fase de imports | header precede imports e imports precedem declarations comuns; recovery não move imports | imports intercalados; import dentro de body; ordenação automática pelo formatter |
| W-755 | body de função | função comum exige body; somente protocol requirement e foreign signature podem omitir body | prototype solto no top-level; body inferido; newline termina signature |
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
| W-789 | initializer de child | `async let` e `spawn let` exigem call root; parent prepara callable, argumentos e captures; child executa o body | mover expressão composta inteira implicitamente; lazy no await; avaliar argumentos no child |
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
| W-840 | operand de ownership prefix | `ref` e `inout` exigem place; `take`, `copy` e `pin` verificam owner, mobility e origem antes de criar delta | borrow de rvalue com lifetime temporário implícito; annotation de lifetime; operação inferida pelo callee |
| W-841 | corpus G4/S0 | sete families de expression, effect e ownership possuem baseline único, inversão syntax-valid, outcome e snapshot D0 | examples sem outcome; um fixture com várias falhas; duplicar errors de parser, type e expression |
| W-842 | ownership de diagnostic const | W-CONST possui sete meanings fechados; a primeira falha que impede ConstValue cria o root e a cadeia permanece estruturada | texto livre do evaluator; um code para toda falha; error por cada caller da mesma cadeia |
| W-843 | operação não const-safe | W-CONST-0001 cobre call, capability ou target semantic que ConstIR não reproduz; target é fact, não outro meaning | usar W-CONST-0007 para target; executar host semantic; fallback runtime num const obrigatório |
| W-844 | ciclo const | grafo falha antes de executar quando contém ciclo; diagnostic registra sequência fechada e todos os members | executar até quota; escolher member por hash; cortar ciclo sem mostrar edge de retorno |
| W-845 | quota const | steps, heap, call depth e result usam W-CONST-0003 com quota, consumed, limit e call chain | um code por resource; wall clock como resultado semântico; quota escondida no compiler |
| W-846 | predicate const false | predicate Bool que rejeita argumento estático usa W-CONST-0004 e preserva head, argumento e causa específica | W-CONTRACT-0003; type mismatch genérico; aceitar type e inserir runtime check |
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
| W-864 | fechamento de cobertura R0 | 69/69 requisitos possuem caso estruturado; o gate do repository exige completude e o índice distingue input pronto de estudo executado | deixar o gate progressivo após completar o corpus; declarar ergonomia ratificada pela contagem; omitir formas ainda válidas como alternativas contextuais |
| W-865 | baseline estática R0S | digest do corpus fixa bytes, code points, non-whitespace, linhas e surface lexemes de tarefa e formas; snapshot é reproduzível | contar manualmente; snapshot sem digest; depender de tokenizer remoto para drift local |
| W-866 | limite de R0S | métrica de superfície é descritiva e não escolhe vencedor, não equivale a token de compiler/LLM e não substitui estudo humano ou de modelo | declarar forma menor como melhor; agregar snippets de escopos diferentes; chamar lexeme de token de modelo |
| W-867 | escala de estudo R1 | R0 mede microformas; compreensão, mudança e surpresa runtime usam bundles executáveis do Última Luz com source base, input e outcome iguais; somente a construção estudada muda | extrapolar preferência de snippet; remover contexto da alternativa; usar programa diferente para cada forma |
| W-868 | schema de bundle R1 | bundle fixa source base, casos R0, variantes distintas, inputs, outcomes, quatro tarefas, ordens, blinding, oracle, digests e estado de evidência | prompt solto; variante sem source; input implícito; ordem fixa; metadata revela a forma; resultado sem toolchain |
| W-869 | seed R1 de controle | scanner de carrier do Última Luz compara labels estruturados e flags mutáveis; duas variantes W fazem parse e dois inputs coincidem no oracle host; execução W permanece ausente | medir snippets R0 como programa; comparar W com C de escopo menor; chamar simulação host de runtime W |
| W-870 | máquina de memória M1 | bindings apontam para payloads; PlaceId usa root e projections; LoanId registra mode, origin, stability e parent; move/drop preservam payload e pin separa handle de payload | owner como Boolean; place sem root; loan sem token; pin copia valor; endereço pertence ao binding |
| W-871 | forma do corpus M1 | casos ligam owner, overlap, reborrow, origins, escapes, await, pin, FFI, representation, ABI e join ao Última Luz; snapshot guarda traces byte-exact; W-917 e W-918 fixam a revisão corrente | exemplos isolados sem state; caso sem source; apenas success; resultado sem trace |
| W-872 | limite de M1 | máquina tabelada e teste Node pequeno são oracles host distintos; modelam outcomes de allocation, shared/weak e Arena (historically region) sem executar allocator, atomics, destructor graph, panic, happens-before ou cancellation física | declarar verifier implementado; reduzir memória a M1; chamar outcome lógico de allocator real; apagar segundo oracle por duplicação aparente |
| W-873 | máquina de execução E0 | grafo separa lifecycle da task, sequência local e edges de publicação; cancelamento não cria happens-before | usar ordem do scheduler como semântica; publicar por cancel; observar outcome antes de cleanup |
| W-874 | corpus E0 | 57 sequências e 527 operações ligadas ao Última Luz cobrem lifecycle, cancelamento, fail-fast, drain, races, modification/seq-cst order, RMW, CAS, fences, extent, exclusividade, subtrees de ticket e 10/10 origens happens-before | apenas casos aceitos; evento sem source; atomic acquire sem relação observada; trace completo repetitivo |
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
| W-886 | corpus R1 ampliado | 21 bundles, 51 variantes e 84 tarefas cobrem os domínios R1 atuais com source base, inputs, digests e oracle; 32/69 casos R0 foram promovidos | extrapolar R0; variante sem contexto; outcome não fixado; chamar host oracle de execução W |
| W-887 | estudo R1 de units | `<unit-expression>` e `[unit-expression]` preservam cálculo; a forma square faz parse como indexação e não é quantity semântica vigente | comparar snippets sem fórmula; tratar parse como type-check; escolher por contagem de caracteres |
| W-888 | estudo R1 de imports | flattening e module binding continuam válidos; estudo mede colisão, provenance, recall e mudança antes de recomendar estilo por contexto | proibir uma forma antes do estudo; comparar conjuntos de imports diferentes; omitir colisão preparada |
| W-889 | estudo R1 de fail-fast | tuple await e espera lexical preservam application error; oracle mede observation tick e cancelamento como diferença estudada | mudar o error esperado; depender do scheduler host; confundir latência observada com ordem semântica universal |
| W-890 | cobertura total de ausências | cada alternativa do ledger declara se muda source; toda ausência comparável liga forma recusada, substituição W, diferença observável e caso R0 antes do freeze | tratar 69 requisitos atuais como auditoria total; listar nome sem source; exigir caso de alternativa interna sem diferença visível |
| W-891 | catálogo std SDK0 | profiles cobrem 285 exports em 18 módulos, 14/14 requisitos contratados e oito carriers (2/8 missing: Blob e FormData); todas as declarations estão draft-ready; scan compara 67 superfícies qualificadas; `std.build.Context` é draft e `std.build@1` continua missing; 12/12 providers continuam missing | contar arquivos como cobertura; inferir API sem scan; tratar provider missing como execução; duplicar o grafo de readiness; omitir carrier ou provider ainda sem execução |
| W-892 | context de host | context público é struct nominal encapsulado sobre provider interno versionado; entry fornece owner e interface lógica esconde RuntimeContext e storage; build Context e HTTP Context mantêm interfaces separadas | existential universal; object com identity; mapa ambiental; singleton; syntax especial por SDK |
| W-893 | build Context | read usa overloads `Input<String|Bytes>` const e limite efetivo; write usa overloads `Output<String|Bytes>`, consome value e possui effect linear por output; codecs são UTF-8 estrito ou bytes identity; `.codec` ocorre somente em `read(Input<String>)`; bounds menores do provider vêm do host profile/toolchain plan e entram na recipe key; operações concorrentes exigem bindings distintos; cancellation invalida a tentativa; o host publica um action-result/manifest atômico após success | filesystem sandbox como API; intrinsic genérico; codec universal; overwrite concorrente; output incremental implícito; Context apagado; commit/rollback ou transaction no handler; duplicate catchable que ainda publica |
| W-894 | superfície Web | client e server compartilham Headers ordenado, Request e Response move-only, URL tipada, BodySource fechado em quatro cases, Body consuming e clone bounded; errors, guards, transfer e commit são tipados; `Context` e `serve` são extensions, com serve usando carrier `std.net`; provider único `std.http@1` continua missing | API HTTP paralela; copiar JavaScript/Web IDL; BodyInit universal com `T??`; aliases `path`, `query` ou `decodeJson`; clone sem bound; constructors `Response.text`, `.bytes`, `.stream` e `.html`; Blob/FormData parcial |
| W-895 | profile WinterTC | `web-common@2025` fixa e classifica o Minimum Common Web API como exact, adapted, extension, browser-only ou não aplicável; W não declara conformidade ECMAScript | alegar conformidade formal; copiar `globalThis`; seguir living surface sem snapshot; chamar extension de API portátil |
| W-896 | URL portátil | `URL` guarda record canônico opaco; getters textuais são views O(1); base é `ref URL`; mutation usa errors tipados; `searchParams()` devolve snapshot owned atual; edição `inout` fallible e scoped faz commit único e distingue query ausente e vazia; `URLSearchParams` preserva ordem, repetição, form encoding e sort UTF-16; WPT fecha o provider | doze Strings owned; params eager; cache interior; callback para leitura; property Web com allocation escondida; `SameObject`; parser parcial no contrato; aliases HTTP; silent no-op; alias mutável escapável; URLPattern no SDK0 |
| W-897 | intrinsic interno da std | `foreign intrinsic from "provider@major"` é primitive unsafe não exportável, restrita a módulos internos do SDK; manifest versionado fixa signatures, effects e gates; wrapper W safe contém a boundary; não existe ABI pública nem capability implícita; bootstrap usa allowlist e os mesmos digests | foreign symbol comum; `fn<C>`; annotation nova; provider ambiental; declaration por package; intrinsic público; authority por nome |
| W-898 | ReadableStream portátil | owner move-only atende diretamente a `Stream`; erasure interna pode usar box/indirection, SBO ou monomorfização com witness exato; `next` é o cursor único; `cancel` segue W-330, com handle inert antes de success, Failure ou task cancellation, sem owner restaurado e com drain estruturado; drop é idempotente e best-effort; BYOB é `ByteSource.read` sobre `Bytes` growable; sem prefetch, controller, reader object, `IncomingBody` público ou `any`; tee exige `Duplicable`; o genérico limita lag em itens e depende do allocation budget, sem promessa de memória transitiva; o overload de bytes limita lag exato e serve clone HTTP; COW preserva independência; branch drop não cancela a irmã; cleanup e pull upstream ocorrem uma vez; pipe fica direção até fechar writable/transform; provider continua missing | runtime de stream paralelo; façade declarada zero-cost; witness apagado sem prova; `getReader`; `IncomingBody`; rollback em catch; retry de cancel; resource owner dentro de error duplicável; lock dinâmico em safe W; tee Web unbounded; item count chamado de byte/memory bound; `sizeOf`, callback de custo ou medida transitiva; tee zero como rendezvous implícito; chunk copy obrigatório; BYOB por ArrayBuffer ou fixed-buffer identity; HWM oculto; pull reentrante; task detached; `pipeTo` antes de WritableStream; cancel como error de task; clone HTTP com pump próprio |
| W-899 | AbortSignal portátil | `std.abort` adapta o first-wins Web sem substituir a cancellation monotônica de W; `AbortReason` é `Error & Copy`, fechado e bounded; signal duplica handle O(1), devolve reason por valor, oferece `throwIfAborted` tipado, espera pelo reason sem perder wake, não concede authority e não é `WireValue`; controller é move-only, first-wins atômico e drop não aborta; timeout zero já está abortado, timeout positivo possui timer-resource independente do creator/root, continua ao escapar e cobra o execution domain sem manter task viva; falha de timer budget publica cancellation e solicita cancellation estrutural; `any` preserva o nome Web e valida antes de registrar: argumentos diretos e folhas pending únicas após flatten/dedup precisam caber no mesmo `maximumSources` por result, inputs abortados só vencem depois das duas validações e cada folha pending recebe uma registration; total vivo do execution domain depende do allocation/admission budget do provider, não do fan-in de uma call; o DAG não cria cycle; Request recebe signal interno dependente; error Web versus task cancellation segue settlement/commit e sempre drena I/O; RPC geral usa automatic call cancellation, Request usa control frames e live-control edge fica alternativa futura; provider continua missing | colocar em `std.runtime`; transformar cancellation de task em application error; reason dinâmico, message, error arbitrário ou resource owner; renomear `any` como `combining`; EventTarget e callbacks no SDK0; signal com authority; controller duplicável; abort implícito no drop; ligar timeout genérico ao creator/root; wall clock; timer ou observer sem bound; tratar fan-in de uma call como limite global de dependents; validar winner antes dos bounds; permitir que terminal direto ou `any` pending contorne limite; ordem total fictícia para races; handle como `WireValue`; AbortSignal remoto geral no SDK0; implementação safe W sem atomics e hooks provados |
| W-900 | JSON bounded SDK0 | `std.json` fornece `Encodable`, `Decodable`, `Codable`, `Limits` Copy/Equatable com defaults finitos, profiles `.interoperable`/`.rfc8259`, `ValueKind`/`ValueConstraint`, errors tipados com `Location`/`SyntaxKind`, `typeMismatch` e `invalidValue`, cursors `Writer`/`Reader` opacos e scoped com callbacks `some take fn`, `Number` nominal validado, `Value` sum type explícito, Object equality map-like com insertion order preservada no re-encode, synthesis somente por conformance JSON fechada (Array/fixed array/Option/Map<String,V>; sem tuple), unknown policy explícita e duplicate rejection; encoder compacto define escapes, shortest-round-trip e signed zero sem alegar canonical JSON; HTTP usa `json.*` e exige `maximumBytes` ou `json.Limits`; adapters direcionais de Command/AppResponse/WifiSession estão no produto de referência; provider `std.json@1` continua missing | serializer universal, reflection, `Any`, annotation, macro, metatype, cursor escapante, route unlimited, duplicate last-wins, NaN/Infinity, conformance Codable global de domain types ou codec automático para Display/outros schemas; chamar o output de JCS ou identity de signature/content |
| W-901 | HTTP SDK0 | um provider `std.http@1` possui handles privados para Request, Response, body, Context e serve; owners são move-only e consuming operations inert antes de suspension/outcome; BodySource aceita somente String, Bytes, URLSearchParams e ReadableStream; RequestInit e RequestOverride separam defaults, inherit e none; `RequestIntegrity` separa inherit/clear; policies são enums fechados com Priority high/low/auto e destinations Web exatos; clone usa tee bounded; Response status 0..<600 e constructors normais 200..<600 rejeitam 204/205/304 body; JSON compõe `bytes`/`json.decode` e `json.encode`/`Response(Bytes)` com `ref Value`; Context nominal process-local expõe random, databases, caches, templates e signal por identity const, registries infallible e `some` owners; TemplateBinding fixa limits/version para extensão host provisória; serve usa `net.ListenAddress`/`ref net.Network` e agora possui declaration draft, mas depende dos providers `std.net@1` e `std.http@1` missing; erros de adapter distinguem 400/422/403 e usam RFC 9457 Problem Details; Blob/FormData continuam profile-final; AppResponse usa adapter borrowed síncrono | BodyInit universal com `T??`; clone sem bound; Context ambiental/string lookup/runtime failure; existential `any`; intrinsics genéricas JSON; template irrestrito; HTTP address/network declarations; Priority.normal; `integrity: String?`; mutation direta de Headers; URL overloads indistinguíveis; Blob/FormData parcial; JSON lossy para Quantity/SI; envelope genérico que mistura decode e domain errors; claims de execução sem provider |
| W-902 | rede SDK0 | `std.net` é módulo T1 com provider intrinsic único `std.net@1` missing; `Network` é capability nominal move-only sem initializer público e as APIs públicas borrowam HostName, Endpoint, ListenAddress e SocketAddress; AddressFamily, Ipv4Address, Ipv6Address, IpAddress, SocketAddress, HostName, Endpoint e ListenAddress são values tipados com bounds DNS/port, UTS #46 nontransitional, STD3, IDNA2008, trailing-dot único, ASCII lowercase e RFC 5952; `SocketAddress` exige port e aceita somente IPv4 `192.0.2.1:443`, IPv6 `[2001:db8::1]:443` e scope IPv6 dentro de brackets, como `[fe80::1%3]:443`; hostname e scope zero são rejeitados; parse/format de IP e socket são estritos, canônicos, fazem round-trip e não resolvem DNS; resolve usa ResolveLimits, port remoto `1...65_535` e connect usa RFC 6724/RFC 8305 bounded com attempts `1...16`, `fallbackDelay` não negativo e finito; cada IP é revalidado imediatamente antes de cada tentativa; calls por borrow em Network criam state independente e podem coexistir sem conflito de capability/policy; calls `mut async` mantêm borrow exclusivo até completion ou cancellation drain; TCP full duplex concorrente exige `split()` síncrono, e `finishWriting()` e `TcpWriteHalf.finish()` são `take async` consumidos antes da suspensão e em todo outcome; EOF é sticky, errors latched são observados antes da próxima operação e reset/abort/disconnect são terminais; drops só ocorrem após drain de borrows, limpam residual e liberam uma vez; UDP serializa no mesmo socket no máximo uma receive ou send em voo e mantém truncation explícita; NetworkLimitKind, `limitExceeded(kind, maximum)` e `messageTooLarge(maximum)` são portáveis; http.serve usa `net.NetworkError`, o carrier `sdk0-net-listener` está draft e o provider `std.net@1` continua missing; gates cobrem parse/format, differential targets, capabilities/SSRF, cancellation, partial I/O, fault injection, sanitizers, leak, limits e fuzzing | socket global, constructor de capability, network String names, raw sockets, fd inheritance, socket-option escape hatch, multicast/broadcast, Unix sockets, named pipes, TLS/STARTTLS, QUIC, WebSocket ou interface enumeration sem contracts próprios; tratar deadline de task como NetworkError.timedOut; prometer reliability, ordering ou congestion control no UDP; prometer cancelamento físico de syscall em target sem suporte; prometer split direcional ou protocolo genérico de datagram no SDK0 |
| W-903 | Quantity/SI | dimensões normalizam para IDs-base e expoentes; `std.si` fixa `m`, `kg`, `s`, `A`, `K`, `mol`, `cd`, Angle usa `rad` como eixo forte W e Temperature point/delta usam K; cada dimensão customizada exige exatamente uma declaration `unit name: Dimension`, independente de source/file order; `Quantity<D, R>` guarda somente magnitude em R; aliases preservam D; linear, affine point e affine delta são identidades distintas; scale/offset são rationals exact; integer exige resultado integral/in-range checked; float usa coeficiente nearest-even, multiply strict e affine multiply-then-add; `canonicalValue`, `value(in:)`, `exactValue(in:)` fallible sem rounding e `try value(in:, rounding:)` formam a surface explícita; layout é o de R sem metadata por value e não promete ABI/FFI; optimizer preserva dimensão, rounding e strict float; wWire carrega somente R e inclui dimensão normal, kind, reference, R, refinements e validation no `WireSchemaDigest`; JSON escolhe schema fixo `{value, unit}` na ordem value/unit, exige token exato, recomenda UCUM e decimal canônica, sem `json.Codable` genérico; Last Light usa `quantity_oracle.w` para 30 s/0.5 min, affine points, bits IEC e schemas `s`/`J`; providers `std.json@1` e wWire de produção continuam missing | **Alternativa:** field name com unit embutida para APIs compactas; **Pesquisa:** optimizer/vectorização e providers; **Rejeitado por enquanto:** `{value, arbitraryUnit}` com conversão dinâmica, level/log em Quantity linear, registry runtime e metadata por value |
| W-904 | texto inteiro canônico | integers fixed, `Int` e `UInt` atendem `Display` em decimal ASCII sem locale, plus ou leading zero; zero usa `0`; signed negative usa um `-`; signed minimum é formatado sem overflow intermediário; `T.parse(view String)` aceita decimal estrita, `+` opcional, `-` somente signed, faz range check e retorna `empty`, `invalidSign`, `invalidDigit(byteOffset)` ou `outOfRange`; schema valida `parse` e igualdade byte a byte com `Display`; radix APIs continuam alternativa futura | locale numérico, parsing permissivo com whitespace/radix/fraction/exponent, formatar signed min por negação intermediária, canonizar por runtime value |
| W-905 | Duration exata | `Duration` continua signed i128 nanoseconds com layout opaco; getter read-only `nanoseconds: i128` e constructor total `Duration(nanoseconds: i128)` expõem o valor sem expor layout; refinements exigem narrowing checked em input runtime; JSON genérico de Duration fica rejeitado e cada endpoint escolhe schema; Wifi usa String decimal `remainingNanoseconds` | layout público, float/infinity, narrowing implícito, serializer Duration universal |
| W-906 | adapters direcionais de JSON | domain types não conformam diretamente `json.Codable`; cada endpoint possui adapter endpoint-owned/dedicated inbound somente `json.Decodable` e outbound somente `json.Encodable`; o módulo pode exportar a plumbing necessária ao host sem criar `json.Codable` no type de domínio ou um contrato global; schemas/versionamento ficam locais sem reflection, annotation ou conformance global; unknown members reject e duplicate sempre falha; IDs u64/u128, Money i128, Duration nanoseconds e completedOrders u64 usam decimal String canônica com parse/display no carrier explícito; u16/u32 usam JSON number; tokens são explícitos ASCII kebab-case; encoder põe `kind` primeiro e `notes` usa null; borrowed AppResponse adapters encodam sem mover/copiar o modelo; Quantity usa adapters nominais fixos para tokens de unit; Problem Details usam `code` estável e status derivado do code; Command, AppResponse e WifiSession têm source oracles provider-gated | conformance direta de domain type congelando um schema global, reflection/universal serializer, tokens derivados de source names, shape dependente de runtime value, envelope genérico que mistura decode e domain errors, status/code livres, unit String arbitrária |
| W-907 | kernel M1 de place loans | HIR usa PlaceId root estável + projections de field, tuple, enum payload, index, range normalizado `[start,endExclusive)`, dereference e view extent; LoanId registra place, mode, origin, emissão/fim, estabilidade e parent; move/drop do root observa descendants | contador global de borrow, place textual sem root, partial move, lifetime metadata no value |
| W-908 | overlap e mutation | paths iguais ou prefixos sobrepõem; fields conhecidos distintos, índices constantes distintos e ranges separados são disjuntos; ProofFacts atuais só refinam index, range e view e identificam os prefixos PlaceId exatos; active variant identifica enum place e case; enum variants distintos, dynamic index/range, deref opaco, union, packed, unaligned, opaque e foreign boundary sobrepõem; mutation estrutural conflita com todo storage | alias por nome, fact sem root/path, guardar ProofFacts no loan, aceitar `end` na HIR, prova limitada por budget, tratar deref ou union como disjunto |
| W-909 | reborrow | shared nasce de shared ou exclusive; exclusive nasce só de exclusive; child deve ser igual/subplace descendente, congela acesso direto do parent e o restaura no end; cópia shared de child preserva parent e conta outra obrigação; `accessLoan` lê shared e lê/escreve exclusive não congelado; children disjuntos coexistem | parent ativo consumível, cópia de child sem parent, exclusive de shared, sibling/widening reborrow, reborrow que usa annotation no source |
| W-910 | valores lifetime-dependent | cada payload possui edges individuais shared/exclusive para owner payload/place ou owner slot de interface; OriginSet é projeção deduplicada, mas duplicatas bloqueiam; composição inclui tuple, struct, object move-first, enum/payload, Option, Array, collection, closure e existential; erasure não apaga edges; stored fields são permitidos sem ARC ou referent ownership | origin sem owner identity, rejeitar stored field local, apagar provenance por erasure, metadata de lifetime em runtime |
| W-911 | containers de refs | Array<ref T> possui owner de descriptor/storage e edges para cada referent; insert/join adiciona edges; join lê source distinto e self-join exige snapshot; remove só reduz quando nenhuma duplicata resta; clear libera edges; o grafo simbólico não limita quantidade por proof budget | join sob loan exclusive, self-join implícito, contador fixo de origins, invalidar por budget, tratar descriptor como único owner |
| W-912 | escapes e await | `.lifetimeIndependent` é ausência de origin dinâmica; static/immortal passam somente esse gate; external escape rejeita edge dinâmica; channel/task exigem transferability, service exige WireValue + transferability mesmo local, persistence exige schema e foreign retention exige FFI; await resolve cada referent vivo e estável, com no-conflict e cleanup/cancel drain | origin immortal como authority de boundary, cópia implícita para escapar, task detached com borrow, usar estabilidade do aggregate para referent, annotation de lifetime como correção |
| W-913 | pin e self-reference | pin exige zero LoanId e zero dependency edge dirigida ao payload; payload pinned tem root estável distinto do handle; mover handle é permitido com obrigação ativa, drop de handle/payload falha, mover payload não; initializer self-referential safe é rejeitado; construção pinned dedicada é alternativa futura | self-reference por initializer comum, unpin implícito, pin que apenas marca pointer |
| W-914 | provenance de interface | body infere mapping exato e separado para cada result dependency slot e slot ausente falha; sem body instance usa receiver compatível e init/static/free usam todos inputs compatíveis por slot; zero input só aceita result independent/static; import expectation e SemanticInterfaceKey do provider coincidem; oracle ignora inferredMapping bodyless; witness e lock detectam mudança | key opcional unilateral, igualar interfaces próprias de módulos distintos, `ref<sources: ...>` no source, colapsar result slots, mapping conservador apagado, witness divergente, docs no semantic key |
| W-915 | FFI de refs | safe ref/inout para C é call-scoped/noescape; retenção exige owner/lease pinned, destroy e unregister; opaque C return, packed, unaligned, union e opaque permanecem conservadores; fn<Language> passa lifetime somente com adapter W confiável | pointer persistente sem lease, free por caller, inferir lifetime de header ou body opaco |
| W-916 | cleanup e diagnostics M1 | deinit/cleanup preserva edges usadas pelos fields; NLL termina no último uso sem deinit observável; diagnostics distinguem overlap, dependency conflict, dependent escape, unstable referent, unstable suspension e frozen parent e sugerem materialize/copy/take, split/clear, reorder ou pin | hidden runtime lifetime, uma mensagem genérica, fix-it que inventa annotation |
| W-917 | endurecimento executável M1 | schema M1 fixa 165 casos e 580 operações; fecha subplace reborrow, child copies, owner access, ProofFacts ligados ao PlaceId, dependency authority, borrow/storage origins, Arena budget/close (formerly region), rehome, shared/weak lifecycle, erasure inline/spill, alias borrows, failure consuming, boundary gates, interface mappings, referent await, pin, cleanup e adapter W; preserva owner, representation, allocator e WAbiKey | aceitar origin implícita, fact sem place, endereço do aggregate como prova, share reparar borrow, mobility declarada na call, self-proof estrangeira, duplicar check M0, chamar oracle de compiler/runtime |
| W-918 | authority de dependency edge | cada edge é obrigação de lifetime e capability; shared permite read; exclusive permite read/write; criação valida loans e edges de modo atômico; IDs são únicos; selector usa ID xor origin e a abreviação exige origin única | edge apenas como bloqueio; write por shared; origin first-match; dois selectors; conjunto parcialmente criado após conflito; operação source `accessDependency` |
| W-919 | estudo R1 de contratos sequenciais | `StagePath` compara `StaticList<T><(predicate)>` com type e predicate fundidos em static list; source, validator, inputs e outcome permanecem iguais; a forma fused faz parse, mas é semanticamente rejeitada | snippet isolado; mudar o algoritmo; tratar static list como lista universal de constraints; chamar oracle host de evaluator W |
| W-920 | cobertura de promoção R1 | índice e checker contam IDs R0 únicos ligados a bundles; 32/69 mede planejamento, não evidência humana, de modelo ou runtime | contar referências duplicadas; dividir bundles por requisitos; chamar promoção de ratificação; esconder o denominador |
| W-921 | inversão semântica de contrato fused | S0 compara `StaticList<T><(predicate)>` com `StaticList<[T, (predicate)]>`; a segunda forma faz parse e falha com W-CONTRACT-0002 no slot `T` antes de resolver o predicate | rejeição somente em prosa; W-CONTRACT-0005 no envelope errado; interpretar lista como constraints; emitir erro secundário de `.member` |
| W-922 | diagnostic de receiver consuming | place owned e movível em member `take fn` exige `(take receiver).member()`; call sem marker produz W-OWNERSHIP-0011 com place/type/category antes do move e não recebe fix automático; receiver não owned falha pela incompatibilidade anterior | inferir take pelo member; consumir e continuar checking; chamar todo receiver de binding; inserir fix que muda ownership; restaurar owner no error |
| W-923 | estudo R1 de receiver consuming | `CommandStream.finish()` compara marker explícito e consumo inferido com source idêntico fora da call; success e error deixam owner indisponível no modelo hipotético; S0 rejeita a forma implicit | comparar APIs diferentes; omitir error; usar owner depois da call válida; chamar host oracle de runtime W |
| W-924 | formatter de receiver consuming | F0 preserva `(take stream).finish()` e prova CST equivalente; os parênteses pertencem ao operand de ownership e não são style opcional | remover grouping; formatar como `take stream.finish()`; mover `take` após member lookup; snapshot sem decisão |
| W-925 | ausência de domain default | header de módulo e execution profile não publicam `parallelDefault`; `spawn` e `parallelMap` exigem domain no call site; S0 preserva a rejeição do slot de módulo com W-CONTRACT-0001 | import escolhe executor; default em cada módulo; módulo concede budget; duplicar profile em source |
| W-926 | estudo R1 de domain (retired) | decisão anterior tratava `<domain: .compute>` como variante de schema; W-1160/W-1162/W-1172 tornam as duas formas equivalentes e preservam o dispatch sem prometer simultaneidade | tratar domain como thread; aceitar alias duplo no mesmo slot; capacity um invalida spawn; chamar oracle de scheduler |
| W-927 | formatter de domain (retired) | F0 preserva a forma positional ou named escrita e a HIR normaliza ambas; spacing e statement boundaries ficam canônicos | apagar label; inserir label; reescrever domain como frase `on`; inferir pool no formatter |
| W-928 | proveniências de borrow e storage | `OriginSet` mantém dependency edges; `AllocationOriginSet` mantém allocator instance, lifetime, mobility, deallocator e adoption family; move transfere ambos, mas nenhum substitui o outro | um origin set universal; allocator como borrow comum; metadata em pointer; lifetimeIndependent apagar storage origin |
| W-929 | criação de shared | `share` e `tryShare` exigem payload lifetime-independent, preservam origins internas e criam origin própria para control block; storage interno precisa sobreviver ao block; shareable só é exigido na fronteira paralela | share prolonga borrow; shareable repara lifetime; ARC universal; control block sem allocator origin |
| W-930 | falha de operação consuming de storage | failure de `pin`, `share`, `tryShare`, `rehome` e `erase` consome e destrói o source uma vez, limpa destino parcial e não publica handle/address/existential; `share` escala por panic e recovery exige operação/outcome explícito | restaurar binding implicitamente; source parcialmente válido; leak do destino; publicar pointer ou existential antes do success |
| W-931 | composição strong/weak | último strong executa deinit uma vez; weak mantém somente control block e allocator origin; upgrade após strong zero devolve none; último weak libera block; borrow shared fica ligado ao strong handle de origem; `inout` exige owner único; cross-domain exige payload shareable, contador thread-safe e todas origins móveis | weak acessa payload sem upgrade; borrow ligado ao contador global; alias sem relação bloqueia drop; ressurreição; contador local cruza domain; shareable ignora allocator mobility |
| W-932 | interface de storage owned | `AllocationOriginMap` liga paths de storage do result a allocator inputs, default do product ou runtime owner; ele é separado do borrow mapping e participa da SemanticInterfaceKey | esconder lifetime do allocator; colocar mapping somente em docs; tratar owned result como lifetime-independent por definição; expor mapping oculto na C ABI |
| W-933 | expansão de composição M1 | a tranche adiciona 21 casos e quatro testes independentes para budget/close, rehome, local versus cross-domain, share dependent, failure consuming, lifecycle strong/weak, borrows por handle e interface storage; W-938 estende o snapshot corrente | exemplo sem state; somente success; simular thread scheduler; chamar origin lógica de allocation física |
| W-934 | fronteira do design freeze | contratos, alternativas, exemplos, modelos host, vetores e spikes descartáveis fecham design; formatter, checker, HIR, scheduler, runtime, providers e compiler de produção começam depois e podem reabrir uma decisão por evidência | exigir implementação ampla para definir a linguagem; chamar oracle de produto; congelar sem modelo adversarial; impedir revisão após evidência real |
| W-935 | auditoria de decisões para freeze | R0 classifica o eixo source; F0/S0/M1/L0/E0/B0/P0/TAB0/DLPack podem ligar decisões ao eixo oracle; decisões mistas declaram todos os eixos obrigatórios; as demais exigem escolha interna, fallback provável, histórico, policy ou waiver; o índice publica a contagem corrente e `--require-complete` permanece desligado até o gate | tratar 68 casos como auditoria do ledger; classificar por keyword; ausência de entrada significar aprovação; somar eixos sobrepostos como decisões distintas; aceitar um único eixo para decisão mista; manter planilha manual fora do repository |
| W-936 | estudo R1 de callables | três variantes completas comparam representação separada, callable universal e protocols nominais; outcomes do restaurante coincidem, enquanto dispatch, custo, consumo e recovery de erasure ficam observáveis; promove três casos R0 | snippet sem capture; comparar somente tokens; esconder segunda call; chamar host oracle de execução W |
| W-937 | storage de erasure | `any P` e `any fn` usam policy versionada de inline/spill; contextual erasure segue OOM normal; `try erase(take value, using:)` é consuming e fallible; box adiciona AllocationOriginMap; `some` e `ref any` não alocam só por opacity; M1 fixa inline, spill, failure, dependency e interface mapping | box universal; SBO ambiental; esconder allocator origin; restaurar source na falha; carrier existential em C/wire |
| W-938 | erasure executável M1 | oito casos e dois testes independentes derivam inline/spill pela policy, preservam payload origins e dependency edges, adicionam box origin, bloqueiam close prematuro, rejeitam spill proibido, convertem budget exhaustion em failure consuming e não publicam target parcial; snapshot totaliza 165 casos e 580 operações | escolher storage por flag do caso; apagar origins; allocation em inline; source restaurado; target parcial; budget rejeita antes do consumo; chamar layout lógico de ABI física |
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
| W-972 | low-ceremony sem dynamic core | defaults, keyword arguments, unpacking, comprehensions, generators, collections e display são ergonomias para estudo R1; carriers exploratórios são `json.Value`, schema, `data.Batch<Row>` ou `data.DynamicBatch` explícitos; `Table`/DataFrame completo fica em package first-party; TAB0 definiu o boundary e TAB1 fechou declarations, contracts, oracles e host evidence de CSV/Parquet/Arrow como design; providers e `w-compile`/`w-run` permanecem missing; duck typing, monkey patching, dynamic global object model, GIL, ambient imports e unchecked reflection não entram | `Any` universal; object model dinâmico; reflection unchecked; import ambiental; decidir todas as ergonomias como syntax vigente agora |
| W-973 | arquivo único hermético | `w run path/file.w -- <args>` usa somente imports explícitos, a root do script ou package context selecionado, e `entryForm` explícito/implícito; fora de package cria package/product efêmero com std e módulos locais; não faz recursive/cwd/PATH/environment discovery, não baixa remote implicitamente e não deixa estado oculto; dependency form externa permanece **Pesquisa** | manifest sibling, metadata inline e `--with` como forma final já escolhida; scan recursivo; ambient package discovery; top-level execution arbitrário; download implícito; lock sem digest ou provenance |
| W-974 | session transacional e generational | `w repl` usa parser, checker e HIR normais; failed submission preserva a generation corrente; declaration aceita cria generation; dependents invalidados ficam indisponíveis, nunca stale ou implicitamente recompilados; resubmission explícita recria e executa effects; redefinição/reset fecha admission, drena children/waits, encerra loans/views e faz drops E1, rejeitando ou escalando se foreign retention permanece | dynamic mode; replay automático de effects; redefinição que mantém dependents silenciosamente; liberar estado vivo; reset que ignora drain; notebook transcript como source de release |
| W-975 | Jupyter como tooling | PYN3 fecha o adapter Jupyter 5.5 sobre PYN2, `presentation.Presentable`, MIME/data bounded e export `.w`/package sem hidden replay; interrupt solicita structured cancellation; nomes de check/export permanecem **Pesquisa** | Jupyter como linguagem; notebook como artifact/release source default; MIME sem limite; fingir kill de foreign code; replay oculto; segundo session model |
| W-976 | interop científico | Python Array API standard é checklist T2; DLPack e Arrow C Data são adapters first-party T2; Python buffer pertence à bridge; copy, device, stream, ownership, lifetime e release são explícitos e provados | Array API como semântica normativa W; copy implícito; lifetime ou release ambiental; buffer protocol no core; pandas clone na std |
| W-977 | dados exploratórios e tabulares | dados usam `json.Value`, schema, `data.Batch<Row>` ou `data.DynamicBatch` explícitos; DataFrame completo é package first-party antes de std estável; TAB0 fecha o carrier mínimo e TAB1 fecha CSV/Parquet/Arrow workflow; sem clone pandas | duck-typed rows; dataframe universal na std agora; object global dinâmico; schema inferido sem limites; tratar ecossistema como syntax |
| W-978 | ergonomia R1 | R1 compara comprehensions com pipelines/loops, checked broadcasting com broadcast explícito, negative/end-relative indices, unpacking/destructuring, display e labels reordenáveis; labels permanecem em ordem até evidence de ganho | escolher syntax final sem R1; broadcast implícito; labels reordenáveis por default; mudar lookup/reproducibility por conveniência |
| W-979 | gates de latency | `time-to-first-result` e `steady-state` são gates separados; first-result cobre hello cold/warm, edit-run e transaction/redefinition/invalidation de 10 cells; steady-state cobre collection transforms, CSV throughput, tensor elementwise/broadcast/matmul CPU e zero-copy DLPack/Arrow overhead; output/semantics precedem tempo e registram compiler version/target/hardware | número fixo de vitória; benchmark isolado; comparar somente tempo; um runtime obrigatório; backend rápido como autoridade semântica |
| W-980 | Python bridge boundary | W-from-Python e Python-from-W usam stable C/Python APIs e data interchange como bridge; CPython ordinário usa bridge, service ou fault boundary; um adapter AOT resolvido pelo manifest pode expor `fn<Python>` somente com artifact hermético, C façade tipada, runtime/deps fixos e diagnostics, effects e provenance de 19.2; generic adapter permanece sem promessa e sem proibição; lifecycle, GIL e interpreter aparecem no adapter | static lib Python previsível sem adapter; Python runtime embutido no core; lifecycle oculto; foreign code sem fault boundary; proibir todo adapter AOT; converter `fn<Python>` em forma core |
| W-981 | fronteira de evidência R1P0 | R1P0 marca `design-oracle-input`; `tree-sitter-parse` e `host-oracle` são evidência corrente; `w-compile`, `w-run`, estudo humano e estudo de modelo permanecem missing; parse e oracle host não ratificam source nem executam W | chamar parse de conformance; tratar host oracle como runtime; promover forma por digest; registrar participante inexistente |
| W-982 | transformação Python em W | pipeline lazy é a Forma vigente para transformação sem side effect; loop explícito é a Forma vigente para controle e side effects; comprehension Python motiva, mas comprehension W continua Pesquisa; limit zero e no-match preservam `[]`; oracle sidecar registra inspeção bounded | adicionar comprehension à grammar; usar pipeline para side effect; remover limit; comparar inputs diferentes; usar oracle eager |
| W-983 | broadcast de shape | scalar expansion é implícita total; shapes diferentes usam broadcast explícito na Forma vigente; checked implicit é Pesquisa; Julia dotted broadcast é Alternativa documental; NumPy evidencia conveniência, intermediários, memória ineficiente e menor legibilidade em dimensões maiores, mas não é autoridade W; R1 mede a troca | broadcast universal oculto; scalar exige annotation; Julia syntax no parser; ignorar mismatch e axis change |
| W-984 | acesso relativo ao fim | `.last` é Forma vigente, retorna `ref String?` e absorve empty sem guard; arithmetic `count - 1` é alternativa com guard; negative indexing é Rejeitado por enquanto por signed/unsigned, `-0`, empty, bounds e contexto; `get(fromEnd:)` e `suffix` são Pesquisa; C# `^1` é alternativa documental | index negativo sem guard; usar `^1` como syntax W; underflow unsigned; converter empty em panic |
| W-985 | ordem de labels de call | a call é sequência ordenada de labels; overload e initializer selecionam essa forma antes de tipos; ordem de declaração é Forma vigente; default em `currency` cria `majorUnits:,currency:` e `majorUnits:`; overload `currency:,majorUnits:` cria terceira sequência; política unordered colapsa as formas completas e diagnostica antes de types; reordering é Pesquisa/Alternativa | ranking por tipos; dizer que fixed-order é ambíguo; colapsar formas por default ou reordering; alterar resolver no estudo |
| W-986 | tuple destructuring fixo | binding de tuple/struct de shape fixo é Forma vigente; projections `.0`/`.1` preservam uma avaliação e exigem `copy` ou borrow explícito para componente move-only; starred unpacking é Rejeitado por enquanto por ownership, aridade dinâmica e partial moves; `each collection` continua call-rest | reavaliar `word()`; starred na grammar; tratar `each` como destructuring; mover tuple parcial |
| W-987 | corpus R1, contagens e limites | o corpus tem 69 casos R0, 21 bundles, 51 variantes, 84 tarefas e 32/69 promovidos; R0S deriva sua contagem de formas por script; bundles fixam primary/adversarial inputs, digests, counterbalancing, blinding e evidência missing | contar manualmente; promover por referência duplicada; omitir adversarial; declarar participante ou modelo executado |
| W-988 | carrier Batch mínimo | `data.Batch<Row>` é finito, owned, columnar, imutável após publicação, schema fechado, row count comum e vazio válido; schema sem fields exige row count explícito; payload publica somente depois da validação | `Table<Row>` estável; DataFrame no core; colunas com counts diferentes; batch vazio como erro; mutação depois da publicação |
| W-989 | DynamicBatch e Array<Row> | `data.DynamicBatch` pode publicar schema runtime; binding tipado explícito valida antes de publicar o `data.Batch<Row>`; `Array<Row>` continua válido para algoritmos row-centric e é rejeitado como carrier universal; DataFrame completo é package first-party | duck typing; `Any` carrier; Array como coluna universal; DataFrame estável na std |
| W-990 | trigger de data.Row | `struct X: data.Row` ativa synthesis por identidade do protocol, struct-only, all-or-none e stored instance fields em declaration order; witness manual e DTO dedicado continuam | annotation genérica; macro; nome textual; synthesis parcial; reflection como trigger |
| W-991 | limites da synthesis Row | schema identity inclui field name, order, logical type, nullability, refinement e semantic extension; unsupported resources e top-level nullable rejeitam; conformance não cria codecs ou ABI schema | aceitar `Any`, service/object identity, task/channel/lock, pointer/ref/view, closure ou foreign handle ilimitado; JSON/CSV/Arrow automáticos |
| W-992 | nullability e names do carrier | `Option` define null; NaN é valor; row index não é especial; field names são UTF-8, nonempty e unique; duplicate/empty externo exige mapping ou rejeição | sentinel row; NaN como null; last-wins; rename silencioso; lookup por nome duplicado |
| W-993 | seleção typed de fields | descriptor gerado e enum-like shorthand selecionam field estático em O(1); dynamic usa nome e binding explícito; read checked de `f64` Copy devolve valor; fields non-Copy ficam em TAB1/**Pesquisa** sob W-420; reflection e String lookup não entram no hot path | reflection unchecked; String lookup estático; acesso sem bounds; `XView` automático; materialização oculta |
| W-994 | acesso e encoding | typed Batch valida upfront, oferece random value O(1), selection O(1) e scan O(rows); run-end é materializado para `plain` com provenance antes da publicação ou falha; layout físico é opaco | scan para cada access; publicar run-end como O(1); ABI derivada do layout; copy silencioso |
| W-995 | copy, device e conversion | `.never`, `.ifNeeded` e `.always` governam payload copy; target device é explícito quando há transferência; sem target fica no device atual; `.never` falha quando target exige transferência; source usa `copyPolicy:` porque `copy` é keyword; `CopyPolicy` é o contrato lógico; conversion lossy, narrowing, unit/timezone, missing/extra/reorder/type change exige mapping explícita; default é exact schema | policy escolher device; transfer implícita; narrowing silencioso; reorder automático; converter por heurística; device mismatch aceito em `.never`; `copy:` como label |
| W-996 | schema estável em stream | `Stream<Batch<Row>, E>` mantém schema identity em todos os chunks; schema change é typed error e nunca union ou promotion silenciosa | union midstream; promotion de nullable/type; schema por chunk sem identidade |
| W-997 | owner e release foreign | import possui um owner; release ocorre exatamente uma vez após views, waits e children drenarem; owned export transfere responsabilidade e torna o owner local `transferred`; borrowed export é scoped; novo owner completa release; C Data exige trusted producer e validação estrutural | release duplicada; release local após transfer; view após release; owner global; borrowed export escapante; raw pointer sem owner |
| W-998 | trust boundary e sanitização | untrusted input valida counts, buffers, offsets, lengths e nesting/bounds antes da publicação; UTF-8 é validado quando a column declara essa codificação; validity + physical values exigem bytes zero/initialized nos nulls antes de boundary | decode ilimitado; publicação parcial; UTF-8 obrigatório em numeric/binary; null físico não inicializado atravessando boundary; confiança por endereço |
| W-999 | limits e arithmetic | limits cobrem rows, columns, fields, buffers, total bytes, allocation bytes, nesting, metadata bytes, string bytes e chunks; overflow e counts reificados falham antes de allocation/publicação | limits somente de rows; overflow depois de allocation; quota implícita; chunks ilimitados |
| W-1000 | Schema identity e extensions | `data.Schema` separa identity semântica de metadata bounded; names/order, type/nullability/refinement/extensions formam identity; cada extension nominal possui ID estável, versão e parâmetros canônicos bounded sem registry ambiental; extension sem adapter é opaque/dynamic e não bind nominal; physical layout não entra | metadata alterar identity; registry ambiental; extension universal; adapter implícito |
| W-1001 | fronteira TAB1 de adapters | TAB0 definiu o boundary; TAB1 fechou declarations, contracts, oracles e host evidence para CSV, Parquet e Arrow como design; DLPack é adapter tensorial em bundle próprio, com classificação tensor vs tabular explícita e nunca carrier tabular; providers e `w-compile`/`w-run` seguem missing sem mover semântica para formats | CSV/Parquet/Arrow definir Row; DataFrame como carrier; DLPack tabular; signature final neste bundle |
| W-1002 | máquina host TAB0 | `tabular-carrier-machine.mjs`, 64 cases, 155 operations, 22 accepted e 42 rejected, snapshot e host tests modelam invariantes de publication, binding, chunks, owner, trust, sanitização e limits sem executar W; casos positivos e negativos evitam tautologia | chamar host oracle de compiler/runtime; snapshot manual; modelo sem adversariais; testar somente happy path |
| W-1003 | estudo R1 tabular | bundle fixa source base, três variantes, mesmos telemetry summary/output, adversariais, quatro tasks, orders, blinding, digests e evidence missing; `typed-batch` é Direção | study de snippet; input implícito; variante caricata; declarar participante/modelo inexistente |
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
| W-1024 | Parquet source | decode recebe `take Source: SnapshotByteSource`; positional footer/row-groups/column-chunks/page access não usa cursor | ByteSource, ambient file seek ou dataset directory discovery |
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
| W-1043 | host evidence TAB1 | `tooling/tabular-adapter-machine.mjs`, 84 cases/184 operations (35 accepted, 49 rejected), checker, JSONL snapshot e host tests derivam state/identity/progress/provenance/tokenizer/footer/page/IPC/ownership/quota; cada case liga símbolo real do Last Light; não compilam ou executam W | expected echo, snapshot manual, boolean rule echo, provider/reader de produção |
| W-1044 | R1 adapter study | `r1-tabular-adapters` fixa três variantes W assíncronas que iteram todos os batches/rows e preservam primary 0.8, empty/negative/NaN/multiline, adversariais, digests, quatro tasks, orders, blinding e host oracle; mede clareza e preservação, não substituição universal | comparar formatos como equivalentes, input implícito, `expect true` ou participante/modelo inventado |
| W-1045 | SDK0/TAB1 projections | std-api contracts e snapshot catalogam std.data/csv/parquet/arrow e SnapshotByteSource como draft; providers permanecem missing; design-index, profiles, requirements e surface são derivados por scripts | alegar provider disponível, editar snapshot manual ou alterar generated tree-sitter src |
| W-1046 | header contextual PYN1 | `script { ... }` é o primeiro item opcional de `module_source`, usa `manifest_record` data-only e não é declaration, annotation, comment ou código | comment metadata, header depois de import, declaration top-level chamada `script` |
| W-1047 | fields e requirements do script | header exige `edition`; somente `dependencies`, `lock` e `requires` são fields adicionais; `schema` é redundante, requires usa enum contextual e unknown/duplicate falham | schema duplicado, tool table aberto, strings/records em requires, fields desconhecidos ou duplicate last-wins |
| W-1048 | context standalone | header torna o source root standalone mesmo dentro de workspace; sem header o arquivo seleciona package context ou ephemeral context sem merge silencioso | workspace sempre vence, merge de locks, package discovery ambiental |
| W-1049 | entry de script | source standalone aceita module/imports/declarations e exige `entryForm` explícito ou implicit final body; não aceita top-level execution arbitrária | entry nomeado obrigatório, execução de módulo arbitrária, `__main__` com args implícitos |
| W-1050 | roots e imports explícitos | grafo usa somente imports explícitos; input físico é opaque e provider retorna canonical token/owner/containment; logical imports são relativos; same-drive/UNC/Unicode equivalentes podem passar e escape/traversal/symlink/different owner falham | replace lexical como canonicalização, recursive scan, cwd/PATH/environment scan, symlink sem provider facts |
| W-1051 | dependency record PYN1 | dependency usa alias, package identity, version constraint, use e source authority do record normal de P0; aliases são únicos | string de dependency sem identity, alias derivado do package, resolver paralelo |
| W-1052 | sources rejeitados em script shareable | `.path`, branch/ref mutable, registry ambiental e local override são rejeitados | override local oculto, mutable ref aceito, registry inferido do host |
| W-1053 | lock root e virtual selection | dependency não vazia exige `lock: "sha256:..."`; payload fiel P0 usa `schema`/`resolver`/`contexts`/`packages`, selection/target/use/root edges/reachable closure são recompostos e `lock.digest` não é autoridade | flatten PYN1 custom, constraint sem lock, auto-default no hash, closure/selection opcional, authority derivada do digest |
| W-1054 | run sem re-resolução | `w run` compila source normal, mas não resolve constraint, atualiza lock, instala package ou executa install/build action; action output necessário deve estar no lock/CAS/policy | resolver implícito, update no run, callback de package ou shell oculto |
| W-1055 | fetch pinned e offline | fetch usa candidate real quando root/artifact não está no CAS; mirror pode servir lock content-addressed; `--offline` exige root e todos os digests de metadata/content/artifacts/action outputs; cache é explícito | network antes de resolution, fallback registry, expectativa como bytes, alias textual no CAS |
| W-1056 | integridade de artifact | digest sempre é verificado antes de publication/build; authority e signature exigem evidence/policy explícitas, mismatch aposenta bytes | executar bytes antes de verificar, defaults `true`, retry em authority não listada, aceitar assinatura divergente |
| W-1057 | requirements de capability | header declara `requires` com enum values e não grants/secrets; baseline channels (`process args`) e authority (`.stdio`) são separados; extras não propagam e handles transitivos exigem owner/contract | source concede authority, secret no header, string/record capability, escalation booleana |
| W-1058 | deployment grant | `.filesystem`, `.network`, `.clock`, `.random` e `.storage` exigem requirement declarado e grant explícito; effective separa offered/matched e ignora grants extras | `fs`/`network` antigos, grant automático do host, source amplia grant, deployment ignora requirement |
| W-1059 | capability transitiva | dependency transitiva não herda authority; effects exigem handles/bindings recebidos e contrato próprio | root grant global, boolean escalation, relay opaco, capability por import |
| W-1060 | identity efêmera | identity deriva de root source bytes digest, ordered logical `{path,digest}` graph, edition, target/host, lock digest, full requirements e toolchain digest, nunca path físico; edição final não inclui histórico | identity por path textual/cwd, cache key só por source, hash de `{previous,header}`, host env oculto |
| W-1061 | cleanup sem estado oculto | product temporário e failed run não deixam manifest/lock oculto; cleanup limpa artifacts transitórios e conserva provenance observável | lock temporário no cwd, manifest oculto, falha publica output parcial |
| W-1062 | commands atômicos | add/remove validam selection+lock antes da troca; source replacement usa bytes finais e digest recomputado; remove último dependency remove `lock`; resolve preserva edition/dependencies; failure preserva bytes exatos | editar header primeiro, hash dependente do histórico, lock parcial, `--with` como substituto |
| W-1063 | promotion equivalente | `script promote` valida root digest do candidate P0 e compara closure/package graph, selection, local graph, entry, requirements e provenance (`source`, script lock, manifest e package lock) sem re-resolve; root encoding pode mudar | promotion re-resolve, arrays soltos, troca entry/requirements/graph ou provenance declarada sem vínculo |
| W-1064 | evidência PEP 723 | PEP 723 é referência de ergonomia e security risk para metadata inline; W rejeita comment metadata, inference e tool table aberto | copiar comment syntax, tratar PEP como semântica W, dependency inference |
| W-1065 | operações do oracle | máquina host deriva parseHeader/evidence, context, roots, imports, resolution, selected target, fetch, artifact/handle/action records, capabilities, build, entry, cleanup e promotion | máquina que ecoa expected, boolean rules sem state, CLI falso |
| W-1066 | corpus PYN1 | 95 casos/546 operações positivos e negativos cobrem P0 lock graph, transitividade, fetch candidate/CAS, path owner, parser evidence, `entryForm` explícito/implicit/missing, body/effect evidence, multi-target, sidecar records, identity final, adversariais, Last Light symbols e IDs do ledger; snapshot JSONL é derivado | happy path only, expected como resultado, referência sem symbol |
| W-1067 | Last Light script oracle | `horizon_script.w` usa header explícito, dependency chart locked pelo digest real do fixture P0, `std.data`, score/menu coerente e statements finais no implicit `.default`; source não afirma execução | snippet isolado, digest manual, dependency fictícia sem authority, execução de módulo arbitrária |
| W-1068 | grammar e projeções | grammar.js e corpus reconhecem header contextual e mantêm `script` como identifier fora do root; Tree-sitter é estrutural e TextMate usa `\\A` conservador; generated src só muda pelo generator | editar parser gerado, reservar `script` global, regex lexical em toda linha, portal definir grammar |
| W-1069 | formatter PYN1 | F0 possui pair CST-equivalent e idempotence para header, module e entry; formatter preserva header como primeiro item | ordenar fields por heurística, apagar header, interpretar comment metadata |
| W-1070 | diagnostics PYN1 | somente posição/duplicate estrutural usam W-PARSE-0031/0032; edition/fields/import misuse/entry/lock/context/roots/evidence/records/capabilities usam W-SCRIPT phases/facts/labels catalogados | validation chamada parse, error genérico sem fact, recovery que muda root, wildcard semântico |
| W-1071 | host tests e snapshot | checker deriva state/trace, host test verifica standalone, CAS/root fetch, final-source identity, capability default, transitividade e atomicity; snapshot é regenerável | snapshot manual, teste só de expected, chamar host oracle de runtime |
| W-1072 | origens rejeitadas | URL, stdin e shebang não entram na baseline por path, environment e portability | executar source remoto, stdin root implícita, shebang com resolver ambiental |
| W-1073 | edition e target | lock/resolution e recipe falham em edition ou target mismatch antes de build | ignorar edition, escolher target do host, compartilhar lock incompatível |
| W-1074 | context explanation | `w context` mostra mode, roots, selected context, source/content digests, lock/fetches, artifact/record/recipe digests, authorities, capabilities e recipe antes do entry | saída resumida sem razão, provenance posterior apenas, merge silencioso |
| W-1075 | status do bundle | PYN1 é direção de design e oracle; CLI, compiler, resolver, provider, runtime e network client continuam missing | apresentar cases como execução implementada, promover oracle a produto, iniciar implementação fora do bundle |
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
| W-1093 | drain preflight | closure separa symbol graph de owner scopes e deriva replaceability/retention/deadline de provider events; resource/task ativo exige `allowDrain` estruturado | booleans de conclusão no caso, iniciar drop durante preflight, drenar sibling |
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
| W-1118 | requests read-only | completion/inspect/is_complete usam snapshot committed e Unicode codepoint offsets; inspect inclui plain text; history tail é baseline, range/search ficam Pesquisa | executar completion, ler staging, byte offset como codepoint, history raw ilimitada |
| W-1119 | metadata e identidades | metadata `w` versionada liga request/incarnation/generation/ordinal/outcome/digests/exportability; msg_id/cell ID/counter nunca substituem identities W; secrets/live values não entram | identity por frontend/timestamp, capability em metadata, cell ID como BindingId, client clock ordenar sessão |
| W-1120 | notebook como exploração | nbformat cell IDs são validados; outputs/trust não são source nem prova W; export reproduzível exige notebook mais receipt manifest explícito | `.ipynb` como release source, notebook signature como build proof, output codegen, hidden sidecar ambiental |
| W-1121 | prova de export | export valida source digest, committed chain, binding versions/edges, lock/context/target, effects e ausência de stdin/secret/degraded/live resource; não executa | replay oculto, export com unknown effect, capturar live value, aceitar cell invalidada ou silent mutation |
| W-1122 | ordem e resultado do export | pure declarations usam topological order com ordinal como tie break; effects preservam execution order; conflito/redefinition não-lossless falha; resultado é PYN1/package mais audit manifest | renomear/remover binding, reexecutar, inserir value literal, ordem do documento como autoridade, comentário gerado de prose |
| W-1123 | output transitório | tail expression summary não cria `_`/`ans`; display_id/update/clear/progress live ficam Pesquisa até owner/drain contract | binding implícito, handle frontend string cru, update atravessar reset, lifetime não bounded |
| W-1124 | status do bundle PYN3 | PYN3 fecha design e oracles; ZeroMQ, sanitizer, kernel process, frontend, compiler/runtime/providers, DLPack e nomes CLI permanecem missing/Pesquisa conforme seção | apresentar oracle como kernel implementado, iniciar provider, misturar DLPack, congelar CLI sem estudo humano |
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
| W-1147 | PYN4 status and evidence | fixture Last Light e machine/checker/snapshot/test host cobrem positivos e negativos e não executam W; C Exchange N0 permanece Pesquisa e non-owning/escaping/suspending callback rejeita | chamar machine de runtime, omitir adversarial ou snapshot, promover provider missing ou tratar C Exchange como semântica fechada |
| W-1148 | R1E0 evidence boundary | os cinco bundles R1E0 usam parse Tree-sitter e host oracles como evidência corrente; compile, run e estudos permanecem missing | chamar parse de ratificação, chamar oracle de execução W, inventar participantes ou modelos |
| W-1149 | R1E0 post-test loop | bundle compara `repeat` selecionado com `while true` válido e mede body, predicate, guard extra da alternativa, `continue`, `break`, cleanup lexical, zero, 9 e multidigit | negar body inicial, reavaliar predicate após break, ou tratar `do/catch` como post-test |
| W-1150 | R1E0 conditional and function return | bundle cobre `if` value, Unit sem else, named value block, join, selected effects, discard/tail, `return` explícito em function e o negativo de implicit function tail | aceitar non-Unit sem else, unir branch types por union implícita, ou tornar function body value block |
| W-1151 | R1E0 assignment | bundle cobre place/RHS one-shot, ledger de ownership, commit após RHS success, preservação após failure, Unit, move-only, compound one-place e rejeição por context/AST | devolver value, encadear, duplicar owner, dropar antes do RHS ou ler/escrever place duas vezes |
| W-1152 | R1E0 power boundary | bundle cobre `**` right-associative, unary base/exponent, `^` XOR e a separação de unit grammar | trocar `^` por power, associar à esquerda, ou exigir parentheses para exponent prefix |
| W-1153 | R1E0 fluent self | bundle seleciona `: self` com fallthrough e compara `return self`; validator separa receiver mode, return contract, exit e storage; omitted type é Unit, `Self` é owned e `take fn` rejeita | tornar `: self` `Self` owned, exigir `return self`, copiar/mover/alocar receiver ou aceitar `take fn` |
| W-1154 | R1E0 corpus metrics and status | no fechamento R1E0, scripts derivaram 20 bundles, 48 variants, 80 tasks e 31/68 R0 promovidos; bundles posteriores preservam o mesmo status `design-oracle-input` | escrever contagens manuais, promover host evidence a runtime, ou declarar estudo humano/modelo executado |
| W-1155 | generic value parameters | parâmetros de valor usam `name: Type` ou `_ name: Type` (`optional(name)`), são compile-time imutáveis, resolvidos após name resolution e participam de identidade, ConstIR e monomorphization sem storage runtime | `const name: Type` no envelope, classificação por casing, inheritance/base-class constraint, storage implícito |
| W-1156 | generic labels e contract values (retired) | interpretação anterior: `_` removia o label externo; W-1160 substitui por label opcional, com as duas formas no mesmo slot; values mantêm associated exposure estática | primary-only, field de instance automático, reorder de labels, member callable implícito |
| W-1157 | implicit script entry | statements finais root-only baixam para wrapper `.default` privado sem args/ctx, com `entryForm` explícito no workflow | top-level execution arbitrária, entry+implicit misturados, args/ctx implícitos, script importável |
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
| W-1171 | evidence de execution ergonomics | máquina pura deriva labels/forms, suspensão/SCC/call form, placement, projections, doctest terminals/effects, std module facts e FIFO/lifecycle/budgets de jobs e frame bytes da lane serial dinâmica; checker compara expected, host test usa entradas independentes e snapshot JSONL fixa o resultado; nenhum artefato executa W | checker que ecoa strings do JSON, snapshot manual, host test tautológico, alegar compiler/runtime |
| W-1172 | dispatch para domain serial | `spawn<domain>` cria child estruturado no domain explícito; serial aceita dispatch, executa um segmento runnable por vez, preserva FIFO no primeiro start e libera o permit durante await/join; `.parallel` é capability do domain, não da keyword | rejeitar serial, tratar spawn como thread paralela, bare `spawn let`, wait síncrono no mesmo domain, `parallelDefault` oculto |
| W-1173 | dispatch de barreira | `spawn<domain, .barrier>` cria um ticket estruturado; prior jobs drenam, a barreira `neverSuspend` executa sozinha e libera jobs posteriores depois do cleanup; o checker pode ordenar `ref`/`inout` somente para um grafo fechado no mesmo domain | shared ownership desnecessário, lock oculto, barrier que suspende, placement como isolation universal, `dispatch_barrier_async` sem queue privada, read/write por convenção não verificada |
| W-1174 | leitura tolerante a staleness | `load<.relaxed>()` continua atômica; storage comum só participa quando happens-before ou barreira prova a ordem; o modifier `atomic` nunca expõe uma view comum dos mesmos bytes | read não atômica concorrente porque o valor pode ser antigo, relaxed como non-atomic, weakening silencioso, `volatile` como synchronization |
| W-1175 | lane serial dinâmica | `ExecutionAuthority.openSerial` cria owner lexical bounded sobre pool existente; primeiro start é FIFO, só um segmento runnable usa o permit, suspension o libera, rejeição devolve o input em `TaskAdmissionError<Input>`, close drena e refs não estendem lifetime | copiar GCD inteiro, reter worker durante suspension, restaurar binding movido, perder input na admission, fila global, sync dispatch, QoS no call site, target queues, fire-and-forget, thread por lane, executor custom safe, usar lane local no lugar de service keyed |
| W-1176 | claim de memória | gerência automática exige prova real de owner/borrow/drop/reclamation, placement semanticamente neutro e contratos explícitos para shared/pin/FFI/OOM | alegar memória resolvida por existir um borrow checker, exigir GC/ARC universal, usar resultado de oracle host como implementação |
| W-1177 | criação shared ergonômica | `share(value)` usa allocator geral e policy normal; `tryShare(value, using:)` torna recovery e allocator explícitos; owner existente exige `take`; expected type nunca promove | `try share` obrigatório no caminho comum, allocation escondida pelo expected type, `Shared<T>.make`, promotion automática em argumento ou return |
| W-1178 | snapshot publicado | `SnapshotCell<T>` é move-only/shareable; `read` scoped vê uma versão, `snapshot` duplica e `publish` consome uma versão completa | guard público, ref escapante, mutation in-place, update closure escondida, safe RCU geral |
| W-1179 | reclamation de snapshot | publicação retira a versão anterior; cada versão executa drop uma vez depois do último reader, sem esperar no publish | liberar no swap, manter tudo até drop do cell, expor grace period, `Atomic<shared T>` |
| W-1180 | oracle SP0 | máquina host pura cobre publication order, staleness, error drain, retirement, close, OOM pré-publicação e estratégias equivalentes | chamar oracle de provider/runtime, snapshot manual, caso sem símbolo Última Luz |
| W-1181 | lock escopado | `Mutex<T>`, `AsyncMutex<T>` e `ReadWriteLock<T>` encapsulam payload non-shareable, expõem `ref`/`inout` por closure `neverSuspend` e não publicam guard | guard escapante, await protegido, payload shareable obrigatório, recursive lock |
| W-1182 | lock admission e failure | tickets FIFO, try sem bypass, cancel pré-grant remove waiter, cancel pós-grant espera unlock e panic falha a fault boundary | barging, poisoning recuperável, closure repetida, unlock antes de cleanup |
| W-1183 | conjunto mínimo de synchronization | atomic, locks, domain barrier, SnapshotCell, channel e service têm papéis distintos; condition e Once raw não entram na safe std; read/write síncrono segue W-1189 | copiar toda primitive host, condition sem ownership, RCU safe |
| W-1184 | oracle LM0 | máquina host pura cobre locks sync/async/read-write, queue, cancellation, failure, lifetime e seleção; não executa provider ou runtime W | expected echo, chamar model de scheduler, caso sem símbolo Última Luz |
| W-1185 | plano cross-axis de ownership | as quatro formas de execução usam PlaceId, LoanId, OriginSet, owner delta e drop obligations comuns; placement não cria outra memória | ownership por scheduler, copy/share implícito, ARC para reparar capture inválida |
| W-1186 | staging e fechamento cross-axis | capture passa parent→staging→child; rejection limpa uma vez; cleanup precede outcome e join restaura somente a autoridade permitida | rollback de take, outcome antes de cleanup, loan lexical encerrado por evento runtime invisível |
| W-1187 | publicação e ownership | happens-before publica mutation autorizada sem conceder owner, ampliar loan ou ressuscitar binding; scope exit cancela, drena e faz join | cancellation como rollback, join como share, binding movido volta por error |
| W-1188 | oracle MX0 | máquina host pura compõe staging, mobility, loans, admission, cancellation, cleanup, outcome, join e drop sem substituir M1/E0/E1 | colar snapshots independentes, expected echo, chamar modelo de compiler/runtime |
| W-1189 | read/write síncrono | `ReadWriteLock<T>` protege storage síncrono por closures `read`/`write` e `try*`; domain barrier continua task-owned | tratar lock e domain como aliases, guard público, async read/write lock baseline |
| W-1190 | admission read/write | tickets abrem ou ampliam a fase com o prefixo contíguo de readers; writer pendente bloqueia readers posteriores e `try*` não ultrapassa fila | fairness do host, barging, starvation por readers, recursion garantida, upgrade ou downgrade |
| W-1191 | lifetime e provider read/write | phase close deriva edges, application error libera, panic falha boundary, drop espera drain e provider preserva a policy de W | poisoning, liberar payload com reader, prometer SRW/pthread fairness, benchmark como semântica |
| W-1192 | evidence read/write LM0 | oracle host deriva reader phases, writer exclusivo, no-bypass, blocking, failure, drain e seleção; provider W continua missing | expected echo, chamar oracle de runtime, caso sem consumer Last Light |
| W-1193 | estado lógico Lazy | `var Lazy` possui initializer armazenado, winner único e uma publicação completa do initializer; contenders não repetem a execução | inicialização duplicada, retorno parcial, `Once` raw, carrier separado para o caso comum |
| W-1194 | lowering Lazy por prova | local, isolation serial e estado atômico com parking preservam a mesma semântica; interface publica `blockingWhenContended` | nome distinto por lowering, wait oculto em domain non-blocking, sincronização sempre ativa |
| W-1195 | failure e lifetime Lazy | initializer é sync/nonthrowing; reentrada dinâmica e panic falham boundary; mutation exige exclusividade; cada capture path e valor retido termina uma vez | poisoning recuperável, retry implícito, cancellation durante initializer, setter concorrente, self-owning closure |
| W-1196 | evidence LZ0 | oracle host deriva winner, waiters, publication edge, lowerings equivalentes, cancellation, mutation e drop; provider continua missing | chamar oracle de runtime/compiler, snapshot manual, caso sem símbolo Última Luz |

Uma revisão pode responder por ID. Uma mudança deve atualizar o exemplo, a
grammar, o formatter, o corpus e a seção semântica correspondente.
