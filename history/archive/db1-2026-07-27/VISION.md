# Visão de produto do W

> **Status:** direção de produto com escolhas de posicionamento ainda abertas
> **Data:** 21 de julho de 2026

## A promessa

W quer tornar programação nativa algo que uma pessoa escolha fazer por prazer, sem pedir que ela aceite surpresas de runtime ou uma sintaxe hostil em troca de desempenho.

Em uma época em que grande parte do código será sugerida, transformada ou revisada por sistemas de IA, a linguagem continua sendo uma interface humana. Quanto mais claro for o contrato entre texto e execução, melhor humanos e máquinas conseguem raciocinar, revisar e confiar nele.

Uma formulação curta da promessa é:

> **W é uma linguagem de sistemas agradável e previsível: prazer para humanos, clareza para máquinas.**

## Posicionamento

W não é “C com mais keywords” nem uma tentativa de esconder completamente a máquina. É uma linguagem nativa com um caminho suave entre três altitudes:

1. código de aplicação legível, com tipos inferidos e memória automática;
2. sistemas concorrentes e paralelos, com efeitos e limites explícitos;
3. interoperabilidade e controle de layout/ABI quando o domínio exige.

A analogia “fazer para C o que TypeScript fez para JavaScript” é útil para
comunicar interoperabilidade e adoção incremental. Ela não deve ser interpretada
literalmente como um superset sintático de C: C tem undefined behavior,
preprocessor, múltiplos dialetos e regras que W não precisa herdar.

## Para quem

O público inicial é a pessoa que:

- gosta da proximidade e do ecossistema de C, mas quer defaults seguros;
- aprecia a legibilidade de Swift e TypeScript, mas precisa gerar código nativo;
- escreve serviços, CLIs, runtimes, bibliotecas, ferramentas, áudio/vídeo, jogos ou software embarcado;
- transporta fórmulas, simulações, álgebra linear ou modelos ML entre notebook,
  CPU, SIMD e aceleradores sem aceitar um runtime opaco como única opção;
- precisa combinar I/O concorrente com trabalho paralelo sem escolher uma biblioteca diferente para cada modelo;
- quer inspecionar custo, ownership, efeitos e artefatos sem escrever toda a mecânica manualmente;
- valoriza uma toolchain first-party e uma supply chain verificável.

W não precisa atender todos esses domínios na primeira versão. Eles delimitam a direção; a primeira fatia deve ser pequena o suficiente para ser implementada e medida.

## Princípios de design

### 1. O source deve prever a execução

Operações que suspendem, podem falhar, transferem ownership ou pedem paralelismo aparecem no código. Otimizações podem remover custos; não podem mudar o significado.

### 2. O caminho comum deve ser leve

A maior parte do código não deve mencionar allocators, ponteiros, lifetimes ou executors. Quando uma decisão afeta segurança ou custo de forma importante, ela se torna explícita no menor ponto possível.

### 3. Segurança e baixo nível são camadas, não adversários

Safe W não expõe ponteiros inválidos. A fronteira `foreign c` permite controle manual e deixa claro onde as garantias terminam. APIs seguras podem encapsular essa fronteira.

### 4. Concorrência é estruturada; paralelismo é intencional

Tasks não sobrevivem acidentalmente ao escopo que as criou. `async` descreve suspensão e concorrência; `spawn` descreve elegibilidade para execução paralela. O runtime decide a mecânica sem apagar a intenção.

### 5. Uma ideia só vira semântica depois de sobreviver a um protótipo

Tagged pointers, arenas por módulo, COW/RCU, multi-language bodies e cálculo de recursos são hipóteses valiosas. Nenhuma delas deve restringir todos os targets antes de demonstrar benefício e fallback.

### 6. Uma forma canônica vale mais que muitos atalhos

Sinônimos de keywords, três tipos equivalentes de aspas ou múltiplas maneiras de declarar a mesma função prejudicam formatter, ensino, busca e raciocínio. Açúcar só entra quando reduz carga cognitiva sem criar ambiguidade.

### 7. Erros de compilação são parte da linguagem

Diagnósticos devem dizer o que aconteceu, qual contrato foi violado, qual custo ou risco motivou a regra e quais correções preservam a intenção. A saída também deve ter formato estruturado para IDEs e agentes.

### 8. Build e distribuição fazem parte do produto

Resolver dependências, reproduzir um artefato, verificar sua origem e explicar o que entrou no binário são tarefas do toolchain oficial. “Funciona na minha máquina” não pode ser a política de segurança.

### 9. Portabilidade vem da semântica, não do menor denominador comum

W pode aproveitar `io_uring`, IOCP, SIMD ou uma GPU quando disponível, mas o programa expressa intenção portável. Dependências de target e diferenças inevitáveis devem ser consultáveis.

### 10. IA amplifica clareza; não justifica opacidade

Assistentes se beneficiam de uma gramática estável, type/effect metadata, formatter canônico e testes executáveis. Código gerado continua sujeito às mesmas regras e deve continuar legível por uma pessoa.

### 11. Granularidade lógica não obriga fragmentação física

Programas podem ser compostos de unidades pequenas, endereçáveis, observáveis e
com contratos simples. Isso não exige um processo, library ou RPC por função: o
compiler e o runtime podem co-localizar, agrupar, inline ou baixar uma call para
um fast path quando preservam ordering, falha, cancelamento e observabilidade.
Complexidade do sistema deve ser absorvida por poucas invariantes previsíveis,
não devolvida ao source como uma coleção de knobs.

### 12. Açúcar de domínio precisa revelar um modelo completo

Notação agradável para unidades, matrizes ou modelos só é uma vantagem quando
shape, precisão, promoção, cópias, device e efeitos continuam previsíveis. W não
deve adicionar um operador isolado para parecer científica: a superfície curta
precisa baixar a contratos tipados, inspecionáveis e interoperáveis.

## Não objetivos da versão zero

Os itens abaixo podem voltar como projetos ou extensões, mas não devem bloquear o primeiro compilador:

- substituir JavaScript no navegador;
- criar um sistema operacional ou uma plataforma serverless completa;
- executar corpos arbitrários de toda linguagem dentro de `fn<lang>`;
- definir wQL/wRPC como protocolo universal;
- tornar toda estrutura lock-free ou toda operação atomic;
- provar estaticamente o máximo exato de memória e threads para qualquer programa;
- exigir SQLite como armazenamento de toda aplicação;
- criar uma representação de string alternativa ao UTF-8 contíguo;
- suportar GPU, HDL, hot reload e debug remoto antes da fatia nativa básica;
- estabilizar uma ABI binária para sempre antes de estabilizar a semântica de source.

## Critérios de sucesso

As primeiras versões devem ser avaliadas por evidência, não só por uma lista de features:

- uma pessoa que conhece C, Swift ou TypeScript entende os exemplos principais em minutos;
- um formatter produz uma única representação estável;
- a diferença entre concorrência e paralelismo pode ser explicada com um exemplo curto;
- os mesmos testes observáveis passam em `debug` e `release`;
- um wrapper C simples não exige cópias ou alocações escondidas;
- uma dependência resolvida pode ser usada offline e seu artefato pode ser explicado/verificado;
- erros comuns de ownership e task lifetime são diagnosticados com correções úteis;
- benchmarks publicam tempo, memória, tamanho de binário e versão da toolchain;
- um assistente consegue consumir AST/HIR, diagnósticos e metadata sem extrair fatos de texto informal.

## Nome e frases

O nome curto **W** é visualmente forte, mas difícil de buscar e potencialmente ambíguo. Antes de um lançamento público será necessário validar domínio, package namespace, executable name, trademark e discoverability.

A frase original, “A última linguagem que você vai querer aprender”, tem uma ambiguidade em português: também pode soar como a linguagem que alguém menos deseja aprender. Opções para teste:

| Uso | Candidato | Observação |
|---|---|---|
| assinatura curta | **Prazer para humanos. Clareza para máquinas.** | comunica as duas audiências sem prometer perfeição |
| ambição | **A última linguagem que você vai precisar aprender.** | forte, mas deliberadamente ousada |
| foco em prazer | **Código que dá vontade de continuar escrevendo.** | humano e memorável |
| foco técnico | **Native code without the guesswork.** | bom para público internacional |
| assinatura inglesa | **Joy for humans. Clarity for machines.** | tradução direta da proposta atual |

Por enquanto, a documentação usa “Prazer para humanos. Clareza para máquinas.” como identidade de trabalho, não como decisão de marca irreversível.

## Regra para novas ideias

Toda proposta deve responder:

1. qual problema observável resolve;
2. por que precisa estar na linguagem e não em biblioteca/tooling;
3. o que aparece no source, no tipo e no runtime;
4. qual é o comportamento em erro, cancelamento e FFI;
5. como funciona em pelo menos dois targets;
6. qual alternativa mais simples foi comparada;
7. como será medida e como pode ser removida se falhar.

Essa regra permite continuar explorando sem transformar toda boa ideia em dependência do bootstrap.
