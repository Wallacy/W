# FRC0 — índice do snapshot histórico

| Artefato | Função |
|---|---|
| `bundle.json` | Bundle canônico `w-substitution-study-bundle-1` |
| `manifest.json` | Cadeia fechada de artefatos e digests |
| `current.w` / `adversarial.w` | Fixtures W finas e parseáveis |
| `oracle.test.mjs` | Testes do contrato host-only |

Os payloads permanecem em `tooling/frontend-freeze-cases.json`,
`tooling/design-freeze-classification.json` e
`tooling/hum0-human-review-protocol.json`. FRC0 referencia esses arquivos por
digest. A máquina fecha W-001–W-1450, valida W-1451 como
`oracle-backed-current`, W-1452/W-1453 como superseded por W-1480/W-1516 e
exige exatamente W-1486 e W-1503 como research gates
post-snapshot depois de DRC0. O fechamento histórico Research=0 permanece
válido até W-1459. Não há payload copiado,
resultado humano/modelo ou evidence de implementação.
