# Atlas sintático de W

O atlas mostra a superfície sintática atual de W para leitura humana. Ele é
separado do produto funcional Última Luz. Os exemplos usam nomes de cidade e
território somente para tornar a leitura local.

## Como navegar

Comece por [`language.w`](language.w). Ele reúne roots de módulo, imports,
declarations, tipos, contratos, patterns, literals e entry explícito.

Depois leia [`execution.w`](execution.w). Ele reúne script header, bindings,
allocator scopes, controle, execução, effects, streams, channels e implicit
entry.

Os arquivos [`package.w`](package.w), [`workspace.w`](workspace.w),
[`lock.w`](lock.w) e [`deployments/local.w`](deployments/local.w) existem porque
cada manifest ocupa um root exclusivo. Eles são pequenos por design.

[`atlas-manifest.json`](atlas-manifest.json) inventaria cada bloco marcado,
cada família e cada regra pública nomeada da grammar. Ele registra design status,
status de validação, refs de design, refs da Última Luz e digests.
O manifest é a fonte de metadados: os markers nos arquivos `.w` somente ligam
um trecho legível ao ID. Ele também fecha o inventário de variantes aceitas;
uma variante marcada como `current` ainda pode ter somente evidência de parse.
Cada regra pública da grammar recebe uma classificação `direct`, `composed`,
`root`, `lexical` ou `recovery`; uma regra nova sem classificação falha o
checker.

[`CHEATSHEET.md`](CHEATSHEET.md) é gerado dos blocos reais dos arquivos `.w`.
Não edite o cheatsheet. Use o gerador para manter os snippets idênticos.

## Estados e limites

`designStatus: current` identifica forma sintática vigente. Isso não prova
compiler, runtime ou provider.

`evidenceStatus: tree-sitter-parse-only` prova somente o parse Tree-sitter sem
recovery. `tree-sitter-parse-only-provider-missing` e
`tree-sitter-parse-only-compiler-runtime-missing` registram superfícies aceitas
que ainda não possuem a rota de implementação correspondente. Este campo não
é uma alegação de implementação.

Research, reserved e rejected ficam nos companions
[`reserved.w-reserved.txt`](reserved.w-reserved.txt) e
[`rejected.w-rejected.txt`](rejected.w-rejected.txt). Esses arquivos não são W
source, têm status explícito no manifest e o checker não os parseia.

O atlas não altera a semântica normativa. Para contratos, use
[`DESIGN.md`](../../DESIGN.md) e os refs do manifest. Para evidência integrada,
use o mapa de [`reference/last-light/README.md`](../last-light/README.md).

## Verificação

Execute no root do repositório:

```text
bun tooling/syntax-atlas.mjs --check
```

O checker rejeita marker ausente, duplicado ou não listado, digest ou snippet
stale, regra pública não classificada, bloco inválido, root incompatível e
cheatsheet stale. Ele também parseia todos os `.w` do atlas sem recovery.

Para atualizar os artefatos depois de uma alteração aprovada:

```text
bun tooling/syntax-atlas.mjs --write
```

O atlas ajuda a preparar futuras tarefas HUM0 e documentação final. Ele não é
um tutorial completo, um formatter ou uma alegação de execução de W.
