# Property behaviors

> **Status: Pesquisa · W-O097 · 19 de julho de 2026**

Property behavior é a hipótese de transformar padrões repetidos de implementação
de propriedades — storage auxiliar, inicialização, leitura, escrita, acesso
mutável e operações relacionadas — em uma abstração tipada de biblioteca. Não é
um sistema genérico de annotations, macro textual ou autorização para esconder
I/O em `object.property`.

## O que vale recuperar do Swift original

A proposta [SE-0030 Property Behaviors](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0030-property-behavior-decls.md)
tentava expressar em biblioteca `lazy`, inicialização tardia, observers,
sincronização, cópia e proxies. O behavior podia definir storage, capturar o
initializer e fornecer ou exigir accessors. A proposta foi retirada e sucedida
por [SE-0258 Property Wrappers](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0258-property-wrappers.md),
que escolheu uma transformação mais estreita baseada em wrapper types.

Wrappers foram mais simples de implementar, mas perderam duas capacidades
relevantes para W: declarar novos tipos de accessor e consultar o `self` que
contém a propriedade. O estudo de implementação original também mostra uma rota
útil: instanciar storage e accessors estaticamente, tratá-los de modo parecido a
witnesses e deixar specialization/inlining remover a abstração, sem metadata
runtime universal. A definite initialization enxerga a propriedade lógica como
proxy do storage e distingue a primeira inicialização de uma atribuição.

## Hipótese recomendada para W

W deve estudar o poder do behavior original com uma superfície menor e mais
explícita. O formato ilustrativo abaixo **não pertence à grammar atual**:

```w
behavior Lazy<Value> for Value {
  storage var cached: Value?
  initialValue

  init { cached = .none }

  mut get {
    if let value = cached { return value }
    let value = initialValue()
    cached = .some(value)
    return value
  }

  set(newValue) { cached = .some(newValue) }
}

object Oven {
  var heatProfile = deriveHeatProfile(model) with Lazy
}
```

`heatProfile` continua tendo seu tipo lógico; o optional de `Lazy` é detalhe de
implementação. `with` torna a feature uma construção dedicada e pesquisável,
sem reabrir `@annotations`. A declaração de behavior produz HIR tipada, não AST
arbitrária, e o compiler conhece storage, lifetime, accessors e efeitos antes do
lowering.

### Contrato mínimo a testar

1. **Storage explícito para a máquina.** Todo byte sintetizado possui tipo,
   init, move/copy/drop, alignment e visibility conhecidos. Layout normal W pode
   inlinear, reordenar ou eliminar esse storage sob as mesmas regras de qualquer
   field privado.
2. **Propriedade lógica para o caller.** Lookup, autocomplete e reflection
   mostram o tipo lógico de `heatProfile`; debug symbols separados podem revelar o backing
   storage. Derivações como serialization e equality ainda precisam decidir se
   observam o valor lógico ou uma escolha explícita do behavior.
3. **Inicialização não é setter.** O behavior declara separadamente `init`,
   `get`, `set` e um accessor de acesso mutável como `modify/yield`. Definite
   initialization e exclusivity verificam a propriedade proxy e impedem dois
   `inout` simultâneos.
4. **Efeitos continuam visíveis.** Um `get`/`set` comum nunca suspende, faz I/O
   remoto nem falha implicitamente. Se accessors `async`/`throws` forem aceitos,
   o uso exige `await`/`try`; blocking, allocation e locks entram no lens de
   custo. `Persisted` ou `Remote` não podem disfarçar uma chamada de rede como
   field access.
5. **Sem runtime obrigatório.** Numa build fechada, storage e accessors são
   especializados como código normal. Separate compilation pode precisar de
   descriptors/witnesses alcançáveis; isso não torna cada propriedade reflexiva
   nem exige heap allocation.
6. **Ownership e concorrência são derivados, não alegados.** Sendability,
   compartilhamento, mutation e cleanup dependem do storage e dos accessors. Um
   behavior não declara `Atomic`, `Shared` ou `Safe` sem satisfazer os verifiers.
7. **Invariantes transferíveis pertencem ao tipo.** Um refined type protege o
   valor em qualquer lugar. Range fornece `contains`/`clamp`; behavior só seria
   necessário para aplicar uma policy repetidamente e não substitui ambos.

## Composição

Composição é útil — por exemplo lazy + observado — mas a ordem normalmente muda
a semântica. Três formas ficam preservadas:

```w
// A. pipeline declarada; ordem definida pela linguagem
var menu = loadMenu() with Lazy, Observed(onMenuChange)

// B. nesting explícito; mais longo, sem ambiguidade
var menu = loadMenu() with Observed(Lazy)

// C. composição nomeada pela biblioteca
var menu = loadMenu() with ObservableLazy(onMenuChange)
```

A máquina prefere B/C: a forma expandida, ownership e ordem dos accessors são
inequívocos. Humanos podem preferir A; ela só sobrevive se formatter, tooltip e
`w explain property` mostrarem a expansão e se composições incompatíveis forem
rejeitadas. Nunca se presume que behaviors comutam.

## Poder que precisa de limites

| Capacidade | Valor | Risco a resolver |
|---|---|---|
| `Lazy`/memoization | remove boilerplate e pode eliminar trabalho | primeira leitura tem custo e pode mutar storage |
| `Once`/delayed init | modela inicialização em fases sem optional público | falha antes/depois da atribuição e interação com `let` |
| observers/validation | centraliza policy local | reentrância, ordem e side effects escondidos no setter |
| COW/weak/handle | integra storage com ownership | drop, cycles, FFI e representação dependem do modelo de memória |
| synchronized/atomic | elimina locks repetidos | blocking escondido, deadlock, memory ordering e falsa atomicidade composta |
| acesso a `self` | usa lock, executor ou contexto do container | ciclos de init, exclusivity e escape de `self` parcial |
| members/projections | expõe `reset`, publisher ou handle | namespace paralelo, visibility, protocols e resilience |

## Alternativas de sintaxe preservadas

| Forma | Leitura | Consequência |
|---|---|---|
| `var value: T with Behavior(...)` | comportamento depois do contrato lógico | recomendação de máquina atual; clara e sem annotation |
| `var [behavior(...)] value: T` | compacta e próxima do SE-0030 | colchetes ganham um segundo papel e o nome chega tarde |
| `var value: Behavior<T>` | usa apenas o type system comum | vaza storage no tipo público e piora calls/generics |
| bloco `var value: T { behavior ... }` | acomoda accessors customizados | mais verboso e composição menos imediata |
| features fixas `lazy`/`atomic`/`observed` | implementação direta | cresce o core e não cobre policies de domínio |

## Perguntas para fechar W-O097

1. `with Behavior(...)` preserva a sensação de uma propriedade comum ou parece
   esconder comportamento demais depois do nome?
2. O conjunto v0 deve aceitar somente `init/get/set/modify`, síncronos e sem
   `throws`, deixando custom accessors, projections e `self` para depois?
3. Composição deve exigir nesting/composite nomeado no primeiro corte?
4. `Lazy` pode manter a ergonomia de field access se o lens mostrar first-read
   work, ou W deve exigir uma operação explícita como `menu.get()`?
5. Qual caso do restaurante valida primeiro a feature: cálculo térmico lazy,
   observer da TUI, init tardia ou storage COW do cardápio?

## Experimento mínimo

Modelar `Lazy`, `Observed` e `Once` em HIR fictícia e expandi-los para fields e
funções W ordinárias. O teste precisa comparar init/set, move/drop, `inout`, duas
ordens de composição, reflection lógica, layout explicado e diagnostics de um
behavior que tenta suspender ou capturar `self` antes da inicialização. Somente
depois disso uma forma entra no Tree-sitter ou no corpus executável.

Referências adicionais: discussão de
[implementação estática](https://forums.swift.org/t/implementation-of-property-behaviors/1143)
e de [definite initialization](https://forums.swift.org/t/definite-initialization-implementation-for-property-behaviors/1647).
