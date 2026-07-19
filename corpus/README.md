# Corpus de contrato W

> **Status:** Candidato — bootstrap da Fase 0; não é ainda uma suíte de execução

Este diretório transforma exemplos pequenos em contratos versionados. O
[manifest](manifest.json) é o índice canônico: cada caso declara família,
classificação, regras relacionadas e observação esperada. O cenário completo do
[restaurante](../examples/restaurant/README.md) continua sendo o guia top-down;
o corpus decompõe suas regras para que parser, formatter, frontend e backends
possam compartilhar os mesmos oracles.

## Classificações

- `executable`: pertence ao subset síncrono planejado para o primeiro executável
  nativo. O `stdout` no manifest é contrato futuro; o runner atual não executa W.
- `frontend-only`: deve produzir CST/diagnostics, mas depende de uma fase ou
  decisão posterior. Parse bem-sucedido não promove sua semântica.

Os negativos também são `frontend-only`. Seu código e mensagem representam o
diagnóstico desejado; snapshots `w.diagnostics/1` registram severidade, span e
offsets UTF-8 obtidos do recovery `ERROR`/`MISSING`. Neste corte o adapter conhece
o código pelo manifest; classificação geral de erros pertence à Fase 1.

## Executar

Instale uma vez a dependência fixada da gramática:

```sh
cd ../tooling/tree-sitter-w
npm install
```

Depois, a partir desta pasta:

```sh
bun run runner.ts
```

Para atualizar snapshots após uma mudança deliberada de gramática:

```sh
bun run runner.ts --update
```

O runner compila a gramática numa pasta temporária, parseia cada source duas
vezes e exige CST idêntica. Snapshots usam LF e não contêm paths absolutos,
tempo ou endereço. Diagnósticos seguem [diagnostics.schema.json](diagnostics.schema.json).
A pasta temporária é removida mesmo quando um caso falha.

## Regras de evolução

1. Uma construção entra com positivo, negativo e referência canônica.
2. Alterações de snapshot exigem revisão do source e da decisão, não update cego.
3. `observable.kind = "stdout"` não afirma que já existe runtime.
4. Formatter, AST/HIR e execução acrescentarão novos oracles ao mesmo manifest;
   não criarão corpora paralelos.
5. O [ROADMAP](../ROADMAP.md) define os gates quantitativos da fase.
