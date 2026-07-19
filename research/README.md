# Catálogo de pesquisa do W

> **Status:** Working Draft
> **Data:** 2026-07-19

## Propósito

Este catálogo reúne hipóteses que não pertencem ao caminho crítico do primeiro
compilador. Ele não transforma uma pesquisa em promessa nem impede que evidência
nova produza componentes úteis.

Os estados seguem o vocabulário de [STATUS.md](../STATUS.md). **Pesquisa** quer
dizer “fora do núcleo v0 e dependente de evidência”, não “aprovado para uma
versão futura”. Quando a baseline atual já exclui uma interpretação universal,
isso é dito explicitamente.

O campo **próximo experimento** descreve a menor prova capaz de produzir dados.
Não é um item de roadmap nem uma decisão de implementação.

## Leitura do catálogo

As decisões correntes estão em [STATUS.md](../STATUS.md), e a regra de produto
está em [VISION.md](../VISION.md): uma ideia só vira semântica depois de
sobreviver a um protótipo, uma alternativa mais simples e análise de portabilidade.
Este arquivo descreve somente hipóteses ainda úteis e seus próximos experimentos.

## Índice

O [programa de pesquisa de longo prazo](long-term-program.md) organiza as
hipóteses por dependência, baseline, gate e onda. As entradas detalhadas abaixo
preservam a história e o próximo experimento de cada ideia original.

| Tema | Estado no mapa atual | Possível destino |
|---|---|---|
| Tagged values e tagged pointers | Pesquisa | otimização de lowering por target |
| Arenas/heaps por módulo | Pesquisa e questão aberta de regiões | allocator/região explícita ou runtime de serviço |
| WC e EmitC | WC público em pesquisa; EmitC em aberto | backend inspecionável para um subset |
| `fn<lang>` | Pesquisa pós-FFI C | ilha da aplicação + adapter de frontend |
| [Property behaviors](property-behaviors.md) | Pesquisa em W-O097 | mecanismo tipado de storage/accessors, se preservar efeitos e ownership |
| WLO/WLON | Pesquisa | literal/serialização canônica opcional |
| wQL, wRPC e RestPC | Pesquisa pós-core | bibliotecas e contratos do ecossistema |
| V6 e Computer Units | Pesquisa separada | runtime/serverless/isolate |
| Numéricos, units, arrays e lowerings científicos | Direção + Pesquisa | core numérico, stdlib e pacotes/lowerings |

## Hipóteses de pesquisa

| Hipótese | Status | Valor | Risco | Próximo experimento |
|---|---|---|---|---|
| musl, Cosmopolitan, instaladores, seccomp, builders herméticos e espelhos | Pesquisa | distribuição e builds mais reproduzíveis | aprisionar cedo targets, sandbox e cadeia de supply | comparar um artefato mínimo em dois targets, com provenance, isolamento e recuperação de cache medidos |
| inspection points de WC e extensões GNU/Clang | Pesquisa | tornar lowering e ABI inspecionáveis | extensões não portáveis e um WC público que vire compromisso de compatibilidade | emitir um subset em C padrão e, separadamente, marcar os pontos que exigiriam extensão em GCC/Clang |
| import dinâmico/configOverride, `#embed`, `#pragma pack`, módulo `fork`/`self` | Pesquisa | cobrir configuração, recursos, layout e isolamento em casos específicos | alterar resolução de módulos, ABI ou semântica sem contrato multiplataforma | prototipar cada mecanismo isoladamente com diagnóstico, provenance e teste em ao menos dois targets |
| Tree strings | Fora de `String` geral | índice/interning especializado | overhead, lifetime e Unicode mal definidos | comparar interning, radix trie e `String` plana |
| SQLite | Pesquisa | adapter oficial ou storage interno de tooling | dependência universal e claims fora do workload | comparar cache/tooling e adapter explícito com alternativas |
| GPU e HDL | Pesquisa pós-pipeline CPU | lowering/target especializado | portabilidade e semânticas de memória distintas | um kernel puro com baseline CPU e fallback |
| Snapshots, PGO e autotests | Pesquisa de tooling | testes executáveis e profiling | oracle circular, dados frágeis ou privados | doc-test e snapshot explícito antes de PGO/IA |
| Unidades e computação científica | Pesquisa com corpus térmico | fórmulas verificáveis e target CPU eficiente | feature soup, float não reproduzível e ABI difícil | type-check dimensional + lowering escalar do forno em dois targets |

## Tagged values e tagged pointers

**Status:** Pesquisa. Tagged pointers para escalares e small values são uma
otimização possível de target, nunca uma representação obrigatória. Isso não deve
ser confundido com `enum`/tagged union na semântica de tipos: a baseline atual
mantém `struct`, `object`, `enum` e `protocol` como conceitos distintos.

**Valor potencial:** aproveitar bits disponíveis ou uma representação compacta
para guardar estado, small integers, tags de ownership ou valores imediatos pode
reduzir indireções, alocações e tamanho de objetos em workloads específicos. Os
protótipos C também exploram checks, operações escalares e promoção para estado
compartilhado.

**Risco:** layouts de ponteiro e espaço disponível variam por arquitetura, ABI e
ferramentas. Misturar valor, erro, null, refcount e ownership na mesma palavra
aumenta o custo de prova e pode prejudicar FFI, debuggers, sanitizers e evolução.
Uma otimização dessas não pode alterar range numérico, checks ou semântica entre
targets, debug e release.

**Próximo experimento:** implementar uma representação imediata e uma fallback
convencional para o mesmo tipo interno; medir memória, branches e tempo em
x86_64 e arm64, com testes diferenciais de overflow, FFI e sanitizers. O resultado
precisa justificar a complexidade sem aparecer no source.

## Arenas e heaps por módulo

**Status:** Pesquisa, relacionada às questões abertas de compartilhamento e
regiões. A baseline atual diz que módulo é namespace/unidade de build, não heap
singleton ou lifecycle de runtime obrigatório. Portanto, `process.flush(module)`
e “uma heap por import” não são semântica candidata universal.

**Valor potencial:** regiões permitem desalocação em lote, locality e custo
previsível em ciclos como request, frame, job ou isolate. Uma arena associada a
um serviço pode simplificar cleanup após cancelamento e evitar refcount em dados
que comprovadamente não escapam.

**Risco:** o lifetime de um namespace não coincide necessariamente com o dos
valores. Retornos, callbacks, tasks, FFI, compartilhamento e ciclos podem escapar
da região. Flush global com trabalho paralelo ativo pode invalidar dados; arena
universal também retém objetos de vida curta até o fim da região e esconde custos.

**Próximo experimento:** comparar allocator normal, ARC e uma região explícita em
três casos pequenos — request HTTP, pipeline concorrente cancelável e batch de
objetos temporários. O teste deve registrar escapes, peak memory, tempo de cleanup
e comportamento na fronteira C.

## WC e EmitC

**Status:** WC como linguagem intermediária pública permanece em pesquisa. A
baseline arquitetural preserva a semântica num dialeto W/MLIR; o uso de EmitC
como backend de inspeção ou portabilidade para um subset é uma questão aberta.

**Valor potencial:** uma saída semelhante a C é fácil de ler, depurar, integrar
com toolchains existentes e usar durante bootstrap. EmitC pode oferecer esse
caminho sem fazer de C o IR semântico primário.

**Risco:** baixar cedo demais para C perde ownership, erros tipados, tasks,
efeitos e layout ainda necessários aos passes. Cobrir toda a linguagem pode
reintroduzir undefined behavior, diferenças de ABI e uma segunda implementação
do backend. Um “WC” público também cria compatibilidade que o projeto teria de
manter.

**Próximo experimento:** selecionar um subset mínimo — inteiros, records, controle
de fluxo e chamadas sem async — e comparar o lowering W/MLIR → EmitC → C com o
lowering W/MLIR → LLVM. O teste deve medir equivalência observável, clareza do
output, qualidade de debug e os pontos que não podem ser representados.

## Corpos multilíngues `fn<lang>`

**Status:** Pesquisa posterior a uma FFI C segura. A baseline usa `foreign c`
para a primeira fronteira externa; `fn<C>`, `fn<JS>`, `fn<TS>`, `fn<Rust>`,
`fn<Zig>` e variantes de GPU não estão no parser mínimo.

**Valor potencial:** manter ilhas de código legado/especializado dentro da
aplicação permite migração gradual e pontes pequenas sem exigir uma library
externa ou uma conversão W idiomática imediata. A ideia se aproxima de `asm` em
C, mas delega um body completo ao frontend da linguagem indicada.

**Risco:** cada linguagem adiciona parser, formatter, toolchain, cache, sandbox,
debug, source maps, regras de ownership, modelo de erro e matriz de targets. A
conversão “automática” pode esconder cópias ou lifetimes inválidos. Código
embutido também complica provenance e reprodutibilidade.

**Próximo experimento:** concluir primeiro uma chamada `foreign c` com tipos,
ownership e erros explícitos. Depois, implementar um adapter C e uma ilha inline
delimitada, com toolchain fixado na receita, e comparar o resultado com o mesmo
source da aplicação em arquivo separado. O
[experimento do equipamento do restaurante](../examples/restaurant/multilingual.md)
mantém `foreign c`, body inline, `fn<C> from`, namespace e adapter declarado lado
a lado sem promover nenhuma forma.

## WLO/WLON

**Status:** Pesquisa fora do parser mínimo. O nome ainda não está estabilizado;
os documentos atuais agrupam `WLO/WLON` como um único espaço de decisão.

**Valor potencial:** uma notação capaz de representar literais W poderia servir
a parse/stringify tipado, resultados de comptime, fixtures, metadata e comunicação
W-a-W sem perder enums ou records conhecidos pelo schema.

**Risco:** uma nova notação precisa de gramática canônica, versionamento, limites,
schema evolution e parser seguro. Sem vantagem medida, ela duplica JSON ou outros
codecs. Reutilizar diretamente toda a sintaxe W pode tornar deserialização tão
complexa quanto o compilador e confundir dados com código.

**Próximo experimento:** especificar apenas scalars, String UTF-8, bytes, listas,
records, opcionais e enums; exigir round-trip e encoding determinístico; fazer
fuzz do parser; comparar tamanho, velocidade e evolução de schema com JSON. O
experimento deve poder ser removido sem alterar a linguagem.

## wQL, wRPC e RestPC

**Status:** Pesquisa de ecossistema, não keywords nem protocolo universal do W.
As notas misturam contrato, query, transporte, codecs e HTTP; o primeiro trabalho
é separar essas camadas.

**Valor potencial:** contratos tipados compartilhados entre cliente e servidor,
chamadas remotas previsíveis, introspecção, queries projetadas e adapters HTTP
podem tornar serviços W agradáveis sem repetir schemas manualmente.

**Risco:** um protocolo próprio exige versioning, autenticação, autorização,
errors, retries, framing, streaming, limits, codegen e interoperabilidade. Acoplar
query a HTTP ou serialização a layout de memória cria promessas incompatíveis.

**Próximo experimento:** representar um contrato unary como dados, gerar stubs
in-process e depois transportar a mesma chamada em JSON por loopback, sem query
DSL e sem retry implícito. Só depois comparar um envelope wRPC e um perfil RestPC.

A decomposição de trabalho está em
[services-and-protocols.md](../ecosystem/services-and-protocols.md).

## V6 e Computer Units

**Status:** Pesquisa separada de runtime/serverless. Não faz parte da linguagem,
do wRPC nem do protocolo de serviços. A relação exata entre V6 e Computer Units
nunca foi especificada nas fontes.

**Valor potencial:** isolates com budgets de CPU/memória/storage e entrypoints
como `fetch`, `cron` ou `main` podem fornecer deployment previsível e um modelo
operacional simples para serviços. CUs também exploram paralelismo por unidades
isoladas em vez de compartilhar tudo por threads.

**Risco:** criar engine JavaScript, runtime serverless, scheduler, sandbox e até
módulo de kernel é um produto muito maior que uma linguagem. Números como 128 MB,
uma vCPU e limites de KV são hipóteses, não contratos. “Módulo de kernel” é
ambíguo e, se literal, amplia muito a superfície de segurança e portabilidade.

**Próximo experimento:** um worker em user space com um único entrypoint, limite
de memória/CPU e mensagens explícitas. Medir startup, isolamento, cancelamento e
comunicação. Não é necessário criar um engine JS nem código de kernel para testar
o valor de uma CU.

## Tree strings

**Status:** fora da representação de `String` geral. Continua pesquisável apenas
como estrutura especializada para interning, prefix indexes ou identificadores.
`String` na baseline é UTF-8 válido com views explícitas.

**Valor potencial:** compartilhar prefixos e internar identificadores repetidos
pode reduzir duplicação em compiladores, registries, paths ou datasets com forte
repetição.

**Risco:** a estrutura descrita passa de árvore a grafo quando há múltiplos pais,
e uma trie de prefixos não resolve busca geral de substrings. O path depende da
ordem de inserção, os exemplos têm overhead maior que a string plana, e faltam
lifetime, mutação, concorrência, Unicode, serialização e ABI C.

**Próximo experimento:** limitar o domínio a símbolos imutáveis do compilador e
comparar tabela de interning, radix trie e `String` UTF-8 plana em memória, lookup
e locality. Nenhuma representação deve ser observável pela API de `String`.

## SQLite como storage

**Status:** Pesquisa como adapter oficial ou storage interno de tooling. SQLite
não é storage obrigatório de módulos, serviços ou aplicações W.

**Valor potencial:** transações, WAL, índices, uma API única e um arquivo portátil
podem simplificar caches do compilador, logs, registries locais e serviços que já
têm um modelo relacional/KV, além de oferecer coordenação transacional e uma
interface `get`/`put`/`list` conveniente.

**Risco:** toda aplicação não tem o mesmo padrão de persistência. Tornar SQLite
implícito adiciona dependência, IO e semântica transacional onde arquivos, memória,
KV remoto ou outro banco podem ser melhores. Desempenho depende do workload e não
de uma porcentagem geral.

O número de “35% mais rápido que o filesystem” vem de uma
[medição oficial do SQLite](https://www.sqlite.org/fasterthanfs.html) para leitura
e escrita de blobs pequenos, próximos de 10 KB. A própria página trata o valor
como aproximado, dependente de hardware/workload, e registra exceção com cache
frio. É uma hipótese para benchmark, não justificativa para storage universal.

**Próximo experimento:** implementar duas provas independentes: índice/cache do
toolchain e adapter explícito de serviço. Comparar com filesystem/content-addressed
storage em cold start, concorrência, corrupção, tamanho e operação offline.

## GPU e HDL

**Status:** Pesquisa posterior ao pipeline CPU nativo. GPU, OpenMP, CUDA/HIP,
Bend e HDL não fazem parte da primeira semântica nem da primeira matriz de targets.
A nota de HDL é apenas uma pergunta, sem modelo proposto.

**Valor potencial:** lowerings especializados podem acelerar kernels paralelos e
preservar uma intenção portável, com fallback CPU e custos consultáveis. MLIR pode
ser uma infraestrutura comum para explorar targets sem colocar um vendor na
sintaxe geral.

**Risco:** memória, sincronização, precisão numérica, divergência, errors e
toolchains variam radicalmente. Traduzir assembly CPU mecanicamente para GPU não
estabelece semântica equivalente. HDL acrescenta timing, hardware e verificação
que não foram delimitados.

**Próximo experimento:** escolher um kernel puro e pequeno, explicitar inputs e
outputs, produzir CPU baseline e um lowering GPU, e comparar resultado, transfer
cost e diagnósticos quando não houver target. HDL só ganha experimento depois de
existirem casos de uso e um contrato observável próprio.

## Snapshots, PGO e autotests

**Status:** Pesquisa de tooling futuro sobre testes e documentação executável.
Não é uma propriedade mágica de função nem uma autorização para IA modificar
testes sem revisão.

**Valor potencial:**

- snapshots determinísticos podem servir de fixture/regressão;
- exemplos de documentação executados evitam docs desatualizadas;
- perfis capturados podem alimentar PGO com workloads conhecidos;
- geração assistida pode sugerir casos, fuzz inputs e benchmarks iniciais;
- `.test.w` separado pode testar artefatos por sua interface pública.

**Risco:** cache e snapshot confundidos com semântica podem esconder side effects;
PGO depende de representatividade; testes aleatórios/gerados podem ser frágeis ou
validar a implementação contra ela mesma. Execução remota traz autenticação e
isolamento. LLMs podem expor código/dados, inventar oráculos e gerar ruído no Git.

**Próximo experimento:** começar por doc-tests e snapshot files explícitos,
determinísticos e revisados. Guardar dados de profiling como artefato separado da
semântica da função. Um gerador pode apenas propor testes em diff, nunca congelar
ou substituir automaticamente o oracle aceito.

## Como uma pesquisa pode avançar

Uma entrada só deve sair deste catálogo quando houver:

1. problema e métrica observáveis;
2. implementação pequena com fallback;
3. comparação com alternativa mais simples;
4. comportamento em erro, cancelamento, FFI e pelo menos dois targets;
5. impacto no parser, formatter, metadata e package/build documentado;
6. uma decisão registrada em `W/design/decisions/` quando houver código que a
   sustente.

Até lá, preservar a ideia com honestidade é mais útil que fazê-la parecer pronta.
