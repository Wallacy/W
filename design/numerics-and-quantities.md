# Numéricos, quantidades e computação científica

> **Status:** Direção + Pesquisa · Working Draft

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

O experimento do forno usa uma notação provisória como `180_Celsius` e tipos
como `Temperature`, `Power`, `Energy` e `ThermalCapacity`. A sintaxe não está
decidida em [W-O036](../STATUS.md). A propriedade desejada está:

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
quanto a possibilidade de representação sem overhead; W precisa ainda resolver
generics, diagnostics, constantes e ABI C.

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

### Operadores de fórmula

`^` já é bitwise na baseline. Exponenciação permanece em
[W-O039](../STATUS.md): `**` é compacto e reconhecível em fórmulas, enquanto
`pow(base, exponent)` torna a família numérica e erros mais fáceis de selecionar.
O teste precisa fixar `-x ** 2`, associatividade, expoente inteiro negativo,
overflow e transformação de dimensions. Até isso acontecer, o corpus escreve
`flow * flow` quando o expoente é dois e não finge que `**` já pertence à grammar.

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

- [F#: Units of Measure](https://learn.microsoft.com/en-us/dotnet/fsharp/language-reference/units-of-measure)
- [MLIR: dialect `arith`](https://mlir.llvm.org/docs/Dialects/ArithOps/)
- [MLIR: dialect `math`](https://mlir.llvm.org/docs/Dialects/MathOps/)
- [MLIR: dialect `vector`](https://mlir.llvm.org/docs/Dialects/Vector/)
- [MLIR: dialect `linalg`](https://mlir.llvm.org/docs/Dialects/Linalg/)
- [LLVM: constrained floating-point intrinsics](https://llvm.org/docs/LangRef.html#constrained-floating-point-intrinsics)
