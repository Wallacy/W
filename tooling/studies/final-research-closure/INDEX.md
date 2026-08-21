# FRC0 — índice de artefatos do snapshot histórico

| Artefato | Função |
|---|---|
| `bundle.json` | Bundle canônico `w-substitution-study-bundle-1` |
| `manifest.json` | Cadeia fechada de artefatos e digests |
| `current.w` / `adversarial.w` | Fixtures W finas e parseáveis |
| `oracle.test.mjs` | Testes do contrato host-only |

Os payloads permanecem em `tooling/frontend-freeze-cases.json`,
`tooling/design-freeze-classification.json` e
`tooling/hum0-human-review-protocol.json`. FRC0 referencia esses arquivos por
digest. A máquina fecha somente W-001–W-1450 com `Research=0` e checa a
reabertura explícita PFU0 de W-1451–W-1453. Não há payload copiado, resultado
humano/modelo ou evidence de implementação.
