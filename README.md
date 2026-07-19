# W

> **Working Draft · 19 de julho de 2026**
> **Prazer para humanos. Clareza para máquinas.**

W é uma proposta de linguagem de sistemas nativa, segura e agradável de escrever. Ela combina a familiaridade de C, a legibilidade de Swift e TypeScript e um modelo explícito para memória, efeitos, concorrência e paralelismo.

Este diretório não afirma que o compilador já existe. Ele é a nova base de design do projeto: mostra como queremos que W seja percebida por quem programa, registra o que já é direção, separa propostas de experimentos e transforma anos de anotações em decisões que podem ser testadas.

## A ideia em um minuto

```w
import { http } from std.net

type UserId = u64 where value > 0

struct User {
  id: UserId
  name: String
}

enum LoadError: Error {
  notFound(UserId)
  transport(HttpError)
  invalidPayload(JsonError)
}

async fn fetchUserResponse(id: UserId): HttpResponse throws LoadError {
  do {
    return try await http.get("/users/${id}")
  } catch let error {
    throw .transport(error)
  }
}

fn decodeUser(response: ref HttpResponse): User throws LoadError {
  do {
    return try response.json(as: User)
  } catch let error {
    throw .invalidPayload(error)
  }
}

async fn loadUser(id: UserId): User throws LoadError {
  let response = try await fetchUserResponse(id)
  guard response.status == .ok else throw .notFound(id)
  return try decodeUser(response)
}

async fn dashboard(id: UserId): Dashboard throws LoadError {
  // Concorrência: duas operações suspensíveis no mesmo escopo.
  async let user = loadUser(id)
  async let activity = loadActivity(id)

  let (user, activity) = try await (user, activity)

  // Paralelismo: trabalho de CPU pode ocupar outro núcleo.
  spawn let score = rank(activity)

  return Dashboard(
    user: user,
    activity: activity,
    score: await score,
  )
}
```

O exemplo acima é **sintaxe candidata**, não uma especificação congelada. Ele já expressa as decisões que queremos testar primeiro:

- `let` é imutável, `var` é mutável e `const` existe em tempo de compilação;
- `struct` tem semântica de valor, `object` tem identidade e `enum` representa alternativas;
- ausência é `T?`/`.none`; `null`, `undefined`, `empty` e `uninitialized` não são o mesmo estado;
- erros recuperáveis são tipados com `throws E` e não dependem de stack unwinding oculto;
- `try` propaga o mesmo error set; boundaries entre tipos de erro fazem a
  conversão explicitamente enquanto a ergonomia final permanece aberta;
- `async let` cria concorrência estruturada; `spawn let` pede paralelismo;
- toda tarefa pertence a um escopo, e toda transferência de dados entre executores precisa ser segura;
- ownership é inferido no caminho comum e fica explícito nos pontos importantes com `ref`, `inout`, `take` e `copy`;
- C é uma fronteira de ABI e interoperabilidade de primeira classe, não a semântica interna inteira de W.

## O que torna W diferente

### Código que antecipa o runtime

O que pode suspender, falhar, mutar estado, transferir ownership ou executar em paralelo deve ser visível no contrato ou no ponto de uso. Builds `debug` e `release` não devem mudar a semântica do programa.

### Concorrência e paralelismo não são sinônimos

`async`/`await` organiza trabalho suspensível e I/O. `spawn` torna explícita a intenção de usar capacidade paralela. Ambos obedecem à mesma árvore de tarefas, propagação de cancelamento e encerramento estruturado.

### Segurança sem fazer da sintaxe um exercício de lifetimes

W busca ownership único por padrão, borrows locais inferidos, valores por cópia/move e compartilhamento explícito. O compilador deve pedir um marcador próprio apenas quando a intenção não puder ser provada com segurança.

### Baixo nível quando ele realmente importa

Layout, inteiros de largura fixa, SIMD, FFI, allocators e APIs de sistema continuam acessíveis. A fronteira de C é deliberadamente evidente; não existe a promessa de que qualquer C inseguro se torne seguro apenas por estar dentro de um arquivo `.w`.

### Toolchain e supply chain são parte da linguagem como produto

O plano inclui formatter, linguagem para editores, testes de exemplos, builds determinísticas, lockfile resolvido, cache endereçado por conteúdo, provenance e artefatos assinados. Uma hash isolada comprova integridade, não autoria nem segurança.

### Boa para IA porque é boa para raciocinar

W não precisa de uma sintaxe “para LLM”. Uma gramática canônica, poucos sinônimos, efeitos explícitos, metadata legível por máquina e diagnósticos estruturados tornam o código mais fácil de entender para humanos e assistentes ao mesmo tempo.

## Arquitetura em uma linha

```text
W source → CST/AST → HIR tipada → dialeto W no MLIR
         → lowerings (ownership, efeitos, tasks, layout)
         → LLVM dialect → LLVM IR → código nativo
         ↘ EmitC/C para subset, portabilidade e inspeção
```

MLIR é a infraestrutura de IR, passes e lowering. Não substitui o frontend, o type checker, o modelo de memória nem o runtime. W preservará suas semânticas de alto nível em um dialeto próprio até haver informação suficiente para baixá-las corretamente.

## Estado do projeto

| Camada | Estado atual |
|---|---|
| Visão e princípios | direção consolidada |
| Baseline de design | DB1 ratificada como candidata; ensaio integrado do restaurante em revisão |
| Sintaxe apresentada aqui | proposta de trabalho |
| Tipos, erros e ownership | modelo candidato; precisa de protótipos |
| Concorrência estruturada e paralelismo | semântica candidata; runtime ainda inexistente |
| Corpus de contrato | 12 positivos, 11 negativos e runner determinístico; revisão humana pendente |
| Frontend e gramática | Tree-sitter candidato produz CST; formatter, AST/HIR e diagnostics gerais ainda não existem |
| Highlighting local | TextMate/portal utilizáveis; Tree-sitter em protótipo não normativo |
| Dialeto W/MLIR | arquitetura proposta |
| Bootstrap | seed C portátil + self-host W cedo; ainda não implementado |
| Documentação/testes | `///`, doctests e testes co-localizados candidatos; runner inexistente |
| Package/build system | design em elaboração |
| Estimativas de recursos | experimento de tooling; sem garantia de runtime |
| Compilador, runtime e stdlib | ainda não implementados |
| wQL, wRPC, V6 e outras extensões | pesquisa fora do núcleo v0 |

O vocabulário normativo usado em toda a documentação está em [STATUS.md](STATUS.md).
O inventário e a ordem de fechamento estão em
[DESIGN_CLOSURE.md](DESIGN_CLOSURE.md). A proposta de fechamento de todas as
questões e o formulário único de ratificação estão em
[DB1_REVIEW.md](DB1_REVIEW.md).

## Como explorar

- Abra a [POC do portal](portal/README.md) para explorar a linguagem, o livro e o
  playground de especificação. Ela roda localmente com Bun, sem dependências externas.
- Percorra [O restaurante W](examples/restaurant/README.md), o cenário canônico
  que combina cálculo, algoritmos, fluxo sequencial, concorrência, paralelismo,
  TUI/HTTP e instâncias fine-grained. Os
  [requisitos derivados](examples/restaurant/REQUIREMENTS.md) ligam o source ao
  frontend, HIR, memória e runtime.
- Experimente o [tooling inicial](tooling/README.md): extensão local do VS Code,
  grammar Tree-sitter candidata e highlighting do portal.
- Inspecione o [corpus de contrato](corpus/README.md): manifest versionado,
  positivos/negativos, snapshots de CST e classificação do subset executável.
- Consulte o [fechamento da baseline](DESIGN_CLOSURE.md) para ver todas as
  pendências e a ordem em que serão decididas antes da implementação.
- Use a [revisão integral da DB1](DB1_REVIEW.md) para aceitar os defaults H01–H14
  em lote e registrar somente as exceções.
- Leia [LANGUAGE_TOUR.md](LANGUAGE_TOUR.md) para percorrer a sintaxe e o comportamento do programa.
- Leia [VISION.md](VISION.md) para entender público, princípios, não objetivos e opções de posicionamento.
- Consulte [techspec.md](techspec.md) para a arquitetura técnica resumida.
- Leia [ARCHITECTURE.md](ARCHITECTURE.md) para as fronteiras de longo prazo entre
  linguagem, implementação, runtime, SDK, build, distribuição e tooling.
- Consulte [spec/syntax.md](spec/syntax.md),
  [spec/types-and-memory.md](spec/types-and-memory.md),
  [spec/concurrency.md](spec/concurrency.md) e [spec/modules.md](spec/modules.md)
  para os modelos candidatos.
- Consulte [design/compiler.md](design/compiler.md),
  [design/documentation-and-tests.md](design/documentation-and-tests.md),
  [design/memory-strategy.md](design/memory-strategy.md),
  [design/modules-and-runtime.md](design/modules-and-runtime.md),
  [design/formatting.md](design/formatting.md),
  [design/numerics-and-quantities.md](design/numerics-and-quantities.md),
  [design/resource-estimation.md](design/resource-estimation.md),
  [design/stdlib.md](design/stdlib.md), [design/packages.md](design/packages.md) e
  [design/verification-and-releases.md](design/verification-and-releases.md)
  para implementação, runtime, biblioteca padrão e distribuição.
- Veja [research/README.md](research/README.md) e o
  [programa de longo prazo](research/long-term-program.md) antes de promover uma
  hipótese a requisito.
- Veja [ROADMAP.md](ROADMAP.md) para a sequência de protótipos.

## Norte imediato

A DB1 foi ratificada; antes de implementar o frontend, o restaurante precisa fazer
o double-check integrado da superfície e registrar qualquer contradição real. Em
seguida, a primeira fatia vertical implementável será:

1. dez a vinte programas que definam a experiência desejada;
2. gramática, formatter e diagnósticos coerentes para esses programas;
3. AST/HIR com tipos, opcionais, `throws`, `ref`/`inout`/`take`;
4. um seed C auditável que produza o primeiro compilador W e um dialeto MLIR
   mínimo com lowering até executável;
5. concorrência estruturada e `spawn` em um runtime pequeno;
6. build reproduzível e pacote local endereçado por conteúdo;
7. um lens de recursos que comece por deltas pós-link exatos e preserve
   `unknown` onde ainda não houver uma estimativa defensável.

O critério não é quantas features conseguimos listar. É se escrever, ler, explicar e executar um programa W parece simples, previsível e prazeroso.
