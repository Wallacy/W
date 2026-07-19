# Estratégia híbrida de memória

> **Status:** Working Draft · 19 de julho de 2026

W deve tornar ownership, transferência, borrowing, custo de cópia e fronteiras
estrangeiras compreensíveis no source. Não deve obrigar todo programa a escolher
um único coletor, allocator ou layout de ponteiro. Esta página organiza a
estratégia de implementação que sustenta a semântica de
[tipos e memória](../spec/types-and-memory.md); ela não a substitui.

## Direção

1. A garantia é semântica: Safe W não produz dangling references, double drop
   nem data races. Stack, heap, arena, reference count e layout são meios para
   preservá-la, não tipos implícitos adicionais.
2. O caminho comum é ownership único, `ref` para borrow imutável, `inout` para
   borrow exclusivo, `take` para transferência e `copy` quando a duplicação é
   relevante. O compilador pode remover cópias ou alocações não observáveis.
3. Compartilhamento e retenção prolongada precisam aparecer no contrato. Uma
   atribuição não pode alternar silenciosamente entre move, RC e arena.
4. Todo target dispõe de representação portável. Melhoria específica de
   arquitetura só entra após provar equivalência, ganho e fallback seguro.

### Fronteiras que não devem se misturar

| Camada | Decide | Não decide |
|---|---|---|
| Semântica | owner, borrow, move, `copy`, cleanup, escape e shared explícito | allocator, bits do ponteiro, thread pool ou heap |
| Frontend/lowering | escape analysis, placement stack/heap/region, drops e task frames | mudar lifetime ou custo prometido pelo source |
| Runtime/allocator | pages, caches, alocação/liberação, telemetria e perfil de target | validar borrow inválido, escolher owner ou esconder ciclo |
| Serviço/isolate | quota, accounting, pressure/falha e isolamento operacional | fazer de namespace/import um heap singleton |

Essa divisão também permite que C/FFI use seu próprio deallocator sem contaminar
a semântica dos valores W.

## Candidato

O mapeamento concreto de quantities, Lists, request buffers, task frames,
`ServiceRef` e state de instância está no
[corpus do restaurante](../examples/restaurant/REQUIREMENTS.md#memória-automática-é-viável-aqui).
Ele é o teste top-down desta estratégia; não cria exceções às regras abaixo.

### Escada híbrida

A escada é uma preferência para diagnósticos, lowering e APIs, não uma ordem
rígida de alocação física. Um `object` owned pode ser elidido para a stack se
identidade e observabilidade forem preservadas.

| Degrau | Contrato no source | Lowering provável | Limite deliberado |
|---|---|---|---|
| 1. Stack/inline | values locais, `let`/`var`, move ou `copy` | registers, inline, stack, scalar replacement | layout não vira ABI sem `repr` |
| 2. Ownership estático | owner único, `ref`, `inout`, `take` | move paths, borrows, drops e escape analysis | inferência não cria alias nem muda transferência documentada |
| 3. Regiões/arenas | bloco explícito ou inferido sem escape | bump/arena, cleanup por escopo e accounting | não é módulo, import ou `flush` global |
| 4. Heap unique | identidade, tamanho dinâmico ou escape com um owner | allocation + `drop` determinístico | pode continuar stack/inline se equivalente |
| 5. Shared explícito | futuro `shared T`/handle e `weak T?` | RC/ARC; RC atômico quando necessário | não é default de cada `object`; ciclos são explícitos |

#### Valores, moves e borrows

O compilador prova inicialização, usos, transferências, exclusividade e cleanup
em toda saída de controle. Só depois escolhe stack, inline ou heap. Move de
último uso pode ser inferido quando inequívoco; `take` continua recomendado em
APIs, fronteiras de task e pontos em que a transferência é relevante à leitura.
Nenhum borrow sobrevive ao owner, e `inout` não coexiste com alias conflitante.

#### Regiões e arenas

Regiões servem a lifetimes comuns — request, compilação, batch, frame de
processamento ou operação de serviço. A forma explícita deve ser avaliada antes:
ela mostra onde vida e limite de memória terminam. Inferência só é válida se
preservar o cleanup, diagnóstico e comportamento de falha dessa forma explícita.

Uma região deve impedir escapes inválidos; encerrar, aguardar ou transferir
children `async` antes de liberar; executar drops e cleanup externo observável;
expor limite/falha quando for boundary de serviço; e manter accounting separado
do namespace estático. Ela não é resposta universal para grafos, caches longos
ou callbacks que cruzam lifecycle desconhecido.

#### Heap unique

Heap unique é o fallback quando valor dinâmico ou com identidade escapa do escopo
local, sem requerer compartilhamento. O owner faz o `drop`; `take` transfere esse
owner de modo verificável. Coleções e buffers declaram se recebem borrow, `take`,
`copy` ou allocator do chamador; nunca liberam memória de deallocator desconhecido.
O objetivo não é prometer zero allocations, e sim torná-las documentáveis e
elidíveis quando houver prova de equivalência.

#### Shared, ARC/RC e ciclos

`shared T` é candidato apenas onde existem owners independentes reais: grafos,
caches, callbacks long-lived e objetos enviados entre unidades paralelas. ARC/RC
é backend plausível; retains/releases podem ser coalescidos depois do check. Ao
cruzar `spawn`, o tipo prova sendability e o contador pode precisar ser atômico;
em executor serial, o backend pode ser mais simples.

RC não coleta ciclos. A primeira proposta exige `weak T?`, owner/serviço que
quebra o ciclo no lifecycle, ou estrutura com `close`/remoção explícita. Cycle
collector global não é pré-requisito do núcleo nem efeito implícito de `shared`.

### Política de allocator

O allocator é selecionado por perfil de runtime/target e registrado na receita
de build/deploy, nunca inferido da sintaxe. A API segura recebe handles de
allocator/budget quando esse controle importa; conveniência usa contexto
configurado, consultável em metadata e testes.

| Escolha | Papel proposto |
|---|---|
| allocator do sistema | fallback portátil, baseline e integração com plataformas/FFI que o exigem |
| mimalloc | candidato de perfil para heaps W; exige benchmark e compatibilidade por target |
| allocator estrangeiro | obrigatório na fronteira C quando o objeto volta ao deallocator de origem |
| arena/região | allocator de escopo delimitado, não opção global escondida |

Trocar perfil não altera resultado, ownership, ordem observável de cleanup, ABI
pública nem classificação de erro. Falha de allocation e budgets devem ser
testáveis; pressão de memória nunca vira corrupção, abort arbitrário ou retenção
infinita de modo silencioso.

### Serviços, isolamento e budgets

Serviço/isolate pode possuir contexto de memória: quota, contador, limite por
request/task, estratégia de pressure e política de término. Esse contexto é
operacional e explícito no contrato do serviço. Ele não cria singleton de módulo
nem autoriza referência compartilhada sem `shared` ou mensagens.

O runtime inicial prioriza accounting por serviço/região quando habilitado, erro
tipado ou cancelamento controlado antes de afetar outro serviço, cleanup
estruturado de children/resources, telemetria de live/reserved/peak bytes, e
isolamento mais forte por processo ou sandbox somente quando o target o oferece.

## Em aberto

### Pointer tagging e tagged addresses

A política aceita de source/fallback e o gate dos profiles compactos estão em
[tagged-values](../research/tagged-values.md#política-candidata-da-db1).
W-C029 não promove nenhum profile de W-O018 por si só.

Tagged addresses são otimização de representação, nunca prova de lifetime,
ownership, thread safety ou validade de ponteiro. Podem compactar `Option<ref T>`,
metadata ou valores imediatos em targets com bits comprovadamente disponíveis,
mas HIR e ABI continuam trabalhando com tipos/operações abstratos.

Antes de habilitar tagging, o backend verifica arquitetura, ABI, alinhamento real
e bits disponíveis; pointer authentication, top-byte-ignore/memory tagging,
capability pointers e hardening; ASan/HWASan, UBSan, debuggers, profilers e
unwinders do perfil; mask/unmask em toda fronteira C/syscall/biblioteca; e
equivalência de atomicidade, ordenação, overflow e representação com a fallback.

A fallback é ponteiro/enum convencional. Target que não passe o gate conserva
toda a semântica W, em debug e release. Não há "tagged pointer" global no source,
ABI C, serialização ou formato de pacote.

### Perguntas para decisão do autor

| Registro | Pergunta | Evidência mínima |
|---|---|---|
| [W-O002](../STATUS.md#questões-abertas-prioritárias) | last-use move pode ser inferido sem surpreender em APIs e diagnostics? | corpus, número de `take` e fix-its compreensíveis |
| [W-O003](../STATUS.md#questões-abertas-prioritárias) | o primeiro slice precisa de `shared T`? | graph/cache/callback, task cancelável, FFI e RC atômico/não atômico |
| [W-O004](../STATUS.md#questões-abertas-prioritárias) | região começa como bloco source, API ou só inferência? | request com escape, erro, child async e cleanup externo |
| [W-O016](../STATUS.md#questões-abertas-prioritárias) | budget falha como `throws`, cancela serviço, ou depende do profile? | recovery em dois hosts e sem corrupção/leak de resource |
| [W-O017](../STATUS.md#questões-abertas-prioritárias) | quais perfis de allocator entram na v0? | benchmark reproduzível contra system fallback, FFI e sanitizers |
| [W-O018](../STATUS.md#questões-abertas-prioritárias) | quais representações tagged passam o gate inicial? | implementação dual, FFI e hardening em x86_64 e arm64 |

## Pesquisa

Experimentos comparam a escada à alternativa mais simples e registram source,
toolchain, target, allocator/profile, carga, pico/live/reserved bytes, latência,
cleanup em erro/cancelamento e resultado da fronteira C.

| Experimento | Baselines | Critério para avançar |
|---|---|---|
| owner único + escape analysis | stack/heap conservador | mesma semântica e diagnóstico; menos allocations sem regressão debug/FFI |
| região explícita | heap unique e RC | sem escape inválido; cleanup correto; ganho em request/batch |
| `shared`/RC | owner único ou região aplicável | resolve caso real, ciclos têm rota explícita e custo parallel conhecido |
| mimalloc profile | allocator do sistema | ganho reproduzível sem falhar em sanitizer/hardening/FFI |
| tagged representation | enum/pointer convencional | ganho paga complexidade e não há divergência observável |
| budget por serviço | sem quota e região com quota | isolamento, recovery e telemetria corretos sob pressão/cancelamento |

Goldens de IR e microbenchmarks não bastam: propostas portáveis exigem testes de
erro, cancelamento, FFI e dois targets. Um experimento só promove perfil/target,
nunca automaticamente uma regra de linguagem.

## Rejeitado por enquanto

- ARC/GC implícito para cada `object` ou atribuição.
- Heap, arena ou `flush` automático por import/módulo estático.
- Bits de endereço como evidência de ownership, lifetime ou autorização.
- Tagged addresses na ABI C, serialização ou pacote.
- mimalloc, SQLite ou outro allocator como requisito sem fallback e evidência.
- Budget como desculpa para pular drops, `defer`, destructors ou cleanup C.
- Sandbox/isolamento de um host como garantia universal da linguagem.
