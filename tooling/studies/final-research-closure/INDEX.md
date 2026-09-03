# FRC0 — índice do fechamento histórico

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
preserva o snapshot que listava W-1486 e W-1503 como gates históricas. W-1517
fecha W-1503 e W-1518 fecha/supersede W-1486; a classificação corrente possui
residual de research exatamente `[]`. W-1517 e W-1518 são closures design-only,
sem claim de implementação. O fechamento histórico Research=0 permanece
válido até W-1459. Não há payload copiado,
resultado humano/modelo ou evidence de implementação.
