# Documentação e testes no source

> **Status:** Candidato · DB1 · ainda não implementado

W trata documentação executável e testes pequenos como parte da interface do
módulo, sem transformar comentários em uma segunda linguagem. Este contrato
moderniza a ideia histórica de “InPlace Test e Debug” de `Y/WIP.MD`; as formas
antigas baseadas em `@`/`@@` permanecem apenas como provenance.

## Comentários de documentação

`///` contém Markdown e se anexa à próxima declaração. Comentários `//` e
`/* ... */` são comuns e nunca viram documentação pública.

```w
/// Calcula a perda térmica entre o forno e o ambiente.
///
/// `inside` precisa representar um ponto de temperatura; a diferença produz um
/// `TemperatureDelta` antes da multiplicação.
///
/// ```w test
/// let loss = heatLoss(2[m^2], transmittance: 4[W/(m^2*K)], inside: 180C, ambient: 25C)
/// expect loss > 0[W]
/// ```
export fn heatLoss(surface: Area, transmittance: ThermalTransmittance, inside: Temperature, ambient: Temperature): Power {
  return surface * transmittance * (inside - ambient)
}
```

A documentação preserva ordem, links e exemplos no artefato de interface, mas não
entra no payload release por padrão. Símbolos privados podem ter documentação e
doctests; somente os públicos aparecem no portal/API por default. Tags mágicas ao
estilo JSDoc não são necessárias para parâmetros, retorno, tipos ou errors: o
gerador lê essas informações da assinatura e exige que o texto não as contradiga.
Extensões futuras usam directives versionadas e reservadas, nunca nomes livres.

## Doctests

Um fence `w test` dentro de `///` é compilado como um pequeno módulo que importa a
declaração documentada. `w test --doc` compila e executa os fences; `w test` inclui
doctests por padrão. Eles obedecem à mesma edição, target profile e lockfile do
módulo.

```w
/// ```w compile-fail
/// let impossible = 1[m] + 1[s]
/// // error: cannot add Length and Duration
/// ```
```

`compile-fail` exige falha e compara IDs/trechos estruturados de diagnóstico, não
texto inteiro frágil. Um fence sem `test` é apenas apresentação. Exemplos com I/O,
tempo, rede ou randomness precisam declarar capabilities de teste e não são
executados durante import, build normal ou geração de documentação.

## Testes co-localizados

Casos pequenos podem ficar no mesmo módulo e se referir explicitamente ao símbolo:

```w
test "PID não acumula contra a saturação" for controlStep {
  var controller = ControllerState(accumulatedError: 0.0, previousError: 0.0)
  let decision = try controlStep(config, model: model, sample: hotSample, controller: inout controller, target: 180C, ambient: 25C, elapsed: 1[s])

  expect decision.duty in 0.0...1.0
  expect controller.accumulatedError == 0.0
}
```

`test` é uma declaração contextual de módulo, não annotation. O nome é obrigatório
e `for symbol` é opcional, mas recomendado: cria navegação, cobertura por API e
execução filtrada (`w test --for controlStep`). O body pode acessar declarações
privadas do próprio módulo. Declarações de teste não são visíveis a imports e são
eliminadas antes do artefato release.

Testes maiores continuam em arquivos `*.test.w`; integração e end-to-end podem
viver em `tests/`. Todos usam a mesma declaração `test`, runner e sistema de
expectations. Não existe função executada implicitamente só porque o arquivo foi
importado.

`expect` é um intrinsic disponível somente no contexto de teste. Ele produz uma
falha estruturada ligada ao source e não pode ser chamado por código release nem
substituído silenciosamente por um `assert` removível.

## Grafo e runner

O frontend extrai docs e testes para HIR própria, preservando source locations.
O build registra cada caso como nó content-addressed com:

- módulo, símbolo relacionado e capabilities;
- target/profile, edição, compiler, std/SDK e lockfile;
- timeout, seed e policy de paralelismo;
- resultado, diagnóstico estruturado e artefatos anexos.

`w test` seleciona `unit`, `doc`, `compile-fail`, `property`, `fuzz`, integração e
ABI sem criar runners incompatíveis. Execução pode ser paralela entre casos
isolados; um caso individual continua determinístico salvo declaração explícita.
Snapshots são arquivos revisáveis e versionados, não comentários reescritos pelo
runtime.

## Contrato do formatter e tooling

- `///` permanece imediatamente antes da declaração; uma linha vazia solta o doc.
- O formatter quebra prose Markdown apenas quando solicitado; não reformata texto
  ou output esperado silenciosamente.
- Código de fence W usa o formatter W e mantém o vínculo com as linhas originais.
- LSP mostra assinatura derivada, docs, exemplos e testes sem duplicar tipos.
- `w doc --check` valida links, símbolos, fences e divergências óbvias entre texto
  e assinatura.

## Alternativas preservadas

| Alternativa | Estado | Motivo |
|---|---|---|
| `//@(2, 1) == 3`, `@a` e `@@` | histórica | compacta, mas cria DSL opaca, difícil de diagnosticar e evoluir |
| tags JSDoc para repetir params/return | rejeitada por enquanto | duplica fatos que a assinatura já fornece |
| testes somente em arquivos externos | suportada, não exclusiva | necessária para casos grandes, ruim para exemplos mínimos próximos da API |
| executar exemplos ao importar | rejeitada | surpreende runtime, segurança e reprodutibilidade |
| annotations para docs/testes | rejeitada | `///`, fences e declaração contextual cobrem o caso sem annotation geral |

## Critérios antes da implementação

O primeiro frontend deve provar attachment e recovery de `///`, fences aninhados,
`test ... for`, eliminação no release, compile-fail estruturado e execução filtrada.
O restaurante é o corpus inicial; nenhuma sintaxe daqui afirma que o runner já
existe.

## Referências

- [Rustdoc: documentation tests](https://doc.rust-lang.org/rustdoc/write-documentation/documentation-tests.html)
- [Rust Reference: doc comments](https://doc.rust-lang.org/reference/comments.html)
- [histórico W: fontes e autoridade](../../Y/W/HISTORY.md)
