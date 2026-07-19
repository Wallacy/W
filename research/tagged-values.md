# Tagged values como otimização de target

> **Status: Research · 18 de julho de 2026**

Este documento preserva a hipótese investigada pelos spikes de março de 2025: alguns valores dinâmicos, opcionais ou referências poderiam ocupar uma única palavra usando bits que uma combinação específica de arquitetura, sistema operacional e ABI deixa disponíveis.

A hipótese é de **representação interna target-specific**. Ela não define a semântica de W, não torna “tudo um enum”, não reduz a precisão dos tipos numéricos normais e não pode ser requisito para executar um programa.

## Hipótese

Em workloads que misturam pequenos escalares e referências, uma representação compacta pode reduzir:

- alocações para boxing;
- tamanho de arrays heterogêneos;
- tráfego de memória e pressão de cache;
- indireções para consultar tipo, estado ou payload pequeno;
- espaço de `Option<Ref>` quando já existe um niche válido.

O benefício só existe se encode/decode, branches adicionais e restrições de ABI custarem menos que a representação portátil. Essa relação deve ser medida em programas reais; “caber em 64 bits” não é, sozinho, um resultado.

## Objetivos do experimento

1. Medir quando `Any`, `Option<Ref>` e referências internas ganham com uma palavra tagged.
2. Preservar exatamente a semântica e todos os bits observáveis do valor lógico.
3. Selecionar a representação tarde no lowering, depois de conhecer target, ABI, sanitizers e fronteiras públicas.
4. Manter uma representação portátil sempre disponível e coberta pelos mesmos testes.
5. Tornar a escolha consultável em metadata/diagnósticos, sem exigir sintaxe diferente no source.
6. Rejeitar a otimização quando conflitar com segurança de memória, provenance de ponteiro, tooling ou interoperabilidade.

## Não objetivos

- mudar `i64`, `u64`, `f32` ou `f64` para tipos de 62 bits;
- representar erro, ausência, vazio e não inicialização com sentinelas universais;
- usar tags como substituto para ownership ou memory reclamation;
- prometer que referências tagged são atômicas ou lock-free;
- estabilizar um layout tagged como ABI pública;
- depender de LAM, UAI, TBI, alinhamento ou largura de endereço para a correção do programa;
- aplicar a mesma representação a todos os tipos e targets.

## Matriz de experimentos

| Experimento | Pergunta investigada |
|---|---|
| promoção para fallback | É possível combinar endereço, small tags, null e estado shared em uma palavra e promover para uma estrutura indireta quando os bits acabam? |
| largura de endereço | Quantos bits sobram em layouts com endereços virtuais de 48, 52, 56 ou 57 bits? |
| classes de payload | Como distinguir scalar, compound, error e shared e quando usar uma estrutura auxiliar? |
| operações | Operações aritméticas e transições de estado podem atuar diretamente sobre um encoding de uma palavra, com variantes atômicas? |
| precisão numérica | Qual perda aparece ao retirar bits de um `double` IEEE-754 e por que o fallback deve preservar toda a precisão? |
| intermediários largos | Tipos de 128 bits ajudam em intermediários, checks ou payloads que não cabem no encoding compacto? |

Cada linha exige uma implementação portátil de controle e outra otimizada. Elas
não são camadas presumidas de um único runtime.

## Separação entre semântica e representação

O frontend e o HIR devem representar tipos lógicos, não o truque de bits escolhido pelo backend.

### `Option<T>`

`Option<T>` continua tendo exatamente `.some(T)` e `.none`.

- `Option<NonNullRef>` pode usar o ponteiro nulo como niche quando isso for válido internamente.
- Um `Option<T>` sem niche usa o fallback equivalente a `{ tag, payload }`.
- Nested options e valores cujo zero é válido não podem ser achatados de forma a perder casos.
- Em `repr(C)`, persistência ou ABI versionada, o layout é o declarado pela fronteira, não o melhor niche local.

### `Any`

`Any` é o principal candidato a tagged scalars porque já precisa carregar identidade de tipo.

- O fallback portátil é uma tag explícita acompanhada de payload inline ou ponteiro para box.
- A variante compacta pode guardar alguns inteiros, bools ou referências diretamente.
- Um valor que não cabe é boxed sem alterar seu tipo lógico.
- Type identity, tracing e ownership não podem depender apenas de bits roubados do endereço.

### Objetos no heap

Uma referência a objeto continua sendo uma referência com provenance, lifetime e ownership definidos.

- Metadata pode ficar em header, handle table ou descritor lateral no fallback.
- Bits livres podem funcionar como cache de estado, nunca como única cópia de informação necessária para liberar ou validar o objeto.
- Refcount, caso exista, pertence ao modelo de ownership compartilhado e precisa de largura, overflow, ordenação e reclamation próprios.

### Tipos numéricos normais

`i64` e `u64` preservam 64 bits; `f64` preserva todos os padrões de bits IEEE-754 suportados pelo target, incluindo `-0`, infinities e payloads de NaN quando observáveis pelas operações definidas.

Se um `Any` compacto não puder armazenar um `i64` ou `f64` integralmente, ele usa box ou payload lateral. Truncar mantissa, reservar extremos como null/error ou reduzir o range deixa de ser otimização transparente e, portanto, está fora desta hipótese.

## Riscos concretos encontrados

### Bitfields e layout C

Os spikes sobrepõem `uintptr_t` a bitfields. Ordem, packing, alinhamento e acesso a unions variam por ABI e implementação. A versão experimental deve usar masks/shifts explícitos sobre inteiros sem sinal, `memcpy`/bitcast definido e `static_assert` para cada layout. Bitfields não podem ser o formato normativo.

### Largura de endereço, LAM, UAI e TBI

Detectar uma feature na CPU não prova que o processo pode usá-la. É necessário distinguir:

1. capacidade da arquitetura;
2. suporte e habilitação pelo kernel/OS;
3. política do loader/runtime para o processo;
4. compatibilidade da toolchain, allocator e bibliotecas chamadas;
5. largura de endereço realmente usada, que pode mudar com configuração ou hardware.

Um ponteiro com bits altos decorados não deve cruzar uma FFI nem ser dereferenced antes de passar pela operação definida de decode. A expansão futura do espaço virtual também pode consumir bits que hoje parecem livres.

### Acesso atômico e não atômico

Declarar o `raw` como `_Atomic` não torna acessos pelos outros membros da union atômicos. Alternar entre os dois modos no mesmo storage durante concorrência cria races e pode violar o modelo de memória C.

As representações devem ser tipos distintos. Uma versão atômica opera somente sobre a palavra codificada com CAS/load/store e memory orders definidos; uma versão local não promete atomicidade. Lock-freedom é propriedade medida por target, não inferida do tamanho.

### Range e overflow do payload

Um inteiro nativo pode não caber depois de reservar bits. Checks feitos em `intptr_t` não detectam necessariamente overflow do payload menor. Shifts de signed negativos e shifts pela largura do tipo também podem ser undefined behavior.

Encode deve validar o range antes de qualquer shift. Se o valor não couber, a operação escolhe a representação boxed; não produz sentinela nem wrap implícito.

### Precisão de ponto flutuante

`tbytes.c` demonstra que remover bits da mantissa muda o número. OR de uma tag sobre bits de `double`, mesmo seguido de restauração aproximada, não preserva todos os valores.

Uma otimização transparente precisa fazer round-trip bit-exact. Na ausência de um encoding que preserve isso, `f64` dentro de `Any` usa box, payload lateral ou uma variante maior. NaN-boxing é uma hipótese separada e também precisa provar preservação, espaço de NaNs permitido e compatibilidade com canonicalização do target.

### Refcount, tags e reclamation

Alguns spikes reutilizam o mesmo campo ora como tag/tamanho, ora como contador de referências, e promovem para `SharedPointer` quando ele satura. Isso deixa ambíguos o valor inicial, o responsável por incrementar e a condição de liberação.

Tags imutáveis de tipo/layout e estado mutável de ownership devem ser separados. Refcount saturado, cycles, weak references, ABA e concorrência de liberação precisam de design independente. Uma palavra tagged não resolve hazard pointers, epochs ou outro mecanismo de reclamation.

### Sanitizers, hardening e provenance

ASan, HWASan, TSan, UBSan, memory tagging, pointer authentication e capability pointers podem usar, validar ou alterar os mesmos bits. Conversões pointer→integer→pointer também não preservam necessariamente provenance em toda arquitetura.

A otimização fica desabilitada por padrão sob um perfil de instrumentation até existir integração testada. “Funciona sem sanitizer” não autoriza ocultar falhas do tooling.

### ABI e fronteiras

Layout interno pode mudar entre targets, perfis e versões do compilador. Uma palavra tagged não cruza automaticamente:

- `foreign c` ou C varargs;
- plugins compilados por outra versão;
- shared memory e IPC;
- serialização/persistência;
- símbolos com layout público ou `repr(C)`.

Essas fronteiras usam layout explícito e funções de marshal. Módulos que compartilham valores compactos precisam concordar por metadata versionada sobre o mesmo representation profile.

## Fallback portátil obrigatório

O primeiro artefato do experimento deve ser o fallback, não a variante compacta. Ele precisa:

- funcionar sem spare bits e sem round-trip de ponteiro por inteiro;
- preservar todos os valores e estados lógicos;
- ter operações de encode/decode usadas como oracle diferencial;
- separar type tag, payload, ownership e error state;
- suportar `repr(C)` por wrappers/layouts explícitos;
- produzir o mesmo comportamento observável em debug, release e sanitizers;
- aceitar objetos boxed quando um payload não cabe;
- permitir que a otimização seja desligada por flag, target ou função sem alterar source;
- registrar no artefato qual representation profile foi usado.

Representações portáteis candidatas, escolhidas por tipo e não globalmente:

```text
Option<T>  = { present: bool, payload: T }
Any        = { type_id: TypeId, payload: InlineStorage | BoxRef }
ObjectRef  = ponteiro/handle normal para header + payload
Shared<T>  = referência para controle de ownership separado
```

Essas formas são ilustrações de lowering, não ABI congelada.

## Matriz inicial de targets e capabilities

O profile precisa ser selecionado pela combinação completa, não apenas por macros como `__x86_64__`.

| Família/profile | Capacidade a investigar | Conflitos/condições | Baseline |
|---|---|---|---|
| x86-64, endereços canônicos convencionais | low alignment bits; high bits somente após mask/decode | 48 vs 57 bits, endereços canônicos, allocator e FFI | fallback; low-bit experiment apenas com alinhamento provado |
| x86-64 com Intel LAM ou AMD UAI | alguns bits altos podem ser ignorados em acessos habilitados | CPU + kernel + habilitação por processo + debugger/sanitizer; não enviar ponteiro decorado a código externo | opt-in experimental após probe de runtime |
| AArch64 com TBI | top byte potencialmente ignorado pelo ambiente | PAC, MTE/HWASan, ABI do OS, allocator e chamadas de sistema podem disputar os bits | fallback; profile específico quando comprovado |
| AArch64 sem TBI utilizável | low alignment bits apenas | alinhamento de cada tipo e pointer authentication | fallback |
| 32-bit nativo | poucos low alignment bits | address-space curto e payload insuficiente | fallback; compactação só se demonstrar ganho real |
| WebAssembly 32/64 | representação controlada na linear memory; possível handle/index | não assumir ponteiro nativo nem high spare bits; host ABI própria | fallback/handle explícito |
| capability pointers, como CHERI | capabilities carregam bounds/provenance não redutíveis a `uintptr_t` comum | integer round-trip e bit stealing podem invalidar a capability | fallback obrigatório |
| perfil com ASan/HWASan/MTE/PAC | instrumentation/hardening usa metadata de ponteiro | conflito direto ou necessidade de hooks oficiais | otimização desligada até integração comprovada |

Cada célula aprovada deve registrar arquitetura, OS, versão mínima, ABI, compiler flags, probe e comportamento ao carregar um artefato incompatível.

## Lugar no pipeline MLIR

`Option<T>`, `Any` e referências devem permanecer tipos/operações de alto nível no dialeto W enquanto ownership, efeitos e fronteiras ainda importarem. Um passe tardio de representation selection recebe:

- data layout do target;
- visibility e estabilidade de ABI;
- capabilities do runtime/OS;
- modo de sanitizer/hardening;
- informação de range e alinhamento provada;
- profile de compatibilidade entre módulos.

O passe escolhe fallback, niche, inline payload ou box. Lowerings seguintes trabalham com operações explícitas de encode/decode; nenhuma otimização pode mudar casos do enum, precisão, overflow ou lifetime.

## Plano de experimento

### Fase 0 — contrato e oracle

1. Definir semanticamente `Option<Ref>`, um subset mínimo de `Any` e `ObjectRef`.
2. Implementar a representação fallback sem bitfields nem extensões de arquitetura.
3. Especificar ownership de boxes, clone/move/drop e comportamento de OOM.
4. Fixar um formato de teste diferencial independente do layout.

### Fase 1 — implementação compacta sem UB conhecido

1. Reescrever encode/decode com masks, unsigned arithmetic e bitcasts definidos.
2. Separar tipos local e atomic.
3. Fazer valores fora do range migrarem para box.
4. Introduzir capability descriptor explícito, em vez de inferir tudo de macros de CPU.
5. Validar invariantes com assertions em build de desenvolvimento.

### Fase 2 — correctness e propriedades

Executar a mesma suíte sobre fallback e cada profile compacto:

- round-trip de valores de fronteira e milhões de padrões aleatórios;
- todos os ranges inteiros suportados, inclusive mínimos e máximos;
- padrões de `f32`/`f64`, incluindo `±0`, subnormals, infinities e NaNs;
- `.some`/`.none`, nested options e payload zero;
- ponteiros com alinhamentos e regiões variadas do allocator;
- move/copy/drop, refcount overflow e último release;
- concorrência da variante atomic com testes de linearizability e reclamation apropriada;
- chamadas através de wrappers C e módulos com profiles distintos;
- differential fuzzing entre resultado lógico do fallback e do encoding compacto.

Rodar os sanitizers disponíveis em cada target. Um profile incompatível com um sanitizer permanece desabilitado; a suíte não deve simplesmente omitir esse modo.

### Fase 3 — matriz de compatibilidade

Para cada target candidato:

1. compilar e executar probes de capability;
2. testar kernels/OS com feature habilitada e desabilitada;
3. testar GCC/Clang ou toolchains relevantes em mais de um nível de otimização;
4. testar debug, release, LTO e shared-library boundaries;
5. verificar debugger, unwind, crash reporting e symbolization;
6. rejeitar de forma segura um artefato cujo profile não é aceito pelo processo.

### Fase 4 — benchmarks

Comparar, no mínimo:

1. fallback explícito;
2. boxing universal;
3. niche/low-bit tagging;
4. high-bit tagging quando a plataforma realmente permitir;
5. alternativa maior, como payload de 128 bits, quando disponível.

Workloads:

- arrays grandes de `Any` com distribuições diferentes de scalars/objects;
- `Option<Ref>` em árvores, tabelas e traversals;
- parsing/serialização com valores temporários;
- dispatch e arithmetic sobre pequenos valores dinâmicos;
- aplicação real curta, não apenas loops de encode/decode.

Métricas:

- tempo total e throughput;
- alocações e bytes alocados;
- tamanho do working set e peak RSS;
- cache misses, branches e mispredictions quando o target oferecer counters;
- tamanho de código e artefato;
- custo de clone/move/drop e contenção atomic;
- tempo de compilação e impacto sobre otimizações posteriores.

Todo resultado registra hardware, OS, toolchain, flags, allocator, dataset, warmup, número de repetições e dispersão. O critério mínimo de ganho e o limite de regressão devem ser fixados antes de olhar o resultado final.

### Fase 5 — decisão

Um profile só vira candidato de implementação quando:

- passa os mesmos testes semânticos do fallback;
- não exige precisão ou range reduzidos;
- não introduz UB conhecido ou desabilita segurança silenciosamente;
- tem detecção/negociação e fallback confiáveis;
- melhora um workload representativo, não apenas microbenchmark;
- documenta ABI, tooling e custo de manutenção;
- pode ser removido sem alterar o source W.

Mesmo aprovado, o resultado será uma decisão de backend por target. A semântica pública continuará sendo `Option`, `Any`, referências e números de largura declarada.

O estado da hipótese e das decisões relacionadas permanece em
[STATUS.md](../STATUS.md).
