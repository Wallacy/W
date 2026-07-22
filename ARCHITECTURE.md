# Arquitetura de longo prazo do W

> **Status:** **Direção** para separação de camadas; componentes concretos variam
> entre **Candidato**, **Em aberto** e **Pesquisa** · 21 de julho de 2026
>
> Compilador, runtime e stdlib ainda não existem. Este documento define fronteiras
> que permitem construí-los e substituí-los sem transformar a primeira
> implementação na definição acidental da linguagem.

O W é mais do que uma gramática, mas não deve virar um produto monolítico. A
linguagem, o compilador, o SDK, o runtime, o build, a distribuição e o tooling
compõem um sistema por contratos versionados. Cada camada pode avançar em ritmos
diferentes sem esconder mudanças semânticas.

Os fatos detalhados continuam nos documentos canônicos linkados abaixo. Este
mapa registra as fronteiras e compromissos que precisam sobreviver a v0.

## Sete planos do sistema

| Plano | Responsabilidade | Não pode assumir |
|---|---|---|
| linguagem | source, tipos, efeitos, ownership e comportamento observável | detalhes de MLIR, allocator, scheduler ou SO |
| implementação | frontend, HIR, verificadores, lowerings e backends | que sua AST/IR interna é formato público eterno |
| execução | cleanup, tasks, execution domains, host entry adapters, I/O, panic, tracing e serviços | um event loop, heap, thread model ou sandbox universal |
| SDK | T0 foundation, T1 systems e T2 domains oficiais | disponibilidade idêntica em todo target |
| build/package | resolução hermética, profiles, cache, recipes e artefatos | rede durante compilação ou estado não declarado do host |
| distribuição | mirrors, updates, provenance, rebuilds e policies | que assinatura, reprodução e auditoria provam a mesma coisa |
| experiência | formatter, docs, testes, LSP, debugger, lens e portal | inferir semântica raspando texto ou logs humanos |

Governança e evolução atravessam os sete planos: editions, schemas, suporte de
targets, vulnerabilidades e deprecações precisam de política explícita.

## Contratos que devem permanecer separados

### Source e edition

Um package declara sua edition. A edition pode alterar parsing, nomes implícitos,
lints e forma canônica, mas packages de editions suportadas continuam compondo
pela mesma HIR conceitual. Migrações devem ser mecanizáveis e revisáveis; uma
edition não autoriza mudança silenciosa de resultado, complexidade assintótica ou
efeitos.

### Semântica e representação

Tipos, ownership, errors e tasks são definidos antes de layout. Niches, pointer
tags, ARC elidido, arenas, stack promotion e SIMD são estratégias *as-if* com
fallback convencional. Nenhum target perde correção porque uma otimização não
existe nele. O contrato detalhado está em
[tipos e memória](spec/types-and-memory.md) e na
[estratégia híbrida](design/memory-strategy.md).

### Interface de módulo e ABI

Há quatro superfícies diferentes:

1. API source pública;
2. interface compilada versionada para type checking e build incremental;
3. ABI W de um profile/target, inicialmente instável;
4. ABI C explícita em `foreign c`.

Um cache interno pode ser descartado; uma interface publicada precisa de reader,
versionamento e migração; uma ABI estável só existe para targets e profiles que a
declarem. Source-first permanece o fallback quando não houver compatibilidade
binária. A experiência de resilience do Swift mostra por que layout opaco e
distribuição binária têm custos próprios, não um checkbox global.

### Runtime e aplicação

`libwrt` é uma família de componentes, não uma máquina virtual obrigatória:

- `core`: panic, allocation hooks, metadata mínima e suporte a cleanup;
- `task`: scopes, cancelamento, timers e executors;
- `platform`: filesystem, rede, clocks e integração de target;
- `service`: instances, mailboxes, calls e adapters duráveis quando usados;
- `observe`: tracing, symbolization e task stacks lógicas.

Reachability remove componentes não usados. Um profile freestanding pode não ter
T1, tasks ou services. Um host pode substituir adapters preservando o contrato.

### Entry descriptor e mundo do host

API W, handler, entry descriptor e product são interfaces separadas. O descriptor
liga slots versionados de um host a funções comuns e registra signatures,
effects, capabilities, isolation e lifecycle. O product escolhe o principal e o
adapter nativo/WASI/test; nenhuma camada procura uma função por nome mágico.

Um host world declara o que chama e o que fornece. Assim, CLI, HTTP, UI, HID e
um component sandboxed podem compor a mesma linguagem sem acrescentar seus
eventos como keywords. Contexts são capability types; um import de código nunca
preenche sozinho um import de authority. A superfície candidata está em
[módulos](spec/modules.md#entrypoints-e-profiles-de-host).

### Tensor lógico, storage e device

Shape/dtype e semântica numérica pertencem ao valor lógico; ownership/alias e
views pertencem ao contrato de memória; strides/layout/address space pertencem à
representação; device transfer e executor pertencem ao runtime/profile. Uma API
ou lowering pode otimizar todas as camadas junto sem fingir que são o mesmo tipo
de decisão.

StableHLO/ONNX são formatos/adapters de interchange. MLIR tensor/linalg/vector e
GPU são lowerings. Nenhum deles se torna a semântica source universal do W. A
baseline CPU correta continua disponível quando model compiler, BLAS ou GPU não
existirem. Veja [numéricos e ML](design/numerics-and-quantities.md#arrays-matrices-tensors-e-ml).

### Artefato, recipe e evidência

Payload executável, envelope da plataforma, recipe, SBOM, provenance, assinatura
e attestations são objetos distintos. O mesmo source só promete o mesmo payload
quando todos os inputs semânticos, toolchain, target e environment declarado são
iguais. Data, paths, locale, ordenação e randomness não declarados são bugs de
reprodutibilidade. Veja [packages](design/packages.md) e
[releases verificáveis](design/verification-and-releases.md).

### Diagnóstico e protocolo de tooling

Texto para humanos é uma projeção. Diagnostics, semantic tokens, edits,
explicações de ownership/effects, task trees e custos usam schemas versionados.
LSP e DAP são adapters; não se tornam o modelo interno. Debug symbols podem ficar
em sidecar removível, mas build ID, source mapping e provenance continuam
associáveis ao payload.

## Invariantes de longevidade

1. A especificação e o corpus vencem qualquer implementação individual.
2. Toda otimização possui representação correta sem ela e teste diferencial.
3. Portabilidade é declarada por `target + profile + capabilities`, nunca por uma
   promessa vaga de “roda em tudo”.
4. Build não consulta relógio, rede, diretório, locale ou randomness sem que o
   valor participe da recipe.
5. Importar código não concede filesystem, rede, environment, clock ou processo.
6. Concorrência, paralelismo, localidade e isolamento continuam conceitos
   separados mesmo quando o runtime usa o mesmo mecanismo físico.
7. Código `debug`, `release`, instrumentado e otimizado preserva a mesma semântica;
   somente custo e observabilidade adicional podem variar.
8. Artefatos publicados informam exatamente o que é compatível, verificável,
   reproduzido e auditado, sem nota agregada enganosa.
9. Unicode, SI, time zones, certificados e outros dados versionados registram a
   versão usada e não mudam retroativamente dentro de uma edition/build recipe.
10. Ferramentas de IA consomem os mesmos fatos estruturados que IDEs e humanos;
    sugestões nunca ganham authority especial sobre testes ou policies.
11. Export W, handler, entry de host e principal de product nunca são inferidos
    como sinônimos por coincidência de nome.
12. Tensor operations não escondem host↔device transfer, materialização de view
    ou mudança de guarantees numéricas.

## Perfis e matriz de suporte

Uma linha de suporte precisa declarar ao menos:

```text
architecture + operating environment + ABI + profile + SDK capabilities
+ compiler/runtime version + status + test evidence
```

Estados de target propostos:

| Estado | Garantia mínima |
|---|---|
| experimental | compila parte declarada; sem compromisso de upgrade |
| tier 3 | build conhecido; manutenção comunitária e CI opcional |
| tier 2 | CI compila e executa corpus relevante; releases podem faltar |
| tier 1 | CI obrigatória, releases oficiais e regressão bloqueante |
| long-term | janela de suporte, patches de segurança e política de deprecação publicadas |

O número do tier não mede segurança do programa. Targets especiais podem ser
excelentes dentro de um profile estreito e ainda não oferecer rede, threads,
dynamic linking ou Unicode completo.

## Bootstrap e diversidade de implementação

O primeiro seed usa o profile W-owned `w-seed-c`, baseado em C11 e validado em
mais de um compilador. [Dependable C](https://dependablec.org/) informa sua
matriz de compatibilidade e legibilidade, mas não é um compilador nem uma licença
para depender de undefined behavior. O próprio projeto se descreve como o oposto
de um dialeto; por isso “profile de seed” é um nome mais preciso para W.

Após o primeiro self-host:

- uma versão W estável anterior é a rota normal de bootstrap;
- o seed C continua pequeno, auditável e capaz de reconstruir um estágio útil;
- builds em estágios comparam outputs e registram divergências;
- ao menos dois compiladores C e dois targets convencionais exercitam o seed;
- a suíte de conformidade deve permitir uma segunda implementação sem copiar
  detalhes internos da primeira.

MLIR permanece uma dependência fixada, não uma ABI W. Sua API C ainda não oferece
garantia de estabilidade e cobre deliberadamente um núcleo pequeno; C++/TableGen
pode existir atrás de um adapter estreito. Bytecode MLIR pode ser cache ou
intercâmbio versionado do toolchain, mas uma evolução do dialeto W exige hooks e
tests próprios, não confiança automática no container.

## Segurança por fronteiras

Safe W reduz classes de memory/data-race bugs, mas não promete segurança total.
O threat model separa:

- safety de linguagem e invariantes da HIR;
- capability/authority concedida a uma instância;
- isolamento no boundary escolhido pelo deployment;
- sandbox de build/comptime;
- confiança em toolchain e dependências;
- atualização, revogação e resposta a vulnerabilidades.

Seccomp é defesa Linux por processo; não isola uma static library importada. WASM
e WASI podem fornecer um profile sandboxed e component boundaries, mas não são a
semântica universal do runtime. Policies portáveis descrevem capabilities; cada
host publica o enforcement real e suas lacunas.

## Evolução, governança e compatibilidade

Antes de 1.0, o projeto precisa publicar:

- processo de proposta com problema, alternativas, prototype gate e decisão;
- política de editions, deprecation, migrations e janela de suporte;
- matriz de source, module, ABI, package e runtime compatibility;
- ownership dos schemas e política de reader/writer versioning;
- suporte/depreciação de targets e SDK domains;
- security contact, embargo, advisories e revogação;
- licença, marca, namespaces e regras contra dependency confusion/squatting;
- conformance suite e regra para implementações independentes.

Nenhum desses itens exige criar uma fundação antes do compilador. Exige apenas
que decisões difíceis não sejam codificadas irreversivelmente por ausência de
política.

## Horizontes de entrega

| Horizonte | Resultado | O que permanece fora |
|---|---|---|
| H0 · contrato | H01–H14 + addendum W-O100–W-O103, corpus, formatter e diagnostics | performance e ABI pública |
| H1 · toolchain nativa | seed, self-host, CPU nativo, C FFI, T0 e package local | registry e services distribuídos |
| H2 · runtime produtivo | ownership completo, tasks, T1, debugger e profiles de target | ABI universal e serverless próprio |
| H3 · ecossistema verificável | registry/mirrors, rebuilds, policies, SDK oficial e conformance | score mágico de segurança |
| H4 · especialização | services duráveis, WASM, ciência, GPU ou outros domains comprovados | feature sem workload/fallback |

O [roadmap](ROADMAP.md) define a ordem implementável; o
[programa de pesquisa](research/long-term-program.md) define como hipóteses de
longo prazo produzem evidência sem bloquear essa ordem.

## Referências primárias

- [MLIR C API](https://mlir.llvm.org/docs/CAPI/)
- [MLIR bytecode format](https://mlir.llvm.org/docs/BytecodeFormat/)
- [Rust editions](https://doc.rust-lang.org/edition-guide/editions/)
- [Swift library evolution](https://www.swift.org/blog/library-evolution/)
- [Unicode normalization](https://www.unicode.org/reports/tr15/)
- [Unicode security mechanisms](https://www.unicode.org/reports/tr39/)
- [WebAssembly Component Model](https://component-model.bytecodealliance.org/)
- [WASI](https://wasi.dev/)
- [Language Server Protocol](https://microsoft.github.io/language-server-protocol/)
- [Debug Adapter Protocol](https://microsoft.github.io/debug-adapter-protocol/)
- [Reproducible Builds](https://reproducible-builds.org/docs/)
- [The Update Framework](https://theupdateframework.github.io/specification/latest/)
