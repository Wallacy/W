# Spikes C históricos

> **Working Draft · pesquisa histórica · 18 de julho de 2026**

Este catálogo cobre os **42 arquivos C** históricos de
[`Y/_w_/C`](../../_w_/C), conferidos no
[registro de consolidação](../consolidation-manifest.md) por caminho e blob. Eles
registram perguntas sobre representação, concorrência, ABI, plataforma e stdlib.
Não formam uma biblioteca única nem uma implementação parcial validada do runtime W.

## Regra de uso

Cada arquivo deve ser tratado como **research, teste negativo ou ponto de partida para um benchmark**. Antes de qualquer código ser incorporado ao compilador, runtime ou stdlib, ele precisa de:

1. objetivo e contrato escritos;
2. provenance e licença verificadas;
3. teste reproduzível nos targets declarados;
4. análise de undefined behavior, concorrência, ownership e falhas de alocação;
5. comparação com uma implementação mantida ou uma alternativa mais simples;
6. reescrita ou vendoring explícito, sem promover o spike silenciosamente.

As falhas abaixo são uma triagem, não uma auditoria completa. A ausência de observação sobre um trecho não comprova correção, segurança, portabilidade ou comportamento lock-free.

## 1. Representação e tagged values — 6 arquivos

| Arquivo | Valor preservável | Limites e falhas conhecidas |
|---|---|---|
| `tagged.c` | primeira sequência extensa de testes com bits livres de ponteiros, tags, null e promoção para shared | bitfields têm layout dependente da implementação; mistura acesso atômico ao `raw` com campos não atômicos; pressupostos de endereço virtual e LAM/UAI precisam ser separados por target e SO; tags e contagem de referência ainda se confundem em alguns caminhos |
| `tagged_8k.c` | compara layouts de 48/52/56/57 bits e quantidade de small data disponível | caminhos de reconstrução ainda assumem 48 bits; x86-64 e AArch64 compartilham detecção baseada em `cpuid.h`; suporte de hardware a LAM/UAI não prova habilitação pelo sistema operacional |
| `tagged_pointer.c` | evolução para distinguir scalar, compound, error e shared | leitura do código mostra caminhos inconsistentes de desalocação: errors são excluídos por `is_indirect`, e shared usa sentinelas de tag diferentes em criação e `dealloc`; payload escalar não preserva todos os 64 bits |
| `tagged_ops.c` | explora aritmética atômica/não atômica, estados especiais e overflow | encoding manual usa type nos bits baixos enquanto os bitfields da union descrevem outra disposição; shifts de signed negativos e type punning precisam ser removidos; o overflow nativo não basta para um payload de 62 bits |
| `tbytes.c` | demonstra visualmente a perda ao guardar um `double` em 62 bits | é prova de conceito de precisão, não argumento para alterar silenciosamente o tipo `Float`; depende de layout IEEE-754 e terminal ANSI |
| `type128.c` | testa extensões `__int128`/`__float128` e impressão de valores grandes | tipos e sufixo `Q` são extensões de toolchain; formatos de `printf` e conversões não são portáveis como escritos |

### Conclusão da categoria

Tagged values continuam uma otimização de target plausível para `Any`, opcionais ou pequenos valores. Não podem definir a representação universal de `Int`, `Float`, ponteiros ou objetos. Um experimento futuro precisa de layout fallback, testes por ABI, sanitizers e interação documentada com pointer authentication, memory tagging e endereços largos.

## 2. Concorrência e runtime — 16 arquivos

| Arquivo | Valor preservável | Limites e falhas conhecidas |
|---|---|---|
| `atomic.c` | contraste mínimo entre contador atômico e data race | `cnt++` concorrente é deliberadamente inseguro; serve como teste negativo, não como benchmark confiável |
| `atomic_array.c` | investiga custo/localidade de arrays atômicos e execução com/sem threads | contador comum tem race quando threads são habilitadas; usa GNU statement expressions e macros que escondem controle de fluxo |
| `atomic_max.c` | pergunta relevante sobre atomics de 128 bits e CAS | disponibilidade e lock-freedom dependem de arquitetura, alinhamento, compilador e possivelmente `libatomic`; não há harness de concorrência |
| `atomic_union.c` | registra a intenção de escolher representação atomic conforme o contexto | acessar o mesmo storage como membro atômico e não atômico durante concorrência não é uma forma segura de alternar políticas; a escolha deve ocorrer no tipo/lowering |
| `aba.c` | introduz contador de versão na cabeça de uma stack | o tag não resolve sozinho memory reclamation: um nó pode ser liberado enquanto outro consumidor o observa; o `main` atual não cria consumidores e tenta fazer join de entradas não inicializadas |
| `async_await.c` | modela queues por módulo, tarefas concorrentes e envio de trabalho a outro módulo | a queue chamada lock-free não usa CAS para suas transições e escreve `next` sem protocolo multi-producer; `has_tasks` não implementa wakeup correto; o uso atual de `setjmp`/`longjmp` não produz suspensão independente |
| `block.c` | avalia Clang Blocks e estado capturado em closures | depende de `Block.h`/Blocks runtime e de extensão Clang; captura implícita e lifecycle precisam ser comparados com closures explícitas de W |
| `catch_sigqueue.c` | exemplo pequeno de `sigaction` com payload | POSIX-specific; usa `printf` dentro do handler, inadequado para código de produção signal-safe |
| `send_sigqueue.c` | explora mensagem entre processos via `sigqueue` | não é portável para todas as plataformas-alvo; `sigval` é union, portanto atribuir `sival_int` e depois `sival_ptr` não envia ambos; lifetime da string não constitui protocolo IPC |
| `signal.c` | reúne shutdown cooperativo de threads e sinalização global | continua POSIX-specific; tratamento de erro dentro do handler e acesso a estado precisam obedecer estritamente às funções async-signal-safe |
| `signal_action.c` | investiga `SIGCHLD`, `siginfo_t` e coleta de processos | `printf` no handler não é signal-safe; fork/signals não oferecem modelo equivalente em Windows |
| `signal_array.c` | tenta acordar consumidor de I/O para processar lotes de function pointers | crescimento por `realloc`, índices e publicação dos arrays não têm protocolo de sincronização suficiente; o consumidor é infinito e o teste faz `join` sem condição de término |
| `signal_info.c` | bom exemplo de bloquear sinais e consumi-los sincronicamente com `sigwaitinfo` | útil como referência POSIX, não como abstração runtime multiplataforma; ainda precisa de testes de lifecycle e shutdown |
| `signal_kill.c` | testa máscara herdada e uma thread dedicada a sinal | POSIX/fork-specific e com loop deliberadamente infinito; não deve determinar a API pública de tasks |
| `stack.c` | documenta limites de stack por processo no Unix | `RLIMIT_STACK` não equivale a stack de cada task/thread e não existe da mesma forma em todos os targets |
| `thread_args.c` | explicita structs de argumentos/resultados e ownership na fronteira pthread | o comentário propõe resultado alocado pelo caller, mas `calculator` ainda faz `calloc`; divisão por zero e falhas de alocação não são tratadas |

### Conclusão da categoria

Os arquivos preservam perguntas importantes — executor por módulo, batching, wakeup, retorno caller-allocated e diferença entre task e thread. Eles não demonstram structured concurrency, cancelamento, propagação de erro, ausência de data races ou scheduler lock-free. Esses contratos devem nascer no HIR/runtime design e só depois ser comparados com primitivas IOCP, Unix, WASI e embedded.

## 3. ABI, FFI, compilação e plataforma — 15 arquivos

| Arquivo | Valor preservável | Limites e falhas conhecidas |
|---|---|---|
| `file1.c` | tabela de exports e objeto de módulo em C; um dos spikes mais diretamente relacionados à ABI | atributos `const`/`pure`, qualifiers de retorno e mutação do singleton não formam ainda um contrato correto; deve virar teste com header gerado e ABI explícita |
| `main.c` | lado consumidor da tabela de exports de `file1.c` | replica structs manualmente, permitindo drift de layout; precisa consumir um único header e testar versões/visibilidade |
| `broken.c` | excelente caso negativo de leitura não inicializada e possível vazamento de stack | inseguro de propósito; preservar como teste de diagnóstico/sanitizer, nunca executar como exemplo de API segura |
| `enum_pay.c` | testa enum denso como índice de tabela e fallback | não valida `argc`; a tabela não define por si só layout estável de enum público |
| `hello.c` | smoke test C mínimo | não contém decisão de linguagem; útil apenas para validar toolchain básica |
| `hello_p.c` | smoke de allocator, `write` e stdio | combina APIs de níveis diferentes e não modela erro/ownership; POSIX no caminho de `write` |
| `hello_s.c` | entrypoint sem libc e syscall direta | assembly e números de syscall são específicos de Linux x86-64; função naked exige convenções de compilador |
| `hello_t.c` | testa TLS e pthread | usa `__thread`/`__FUNCTION__` e assinatura de start routine não canônica; precisa de variante por plataforma |
| `macros.c` | ótimo caso negativo para determinismo de builds | `__DATE__` e `__TIME__` tornam outputs dependentes do momento da compilação |
| `mp4.c` | candidato valioso a teste de FFI com uma biblioteca C grande, callbacks e buffers | ownership entre `malloc`, `av_free` e `AVIOContext` não está consistente; há caminhos de erro/leak e requisitos FFmpeg não documentados |
| `nmap.c` | explora acesso aleatório por `mmap`/`madvise` | não compila como está (`return 0/`), faltam includes, bounds check, fechamento completo e `munmap`; é Unix-specific |
| `parser.c` | registra por contraste por que regex não basta para o frontend | tenta escrever terminadores dentro de string literal e invalida offsets ao recortar o mesmo buffer; não deve ser reaproveitado como parser |
| `parser_w.c` | experimento de templates/macros para visualizar lowering W→C | depende de GNU statement expressions e usa `sprintf` com reserva arbitrária; não contém análise sintática ou semântica |
| `struc.c` | spike útil de passagem de structs/ponteiros e otimização whole-program | não possui benchmark ou assembly esperado versionado; resultados dependem de ABI, flags e escape analysis |
| `sum_depth.c` | caso de teste para recursão e custo de stack | a recursão é exponencial e não é tail-call; `sum(32, 0)` não é um teste pequeno nem prova suporte stackless |

### Conclusão da categoria

`file1.c`/`main.c`, `broken.c`, `struc.c`, `macros.c` e uma versão corrigida do caso FFmpeg podem virar testes de compilador/FFI. Os parsers por regex/macros devem permanecer apenas como registro de alternativas abandonadas.

## 4. Estruturas de dados e algoritmos — 5 arquivos

| Arquivo | Valor preservável | Limites e falhas conhecidas |
|---|---|---|
| `bs.c` | smoke test de busca binária | exemplo genérico sem abstração, propriedade ou benchmark específico de W |
| `btree.c` | implementação substancial com allocator customizável, COW, hints e iteração | é código de terceiro com copyright de Joshua J. Baker; mistura header, implementação e exemplo; precisa de origem, revisão e licença upstream antes de qualquer uso |
| `btreemap.c` | experimento com `_Generic`, chaves/valores genéricos e API semelhante a Rust | a estrutura é uma BST desbalanceada, não B-tree; `size` cresce em replace e pode diminuir quando a chave não existe; borrowed pointers não têm lifecycle definido |
| `red_black.c` | referência didática de rotações e fixup | atribuição limita-se a “costheta_z”; não há delete/free nem provenance suficiente para distribuição |
| `timsort.c` | implementação/benchmark que compara Timsort e `qsort` | não há origem/licença no arquivo; há alocações sem cleanup em caminhos antecipados e o harness usa APIs não portáveis como `drand48` |

### Conclusão da categoria

W não deve reimplementar uma coleção porque há um snippet disponível. A seleção da stdlib precisa de API, complexidade, allocator/ownership, fuzzing, benchmarks, provenance e manutenção. Uma dependência upstream bem pinada pode ser melhor que uma cópia local sem histórico.

## Provenance e licença

O `LICENSE` na raiz declara copyright de Wallacy Freitas sobre o repositório; ele **não substitui automaticamente licenças e avisos de material importado**.

Pontos que exigem resolução:

- `btree.c` declara copyright de Joshua J. Baker e diz que a licença está em um arquivo `LICENSE`, mas não registra URL, revisão nem uma cópia identificada da licença upstream;
- `red_black.c` atribui o código apenas a `costheta_z`;
- `btreemap.c` aponta para uma URL de playground, não para fonte versionada;
- `timsort.c` não contém aviso de origem ou licença;
- outros exemplos podem ter sido adaptados de respostas, manuais ou snippets e também precisam de pesquisa antes de redistribuição como produto.

Para cada spike promovido ou dependência vendorizada, registrar no mínimo:

```text
origin URL
upstream project and author
commit/release/content hash
SPDX license identifier and license text
local modifications
date reviewed
tests and supported targets
update/security policy
```

Se a origem não puder ser demonstrada, o caminho seguro é reescrever a partir de uma especificação pública e testes próprios, sem copiar a expressão original.

## Critério de promoção

Um spike só deixa este catálogo quando existir uma implementação nova ou vendorizada em um local de produto, acompanhada de testes e de uma decisão. O arquivo histórico permanece para explicar a pergunta original.

Em particular:

- nomes como `lock-free`, `safe`, `atomic` ou `reproducible` exigem propriedades demonstradas, não apenas o uso de uma primitiva com esse nome;
- compilar com uma configuração não valida as demais arquiteturas nem elimina undefined behavior;
- um benchmark sem versão de toolchain, flags, input e baseline não decide a stdlib;
- um spike C não define sozinho a semântica visível de W;
- pesquisa preservada pode inspirar lowering target-specific, mas sempre precisa
  de fallback portável conforme [`W/STATUS.md`](../../../W/STATUS.md).
