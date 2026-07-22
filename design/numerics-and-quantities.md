# Numéricos, quantidades e computação científica

> **Status:** Candidato + Pesquisa · DB1
> **Data:** 21 de julho de 2026

W pretende cobrir desde controle de equipamento até trabalhos acadêmicos sem
transformar o core numa coleção de frameworks. O primeiro corpus é o modelo
térmico do forno do restaurante; ele deve revelar quais garantias pertencem à
linguagem, à stdlib e a pacotes científicos.

## Direção

- Inteiros mantêm largura/overflow observáveis conforme `STATUS.md`.
- `f32` e `f64` têm semântica IEEE explícita; otimizações que reassociam,
  contraem ou ignoram NaN/Inf não aparecem silenciosamente em release.
- Quantidades incompatíveis falham no type checker, não após um cálculo físico.
- Dinheiro e medições não compartilham automaticamente a mesma representação:
  `Money` precisa de decimal/arredondamento; sensores normalmente usam float.
- Arrays/tensors, reductions, SIMD, BLAS e GPU são bibliotecas/lowerings sobre
  valores tipados. Não exigem uma segunda linguagem para o caminho CPU.
- O HIR preserva tipos, unidades, rounding, overflow e permissões de fast math
  até o lowering que consegue prová-los.

## Unidades físicas

W-C041 adota uma unit expression delimitada como forma canônica e estável. Ela é
próxima da fórmula, não depende do teclado do autor e não colide com lookup comum:

```w
import si

let setpoint = 180[degC]
let gravity = 9.80665[m/s^2]
let energy = 3.6[kW*h]
let payload = 64[KiB]
```

`[...]` é gramática de unit literal, não indexação nem annotation. Dentro dela,
`^` significa potência dimensional; fora dela, `^` continua XOR e `**` é a
exponenciação de expressões W. O parser conhece os delimitadores antes de resolver
nomes, portanto `9.81[m/s^2]` não cria ambiguidade com operadores gerais. A
propriedade semântica desejada é:

```text
Power × Duration             → Energy
Energy / ThermalCapacity     → TemperatureDelta
Area × ThermalTransmittance
     × TemperatureDelta      → Power
```

O compilador deve normalizar dimensões equivalentes e pode apagar as unidades no
runtime quando nenhuma reflexão foi solicitada. Conversões com offset, como
Celsius/Kelvin, não podem ser tratadas como simples escala multiplicativa. A
experiência do F# mostra tanto o valor do check dimensional em compile time
quanto a possibilidade de representação sem overhead; W ainda precisa provar em
protótipo generics, diagnostics, constantes e ABI C.

### Forma canônica e açúcares de edição

A identidade semântica é sempre uma unidade registrada e versionada, não o texto
do açúcar. Tooling deve conseguir expandir qualquer forma aceita para a canônica:

| Intenção | Forma canônica estável | Açúcares candidatos da edição |
|---|---|---|
| Celsius | `90[degC]` | `90[°C]`, `90C` |
| Fahrenheit | `90[degF]` | `90[°F]`, `90F` |
| delta Kelvin | `2[deltaK]` | `2[ΔK]` |
| delta Celsius | `2[deltaDegC]` | `2[Δ°C]` |
| quilômetro | `5[km]` | `5km` |
| aceleração | `9.81[m/s^2]` | — |
| kibibyte | `64[KiB]` | `64KiB` |

Espaço entre número e açúcar não é permitido: `64 KiB` continua dois tokens e
recebe fix-it para `64KiB` ou `64[KiB]`. O catálogo da edição só aceita sufixos
sem ambiguidade lexical; `C` e `F` significam temperatura apenas nesse contexto
de sufixo, enquanto dentro de `[...]` preservam coulomb e farad. Mudanças no mapa
de açúcares exigem mudança de edição, diagnóstico de migração e representação
canônica inalterada. Código gerado e APIs públicas devem preferir a forma canônica.

Prefixos SI preservam caixa e significado oficiais (`k`, `M`, `G`, `m`, `µ`,
etc.). Aliases ASCII como `um` para `µm` podem ser açúcares explícitos da edição,
nunca correções case-insensitive. `Ki`, `Mi`, `Gi` e seus múltiplos pertencem ao
catálogo T2 de informação baseado nos prefixos binários IEC, não ao SI; `kB` e
`KiB` permanecem valores diferentes.

### Modelo T2 recomendado

`std.si` não precisa declarar manualmente um tipo runtime para cada combinação.
Ele define as sete dimensões base do SI, expoentes racionais normalizados no type
system, unidades como scale/offset/symbol e aliases nomeados para todas as
unidades base, derivadas e prefixos oficiais. O catálogo normativo carrega a
versão da [SI Brochure do BIPM](https://www.bipm.org/en/publications/si-brochure/);
unidades não-SI aceitas ficam numa seção distinta. As sete constantes definidoras
do SI têm valores exatos; outras constantes físicas, incertezas e correlações
vêm de um bundle [CODATA/NIST](https://physics.nist.gov/cuu/Constants/) separado
e igualmente versionado.

```text
Quantity<Dimension, Representation>
Unit<Dimension, Scale, Offset>

Length × Length                    → Area
Power × Duration                   → Energy
TemperaturePoint - TemperaturePoint → TemperatureDelta
TemperaturePoint + TemperatureDelta → TemperaturePoint
```

Temperaturas absolutas formam pontos de um espaço afim; deltas são vetores. Isso
impede `20[°C] + 30[°C]` de parecer uma operação física normal e torna
conversões Celsius/Kelvin diferentes de uma mera multiplicação.
`K`/`degC`/`degF` constroem pontos quando usados como literal; `deltaK`,
`deltaDegC` e `deltaDegF` constroem diferenças. Assim, APIs e diagnostics não
dependem de contexto para decidir se `2[K]` era um ponto ou intervalo.

Units são normalmente apagadas no lowering, mas o tipo lógico pode reter
metadata alcançável para formatting/reflection. Conversão implícita só ocorre
quando é total, exata e única; scale com rounding, offset e perda de precisão são
explícitos conforme W-C031.

A gramática dentro de `[...]` é própria: símbolos oficiais como `°C` não viram
identifiers gerais de W. Escalas exatas usam rationals compile-time quando
possível. Raízes dividem expoentes dimensionais; potências que produziriam uma
dimensão irracional são rejeitadas até que exista um contrato explícito para ela.

## Ranges como domínios numéricos

Range é a abstração geral para membership, refinement e saturação:

```w
let allowed = 30[degC]...300[degC]

requested in allowed
allowed.clamp(requested)
allowed.intersection(calibrationRange)
```

`contains` não modifica, `clamp` satura e construir um refined type valida ou
falha. `clamp` é total apenas quando o range contém os dois extremos — como
`a...b`; um bound aberto não inventa silenciosamente `nextUp`/`nextDown`.
Intersection retorna `Range<T>?`; union retorna `Range<T>` quando o resultado é
contíguo e `RangeSet<T>` quando precisa preservar um buraco. Um property behavior
pode chamar essas operações, mas não deve reinventar range. `Range<T>` representa
intervalo; somente bounds discretos/strideable o tornam iterável. Progressões com
step usam `stride`, evitando fingir que um intervalo de `f64` possui uma
enumeração natural.

## Análise numérica e simbólica

O tier T2 pode oferecer `std.math.analysis` com integral numérica, derivada,
roots, interpolation, optimization e estimativa de limite. Cada API declara
método, tolerância, máximo de iterações, erro/convergência e propagação de units;
“limit” numérico não afirma uma prova matemática.

Álgebra simbólica, manipulação de expressões e theorem proving possuem escala e
evolução diferentes. Permanecem package first-party separado em **Pesquisa** em
[W-O099](../STATUS.md); o SDK pode distribuí-lo como T2 experimental sem fingir
que simplificação simbólica ou prova matemática já são contratos estáveis.

## Famílias numéricas candidatas

| Necessidade | Casa provável | Questão principal |
|---|---|---|
| inteiros fixos/nativos | core | overflow, cast e layout |
| `f32`/`f64` | core + math portátil | rounding, NaN, contração e reproducibilidade |
| decimal e `Money` | stdlib portátil | escala, contexto e arredondamento explícito |
| `BigInt`/`Rational` | pacote first-party | alocação e crescimento visíveis |
| complexos | stdlib científica | layout, branch cuts e interop |
| `Array<T, Shape>`/tensor | pacote + lowering | shape, strides, views, aliases e device |
| sparse/dataframe/table | pacotes | formatos, nullability, streaming e memória |

## Arrays, matrices, tensors e ML

> **Status:** direção de camadas; superfície **Em aberto** em
> [W-O102](../STATUS.md), sobre o modelo de W-O082 e os value parameters de
> W-O103.

### O contrato que importa

Notação matricial sozinha não cria uma linguagem de ML. O modelo precisa manter
até o lowering:

- rank e shape estáticos, simbólicos e dinâmicos;
- element type, promotions, rounding, overflow e quantization;
- ownership, mutation, aliases, views, offset, strides e layout;
- broadcasting, reductions e ordem numérica;
- dense, sparse e sharded tensors;
- device/address space e transfer cost;
- autodiff, random e reproducibility;
- import/export de model IR sem reduzir W a esse IR.

### Camadas

| Camada | Conteúdo proposto |
|---|---|
| T0 | `Array<T>`, fixed/inline array a decidir, `Slice<T>`/borrowed view, bounds e iteration |
| T2 `tensor` | `Tensor`, `TensorView`, shapes, slicing, reductions, linalg, CPU/SIMD e device protocol |
| T2 experimental `ml` | autodiff, optimizers, graphs/models, quantization, sparse/sharding e interchange |
| compiler | shape/refinement facts, fusion, vectorization, bufferization e target lowering |

O T2 pode ser oficial e bundled sem fazer tensor parte da grammar ou de todo
runtime. O core oferece generics/value parameters, refinements, ownership e
operators; a biblioteca e os protocols fornecem o domínio.

### Shape e tipo lógico

Sketch dependente de W-O103:

```w
fn classify<const batch: usize>(
  input: ref Tensor<f32, [batch, 784]>,
  weights: ref Tensor<f32, [784, 128]>,
): Tensor<f32, [batch, 128]> {
  // ...
}
```

O candidato é preservar rank conhecido para kernels compilados e permitir
extents estáticos, símbolos ou valores validados na entrada. Uma conversão de
buffer/shape dinâmico é fallible e estabelece o proof; operações posteriores
com shapes compatíveis não repetem checks. A alternativa mínima é
`Tensor<f32, rank: 2>` com constraints locais; `Tensor<f32>` totalmente unranked
fica para adapters/dados realmente dinâmicos.

`Tensor` é o valor lógico. `TensorView` é um borrow com metadata de shape,
strides, offset e address space. Layout row/column-major, tiles e packing só
entram na identidade pública quando uma ABI/kernel exige; otherwise são escolhas
de bufferization/optimizer explicáveis.

### Literals de matrix

Nested arrays contextuais formam a baseline de menor sintaxe:

```w
let transform: Matrix<f32, 2, 3> = [
  [1.0, 0.0, 10.0],
  [0.0, 1.0, 20.0],
]
```

O expected type exige rectangularidade e converte os elementos. As alternativas
continuam documentadas:

```w
// Sugar MATLAB/Julia-like; Em aberto
let transform = [1.0, 0.0, 10.0; 0.0, 1.0, 20.0]

// API explícita; não exige grammar especial
let transform = try Matrix.from(rows: [[1.0, 0.0, 10.0], [0.0, 1.0, 20.0]])
```

`;` é atraente em matrix 2D, mas não generaliza rank N e já aparece como
statement terminator opcional e no antigo MultiRange. Só vira sugar candidata
depois de um corpus científico e teste de parser/formatter.

### Operators e broadcasting

| Intenção | Candidato inicial | Alternativas |
|---|---|---|
| elementwise | `+ - * / **` com shape igual; scalar expansion total | métodos ou operators pontuados |
| matrix/tensor contraction | `a @ b`, sugar de `matmul(a, b)` | `*` linalg + `.*` elementwise; apenas API nomeada |
| broadcast entre shapes | `broadcast(to:)` explícito no primeiro corte | trailing-dimension implícito no estilo Array API |
| transpose/permutation | APIs `transpose`/`permuted` | `.T` ou postfix futuro |
| Einstein contraction | `einsum`/`contract` T2 com spec compile-time | DSL de índices no core |

Scalar expansion é total e pouco ambígua. Broadcast de shapes diferentes pode
ser uma view/fusão sem copy, mas também mascara eixo errado e multiplica trabalho;
por isso começa explícito. O corpus ML decide se a regra Array API merece sugar.
`@` separa produto linear de `*` elementwise sem introduzir toda a família `.*`,
mas permanece uma recomendação, não decisão ratificada.

### Ownership e copies

- `Tensor` owned é move-first; mutation exige exclusividade.
- slicing retorna `TensorView` borrowed por default; `copy()` materializa.
- shared mutable storage usa tipo/policy explícito, não COW invisível.
- `spawn` move, usa shared immutable ou mantém borrow sob join provado.
- output allocation é observável no lens; destination/in-place APIs podem
  eliminar allocation quando exclusivity e shapes permitirem.
- broadcast, transpose e reshape documentam quando são views e quando precisam
  materializar.

### Devices e execução

Transfer host↔device não pode surgir por causa de um operador local. Device,
executor e allocator são capabilities relacionadas, mas diferentes:

```w
let deviceModel = try await model.to(device)
let deviceBatch = try await batch.to(device)
spawn on device let logits = deviceModel(deviceBatch)
let result = await logits
```

Esse sketch depende de W-O100. Se um device não satisfizer a intenção paralela
de `spawn`, uma API async de submit pode ser correta; em nenhum caso compilation
latency, transfer ou synchronization ficam invisíveis no tooling/trace.

### Autodiff, random e modelos

Autodiff começa como transformação explícita e tipada, sem annotation universal:

```w
let (loss, gradients) = valueAndGrad(lossFor)(parameters, batch)
```

O pacote declara operações differentiable, mutation/alias rules, branches não
diferenciáveis e unsupported errors. Random usa generator/capability com seed
observável. Um model graph é biblioteca/IR, não a única forma de executar uma
função tensor W.

StableHLO e ONNX são adapters de interchange. StableHLO é especialmente útil
para operação ML portátil; ONNX para modelos/ecossistema. Nenhum deles consegue
representar sozinho toda a linguagem de systems, ownership, services e effects.

### Lowering

```text
W Tensor HIR
  → shape/refinement + ownership/alias + numeric mode
  → MLIR tensor/shape/linalg
  → vector | sparse/quant/shard | bufferization/memref
  → CPU/SIMD/BLAS | gpu/SPIR-V/vendor adapter
```

O dialect W retém error semantics, copy/materialization, device transfers e
strict/reproducible/fast mode até um passe conseguir prová-los. O dialect
`linalg` já modela operations estruturadas e lowering para loops, vector ou
library calls; `tensor` não substitui bufferization e `gpu` ainda exige device
management no runtime/adapters.

### Corpus mínimo antes de promover W-O102

1. matmul com shape error estático e dinâmico;
2. batched inference com extent simbólico;
3. slice/view sem copy e materialização explícita;
4. reduction nos modos strict/reproducible/fast;
5. autodiff pequeno com gradient check;
6. CPU escalar, SIMD e um device com tolerances declaradas;
7. import/export StableHLO ou ONNX com unsupported-op diagnostic;
8. benchmark separando compile, transfer, allocation e kernel.

As escolhas humanas sobre literal, operators, broadcast e tiers estão reunidas
no [addendum DB1](../DB1_ADDENDUM.md#a03--matrices-tensors-e-ml).

### Operadores de fórmula

Na gramática geral, `^` é bitwise XOR e `**` é exponenciação. `**` associa à
direita e tem precedência maior que prefixos, de modo que `-x ** 2` é
`-(x ** 2)`. `pow(base, exponent)` continua disponível quando a família numérica,
o método ou a policy de erro precisam ser nomeados. O type checker valida
expoente inteiro negativo, overflow e transformação de dimensões. Na subgramática
de unidade, a escrita matemática mais comum prevalece: `m/s^2`.

## Execução reproduzível

Build reproduzível não garante resultado numérico idêntico. Reductions paralelas
podem mudar a ordem de soma; targets podem contrair `a * b + c`; bibliotecas
externas podem escolher kernels diferentes. [W-O037](../STATUS.md) deve comparar:

1. modo estrito default, sem `fast-math` implícito;
2. modo reproduzível que também fixa reduction/order e dependências numéricas;
3. otimizações explícitas por função/scope com diagnostics da garantia perdida.

MLIR já separa operações aritméticas, vetoriais e lineares, e suporta atributos
de rounding/fast-math em operações relevantes. Isso favorece preservar intenção
numérica num dialect W antes de baixar para `arith`, `math`, `vector` e `linalg`,
em vez de apagar cedo as garantias num C intermediário.

## Experimentos derivados do restaurante

- type-check negativo trocando `Power` por `Energy`;
- simulação térmica pura com passo fixo e golden numérico;
- controlador PID com saturação e anti-windup;
- scheduler de lotes com algoritmo determinístico e custo medido;
- conta com `Money`, imposto e arredondamento sem binary float;
- redução serial versus paralela com erro e determinismo reportados;
- lowering escalar e vetorial equivalentes no subset suportado.

## Referências

- [BIPM: SI Brochure, 9th edition](https://www.bipm.org/en/publications/si-brochure/)
- [NIST: CODATA recommended values of the fundamental constants](https://physics.nist.gov/cuu/Constants/)
- [NIST: prefixes for binary multiples](https://www.physics.nist.gov/cuu/Units/binary.html)
- [F#: Units of Measure](https://learn.microsoft.com/en-us/dotnet/fsharp/language-reference/units-of-measure)
- [MLIR: dialect `arith`](https://mlir.llvm.org/docs/Dialects/ArithOps/)
- [MLIR: dialect `math`](https://mlir.llvm.org/docs/Dialects/MathOps/)
- [MLIR: dialect `vector`](https://mlir.llvm.org/docs/Dialects/Vector/)
- [MLIR: dialect `linalg`](https://mlir.llvm.org/docs/Dialects/Linalg/)
- [MLIR: dialect `tensor`](https://mlir.llvm.org/docs/Dialects/TensorOps/)
- [MLIR: dialect `shape`](https://mlir.llvm.org/docs/Dialects/ShapeDialect/)
- [MLIR: dialect `gpu`](https://mlir.llvm.org/docs/Dialects/GPU/)
- [MLIR: dialect `sparse_tensor`](https://mlir.llvm.org/docs/Dialects/SparseTensorOps/)
- [MLIR: dialect `quant`](https://mlir.llvm.org/docs/Dialects/QuantDialect/)
- [MLIR: dialect `shard`](https://mlir.llvm.org/docs/Dialects/Shard/)
- [OpenXLA: StableHLO specification](https://openxla.org/stablehlo/spec)
- [Python Array API: `matmul`](https://data-apis.org/array-api/2023.12/API_specification/generated/array_api.matmul.html)
- [ONNX: IR specification](https://onnx.ai/onnx/repo-docs/IR.html)
- [LLVM: constrained floating-point intrinsics](https://llvm.org/docs/LangRef.html#constrained-floating-point-intrinsics)
