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
digest. A máquina fecha W-001–W-1450, valida W-1451–W-1453 como
`oracle-backed-current` e exige a lista Research global vazia depois de DRC0.
Não há payload copiado,
resultado humano/modelo ou evidence de implementação.
