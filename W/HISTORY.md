# História e linhagem documental do W

> **Working Draft · histórico · 18 de julho de 2026**

Este documento registra o que o Git permite afirmar sobre a origem e a evolução do W neste repositório. Ele não tenta reconstruir lembranças anteriores ao versionamento e não transforma uma ideia antiga em decisão atual.

## O que o histórico comprova

O repositório foi criado em 27 de julho de 2020. O primeiro commit que contém um caminho dedicado ao W é:

| Commit | Data | Mensagem | Conteúdo |
|---|---|---|---|
| `9410b5c` | 22 de dezembro de 2020 | `Adicionando ideas sobre o W` | cria `W/README.MD`, com exemplos de funções, destruturação, `const`/`let` e lowering exploratório para C |

Portanto, **22 de dezembro de 2020 é a primeira data de W demonstrável por este Git**. Não há commit, tag, reflog versionado ou arquivo preservado no repositório que comprove trabalho sobre W em 2015. Isso não significa que a ideia não existisse: apenas que uma origem em 2015 é memória pessoal ou material externo ainda não incorporado, e deve ser descrita como tal até aparecer uma fonte datada.

## Cronologia resumida

### 2020: primeiro registro

O arquivo inicial já contém o núcleo estético que reapareceria nos anos seguintes: sintaxe curta, tipos junto aos argumentos, destruturação, bindings imutáveis e interesse em traduzir construções de alto nível para uma ABI próxima de C.

### 2021: de sintaxe para sistema

Entre abril e maio de 2021, o caderno cresce rapidamente:

- `35b0084` e `be6aada` desenvolvem módulos, imports, memória por módulo e experimentos W+C;
- `18cd0f3` transforma o antigo `README.MD` em `.WIP.MD`, deixando claro seu caráter de caderno;
- `d3fb175` introduz a frente de package manager;
- `ac5df65` introduz services;
- `767589d` adiciona o primeiro spike de atomics em C;
- `002fc3b` registra snapshots;
- `e13ddfe` consolida uma longa exploração de estrutura, memória, módulos, concorrência, FFI e tipos.

Esse período é a origem de várias direções duradouras, mas os textos continuam deliberadamente especulativos. Expressões como “pensar”, “poderia” e alternativas incompatíveis fazem parte da fonte.

### 2022–2023: protocolos, WC e projetos satélite

Em 2022 aparecem `wQL.MD`, `RestQL.MD`, `V6.MD` e o primeiro caderno de WC. W passa a ser pensado também como ecossistema: comunicação, serverless, build, runtime e extensão de C.

Em dezembro de 2023, vários projetos satélite deixam `W/`. Parte dessas linhas de pesquisa hoje vive em `TK/`. A mudança é organizacional; não torna wQL, RestQL ou V6 semântica obrigatória da linguagem.

### 2024: laboratório de C, build e parser

Durante 2024 são adicionados experimentos de:

- ABI de módulos e tabelas de exports;
- pthreads, atomics, signals, stack e callbacks;
- estruturas de dados e ordenação;
- strings UTF-8, C23, Cosmopolitan, GCC e Clang;
- build, artefatos estáticos/dinâmicos e reprodutibilidade;
- parsing por regex/macros e, depois, Tree-sitter.

O `grammar.js` surge em `173b81f`, de 2 de julho de 2024. Ele é um primeiro sketch Tree-sitter; os commits posteriores apenas o movem. Não houve uma evolução correspondente até a sintaxe descrita nos cadernos.

### Fevereiro de 2025: tentativa de reorganização

O histórico registra duas mudanças sucessivas:

1. `7ce1830` move os laboratórios de `W/C` e documentos auxiliares para `W/_w_/`;
2. `5110d51` remove `W/_w_/` e recria esse material em `Y/_w_/`, enquanto expande o cheatsheet.

Por isso, `Y/_w_/` não é um projeto separado surgido do zero. Ele é a continuação dos spikes que antes estavam em `W/C` e `W/_w_`.

No mesmo período, modelos produziram resumos mais organizados, incluindo um cheatsheet que chegou a 1.041 linhas. Esses documentos são úteis como catálogo de possibilidades, mas misturam escolhas do autor, preenchimento automático e sintaxes mutuamente incompatíveis.

### Março a outubro de 2025: tagged values e async

Março concentra várias iterações de tagged pointers e aritmética em uma palavra. Em `0a0cba0`, uma especificação de 142 linhas sobre essa representação passa a ocupar `W/techspec.md`; o mesmo commit copia o antigo resumo geral de W para `Y/_w_/README.md`.

Em maio e outubro surgem `aba.c`, `signal_array.c` e duas versões de `async_await.c`. Eles documentam perguntas reais sobre filas, tarefas entre módulos e corrotinas, mas não constituem um runtime validado.

### Novembro de 2025: o refactor destrutivo

O commit `2acbb8c`, de 24 de novembro de 2025, tem a mensagem `Refactor code structure for improved readability and maintainability`. Documentalmente, ele é uma condensação destrutiva, embora o conteúdo anterior continue recuperável pelo Git:

- cria um novo `W/README.md` de 77 linhas;
- reduz `W/cheatsheet.md` de 1.041 para 163 linhas;
- substitui a techspec específica de tagged pointer por uma narrativa geral de 269 linhas;
- move `W/WIP.MD` para `Y/WIP.MD`, preservando 98% do arquivo e acrescentando material.

O novo resumo promove a fatos ideias que as fontes tratavam como alternativas, entre elas “Everything is an Enum”, arenas como único modelo de memória, protothreads como implementação async, XXH3 como dispatch sem colisões e um Computer Unit fixo. A frase literal **“Everything is an Enum” aparece pela primeira vez nesse commit**. Nas notas de 2021, o autor considera semântica semelhante a enums, mas também registra que uma estrutura comum talvez seja melhor.

“Destrutivo” aqui descreve a perda de nuance na árvore de trabalho, não perda irrecuperável: blobs anteriores podem ser consultados por commit.

### 2026: reconstrução com estados explícitos

Em 18 de julho de 2026 começa a reconstrução de `W/` com uma regra diferente: separar visão, escolhas candidatas, questões abertas e pesquisa. O objetivo não é apagar as ideias antigas, mas impedir que uma hipótese seja confundida com promessa atual.

## Documentação e testes no próprio source

`Y/WIP.MD`, linhas históricas em torno de 1914–2003, já propunha testes ao lado da
função, arquivos `file.test.w`, remoção dos testes do artefato release e reserva
de vocabulário inspirado em JSDoc. A notação explorada incluía `//@(2,1) == 3`,
`@a` para argumentos e `@@` para resultado, além de ideias de debug inline.

A DB1 preserva a intenção, não essa DSL: a versão atual usa comentários `///`
Markdown, fences `w test`, declaração contextual `test "..." for symbol` e os
mesmos testes em `*.test.w`. O contrato está em
[`W/design/documentation-and-tests.md`](../../W/design/documentation-and-tests.md).
As formas `@`/`@@`, execução de debug embutida e snapshots remotos continuam
históricas até demonstrarem uma necessidade que a sintaxe atual não cubra.

## Linhagem dos caminhos

```text
W/README.MD (2020)
  └─ W/.WIP.MD (2021)
       └─ W/WIP.MD
            └─ Y/WIP.MD (2acbb8c, 2025)

W/C/* + W/WC.MD + documentos auxiliares
  └─ W/_w_/* (7ce1830, 2025)
       └─ Y/_w_/* (5110d51, 2025)

W/README.md antigo
  └─ Y/_w_/README.md (mesmo blob em 0a0cba0)

W/cheatsheet.md extenso
  └─ W/cheatsheet.md condensado (2acbb8c)

W/techspec.md de tagged pointer
  └─ W/techspec.md geral (2acbb8c)
```

Renames nem sempre foram detectados como tal pelo Git, pois vários arquivos foram removidos e adicionados juntos. A identidade de conteúdo e a sequência de commits permitem reconstruir a linhagem.

## Mapa das fontes históricas

A árvore `Y/` passou por uma consolidação temporária e foi restaurada como o
arquivo histórico externo à árvore publicável `W/`. O
[registro da consolidação](consolidation-manifest.md) preserva os hashes e blobs
que permitiram conferir a restauração.

| Fonte | O que contém | Como usar |
|---|---|---|
| [`Y/WIP.MD`](../WIP.MD) | caderno principal e mais contínuo de ideias | fonte de intenção histórica, não especificação atual |
| [`Y/_w_/`](../_w_) | WC, build, Tree-sitter, referências e spikes C | laboratório e evidência de exploração |
| [`TK/`](../../TK) | wQL, RestQL, V6, strings e outros projetos relacionados | projetos satélite; só entram no núcleo por decisão explícita |
| histórico de `W/cheatsheet.md` | grande catálogo de sintaxe possível | recuperar exemplos, nunca importar todos como uma linguagem coerente |
| histórico de `W/techspec.md` | pesquisa de tagged representation | base para research e testes target-specific |
| [`W/VISION.md`](../../W/VISION.md) | promessa e princípios atuais | autoridade para direção de produto |
| [`W/STATUS.md`](../../W/STATUS.md) | estado de cada escolha atual | autoridade para distinguir Direção, Candidato, Em aberto e Pesquisa |

Comandos úteis para consultar material substituído:

```console
git show 9410b5c:W/README.MD
git show 2acbb8c^:W/cheatsheet.md
git show 0a0cba0:W/techspec.md
git show 2acbb8c^:W/WIP.MD
git log --follow -- W/cheatsheet.md
```

## Regra de autoridade

Quando documentos discordarem, a autoridade não é determinada apenas pela idade, pelo tamanho ou por ser o arquivo mais recente. A ordem é:

1. uma decisão aceita e identificada em `W/design/decisions/`, quando existir;
2. o estado registrado em [`W/STATUS.md`](../../W/STATUS.md);
3. a especificação e os exemplos canônicos atuais, sempre limitados pelo status de Working Draft;
4. [`W/VISION.md`](../../W/VISION.md), apenas para direção de produto e princípios;
5. os cadernos históricos, o histórico Git e os spikes, como provenance e alternativas.

Regras adicionais:

- uma frase de um resumo gerado não prevalece sobre uma fonte que registra alternativas;
- código experimental prova que uma pergunta foi investigada, não que a solução está correta;
- aparecer repetidamente aumenta a relevância histórica de uma ideia, não seu status normativo;
- promover pesquisa exige exemplos coerentes, semântica, teste e plano de lowering conforme `STATUS.md`;
- se a origem de uma afirmação não puder ser localizada, ela deve ser marcada como reconstrução ou hipótese.

Assim, a documentação atual pode evoluir sem reescrever o passado e o passado pode continuar útil sem governar acidentalmente a linguagem.
