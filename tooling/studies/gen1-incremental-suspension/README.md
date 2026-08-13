# GEN1 — suspensão incremental

Este estudo informa e estreita o gate `GEN0-R1` como um oracle de design. Ele compara quatro
formas para os mesmos traces do restaurante Last Light:

- A: `Stream` pull, adapters e tasks estruturadas;
- B: máquina de estados nominal explícita;
- C: dois canais bounded para o diálogo;
- D: witness textual de um bloco `Stream` compiler-owned, mantido Research;
- E: witness textual de frame/resume público, intencionalmente rejeitado.

O estudo mede ergonomia a partir das declarações source aplicáveis a cada
scenario slice. As métricas contam
conceitos públicos, handoffs de ownership, pontos explícitos de erro/cancel/
cleanup, estado oculto, declarações públicas como proxy de type/ABI e operações
de source.
LOC é apenas informativo. A máquina não executa W. Uma comparação só usa
símbolos únicos e digests do mesmo cenário; uma variante sem símbolo aplicável
não entra na comparação.

Cada trace roda em dois lowerings lógicos: `switched-resume-frame` e
`returned-continuation-state-loop`. O resultado exige igualdade de owner graph,
commit/happens-before, resultado tipado, cancelamento e cleanup/drop/drain.
Somente o trace físico e o packing podem mudar.

O estudo usa fontes Last Light por `sourceRefs` e digests. As variantes `.w`
são witnesses parseáveis; `compiler-stream-block.txt` e
`public-resumable-frame.txt` são witnesses reservados e não são parseados. O
estudo não altera grammar, parser, Last Light, std, compiler,
runtime ou provider. A evidência corrente é parse Tree-sitter e oracle host;
compile, run, estudos humanos/modelo e provider continuam ausentes.

O resultado deriva capability gap por cobertura dos cenários e uma pergunta de
ergonomia aberta: diferenças estruturais observadas em slices do mesmo cenário
são candidatas, não prova de gap humano. A ausência de evidência humana/modelo
mantém `humanDecisionPending`. As dispositions são separadas: frame/resume
público é rejeitado; o builder bounded é composable/current-candidate somente
para diálogo; e o bloco Stream compiler-owned é Research-candidate, não rejeitado.

Antes de considerar D, o estudo também mede o helper parseável
`builder-helper.w`: ele cria dois pares de `Channel` bounded e devolve endpoints
owned. Isso resolve somente o diálogo. Um bloco Stream compiler-owned continua
Research até prova de compiler/lowering e evidência humana/modelo.

Use, na raiz do repositório:

```sh
bun test tooling/gen1-incremental-suspension-reference.test.mjs
bun tooling/check-gen1-incremental-suspension.mjs
bun run --cwd tooling/tree-sitter-w parse:studies
```
