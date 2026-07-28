# Sistema de pacotes e builds do W

**Status:** Working Draft
**Data:** 2026-07-18

## 1. Resumo

O sistema de pacotes e builds é uma vantagem *first-party* do W. Ele deve tornar
o caminho entre fonte, dependências e executável previsível, auditável e rápido,
sem transformar resolução de pacotes em semântica da linguagem.

O compilador conhece módulos, tipos, ABI e imports. O toolchain `w` resolve de
onde vêm os módulos, fixa versões e artefatos, executa builds isolados, mantém o
cache e verifica evidências. Um programa W continua sendo um programa W mesmo
quando é compilado por outro build system; `package.w` e `package.lock` são
entradas do toolchain, não construções executadas em runtime.

O desenho combina cinco propriedades:

1. manifesto declarativo e legível;
2. lockfile resolvido obrigatório;
3. cache imutável endereçado por conteúdo;
4. fonte e binários vinculados a target, ABI, toolchain e provenance;
5. distribuição por mirrors não confiáveis, com confiança ancorada em metadata
   assinada e verificável.

Nenhum selo isolado significa “este pacote é seguro”. Integridade, identidade,
proveniência, reprodutibilidade, revisão e estabilidade são evidências distintas,
avaliadas por políticas explícitas.

## 2. Escopo e separação da linguagem

Este documento especifica o produto `w` nas funções de resolver, buscar,
verificar, construir, empacotar e publicar. Ele também define os contratos dos
arquivos `package.w` e `package.lock`.

Pertencem à linguagem:

- a semântica de módulos e imports;
- o sistema de tipos e a representação de interfaces exportadas;
- a ABI W e suas regras de compatibilidade;
- a emissão dos metadados necessários para consumir um módulo compilado.

Pertencem ao toolchain:

- nomes, versões e origem de pacotes;
- resolução do grafo de dependências e features;
- seleção entre fonte, biblioteca estática ou dinâmica;
- cache, download, build, assinatura, SBOM e provenance;
- políticas de rede, confiança, sandbox e publicação.

O compilador pode receber do toolchain um mapa já resolvido de módulos e
artefatos. Ele não consulta registries, escolhe mirrors nem atualiza versões por
conta própria.

## 3. Linguagem de requisitos candidatos

Este documento ainda é um Working Draft. Portanto, **DEVE**, **NÃO DEVE**,
**DEVERIA** e **PODE** expressam requisitos candidatos para prototipação e
revisão; não são uma norma já aprovada nem uma promessa de compatibilidade.
Entre esses requisitos candidatos, **DEVE** e **NÃO DEVE** indicam obrigação,
**DEVERIA** indica o padrão recomendado, que pode ser substituído por uma política
explícita, e **PODE** indica capacidade opcional.

## 4. Objetivos

- Produzir o mesmo grafo de dependências para o mesmo manifesto, lockfile,
  target e conjunto de políticas.
- Permitir builds herméticos e, quando o ecossistema permitir, reproduzíveis
  bit a bit.
- Reutilizar downloads e outputs com segurança entre projetos por meio de um
  cache content-addressed.
- Tratar código-fonte, bibliotecas estáticas, bibliotecas dinâmicas, executáveis,
  headers, símbolos e recursos como artefatos verificáveis de primeira classe.
- Preferir distribuição estática quando ela simplificar deployment e preservar
  otimização, sem impô-la onde dinâmica, source build ou uma ABI do sistema
  forem mais corretas.
- Operar com registries centralizados, registries privados, GitHub Releases,
  object storage, mirrors comunitários e mídia offline sem confiar no transporte.
- Tornar visível por que cada pacote e artefato foi escolhido.
- Fornecer provenance, SBOM e material suficiente para auditoria e reconstrução.
- Ser útil também para bibliotecas C e outros artefatos nativos usados pelo W.
- Permitir políticas organizacionais mais estritas sem criar um fork do formato.

## 5. Não objetivos

- Provar que uma dependência não contém vulnerabilidades ou comportamento
  malicioso.
- Considerar uma assinatura, hash, tier ou build reproduzível como certificado
  universal de segurança.
- Instalar silenciosamente drivers, serviços do sistema ou pacotes privilegiados.
- Substituir, na primeira versão, `apt`, `rpm`, Homebrew, winget e os mecanismos
  de atualização do sistema operacional.
- Inferir com perfeição dependências a partir de um binário. Inspeção do loader
  é diagnóstica: ela não encontra, por exemplo, código estático, header-only,
  plugins carregados dinamicamente ou caminhos opcionais.
- Definir um formato universal de instalador ou uma loja de aplicativos.
- Exigir um serviço hospedado pela equipe do W para builds locais ou privados.
- Fazer o manifesto executar código arbitrário.
- Prometer reprodutibilidade quando algum input relevante não foi capturado.

## 6. Modelo de ameaça

### 6.1 Adversários considerados

O sistema assume que um adversário pode:

- controlar a rede, DNS, CDN ou um mirror;
- modificar, omitir, truncar ou repetir respostas antigas;
- comprometer uma conta de publicação, uma chave online, um registry ou parte
  da CI;
- publicar um pacote com nome semelhante, criar dependency confusion ou trocar
  o conteúdo associado a uma referência mutável;
- oferecer metadata e artefatos de momentos diferentes (*mix-and-match*);
- tentar downgrade, rollback, freeze ou versão artificialmente avançada;
- enviar payloads excessivos, archives malformados ou cache poisoning;
- incluir scripts de build que leem secrets, acessam a rede, alteram o host ou
  produzem outputs não determinísticos;
- oferecer um binário legítimo para o target errado ou incompatível com ABI,
  libc, runtime, features de CPU ou toolchain;
- comprometer uma dependência direta ou transitiva de forma válida do ponto de
  vista criptográfico.

### 6.2 Premissas de confiança

O projeto local, seu lockfile aceito e as raízes de confiança configuradas não
podem estar todos comprometidos ao mesmo tempo. O cliente `w` e as primitivas
criptográficas fazem parte da base confiável. Builds com exigência forte também
declaram qual builder e toolchain são confiáveis.

Comprometimento do kernel, do hardware, do compilador bootstrap ou de todas as
chaves acima do quorum está fora da proteção completa desta versão. O sistema
deve, contudo, guardar evidências que ajudem a detectar e recuperar incidentes.

Um atacante de rede sempre pode negar acesso a todos os mirrors. O cliente não
consegue impedir esse DoS, mas DEVE distinguir indisponibilidade de uma resposta
válida dizendo que não há atualização.

### 6.3 Propriedades esperadas

Sob essas premissas, o cliente DEVE detectar:

- bytes diferentes dos que foram resolvidos;
- metadata expirada, rollback e combinações inconsistentes de snapshots;
- target, ABI, toolchain ou feature set incompatível;
- assinatura ausente ou identidade não autorizada quando a política exigir;
- execução de build que solicite capacidade não concedida;
- lockfile divergente do manifesto ou da seleção pedida;
- tentativa de substituir um objeto já presente no cache pelo mesmo digest.

## 7. Por que um hash isolado não basta

Um digest responde apenas: “estes bytes são iguais aos bytes que alguém
esperava?”. Ele não responde:

- quem declarou o digest esperado;
- se essa pessoa ou CI estava autorizada a publicar o pacote;
- se o digest é o mais recente ou foi reapresentado em um rollback;
- se a metadata de dependências pertence à mesma release do artefato;
- se o binário foi produzido a partir da fonte declarada;
- qual toolchain, configuração e grafo entraram no build;
- se o pacote foi revisado ou contém vulnerabilidades conhecidas.

Se um registry comprometido puder trocar ao mesmo tempo o arquivo e o hash
exibido ao cliente, a comparação passa e nada foi autenticado. Por isso, digests
devem estar dentro de metadata versionada e assinada, ancorada em uma identidade
ou raiz de confiança conhecida. Freshness, delegação e revogação são requisitos
separados. Provenance vincula output a inputs e processo; transparência torna a
publicação observável; reprodução independente testa a alegação do builder.

O hash continua essencial: ele é a identidade imutável do objeto no cache e a
última verificação dos bytes. Ele só não deve carregar sozinho um significado que
não possui.

## 8. Manifesto `package.w`

### 8.1 Natureza declarativa

`package.w` usa uma forma legível como W, mas aceita apenas um subconjunto de
dados: records, listas, strings, números, booleanos, enums e referências locais
declaradas pelo schema. Não há loops, IO, imports executáveis, chamadas de função,
leitura de ambiente ou avaliação de código.

O parser do manifesto é independente do parser completo da linguagem. Isso
permite validar e resolver um projeto antes de existir um compilador W funcional,
reduz o bootstrap confiável e evita que apenas “ler dependências” execute código.

Campos desconhecidos geram erro por padrão. Extensões usam namespaces explícitos
e preserváveis. O formato canônico usado para digest e assinatura não depende de
whitespace, comentários ou ordem irrelevante de campos.

### 8.2 Exemplo proposto

```w
package {
  schema: "w.package/1"

  name: "acme/echo-server"
  version: "1.4.0"
  license: "Apache-2.0"
  sourceRoot: "."

  language: {
    w: ">=0.1 <0.2"
  }

  products: [
    {
      name: "echo"
      kind: .executable
      root: "src/main.w"
    },
    {
      name: "echo-core"
      kind: .library
      root: "src/core.w"
      linkage: .preferStatic
    }
  ]

  dependencies: [
    {
      alias: "http"
      package: "w/http"
      version: "^2.3"
      source: {
        kind: .registry
        registry: "w"
      }
      features: ["http1", "tls"]
    },

    {
      alias: "sqlite"
      package: "community/sqlite"
      version: ">=3.50 <4"
      source: {
        kind: .registry
        registry: "w"
      }
      linkage: .preferStatic
    },

    {
      alias: "metrics"
      package: "acme/metrics"
      version: "0.8.2"
      source: {
        kind: .git
        url: "https://github.com/acme/metrics"
        rev: "9b6d4a1"
      }
      optional: true
    }
  ]

  features: {
    default: ["metrics"]
    metrics: ["dependency:metrics"]
  }

  build: {
    minimumSandbox: .isolated
    network: .deny
    environment: []
    outputs: ["generated/**"]
  }

  artifacts: {
    source: .required
    binary: .allowed
    linkage: .preferStatic
  }
}
```

O manifesto expressa intenção e intervalos. Ele NÃO DEVE conter o resultado da
resolução. Digests exatos, versões transitivas, URL final, seleção por target e
evidências pertencem ao lockfile e à metadata de release.

### 8.3 Campos mínimos

Um pacote publicável DEVE declarar `schema`, nome canônico, versão, licença ou
marcação explícita de licença não informada, ao menos um produto e sua origem.
Dependências DEVEM possuir nome, constraint e origem não ambígua.

Referências Git por branch ou tag PODEM existir no manifesto para ergonomia, mas
o lockfile sempre as converte em commit imutável e snapshot com digest. Imports
por URL seguem a mesma regra: a URL localiza; o lock identifica.

## 9. Lockfile resolvido obrigatório

### 9.1 Regra

Todo workspace com build reproduzível ou com dependências DEVE possuir um
`package.lock` atual. O arquivo é gerado pelo resolver, versionado junto ao projeto
e tratado como input do build. Um pacote-biblioteca publicado também publica seu
manifesto; consumidores resolvem seu próprio grafo, mas aplicações e releases
sempre usam seu lockfile.

O lockfile é necessário porque o manifesto não determina sozinho:

- a versão exata escolhida para cada intervalo;
- as versões transitivas e o grafo completo;
- o commit por trás de uma tag ou branch;
- o conjunto final de features e dependências opcionais;
- o artefato selecionado para cada target, ABI e profile;
- o snapshot de metadata e as regras do resolver usadas;
- a fonte exata usada como fallback de um binário.

Sem lockfile, dois builds feitos em dias, plataformas ou registries diferentes
podem escolher grafos distintos e ainda obedecer ao mesmo manifesto. Espalhar
hashes pelo source apenas fixa alguns arquivos; não registra a decisão global nem
prova que todas as arestas foram resolvidas juntas.

### 9.2 Conteúdo mínimo

`package.lock` DEVE registrar, em forma canônica:

- versão do schema e do algoritmo de resolução;
- digest do manifesto e policies relevantes;
- contexts resolvidos: target, profile, features e opções ABI;
- versão, origem imutável e digest da fonte de cada pacote;
- arestas exatas do grafo e razão de seleção;
- artefato escolhido, tamanho, digest, target, ABI, linkage, CPU baseline e
  identidade do toolchain;
- digests de provenance, SBOM, assinatura e bundle de transparência quando
  disponíveis ou exigidos;
- snapshot/version da metadata confiável usada na resolução;
- exceções de política aprovadas, sem armazenar secrets.

O formato pode ter uma visualização humana, mas a representação assinada e
hasheada DEVE ser determinística. Alterações manuais tornam o arquivo inválido;
`w explain` e `w diff-lock` fornecem a interface de inspeção.

### 9.3 Operação

- `w resolve` cria ou atualiza o lockfile.
- `w build --locked` falha se manifesto, context ou lock divergir.
- CI e release usam `--locked` por padrão.
- `w update <pacote>` altera somente a parte necessária do grafo, salvo quando
  a restrição tornar isso impossível, e mostra o diff antes de gravar.
- Um build para novo target adiciona um context ao lock; não substitui
  silenciosamente os contexts existentes.

## 10. Cache content-addressed

O cache global guarda objetos imutáveis por digest, não por nome ou URL. O digest
é prefixado pelo algoritmo, no formato `<algorithm>:<hex>`, para permitir migração
sem fixar o desenho a uma função específica.
Blobs, árvores de fonte, manifests canônicos, artefatos, SBOMs e attestations são
tipos distintos, mesmo quando compartilham o mesmo armazenamento físico.

Fluxo de ingestão:

1. baixar para arquivo temporário com limite de tamanho;
2. verificar tamanho, formato e digest esperado durante o streaming;
3. verificar metadata, assinatura e policy antes de marcar o objeto como usável;
4. mover atomicamente para o endereço final;
5. nunca sobrescrever um objeto existente; discrepância é corrupção/incidente.

Índices por nome, versão, URL ou último acesso são reconstruíveis e não são fonte
de confiança. A presença no cache não implica aprovação: a decisão de uso depende
do lockfile, raízes de confiança e policy atual.

O garbage collector preserva objetos fixados por workspaces, lockfiles, bundles
offline e releases. `w cache gc --dry-run` DEVE explicar cada retenção. O cache
PODE ser compartilhado por usuários ou CI somente quando permissões e isolamento
impedirem que um usuário substitua objetos ou metadata confiável de outro.

## 11. Fonte e artefatos binários

### 11.1 Fonte como artefato

O snapshot de fonte é um artefato imutável com digest próprio. Ele inclui apenas
arquivos declarados para a release e normaliza detalhes de archive que não fazem
parte do conteúdo lógico. Releases oficiais dos tiers mantidos pelo W DEVEM
publicar fonte. Pacotes binários fechados PODEM existir nos tiers externos, mas
devem declarar explicitamente `source: .unavailable` e podem ser bloqueados por
policy.

### 11.2 Chave de compatibilidade binária

Um artefato nativo só é candidato quando toda sua chave de compatibilidade bate:

- target triple e versão mínima do sistema;
- arquitetura e baseline/extensões de CPU;
- ABI W e ABI C/C++ relevante;
- runtime e libc, quando aplicável;
- identidade e versão do toolchain/linker;
- profile, features e configuração pública;
- linkage: static, dynamic, executable ou interface/header;
- formato e versão dos metadados de módulo exportados.

Compatibilidade não é inferida por “parece próximo”. Se não houver artefato
confiável e exatamente compatível, o resolver tenta um build da fonte, se a policy
permitir, ou falha com diagnóstico reproduzível.

### 11.3 Static preferencial, não universal

`.preferStatic` é a policy padrão para pacotes W portáveis porque simplifica
deployment, permite otimização intermodular e fixa mais do grafo no executável.
Não é uma obrigação universal. Linkagem dinâmica pode ser correta para:

- bibliotecas do sistema e frameworks com ABI administrada pela plataforma;
- plugins carregados em runtime;
- componentes compartilhados que precisam de atualização independente;
- restrições de licença;
- redução de duplicação ou memória em um deployment específico;
- interoperabilidade com software que só fornece ABI dinâmica.

As opções são `.requireStatic`, `.preferStatic`, `.preferDynamic`,
`.requireDynamic` e `.fromSource`. O lockfile registra a decisão real e sua razão.
Uma alteração de linkage é uma alteração de resolução, não um detalhe invisível
do linker.

Headers, símbolos de debug e recursos DEVEM permanecer vinculados ao artefato
principal por metadata e digest. Inspeção de `LD`, PE ou Mach-O pode conferir o
resultado, mas a declaração de dependências do pacote é normativa.

## 12. Build, provenance, SBOM e reprodutibilidade

O contrato detalhado de payload determinístico, receita, attestations, envelopes
de plataforma, rebuilds N-de-M e publicação está em
[Builds verificáveis e releases reproduzíveis](verification-and-releases.md).
Este documento mantém apenas as regras necessárias à integração com resolução,
lockfile e cache.

### 12.1 Receita resolvida

Antes da execução, `w build` materializa uma receita resolvida contendo:

- fonte e grafo do lockfile;
- toolchain e builder por digest;
- target, profile, flags e features;
- variáveis de ambiente permitidas e seus valores não secretos ou digests;
- locale, timezone, epoch e outros inputs relevantes;
- capacidades de sandbox;
- outputs esperados.

Informação dinâmica como data de release deve entrar como input explícito, não
ser lida silenciosamente do relógio. Secrets nunca entram em artefatos ou logs;
quando inevitáveis para publicação, sua presença é registrada como uma capacidade,
não como conteúdo.

### 12.2 Provenance

Cada build publicável DEVE emitir uma attestation que vincule outputs, fonte,
dependências, receita, builder e toolchain. O modelo interno deve poder ser
exportado para formatos interoperáveis, como [SLSA Provenance](https://slsa.dev/spec/v1.2/provenance),
sem exigir que toda build local use infraestrutura SLSA.

Provenance diz onde, quando e como um output foi produzido; não prova que o
builder era honesto. A policy decide quais identidades de builder aceita e pode
exigir reprodução por outra infraestrutura.

### 12.3 SBOM

Uma release DEVE gerar SBOM do grafo resolvido, incluindo componentes estáticos,
dinâmicos, gerados e ferramentas que contribuíram para o output. O SBOM referencia
digests e identificadores do lockfile, licenças conhecidas e relação entre
componentes. Formatos externos podem ser exportados por adapters; o grafo do lock
continua sendo a fonte de verdade para resolução.

O SBOM não deve depender de encontrar “assinaturas” de uma biblioteca já
otimizada dentro do binário: LTO, stripping e deduplicação tornam essa heurística
incompleta. Scanners binários são evidência adicional.

### 12.4 Builds reproduzíveis

O W adota a definição do projeto [Reproducible Builds](https://reproducible-builds.org/docs/definition/):
mesma fonte, ambiente e instruções devem permitir que outra parte produza os
mesmos artefatos bit a bit.

O status de uma release é registrado separadamente:

- `.unknown`: nenhuma alegação;
- `.claimed`: o produtor afirma determinismo;
- `.reproduced`: ao menos uma reconstrução independente bateu;
- `.diverged`: uma tentativa comparável não bateu e requer investigação.

Uma divergência não é automaticamente prova de ataque, mas não pode ser ocultada.
Logs e diffs ficam fora do digest do artefato principal, vinculados como evidência.

## 13. Assinatura, delegação e transparência

O sistema distingue quatro atos:

1. mantenedores autorizam uma release e sua metadata;
2. builders atestam como produziram artefatos;
3. registries publicam snapshots consistentes e frescos;
4. plataformas podem aplicar assinaturas nativas ao instalável final.

Uma única chave não deve acumular todos os papéis. A metadata suporta delegação,
thresholds, expiração, rotação e revogação. Chaves raiz podem permanecer offline;
chaves online têm autoridade limitada. O desenho deve ser compatível com as
proteções do [The Update Framework](https://theupdateframework.github.io/specification/latest/)
contra rollback, freeze, mix-and-match e comprometimento parcial de chaves, seja
por integração com TUF ou por um perfil documentado e auditado equivalente.

Assinatura tradicional com chave própria, KMS/HSM e assinatura baseada em
identidade são políticas possíveis. [Sigstore](https://docs.sigstore.dev/) é uma
integração preferencial para projetos públicos: certificados curtos vinculam uma
identidade ao evento, e o log de transparência torna a assinatura auditável.
Empresas e ambientes air-gapped podem usar raízes e logs privados.

O cliente verifica a identidade esperada, não apenas “há uma assinatura válida”.
Bundles devem carregar material suficiente para verificação offline, incluindo
prova/timestamp de transparência quando a policy exigir. Transparência permite
detectar publicação indevida; não transforma código assinado em código seguro e
requer monitoramento.

## 14. Registries e mirrors

Registry é autoridade de metadata; mirror é transporte. Um mirror pode hospedar
qualquer subset de metadata e artefatos, mas nunca ganha autoridade por estar em
uma lista de URLs. Todo conteúdo recebido é limitado por tamanho, verificado pelo
digest e validado contra metadata assinada antes de entrar no conjunto utilizável.

O cliente pode tentar mirrors em paralelo ou por latência, custo e preferência
local. Respostas diferentes para o mesmo digest são corrupção. Falta de conteúdo
gera fallback. Metadata antiga não substitui snapshot mais novo já conhecido.

Uma lista de “mirrors seguros” pode existir como UX e disponibilidade, mas não é
uma raiz de confiança nos bytes. Mirrors comunitários, GitHub Releases, buckets,
CDNs e caches corporativos são equivalentes do ponto de vista criptográfico.

O registry DEVE prevenir dependency confusion com nomes canônicos e namespaces
delegados. Aliases locais são permitidos; identidade do lockfile sempre usa o
nome canônico e a origem. Registries múltiplos formam raízes distintas e não são
mesclados implicitamente.

## 15. Políticas offline e ambientes isolados

O toolchain oferece modos explícitos:

- `online`: atualiza metadata de freshness e busca objetos ausentes;
- `prefer-offline`: usa o cache quando válido e consulta a rede se necessário;
- `frozen`: não altera o lockfile, mas pode buscar exatamente seus objetos;
- `offline`: não realiza nenhuma operação de rede e falha se faltar objeto ou
  evidência exigida.

`w bundle offline` cria um pacote content-addressed com lockfile, objetos, roots,
metadata, assinaturas, provenance, SBOM e provas necessárias. `w cache import`
verifica tudo antes de disponibilizar o bundle. Isso permite CI isolada e redes
air-gapped sem transformar mídia removível em raiz de confiança.

Expiração de metadata continua relevante offline. A policy pode:

- rejeitar metadata expirada;
- aceitar somente objetos já fixados por um lock e snapshot anteriormente
  confiável, com warning e registro da exceção;
- usar roots, timestamps e revogações transportados por um bundle administrativo.

O cliente NÃO DEVE ignorar expiração silenciosamente. `--allow-stale-metadata`
exige consentimento interativo ou regra organizacional pré-aprovada e aparece na
attestation do build. Cache pinning protege os objetos necessários contra GC.

## 16. Scripts, geração e comptime

### 16.1 Separação

O manifesto nunca é um script. Quando um pacote precisa gerar código, adaptar uma
biblioteca externa ou executar comptime, ele declara uma unidade de build separada,
cujo source e permissões são fixados no lockfile e na provenance.

### 16.2 Sandbox padrão

Builds de dependências executam sem rede, sem acesso ao diretório pessoal, sem
secrets, sem dispositivos e sem escrita fora dos diretórios de input/output. O
relógio, locale, randomness e enumeração do host são normalizados ou negados.
CPU, memória, processos, tempo e tamanho dos outputs têm limites.

Capacidades possíveis incluem:

- ler um conjunto de inputs por digest;
- gravar somente outputs declarados;
- executar tools fixadas no lockfile;
- ler variáveis explicitamente permitidas;
- acessar domínio e método de rede específicos;
- chamar uma ferramenta do sistema explicitamente aprovada.

Cada ampliação aparece antes da execução. Em terminal interativo, o usuário pode
aprovar uma vez, para o workspace ou negar. Em CI, capacidade não prevista pela
policy falha; não há prompt implícito nem `--yes` universal.

Comptime segue as mesmas regras. Uma função comptime não recebe autoridade do
compilador apenas por estar escrita em W. Inputs e outputs entram na chave do
cache; acesso não determinístico desabilita a alegação de reprodutibilidade.

Instalar driver, iniciar serviço, chamar `sudo` ou modificar package manager do
host nunca é uma capacidade transitiva normal. Nas fases iniciais, `w doctor`
apenas detecta a dependência de sistema e mostra origem e instruções. Integrações
privilegiadas futuras exigem produto, consentimento e threat model próprios.

## 17. Estabilidade e evidência de segurança são eixos distintos

Os tiers respondem “quem mantém e qual compatibilidade promete?”, não “é seguro?”:

| Tier | Significado |
|---|---|
| `core` | Necessário à implementação da linguagem; mudança segue governança do W |
| `official` | Mantido ou adotado oficialmente, com política de compatibilidade |
| `community` | Mantido pela comunidade sob requisitos mínimos de publicação |
| `experimental` | API e manutenção sem garantia forte |
| `external` | Fora do índice governado; origem explícita |

Evidências são registradas como facetas, não como um ranking único:

- integridade conhecida;
- identidade e autorização verificadas;
- metadata fresca e consistente;
- provenance disponível e builder aceito;
- SBOM disponível;
- reprodução independente;
- revisão manual ou automatizada identificada;
- advisories e revogações aplicáveis.

Um pacote `core` pode sofrer incidente; um pacote `external` pode ter excelente
provenance. Um pacote assinado pode ser malicioso. A policy combina facetas e
contexto. Estados como `revoked`, `known-vulnerable` ou `diverged` bloqueiam ou
alertam conforme policy, mas não reclassificam o tier de estabilidade.

Promoção entre tiers é decisão de manutenção e API. Conceder confiança a uma
identidade, aceitar um builder ou retirar uma release é decisão de segurança.

## 18. CLI proposta

### 18.1 Projeto e resolução

```text
w init                         cria package.w e package.lock mínimos
w add <pacote>                 altera manifesto e resolve o novo grafo
w remove <pacote>              remove e resolve
w resolve                      resolve todos os contexts e grava o lockfile
w update [pacote]              atualiza dentro das constraints e mostra o diff
w explain <pacote>             explica origem, versão, features, linkage e policy
w diff-lock [ref]              apresenta mudanças semânticas do lockfile
```

### 18.2 Fetch, build e verificação

```text
w fetch --locked               popula o cache sem construir
w build --locked               constrói sem alterar resolução
w build --offline --locked     constrói sem rede usando apenas cache confiável
w test --locked                testa o grafo fixado
w verify [pacote|artefato]     verifica bytes, metadata, identidade e evidências
w audit                        resume provenance, SBOM, advisories e exceções
w sbom --format <formato>      exporta o SBOM do lockfile
w reproduce <artefato>         tenta reconstrução independente e compara outputs
```

### 18.3 Cache, confiança e publicação

```text
w cache status
w cache gc --dry-run
w cache import <bundle>
w bundle offline
w trust list
w trust add <root|identity>
w trust revoke <root|identity>
w publish --locked
w yank <pacote>@<versão>       impede nova resolução sem apagar metadata de auditoria
w doctor                       diagnostica toolchain e dependências do sistema
```

`w install` pode permanecer como alias ergonômico de `w fetch`, mas não significa
copiar dependências para uma pasta global nem instalar pacotes do sistema. Toda
operação mutável mostra quais arquivos, roots ou policies serão alterados.

Erros devem distinguir resolução, rede, integridade, autenticação, freshness,
policy, incompatibilidade ABI, sandbox e falha do build. `w explain-error` pode
apresentar a cadeia completa sem recomendar bypass inseguro como primeira opção.

## 19. Rollout em fases

### Fase 0 — contratos e protótipo

- congelar schemas experimentais de manifesto, lock e identidade de módulo;
- implementar resolver determinístico e casos de teste adversariais;
- definir target triple, ABI e formato de digest;
- construir threat model e formato de policy antes do registry público.

### Fase 1 — fonte, lock e cache local

- dependências por path, Git imutável e HTTPS;
- lockfile obrigatório e CAS local;
- build da fonte com sandbox sem rede;
- `w resolve`, `fetch`, `build --locked`, `explain` e `cache`;
- nenhum script privilegiado e nenhuma instalação automática do sistema.

### Fase 2 — artefatos nativos e auditoria

- static/dynamic/source por target, ABI, toolchain e features;
- static preferencial com fallback explícito;
- receita resolvida, provenance e SBOM;
- builds determinísticos, bundles de release e comparação reproduzível;
- registries ainda podem ser simples índices assinados.

### Fase 3 — distribuição segura e colaboração

- metadata com delegação, thresholds, expiração e snapshots consistentes;
- integração TUF/Sigstore ou perfis interoperáveis equivalentes;
- logs de transparência, rotação/revogação e mirrors não confiáveis;
- registries privados, caches corporativos e bundles offline.

### Fase 4 — governança e experiência do ecossistema

- tiers, promoção e políticas de compatibilidade;
- UI web/IDE para explicar grafo e evidências;
- reprodução comunitária, advisories e revisão automatizada;
- métricas com privacidade e sem convertê-las em prova de segurança.

### Fase 5 — produtos adjacentes opcionais

- adapters declarativos para dependências de sistema;
- installers e auto-update de aplicações;
- integração com lojas e package managers de cada plataforma;
- serviços hospedados de build, suporte e distribuição.

Esta fase é deliberadamente separada: ela reutiliza o CAS, metadata e provenance,
mas não faz parte do bootstrap da linguagem nem do gerenciador de bibliotecas.

## 20. Questões em aberto

- Escolha inicial do formato canônico do lockfile e das attestations.
- Política de resolução de múltiplas versões do mesmo pacote em um executável.
- Definição e evolução da ABI W antes de distribuir binários duráveis.
- Granularidade das features na chave de artefato e no grafo.
- Algoritmo de digest inicial e plano de migração criptográfica.
- Modelo de identidade/namespaces para registry público e organizações privadas.
- Regras exatas para yanks, advisories, revogação e builds offline antigos.
- Quais partes do toolchain podem ser autocontidas no bootstrap mínimo.
- Como representar licenças, exceções e obrigações sem criar um novo padrão.
- Formato de policies organizacionais e sua composição com policies do projeto.

## 21. Referências

- [The Update Framework — Specification](https://theupdateframework.github.io/specification/latest/)
- [Sigstore — documentação oficial](https://docs.sigstore.dev/)
- [Reproducible Builds — definição](https://reproducible-builds.org/docs/definition/)
- [SLSA — Provenance](https://slsa.dev/spec/v1.2/provenance)
