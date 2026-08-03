# Serviços e protocolos do ecossistema W

> **Status:** Working Draft
> **Data:** 2026-07-18
> **Escopo:** pesquisa pós-core; nenhum item deste documento reserva keyword ou
> compromisso de compatibilidade.

## 1. Objetivo

As propostas de serviços misturam cinco problemas diferentes: declarar um
contrato tipado, chamar uma operação remota, serializar valores, consultar dados
e mapear tudo para HTTP. Este documento separa essas camadas e descreve o menor
protótipo plausível para cada uma.

Tudo aqui pertence a bibliotecas, codegen e runtime do ecossistema depois que o
core tiver tipos, errors, ownership, async/cancelamento e metadata de módulo
suficientemente estáveis. Uma aplicação W pode usar HTTP, gRPC, sockets ou outro
sistema sem adotar wRPC, WLO, wQL ou RestPC.

Os nomes também são provisórios. `WLO` aparece historicamente como `WLON/wlon`;
`RestPC` surgiu como alternativa a `RestQL` porque esse nome já era usado. Este
rascunho não resolve naming.

## 2. Camadas

| Camada | Responsabilidade | Não decide |
|---|---|---|
| Contrato tipado/service | operações, tipos, errors e metadata semântica | transporte, framing, storage |
| Modelo de operação | chamada direta ou query wQL | encoding de bytes |
| Envelope/perfil | wRPC ou RestPC/HTTP | representação interna dos valores W |
| Codec | JSON, WLO ou wStruct | autenticação e retry |
| Transporte | TCP, Unix socket, pipe, QUIC, HTTP etc. | contrato de domínio |

Uma composição possível é:

```text
contrato tipado ── chamada unary ── wRPC ── JSON/WLO ── byte stream
        │
        ├────────── query wQL ───── wRPC ── JSON/WLO ── byte stream
        │
        └────────── operação ────── RestPC/HTTP + JSON
```

Não é obrigatório que RestPC encapsule o framing binário de wRPC: HTTP já é o
envelope de transporte. As duas opções podem compartilhar contrato, IDs de call,
errors e regras de cancelamento sem duplicar bytes.

## 3. Contrato tipado de serviço

### 3.1 Papel

O contrato é a fonte de verdade independente do transporte. Ele pode ser extraído
de uma API W exportada, escrito em um IDL de biblioteca ou produzido por tooling;
este documento não cria uma keyword `service`.

O contrato mínimo descreve:

- identidade lógica e versão do serviço;
- nome e identidade estável de cada operação;
- tipo de input e output;
- errors de aplicação declarados;
- se a operação é unary; streaming fica fora do primeiro protótipo;
- propriedades relevantes como read-only e idempotência, quando conhecidas;
- limites sugeridos de payload e deadline;
- schema dos tipos alcançáveis pela interface.

O subset wire inicial pode conter bool, inteiros de largura fixa, floats com regra
explícita, `String` UTF-8, bytes, opcionais, listas, mapas com chave suportada,
records e enums. Ponteiros, borrows, handles locais e layout de objetos não cruzam
a fronteira diretamente. Codegen converte entre valores do programa e DTOs wire.

Nomes devem acompanhar IDs para diagnóstico e detecção de colisão. Um digest do
schema pode identificar exatamente uma versão, mas hash sozinho não define
compatibilidade nem substitui regras de evolução.

### 3.2 O que fica de fora

O contrato não cria banco, resolver, cache nem endpoint automaticamente. Storage
SQLite/KV, declarations, middleware, autorização e deployment são adapters. Uma
operação local não se torna remota só porque está exportada, e imports de release
não podem ser redirecionados para rede de forma implícita.

## 4. wRPC

### 4.1 Papel

wRPC é o candidato a envelope e semântica de chamada tipada sobre um transporte
de mensagens ou byte stream. Ele não é query language, codec nem mecanismo de
descoberta de serviços.

### 4.2 Envelope abstrato mínimo

Antes de escolher o layout binário, toda mensagem precisa representar:

```text
protocolVersion
messageKind       request | response | cancel | protocolError
callId
serviceId
operationId
codecId
metadata
payload
```

Em transporte de stream, o frame também precisa de tamanho total validado antes
da alocação. `serviceId`/`operationId` não devem ser apenas hashes sem nome,
metadata de schema e estratégia de colisão.

O primeiro protótipo deve fixar um único codec por conexão/profile, chamadas
unary e um limite pequeno de frame. Handshake de capabilities, multiplexação
sofisticada e compressão podem esperar.

### 4.3 Semântica mínima de call

- Uma request aceita produz uma response de sucesso, um error de aplicação ou
  falha de transporte/protocolo.
- Se a conexão cair depois do envio, o caller pode não saber se houve efeito. O
  runtime não faz retry implícito de operação mutante.
- Idempotência é propriedade declarada e pode habilitar uma policy de retry; ela
  não equivale a exactly-once.
- Deadline e `cancel` são cooperativos. Confirmação de cancelamento não implica
  rollback de efeitos já realizados.
- Error declarado pertence ao contrato. Malformed frame, codec failure, versão
  incompatível e autorização negada são categorias distintas.
- Toda implementação aplica limites de bytes, profundidade, coleções, calls em
  voo e tempo antes de materializar objetos do domínio.

### 4.4 Requisitos antes de fixar um framing

Este rascunho ainda não adota um layout binário. Um framing candidato precisa
definir endianness, versão, call ID, partial reads, limites, errors e
multiplexação, mantendo tipo, operação e corpo como campos semanticamente
separados. Números de bytes só podem ser fixados depois de medir compatibilidade,
overhead e evolução de schema.

## 5. Codecs

Codec transforma DTOs validados em bytes e vice-versa. Ele não decide qual
operação pode ser chamada nem autentica o peer.

| Codec | Uso candidato | Estado e limite |
|---|---|---|
| JSON | interoperabilidade, debug e perfil HTTP | primeiro protótipo plausível; mapeamento tipado ainda precisa ser definido |
| WLO/WLON | representação W-a-W e literals canônicos | pesquisa; gramática e canonicalização inexistentes |
| wStruct | IPC binário orientado a layout | pesquisa de mesma máquina/ABI; não é raw struct portátil |

### 5.1 JSON

JSON deve ser UTF-8 e validado contra o contrato. Ainda precisam ser definidos:
inteiros fora do range interoperável, NaN/infinito, bytes, enum payloads, mapas,
campos desconhecidos e canonicalização. O JSON recebido nunca ganha tipo W apenas
por ter shape parecido; decode produz sucesso tipado ou codec error.

JSON é um bom baseline porque permite inspecionar o primeiro sistema. Não é uma
decisão de performance ou codec padrão permanente.

### 5.2 WLO/WLON

A intenção da hipótese é representar todos os valores literais W, suportar
`parse<T>`/`stringify` e reutilizar o resultado em comptime ou RPC. O mínimo seguro
exigiria uma gramática de dados menor que a linguagem, encoding determinístico,
schema versionado, limites e fuzzing.

Este documento usa “WLO” apenas como nome de trabalho. Não decide se será textual,
binário, extensão de literals W ou se terá vantagem suficiente para existir.

### 5.3 wStruct

wStruct explora IPC eficiente na mesma máquina usando layout conhecido. Copiar
bytes de uma struct C/W não é suficiente: padding pode conter dados indefinidos,
ponteiros não são transferíveis, e alinhamento/ABI podem mudar entre builds.

Qualquer protótipo precisa fixar target, endianness, ABI, schema, layout e lifetime;
rejeitar pointers/handles crus; e fornecer fallback de campo a campo. Até a ABI W
existir, wStruct não pode ser um formato estável.

### 5.4 Relação com `tree_string`

Nenhum codec observa a representação interna de `String`. O valor wire é uma
sequência UTF-8 válida ou bytes explicitamente tipados. Mesmo que tree strings
sejam testadas para interning/índices, o codec serializa o valor lógico, não paths
da estrutura. Isso mantém o experimento fora do protocolo e da ABI pública.

## 6. wQL

### 6.1 Papel

wQL é o candidato a modelo de query/mutation sobre um contrato tipado. Ele não é
o envelope wRPC nem o codec WLO. A primeira decisão necessária é um AST lógico;
uma DSL textual SQL-like, tagged template ou shape JSON pode ser adicionada por
biblioteca depois.

### 6.2 Mínimo plausível

Um protótipo pode trabalhar com três classes de operação:

- `query`: leitura sem efeito, com path/operation, argumentos tipados e projeção
  de campos;
- `command`: operação que pode mutar estado, com input e output tipados;
- `introspect`: obtém a versão pública do contrato quando o servidor habilitar.

Parâmetros são enviados separadamente da query/AST; interpolar texto não deve ser
o único mecanismo. A validação ocorre contra o contrato antes de chamar handlers.
O resultado é tipado ou um error declarado, sem presumir que toda resposta seja
um objeto JSON sem schema.

As palavras candidatas `select`, `insert`, `update`, `delete` e `call` podem ser
frontends para esse modelo, mas não estão adotadas como keywords. Create versus
replace, PUT versus PATCH, field update, filtros, joins e transações precisam de
semântica própria antes de entrar.

Batching fica fora do mínimo. Uma lista de operações levanta atomicidade, ordem,
dependências entre resultados e partial failure que uma simples lista de JSON não
resolve.

### 6.3 O que wQL não promete

- compatibilidade GraphQL sem um perfil definido;
- gerar todo backend a partir do schema;
- ser agnóstico a transporte enquanto depende de headers/status HTTP;
- substituir SQL para consultas arbitrárias;
- esconder custo de N+1, paginação, autorização ou materialização;
- executar paths recebidos sem validação e limites.

## 7. RestPC: perfil HTTP

RestPC é o nome de trabalho para um adapter que usa HTTP/JSON e o contrato tipado
sem exigir um parser wQL. Ele preserva a direção “REST-like RPC”:

| HTTP | Uso mínimo |
|---|---|
| `GET` | leitura segura/idempotente ou obtenção de propriedade/recurso |
| `POST` | call genérica e create quando o contrato expressar criação |
| `PUT` | replace/set completo quando essa semântica existir |
| `DELETE` | remoção declarada pelo contrato |
| `OPTIONS` | introspecção opcional da definição pública |

`PATCH` para update parcial é uma extensão possível, não parte do mínimo. Operação
que não tem semântica de recurso usa `POST`; o adapter não deve fingir REST apenas
para evitar a palavra RPC.

HTTP status descreve o resultado do transporte/profile; o body preserva error
tipado quando possível. A correspondência exata entre errors e status ainda está
aberta. JSON é o codec inicial plausível. Query em header, request GET com body,
batch e sequência PUT+GET referenciada por ID permanecem alternativas de pesquisa,
não baseline.

RestPC pode futuramente gerar ou consumir uma descrição interoperável, mas este
rascunho não exige OpenAPI nem compatibilidade com `trpc-openapi`.

## 8. Segurança mínima

- Todo payload remoto é não confiável e validado antes de virar valor do domínio.
- Framing e codecs aplicam limits antes de alocar coleções ou recursão profundas.
- Authn e authz são policies explícitas; introspecção não as substitui.
- Metadata de tracing não pode sobrescrever identidade autenticada.
- Errors públicos não vazam stack, paths, secrets ou conteúdo interno por padrão.
- Nenhum schema recebido executa código, instala plugin ou redireciona import.
- Remote modules implícitos podem ser úteis em debug experimental, mas não fazem
  parte de release segura.
- Codegen e schemas entram no lock/provenance como qualquer outra ferramenta ou
  input de build.

Este mínimo ainda não escolhe mecanismo de autenticação. TLS, mTLS, token,
assinatura de mensagem e credenciais de plataforma têm trade-offs diferentes e
devem ser perfis, não uma ausência silenciosa de segurança.

## 9. Lacunas antes de uma especificação

| Área | Questões abertas |
|---|---|
| Versioning | versão de protocolo e serviço; compatibilidade de campos, enums e operations; unknown fields; deprecation |
| Identidade de schema | IDs estáveis, nomes canônicos, hash/collision handling, negociação e cache |
| Auth | autenticação, autorização por operação/campo, delegação, credentials e proteção contra confused deputy |
| Errors | categorias wire, typed errors, redaction, mapping HTTP e retryability |
| Call semantics | idempotência, dedup, retry, timeout, unknown outcome e limites de at-most-once/exactly-once |
| Batching | ordem, atomicidade, transações, referências entre operações e partial failure |
| Streaming | framing, flow control, backpressure, ordering, half-close, cancelamento e resume |
| Wire format | endianness, largura/varints, partial frame, max size, checksum, compressão e bombs |
| Transporte | TCP, QUIC, Unix socket, pipe, shared memory, HTTP, WebSocket e reconexão |
| Codec JSON | integers, floats especiais, bytes, maps, enums, canonicalização e duplicate keys |
| WLO | gramática, textual/binário, canonicalização, schema evolution, segurança e vantagem medida |
| wStruct | layout, padding, ABI, pointers, handles, lifetime, cross-process e fallback |
| wQL | filtros, aliases, joins, paginação, ordenação, custo, CRUD, transactions e subscriptions |
| RestPC | roteamento, URL/version, cache HTTP, content negotiation, PATCH e introspecção |
| Observabilidade | trace/correlation IDs, métricas, logs, baggage limits e privacidade |
| Descoberta | endpoint discovery, load balancing, health, capability negotiation e downgrade |
| Codegen | linguagem dos stubs, sync/async APIs, ownership, ABI, source maps e compatibilidade |
| Operação | rate limits, quotas, admission control, shutdown, deploy e rolling upgrade |
| Storage | transações e autorização não podem ser inferidas automaticamente de SQLite/KV/schema |

## 10. Sequência de experimentos

Esta sequência é uma forma de reduzir variáveis, não um roadmap comprometido:

1. representar contrato e schema como dados canônicos;
2. gerar caller e handler in-process para uma operação unary;
3. transportar a mesma operação em JSON por loopback, com typed error e limits;
4. medir um perfil RestPC/HTTP sem DSL;
5. testar envelope wRPC length-delimited com call ID e cancelamento;
6. modelar wQL como AST tipada antes de escolher sintaxe textual;
7. só então comparar WLO, codec binário e streaming.

Cada passo precisa de fuzz, malformed inputs, timeouts, cancelamento e schema
incompatível, não apenas um happy path.

## 11. Dependências do core

O trabalho só pode reivindicar integração oficial depois de o core responder:

- representação pública de records, enums, optionals e errors;
- regras de ownership/cópia na fronteira wire;
- structured concurrency, deadline e cancelamento;
- metadata de módulo e evolução de ABI;
- sandbox/provenance para codegen;
- packages capazes de fixar schema, generator e runtime.

Enquanto isso, protótipos podem usar DTOs explícitos e JSON sem influenciar a
gramática W.

## 12. V6 e Computer Units são outro experimento

V6 foi anotado como um runtime “V8-like”, reduzido, com foco serverless e possível
módulo de kernel. Computer Units foram descritas como isolates com budgets e
entrypoints. Nenhuma fonte define V6 como transporte, codec ou implementação de
wRPC.

Um runtime desses poderia futuramente hospedar um contrato tipado ou RestPC, assim
como qualquer outro host. Ele não define as regras do protocolo. Engine JS,
scheduler, sandbox, storage e kernel permanecem no catálogo separado em
[research/README.md](../research/README.md).
