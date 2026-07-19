# Módulos, imports e instâncias de execução

> **Status:** direção consolidada e semântica candidata para protótipos; a
> sintaxe de serviços ainda está em aberto
> **Data:** 19 de julho de 2026

Este documento separa três conceitos que não podem compartilhar a mesma
semântica:

| Conceito | Existe quando | Identidade | O que possui |
|---|---|---|---|
| **módulo estático** | resolução, type checking e build | nome lógico + interface + inputs de build | declarações e dependências; não possui heap, thread ou lifecycle |
| **instância de execução** | runtime/deployment | tipo do serviço + chave/escopo + geração | estado, tasks, mailbox, capabilities, budgets e lifecycle explícitos |
| **package** | resolução e distribuição | nome, versão, origem e digests fixados | fontes, artefatos, dependências, provenance e policy de linking |

Essas camadas podem se mapear uma na outra durante um build, mas não são
sinônimas. Em particular, importar um módulo não cria uma instância e carregar
uma biblioteca não cria uma fronteira de segurança.

O texto detalha [W-C011](../STATUS.md), preserva a diferença entre concorrência e
paralelismo de [spec/concurrency.md](concurrency.md) e mantém distribuição sob o
contrato de [design/packages.md](../design/packages.md). Nada aqui afirma que um
runtime conforme já existe.

## Direção

### Módulo é uma unidade estática

Um módulo W é, conceitualmente:

- um namespace de símbolos;
- uma interface pública verificável;
- uma unidade de resolução, análise incremental e build;
- um nó do grafo de dependências;
- uma origem de metadata para diagnóstico, debug e artefatos.

Um módulo não ganha implicitamente:

- instância singleton;
- estado mutável global;
- thread, executor ou fila própria;
- heap, allocator ou região exclusiva;
- acesso a filesystem, rede, storage, relógio ou ambiente;
- inicialização ou finalização em runtime.

Arquivos e módulos não precisam manter relação um-para-um. A regra para um
módulo abranger vários arquivos, a necessidade de uma declaração `module` e a
forma de módulos internos continuam **Em aberto**. O nome lógico usado pelo
source é resolvido pelo manifest e fixado pelo lockfile.

### Import é estático e sem autoridade

A forma candidata permanece:

```w
import { User, UserId } from app.models
import geometry as geo from acme.geometry
```

Um import:

1. resolve nomes e dependências de interface;
2. não consulta a rede durante compilação normal;
3. não executa código do módulo importado;
4. não concede capabilities;
5. não escolhe static ou dynamic linking;
6. não redireciona uma chamada local para RPC.

URLs, versões, mirrors, source e artefatos pertencem a `package.w` e
`package.lock`, não à instrução de import. A policy de build pode satisfazer o
mesmo import por source, objeto, archive estático ou biblioteca dinâmica
compatível sem mudar a semântica observável do programa.

### Prelude e imports implícitos

A baseline anterior limitava a prelude a uma lista pequena de nomes puros. O
primeiro protótipo deve comparar essa opção com um **mapa de exports implícitos
congelado pela edição**. Esse mapa é parte do toolchain/lock, não é reconstruído
a partir da versão mais nova da stdlib nem das dependências instaladas.

No experimento mais amplo, todo export elegível da stdlib pode ser usado sem um
`import` quando seu nome é único no mapa daquela edição. Namespaces oficiais
também ficam disponíveis, portanto `http.serve` permanece uma forma estável
quando `serve` não é único. `print` não é uma keyword ou açúcar especial: é um
símbolo normal cuja origem pode ser mostrada pelo compiler e pelo LSP.

A resolução candidata procura, sem fallback ambíguo:

1. declarations e bindings lexicais;
2. imports explícitos e seus aliases;
3. exports livres do mapa implícito da edição, apenas quando há um único
   candidato;
4. nomes qualificados sob um namespace std implícito.

Se dois imports explícitos fornecem o mesmo nome, o source precisa selecionar ou
dar alias; ordem textual não desempata. Auto-import da IDE pode inserir a forma
canônica no arquivo, mas não altera a resolução do compiler por conta própria.

Uma colisão no mapa não escolhe pela ordem, popularidade ou target. O nome livre
deixa de existir e o diagnóstico oferece as formas qualificadas ou um import com
alias. Adicionar uma API à stdlib dentro da mesma edição não altera o mapa; o
novo símbolo só fica disponível por qualificação/import explícito até uma edição
posterior. A migração de edição consegue assim listar cada nova ambiguidade.

Visibilidade de nome não concede autoridade. Resolver `print` não cria terminal;
resolver `http.serve` não abre socket. Effects/capabilities, reachability e custo
do módulo usado continuam na interface inferida, no lens do editor e na receita
de build. Essa separação é condição do experimento, não detalhe opcional.

### Interface e visibilidade

`export` marca a API W visível fora do módulo:

```w
export struct UserId {
  value: u64
}

export fn displayName(id: UserId): String throws UserError {
  // ...
}
```

Um `export` W não cria automaticamente um símbolo C nem uma operação remota.
Exportar para C requer a fronteira `foreign c`/wrapper correspondente. Expor uma
operação de serviço requer um contrato de instância e tipos transportáveis.

A interface serializada de um módulo deve registrar, quando aplicável:

- nomes, tipos, generics e visibilidade;
- ownership, borrows, mutabilidade e error sets;
- `async`, requisitos de sendability e outros efeitos adotados;
- layout/ABI apenas onde explicitamente estabilizados;
- capabilities requeridas pela API, quando o modelo de effects for decidido;
- versão do schema e identidade do compilador que a produziu.

Cache de interface é descartável. O source e a receita resolvida continuam sendo
autoridade antes de existir uma ABI W estável.

Por default, uma declaration sem `export` é privada ao módulo. Visibilidade
restrita ao package, re-export de facade e acesso `friend` permanecem **Em
aberto**; nenhum deles deve depender do layout de diretórios por acidente.

Para o corpus inicial, `export struct` e `export enum` descrevem values
transparentes: fields/cases necessários para construção e pattern matching
cross-module entram na interface. `export protocol` exporta seus requirements.
Um `object` mantém storage privado mesmo quando sua identidade é exportada;
operações públicas são `export fn` ou, preferencialmente nas boundaries de
execução, um `protocol` exportado. Values com invariantes e representação oculta
continuam em [W-O035](../STATUS.md); não serão simulados com `private`/`public`
redundantes.

### Dependências e ciclos

Como imports não executam inicializadores, W não precisa inventar uma ordem de
runtime para percorrer o grafo. Ainda assim, ciclos afetam resolução de tipos,
generics, fingerprints e rebuild incremental.

O frontend inicial pode exigir um DAG de módulos e diagnosticar o caminho exato
do ciclo. Permitir SCCs apenas de interfaces/tipos é uma alternativa **Em
aberto**, condicionada a um algoritmo determinístico e a benefício demonstrado
em projetos reais. Introduzir inicializadores globais para “resolver” o ciclo é
**Rejeitado por enquanto**.

### Top-level não esconde lifecycle

Na baseline, o top-level aceita imports, declarações e `const` avaliável de forma
determinística. `var` global, I/O de inicialização e destructors ordenados entre
módulos não fazem parte da semântica candidata.

O executável começa num entrypoint explícito, inicialmente `main`. Um host de
serviços registra e inicia instâncias por API ou metadata de deployment. Isso
evita um grafo de inicializadores implícitos cuja ordem dependa do linker.

## Instância de execução explícita

### Conceito

Uma instância — chamada provisoriamente de **service**, **worker** ou
**isolate** conforme o profile — é um owner runtime explícito. Ela pode possuir:

- estado em memória;
- uma árvore de tasks estruturadas;
- uma mailbox limitada;
- capabilities recebidas na criação;
- budgets de memória, CPU, I/O e calls em voo;
- um adapter opcional de durable state;
- lifecycle, política de falha e observabilidade.

Os três nomes não são ainda keywords. **Em aberto:** usar uma única declaração
`service`, reservar `worker` para deployment e `isolate` para isolamento, ou
expressar tudo inicialmente por `object` + API de runtime.

Um esboço compatível com a sintaxe já candidata usa tipos comuns e uma API
explícita; os nomes de runtime abaixo não são stdlib congelada:

```w
protocol CounterApi {
  fn increment(): u64 async throws CounterError
  fn current(): u64 async throws CounterError
}

object CounterState {
  var value: u64

  mut fn increment(): u64 async throws CounterError {
    value += 1
    return value
  }

  fn current(): u64 async throws CounterError {
    return value
  }
}

enum CounterAppError: Error {
  start(StartError)
  counter(CounterError)
}

fn startCounter(
  host: inout ServiceHost,
): ServiceRef<CounterApi> async throws CounterAppError {
  do {
    return try await host.startService(
      CounterState(value: 0),
      as: CounterApi,
      scope: .process,
      policy: .serial,
    )
  } catch let error {
    throw .start(error)
  }
}

fn incrementCounter(
  counter: ServiceRef<CounterApi>,
): u64 async throws CounterAppError {
  do {
    return try await counter.increment()
  } catch let error {
    throw .counter(error)
  }
}

fn main(host: inout ServiceHost): Void async throws CounterAppError {
  let counter = try await startCounter(inout host)

  let value = try await incrementCounter(counter)
  print("counter: ${value}")
}
```

O handle conceitual `ServiceRef<CounterApi>` concede somente as operações do
contrato. Ele não fornece borrow para o estado interno nem promete que a
instância está no mesmo processo.

Este esboço faz `startService` suspender até a instância estar pronta. Uma API
não bloqueante teria de devolver um lifecycle handle distinto; a superfície
final continua aberta, mas os exemplos não alternam silenciosamente entre as
duas semânticas.

### Fine-grained é uma lente, não outra entidade

[W-D014](../STATUS.md) adota a direção de unidades lógicas *fine-grained* sem
impor uma fronteira física para cada uma. A frase “the future of compute is
fine-grained”, preservada na [discussão indicada pelo
projeto](https://news.ycombinator.com/item?id=31759801), inspira dividir trabalho
e estado em unidades menores e endereçáveis. Ela não define a semântica de W nem
serve como benchmark ou prova de segurança.

**Nanoservice** é apenas uma lente/nome de trabalho para essa ideia; não aparece
como termo na discussão do Hacker News, não é keyword e não é o nome público
decidido. O [post de apresentação do
`workerd`](https://blog.cloudflare.com/workerd-open-source-workers-runtime/) usa
o termo para componentes de funcionalidade implantáveis de modo independente,
mas co-localizáveis, com chamadas explícitas que naquele runtime podem executar
na mesma thread/processo. O próprio post alerta contra transformar cada função
em serviço apenas por sua forma sintática. W preserva essa intuição, não a
arquitetura de isolate JavaScript, o deployment homogêneo ou alegações de custo
do `workerd`.

Na semântica atual, a menor unidade stateful relevante continua sendo uma
instância explícita de `service`, possivelmente keyed, e a menor unidade de
trabalho é um turn/evento pertencente à sua árvore de tasks.
“Independentemente implantável”, se adotado por um profile W, descreve a
implementação/descriptor do tipo de serviço; não transforma cada key, instância
ou turn em package e artefato próprios.

Uma unidade fine-grained deve tornar pequenos e previsíveis:

- endereço lógico: tipo de serviço + escopo/chave + geração;
- interface de entrada e saída, incluindo errors;
- owner e lifetime do estado;
- lifecycle, cancellation e failure policy;
- capabilities e budgets;
- ordering, backpressure e vínculo com durable state;
- identidade lógica em traces, métricas e diagnostics.

Essa granularidade é lógica. Desde que preserve toda semântica observável, o
compiler/runtime pode co-localizar muitas instâncias, inlinear uma call local,
agrupar wakeups e mensagens, formar batches ou colapsar uma boundary que não seja
contrato de segurança. Mesmo otimizada, uma `ServiceRef` continua respeitando
seu `await`, errors, ordering, cancellation, quotas e identidade observável.

Fine-grained não significa:

- um processo, isolate, arquivo ou library por função;
- RPC de rede para toda call;
- transformar o programa em microservices distribuídos;
- persistir todo valor ou criar um banco por instância;
- esconder serialização, filas, cold start ou outro overhead do programador.

A regra de simplicidade é obter comportamento complexo pela combinação de poucas
invariantes — turn serial, children estruturados, mailbox limitada, capabilities
e commit/output gates explícitos — em vez de criar keywords paralelas para
actor, worker, isolate, nanoservice e cada deployment possível. Uma nova sugar
só avança quando reduz o source e ainda pode ser explicada por esse modelo menor.

### Lifecycle

O lifecycle mínimo de uma instância tem eventos observáveis:

1. criação com configuração, capabilities e budget validados;
2. inicialização, que pode falhar antes de aceitar eventos;
3. estado pronto, quando o runtime admite chamadas;
4. draining, que rejeita trabalho novo e conclui/cancela a árvore existente;
5. shutdown e cleanup;
6. falha e eventual criação de uma nova geração conforme policy explícita.

Uma instância singleton só o é dentro de um **escopo nomeado**, por exemplo
processo ou chave de deployment. “Singleton global” não é uma propriedade que a
linguagem possa prometer sem coordenação distribuída. O handle identifica uma
instância lógica e o runtime pode trocar sua geração após falha; referências
diretas ao endereço da implementação nunca fazem parte da identidade.

### Entradas, eventos e saídas

Cada profile define entrypoints tipados — por exemplo `request`, `message`,
`timer` ou uma API RPC — e os liga a handlers declarados. Não existe fallback
para procurar funções por nome em runtime.

Ao aceitar uma entrada, o runtime cria uma root task da instância. Todo trabalho
concorrente iniciado pelo handler pertence a essa árvore, salvo uma transferência
explícita para outro owner runtime. A conclusão produz exatamente um dos
resultados:

- valor de sucesso;
- error de aplicação declarado;
- cancelamento/deadline;
- falha de transporte ou indisponibilidade;
- panic/falha da instância.

Saídas externas — response, mensagem, mutation remota — precisam de ordem e
relação com durable writes definidas pelo profile. “Retornar sucesso” não deve
significar silenciosamente que uma escrita ainda pode ser perdida.

## Concorrência dentro de uma instância

### Candidato para o primeiro protótipo

O candidato inicial é uma instância **logicamente single-threaded e não
reentrante por padrão**:

- no máximo um handler externo altera o estado da instância por vez;
- o handler pode suspender com `await` sem bloquear a thread do executor;
- outros eventos para a mesma instância aguardam na mailbox limitada até o
  handler terminar;
- `async let` permite concorrência estruturada dentro do handler;
- `spawn let` executa trabalho paralelo apenas com captures permitidos por
  ownership/`Send`; ele não recebe acesso mutável direto ao estado da instância;
- instâncias diferentes podem rodar simultaneamente em threads/cores distintos.

“Single-threaded” descreve exclusão lógica, não afinidade permanente a uma
thread do sistema. Entre suspension points, o runtime pode retomar a instância em
outra thread se a ABI e os tipos preservarem o contrato.

Este default prioriza raciocínio local. Ele também pode causar head-of-line
blocking e deadlocks em ciclos de calls; deadlines, sharding por chave,
diagnósticos de self-call e políticas explícitas de reentrância devem ser
testados antes de estabilizá-lo.

### Reentrância

`await` não autoriza sozinho a entrega de outro evento ao mesmo estado. Perfis
futuros podem oferecer reentrância explícita ou gates mais finos, mas precisam
definir:

- quais invariantes podem mudar durante a suspensão;
- se callbacks e respostas de calls externas contam como novos eventos;
- como impedir deadlock em ciclos entre instâncias;
- que estado pode atravessar o ponto reentrante;
- como errors, cancelamento e rollback interagem.

Os *input gates* de Durable Objects são uma inspiração de **Pesquisa**: eventos
externos podem ser adiados enquanto operações de storage estão pendentes. Eles
não protegem automaticamente duas operações concorrentes iniciadas pelo mesmo
handler, portanto não devem ser copiados como uma promessa genérica de
“race-free por async/await”.

## Calls tipadas e structured RPC

Uma call por `ServiceRef<Api>` é sempre explicitamente suspensível e falível,
mesmo quando o runtime usa um fast path in-process:

```w
let seat = try await flight.assignSeat(user, preferred)
```

O contrato transportável contém operação, input, output e errors declarados. Na
fronteira de instância:

- valores são movidos, copiados ou serializados conforme seu tipo;
- ponteiros, borrows e layout interno não atravessam;
- um handle de serviço é uma capability não forjável, com autoridade limitada à
  interface concedida;
- localidade não é inferida pelo source;
- retry de operação mutante nunca é implícito;
- idempotência declarada permite uma policy explícita, mas não cria
  exactly-once.

Promise pipelining, referências remotas e batches encadeados, como os explorados
pelo Cap’n Web, são **Pesquisa** para reduzir round trips. W não deve introduzir
um Promise silencioso nem disfarçar lifetime remoto: qualquer pipeline futuro
precisa de builder/handle estruturado, limites, cancelamento e liberação das
capabilities remotas.

### Ordering

Baseline candidata:

- calls aceitas do mesmo sender para a mesma instância preservam a ordem de
  admissão, salvo API explicitamente unordered;
- senders distintos não recebem ordem total implícita;
- ordem de conclusão de child tasks não muda a ordem lexical de joins;
- restart cria uma nova geração; a relação entre mailbox anterior e nova
  geração precisa ser definida pelo adapter/deployment;
- persistência de uma sequência exige um log/transaction durável, não apenas a
  mailbox em memória.

### Backpressure

Mailbox, payload e calls em voo são sempre limitados pelo profile. Quando o
limite é alcançado, a operação deve:

- aguardar capacidade num suspension point cancelável; ou
- falhar de forma tipada conforme uma variante explícita `try`/policy.

Drop silencioso e fila ilimitada são **Rejeitado por enquanto**. Prioridades,
fairness, batching e limites default permanecem **Em aberto**.

### Cancelamento

Cancelar uma call solicita cancelamento cooperativo da root task correspondente
e de seus filhos. Não desfaz efeitos já confirmados, não interrompe uma foreign
call bloqueante sem adapter e não transforma timeout em garantia de rollback.

O serviço termina cleanup antes de reutilizar estado observável. Trabalho que
deve sobreviver ao caller precisa ser transferido explicitamente a um owner de
lifecycle maior; destruir o handle de uma call não cria uma task detached.

### Falha, restart e entrega

Uma falha de aplicação declarada não reinicia a instância. Um panic ou falha de
runtime pode encerrá-la e acionar uma policy de restart do host. Nesse caso:

- estado somente em memória é perdido;
- durable state reaparece apenas até o último commit confirmado;
- nenhuma garantia de exactly-once é inferida;
- calls sem resposta recebem um resultado de outcome conhecido ou desconhecido,
  nunca sucesso fabricado;
- uma nova instância tem outra geração e refaz inicialização.

Reiniciar uma instância lógica dentro do mesmo processo não contém corrupção de
memória nativa. Isolamento de falhas e isolamento contra código não confiável são
decisões de deployment descritas em
[design/modules-and-runtime.md](../design/modules-and-runtime.md).

## Durable state

Durable state é um adapter explícito concedido à instância. Campos de um
`object` não se tornam persistentes por declaração. O contrato mínimo precisa
definir:

- atomicidade e isolation das transactions;
- quando uma escrita está confirmada;
- relação entre commit e saídas externas;
- recovery, migração de schema e corrupção;
- quotas, backpressure e erro por indisponibilidade;
- snapshot/log, backup e política de retenção;
- comportamento de cancelamento antes, durante e depois do commit.

SQLite é um candidato de **Pesquisa** para serviços duráveis locais ou
embarcados, não storage universal. Memory KV, filesystem, banco remoto e
adapters específicos continuam válidos. A API síncrona de SQLite em Durable
Objects depende da
co-localização, cache e sistema de confirmação daquele produto; não estabelece
que todo storage W deva bloquear ou parecer síncrono.

Um *output gate* que retenha mensagens até a confirmação das writes relacionadas
é **Pesquisa** valiosa. O primeiro protótipo pode começar com transaction +
commit aguardado antes da response; só depois deve medir se inferir a relação de
causalidade é seguro e mais agradável.

## Memória e resources

O módulo estático não possui heap. Uma instância pode receber allocator, região
e budget como parte de sua configuração, mas isso não escolhe o modelo de memória
da linguagem:

- owner único continua sendo a baseline;
- regiões podem alinhar allocations ao lifecycle de request/instância;
- `shared T`/ARC continua uma alternativa explícita para múltiplos owners;
- mimalloc pode ser um adapter/implementação medido;
- tagged addresses podem otimizar representação em targets compatíveis;
- nenhum desses mecanismos redefine import, identidade do módulo ou isolamento.

Exceder budget deve resultar em erro de allocation/resource tipado quando houver
recovery seguro, ou encerrar a instância conforme profile. O comportamento não
pode variar silenciosamente entre debug e release.

## Capabilities e isolamento

Uma instância recebe apenas capabilities explicitamente concedidas: handles de
filesystem, rede, storage, relógio, entropia, processo ou serviços específicos.
Importar `std.net` disponibiliza tipos; não concede um socket.

Capabilities são a interface portátil e reduzem autoridade acidental em safe W.
Elas não isolam código nativo malicioso ou memória corrompida dentro do mesmo
address space. Para esse threat model, o profile precisa de processo/VM ou de um
sandbox verificável separado. Seccomp é um hardening Linux por processo/task, não
uma fronteira individual por biblioteca e, segundo a documentação do próprio
kernel, filtragem de syscalls não constitui um sandbox completo.

O `workerd` é uma referência útil para capability bindings entre componentes,
mas seu próprio [README](https://github.com/cloudflare/workerd) avisa que o
runtime sozinho não é um sandbox endurecido e recomenda uma boundary segura
adicional para código possivelmente malicioso. Isso reforça a separação de W
entre authority lógica e isolamento físico.

WebAssembly pode ser um target futuro para playground, plugins ou deployment com
imports limitados. Isso não torna W substituto de JavaScript nem incorpora DOM,
event loop do browser ou semântica web ao core.

## Relação com packages e artefatos

O package manager decide como obter, verificar e construir releases. O build
decide se módulos acabam em objetos, archives static, bibliotecas dynamic,
executável, interface ou futuro componente WASM.

Compilation units como bibliotecas podem melhorar cache, inspeção e
reprodutibilidade. Elas são uma alternativa de artefato, não obrigação sem dados:
LTO pode fundir unidades, uma archive não é carregada em runtime e uma `.so`/DLL
não cria isolamento. A receita, provenance, SBOM e digests preservam a origem
mesmo quando o linker transforma a representação.

## Matriz de status

| Tema | Status | Gate antes de estabilizar |
|---|---|---|
| módulo como namespace/interface/build unit | **Direção** | corpus multiarquivo e rebuild incremental |
| import sem execução, rede ou authority | **Direção** | resolver hermético e negative tests |
| mapa std implícito congelado por edição | **Candidato** | colisões, effects/capabilities, autocomplete e migração determinística |
| lifecycle somente em instância explícita | **Direção** | protótipo com start/drain/restart |
| unidades lógicas fine-grained sem boundary física obrigatória | **Direção** | preservar contrato, custos e observabilidade sob co-location |
| handler serial não reentrante por default | **Candidato** | latência, deadlocks, diagnostics e ergonomia em três workloads |
| múltiplas instâncias + `spawn` para paralelismo | **Candidato** | type checking de captures e pool limitado |
| mailbox limitada e backpressure observável | **Candidato** | overload, fairness e cancelamento determinísticos |
| sintaxe/keyword `service` | **Em aberto** | comparar API, declaração própria e codegen de contrato |
| input/output gates | **Pesquisa** | oracle de transação, falhas e causalidade de outputs |
| SQLite como adapter durable | **Pesquisa** | benchmark e recovery contra adapters alternativos |
| seccomp por target | **Pesquisa** | threat model Linux e boundary real por processo |
| singleton/heap/thread para todo módulo | **Rejeitado por enquanto** | conflita com W-C011 e imports previsíveis |
| uma library por compilation unit obrigatória | **Rejeitado por enquanto** | não prova isolamento e restringe otimização/linking |
| processo por função ou RPC em toda call | **Rejeitado por enquanto** | confunde granularidade lógica com topologia física e esconde custo |
| `nanoservice` como keyword/nome final | **Em aberto** | vocabulário só avança após limites e UX do modelo estarem provados |
| WASM como substituto de JavaScript | **Rejeitado por enquanto** | fora do objetivo declarado |

## Perguntas diretas para a próxima revisão

1. [W-O023](../STATUS.md): você quer que `service` seja sintaxe da linguagem ou que comece como
   `object` + contrato/runtime e só seja promovido após o protótipo?
2. [W-O024](../STATUS.md): o default deve realmente impedir qualquer novo evento até o handler terminar,
   ou você prefere reentrância somente em `await` marcado como seguro?
3. [W-O025](../STATUS.md): quais escopos de singleton são indispensáveis no primeiro corte: processo,
   request, chave persistente ou deployment?
4. Uma call para `ServiceRef` deve sempre exigir `await`, inclusive no fast path
   local, para preservar previsibilidade de localidade e falha?
5. Em overload, o default aguarda espaço na mailbox ou falha imediatamente com
   erro tipado?
6. O primeiro durable prototype deve usar SQLite ou uma interface memory KV com
   SQLite como segunda implementação?
7. A resposta externa só pode sair depois de todo commit causal estar confirmado,
   ou haverá uma operação explícita para aceitar confirmação eventual?
8. Um panic reinicia apenas a instância em profiles isolados e encerra o processo
   nos demais, ou o comportamento deve ser sempre escolhido no deployment?
9. Você quer uma sintaxe source para budgets/capabilities ou prefere mantê-los no
   manifest/deployment enquanto [W-O006](../STATUS.md) estiver aberto?
10. Promise pipelining/capability RPC é parte da visão do runtime ou apenas um
    experimento posterior de wRPC?
11. [W-O026](../STATUS.md): o protótipo amplo com todo export std único supera
    uma prelude curada ou somente namespaces implícitos? Compare `print`,
    `http.serve`, autocomplete, colisões e atualização de edição.
12. [W-O027](../STATUS.md): “Nanoservice” ajuda a comunicar a visão ou deve permanecer somente uma lente
    interna? Qual limite mínimo justifica uma instância separada: state keyed,
    lifecycle/capability independente ou também um handler stateless curto?

## Referências

- [Cloudflare — Durable Objects: Easy, Fast, Correct — Choose three](https://blog.cloudflare.com/durable-objects-easy-fast-correct-choose-three/)
- [Cloudflare — Cap’n Web](https://blog.cloudflare.com/capnweb-javascript-rpc-library/)
- [Cloudflare — SQLite em Durable Objects](https://blog.cloudflare.com/sqlite-in-durable-objects/)
- [Cloudflare — sandbox/seccomp](https://github.com/cloudflare/sandbox)
- [Linux Kernel — seccomp filter](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html)
- [Wasmtime — Security](https://docs.wasmtime.dev/security.html)
- [Cloudflare — apresentação do `workerd` e nanoservices](https://blog.cloudflare.com/workerd-open-source-workers-runtime/)
- [Cloudflare — repositório `workerd`](https://github.com/cloudflare/workerd)
- [Hacker News — discussão “the future of compute is fine-grained”](https://news.ycombinator.com/item?id=31759801) — inspiração não normativa; não é a origem do termo “nanoservice”
