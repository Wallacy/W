# HRD0 — hot reload somente para desenvolvimento

Status: `design-oracle-input`. HRD0 não adiciona syntax, profile de source ou
modo dinâmico de release. O objeto estudado é um runner de tooling que observa
mudanças em units W normais, recompila/reabre essas units com o frontend
existente e publica uma troca limitada por `prepare → validate → preflight →
ready → switch`.

O problema é operacional: durante o desenvolvimento, uma edição deve aparecer
sem aceitar completion, message ou capability da generation antiga. A generation
antiga fica retida até fechar admissão e terminar drain. A nova recebe roots
novos. Nenhum heap, task, loan, frame, `ServiceRef`, callback ou provider handle
é migrado. Rollback só existe antes de publication e exige receipt estruturado;
falha de drain depois de publication é `degraded`/fault e nunca rollback.

| Rota | Problema | Disposição |
| --- | --- | --- |
| A | unit normal, snapshot REPL e falha bounded | composição existente; evidência de implementação ainda ausente |
| B | generation de service/plugin tipado, ABI/closure/schema e local/split | composição existente de DYN1; reducers independentes |
| C | generated content-addressed module set and invocation ownership | Historical SYN1 provenance; generated units must reopen and pass checks; W-1398/W-1399 own the current implementation evidence gap and no CLI spelling is selected |
| D | production reload, eval/exec, monkey patch, active frame, native sandbox e `dlclose` vivo | intencionalmente rejeitado |

O corpus e o reducer são host-only. Eles preservam package identity, recipe,
artifact, source map, `SemanticInterfaceKey`, `ServiceIRKey`, `WAbiKey`,
`RuntimeClosureKey`, schema, effects, capabilities, quotas, isolation e receipts
como facts fechados. A comparação local/split usa duas reduções separadas e
compara somente o resultado lógico; trace físico pode divergir. O corpus tem 20
casos e cinco mutations adversariais para step físico extra, step lógico ausente,
ordem errada, nominal duplicado e interface digest drift. O contrato comum
[`hot_reload_dev_contract.w`](../../../reference/last-light/hot_reload_dev_contract.w)
declara os tipos nominais usados pelos dois witnesses. `process`, Wasm
ou component são exigidos para código não confiável; native dylib não é sandbox.

O bundle [`bundle.json`](bundle.json) usa duas variantes source-shaped, tasks e
oracle como evidência de estudo; não contém participantes ou resultados humanos.
HRD0 reutiliza DYN1, SYN1 e CAP0, em vez de duplicar seus contratos. Os dois
witnesses Last Light (`hot_reload_dev_local.w` e `hot_reload_dev_split.w`)
importam o contrato comum, são fixtures parseáveis e não executam reload. A ausência de compiler, runtime,
provider, isolamento real, stress e estudos humano/modelo permanece explícita.

Checks direcionados:

```sh
bun test tooling/studies/hrd0-hot-reload-dev/oracle.test.mjs
bun tooling/check-hot-reload-dev.mjs
```

O checker aceita `--write` somente para atualizar o snapshot host-derived. Não
use o snapshot como prova de implementação. Pare se houver stale publication,
cleanup/leak/unbounded resource, divergence local/split, identidade/receipt
forged, migração de estado vivo, rollback pós-publication, profile de release
dinâmico ou qualquer autoridade ambiental.
