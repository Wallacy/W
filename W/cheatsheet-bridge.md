# Cheatsheet do W

Esta rota foi preservada para não quebrar links antigos. O rascunho textual que
existia aqui promovia hipóteses históricas — como “tudo é enum”, módulos como
singletons/arenas obrigatórios e vários estados sentinela — que não representam
mais o desenho atual.

A antiga cheatsheet evoluiu para a [POC do portal em Bun](../../W/portal/README.md).
Para abri-la:

```powershell
cd W\portal
bun run start
```

Depois acesse `http://localhost:3000`.

Use os seguintes documentos como autoridade:

- [STATUS.md](../../W/STATUS.md) para distinguir decisões, candidatos e pesquisa;
- [LANGUAGE_TOUR.md](../../W/LANGUAGE_TOUR.md) para a apresentação legível da sintaxe;
- [spec/syntax.md](../../W/spec/syntax.md) para o rascunho sintático;
- [spec/types-and-memory.md](../../W/spec/types-and-memory.md) e
  [spec/concurrency.md](../../W/spec/concurrency.md) para comportamento e runtime.

A interface visual é uma projeção desses documentos. Quando houver conflito, a
spec e o registro de decisões vencem.
