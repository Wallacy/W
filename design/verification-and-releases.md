# Builds verificáveis e releases reproduzíveis

**Status:** Working Draft

**Data:** 2026-07-19

Este documento define a arquitetura candidata que liga fonte, build, artefato,
publicação e verificação no ecossistema W. O [sistema de pacotes](packages.md)
continua responsável por resolução, lockfile, cache e seleção de artefatos; aqui
fica o contrato detalhado de reprodutibilidade e evidências de uma release.

## 1. Estados das decisões

- **Direção:** mesmos inputs declarados, target, toolchain e ambiente fixados
  devem produzir exatamente os mesmos bytes do **payload não assinado**.
- **Direção:** integridade, autorização, provenance, reprodução, análise de
  segurança e auditoria humana são evidências independentes.
- **Candidato:** todo build publicável materializa uma receita canônica e um
  manifesto de artefato; ambos são endereçados por digest.
- **Candidato:** o payload carrega uma nota W mínima e determinística, enquanto
  attestations e provas de transparência ficam externas.
- **Candidato:** o registry governa metadata assinada; CDNs, GitHub Releases,
  object storage e caches apenas transportam objetos imutáveis.
- **Em aberto:** formatos criptográficos, algoritmo inicial de digest, política
  padrão de quorum e formato físico da nota W.
- **Pesquisa:** builders confidenciais e provas de execução em hardware
  atestado para código fechado.
- **Rejeitado por enquanto:** um JWT embutido no executável como prova completa
  de origem, segurança ou reprodução.
- **Rejeitado por enquanto:** uma nota de zero a cinco estrelas que comprima
  todas as evidências de segurança em um único número.

## 2. Garantia de reprodução

Para uma receita resolvida `R`, o contrato candidato é:

```text
build(R) -> payload

R1 == R2  =>  bytes(payload1) == bytes(payload2)
```

A igualdade inclui, no mínimo, o snapshot de fonte, grafo fixado pelo lockfile,
target, ABI, profile, features, toolchain, SDK, sysroot, ferramentas auxiliares,
ambiente de build e flags. A garantia não diz que targets diferentes produzem o
mesmo arquivo nem que dois compiladores apenas nominalmente equivalentes o farão.

Data, commit, branch, diretório do checkout, timezone, locale, ordem de arquivos,
IDs incrementais e aleatoriedade não podem entrar por acidente. Quando forem
necessários, tornam-se inputs:

- data/epoch é um valor fixado pela receita;
- commit é a identidade imutável da fonte, não uma consulta implícita ao Git;
- paths do host são remapeados para paths virtuais estáveis;
- sementes aleatórias são declaradas;
- locale, timezone, permissões e ordenação são normalizados;
- timestamps, owners e ordem de membros de archives são canônicos;
- paralelismo de build não pode alterar a ordem ou o conteúdo do output.

Um input não capturado invalida a alegação de reprodução. O toolchain deve falhar
em modo estrito ou marcar a release como não reproduzível; não deve completar o
build e publicar silenciosamente um selo mais forte do que a evidência permite.

Reprodutibilidade demonstra que duas execuções comparáveis chegaram aos mesmos
bytes. Ela não demonstra que a fonte é benigna, que o compilador é honesto ou
que o programa não tem vulnerabilidades.

## 3. Três identidades que não podem ser confundidas

### 3.1 Payload determinístico

O payload é a biblioteca, executável, módulo ou bundle antes de assinaturas e
notarização específicas da plataforma. Seu digest é a identidade imutável usada
no cache, no lockfile e na metadata da release.

O payload pode conter uma seção ou nota W mínima, por exemplo `.note.w`, com
campos estáveis que auxiliem inspeção e diagnóstico:

- versão do formato;
- nome e versão canônicos do pacote;
- target, ABI W e linkage;
- digest da receita, do lockfile e da árvore de fonte;
- identidade por digest do toolchain;
- perfil e conjunto de features.

A nota não contém o digest do payload que a contém, pois isso criaria uma
referência circular, nem carrega a assinatura da própria release. O digest do
payload é registrado no manifesto externo. Ferramentas de segurança podem ler a
nota sem executar o programa e então consultar ou verificar as evidências
vinculadas.

### 3.2 Envelope da plataforma

Code signing, notarização e empacotadores de cada sistema podem incluir
timestamps, certificados ou estruturas que mudam os bytes finais. Eles formam
um envelope separado em torno do payload reproduzível:

```text
payload determinístico -> digest do payload -> envelope assinado da plataforma
```

O manifesto registra os digests de ambos. Uma política pode exigir a assinatura
nativa para instalação e, de forma independente, exigir que o payload interno
corresponda ao reproduzido. A assinatura do envelope não redefine a identidade
do payload.

### 3.3 Provenance e attestations

Provenance é uma declaração assinada sobre quem executou qual receita, em qual
builder, e quais outputs observou. Reprodutores independentes emitem attestations
adicionais que ligam seu resultado ao mesmo digest de receita e payload. Essas
declarações ficam externas ao payload e podem ser incluídas em um bundle offline
e/ou registradas em um log append-only de transparência.

O modelo não depende de um fornecedor ou formato específico. Implementações
podem exportar formatos interoperáveis, mas a semântica W exige que identidade,
papel, subject, recipe, timestamp, assinatura, expiração/revogação e prova de
inclusão sejam verificáveis.

### 3.4 Por que não um JWT nativo

Um token embutido parece oferecer uma identidade observável, mas não resolve o
problema completo:

- assinar bytes que contêm a própria assinatura cria dependência circular;
- timestamps e rotação de certificados quebrariam os bytes reproduzíveis;
- uma assinatura válida prova controle de uma identidade, não correção ou
  segurança do source;
- revogação, múltiplos builders, auditorias e transparência evoluem sem que o
  executável deva ser reescrito;
- JWT é um envelope de claims, não um modelo de autorização de releases,
  reprodução independente ou defesa contra rollback.

A nota W determinística mais attestations externas preserva a descoberta local
pretendida pelo “JWT observável”, sem fundir identidades incompatíveis.

## 4. Receita canônica e manifesto de artefato

Antes de executar o compilador, `w` serializa canonicamente uma receita. Campos
secretos nunca são serializados; se uma fase autorizada usar um secret, a receita
registra somente o tipo de capacidade e perde a alegação de hermeticidade quando
esse valor puder afetar o payload.

Campos candidatos da receita:

```text
schema
package { namespace, name, version }
source { tree_digest, immutable_reference }
lock_digest
target { triple, os_minimum, cpu_baseline, abi_w, foreign_abis }
profile { name, features, linkage, compiler_flags, linker_flags }
toolchain { w_digest, tools[], sdk_digest, sysroot_digest }
environment { image_or_snapshot_digest, allowed_env, virtual_paths }
normalization { epoch, timezone, locale, random_seeds, archive_rules }
sandbox { capabilities, network, resource_limits }
inputs[] { logical_name, digest, media_type }
expected_outputs[] { logical_name, media_type }
```

O manifesto emitido depois do build referencia a receita e descreve o que foi
produzido:

```text
schema
recipe_digest
outputs[] { logical_name, payload_digest, size, media_type }
envelopes[] { payload_digest, envelope_digest, platform }
sbom_digest
provenance_digests[]
rebuild_attestation_digests[]
```

O manifesto é metadata, não parte do payload cujo digest registra. A codificação
canônica e a separação entre campos assinados e material de transporte devem ser
especificadas antes de interoperabilidade pública.

## 5. Fluxo de uma release

1. **Resolver.** `package.w`, policies e roots produzem `package.lock`; versões,
   fontes e ferramentas ficam imutáveis por digest.
2. **Materializar.** `w` cria a receita canônica e apresenta capacidades ou
   inputs ambientais ainda não fixados.
3. **Construir.** um builder isolado produz o payload não assinado; a rede fica
   negada e somente inputs declarados são legíveis.
4. **Identificar.** `w` calcula o digest do payload, emite SBOM, manifesto e
   provenance do builder.
5. **Reproduzir.** builders administrativamente independentes executam a mesma
   receita e publicam `match` ou `diverged`, com logs/diffs externos ao payload.
6. **Autorizar.** mantenedores aprovam a release e vinculam package/version aos
   digests e à policy aplicável.
7. **Envelopar.** quando necessário, a plataforma assina ou notariza uma cópia
   que referencia o mesmo payload interno.
8. **Publicar.** o registry publica um snapshot consistente de metadata e as
   attestations entram no log de transparência configurado.
9. **Replicar.** mirrors copiam os objetos por digest, sem ganhar autoridade
   sobre package, versão ou status.
10. **Consumir.** o cliente verifica freshness, autorização, digests, target/ABI,
    policy, evidências e revogações antes de disponibilizar o objeto no cache.

Uma falha de reprodução não apaga a release nem prova automaticamente ataque. O
estado `diverged` permanece visível, bloqueia policies que exigem reprodução e
abre investigação sobre inputs ou comprometimento.

## 6. Registry, mirrors e disponibilidade

O registry é uma autoridade de **metadata**: liga namespace, release, digests,
delegações, snapshots, expiração, yanks e revogações. Seus objetos podem estar no
mesmo provedor físico que um mirror, mas a confiança vem das assinaturas e roots,
não do hostname.

Bytes oficiais de W e da standard library podem ser servidos por caches da
Cloudflare. Pacotes comunitários podem apontar para GitHub Releases, registries
privados, buckets ou vários mirrors. Todos são transportes intercambiáveis: o
cliente aceita somente o objeto cujo digest aparece na metadata autorizada.

Esse desacoplamento permite trocar CDN, sobreviver à indisponibilidade de um
host e replicar releases sem alterar identidade. Também impede que “mirror safe”
seja interpretado como auditoria ou raiz de confiança.

## 7. Builders e reprodução N-de-M

O serviço oficial não deve depender da promessa de capacidade ilimitada de um
único provedor de CI. O protocolo candidato admite:

- builders mantidos pelo projeto;
- runners fornecidos por mantenedores ou organizações;
- reprodutores comunitários;
- builders corporativos privados;
- importação de attestations verificáveis feitas offline.

Uma policy pode exigir `N` resultados idênticos em `M` tentativas comparáveis.
O status registra também as identidades e domínios administrativos: dez runners
sob a mesma credencial não oferecem a mesma independência que builders operados
por partes distintas. O quorum não substitui revisão de source e não deve ser
fixado globalmente antes de medir custo, diversidade real e resistência a abuso.

GitHub Actions é um builder ou coordenador possível, especialmente para projetos
públicos; não é parte necessária do formato nem uma garantia permanente de
execução gratuita ou ilimitada. Cache por conteúdo e builders federados permitem
adotar limites, filas ou migração sem mudar a identidade das releases.

## 8. Fonte pública e código fechado

### 8.1 Fonte pública

Uma release pública fornece snapshot imutável de fonte, lockfile e receita. Um
terceiro pode reproduzi-la sem acesso privilegiado e emitir evidência
independente. O registry só mostra `reproduced N-of-M` para tentativas que usaram
a receita comparável e encontraram exatamente o digest publicado.

O projeto pode automatizar rebuilds e análises, mas publicação por uma CI e
reprodução pela mesma identidade são provenance do produtor, não confirmação
independente.

### 8.2 Código fechado

Um pacote fechado pode entregar fonte temporariamente a um builder contratado,
confidencial ou controlado pelo registry. Esse builder pode confirmar que gerou
o payload e emitir provenance, mas o público não consegue reproduzir nem auditar
a alegação sem o mesmo acesso.

O status deve dizer `source: confidential` ou `source: unavailable` e identificar
o builder confiado. “Inputs apagados depois do build” é uma declaração de processo
que exige confiança e auditoria do operador; não é uma garantia criptográfica.
Execução confidencial e atestação de hardware permanecem **Pesquisa** até existir
um threat model que cubra operador, logs, cache, snapshots, chaves e exfiltração.

Uma UI não pode chamar esse caso de “publicamente reproduzível”, “open audit” ou
equivalente. Policies podem aceitá-lo, recusá-lo ou exigir múltiplos builders
privados independentes.

## 9. Evidências exibidas em eixos independentes

O portal e `w verify` apresentam fatos e escopo, nunca uma estrela agregada:

| Eixo | Exemplos de estado | Informação obrigatória |
|---|---|---|
| autorização/assinatura | `verified`, `missing`, `invalid`, `revoked` | identidade, papel, threshold e validade |
| disponibilidade da fonte | `public`, `confidential`, `unavailable` | digest e regra de acesso, quando houver |
| reprodução | `not-attempted`, `claimed`, `N-of-M matched`, `diverged` | receita, builders e tentativas |
| provenance | `available`, `policy-accepted`, `rejected` | builder, subject e recipe digest |
| análise automatizada | `not-run`, `passed`, `findings` | ferramenta, versão, regras, data e escopo |
| auditoria humana | `none`, `reviewed`, `findings` | equipe, commit/digest, escopo e data |
| advisories/revogação | `clear-as-of`, `affected`, `yanked`, `revoked` | fonte, timestamp e motivo |

`passed` significa apenas que aquela ferramenta e regras não encontraram um
problema dentro do escopo informado. Auditorias expiram semanticamente quando a
fonte, receita, dependências ou escopo mudam. A UI pode resumir “policy atendida”,
mas deve permitir inspecionar cada predicado e nunca traduzi-lo para “seguro”.

## 10. Fonte, binário e compatibilidade

O modelo é híbrido:

- fonte é o fallback auditável e reproduzível para pacotes públicos;
- bibliotecas estáticas são preferidas quando simplificam distribuição e
  otimização;
- bibliotecas dinâmicas são válidas para ABI de plataforma, plugins, updates,
  compartilhamento ou restrições de licença;
- bundles e executáveis são artefatos específicos de aplicação.

Um artefato binário nunca é escolhido apenas por nome e versão. Sua chave inclui
target triple, versão mínima do sistema, ABI W, ABIs estrangeiras, linkage,
profile, features, CPU baseline, toolchain e runtime. Incompatibilidade causa
fallback explícito para source, se a policy permitir, ou erro; não há seleção
silenciosa do “binário mais próximo”.

Distribuir bibliotecas intermediárias reduz trabalho de build, mas não torna o
executável final automaticamente reproduzível. Linker, ordem de objetos, LTO,
stdlib, recursos, envelope e configuração final continuam na receita.

## 11. Comandos candidatos

Os nomes abaixo descrevem uma experiência a prototipar, não uma CLI já
implementada:

```text
w build --locked --reproducible     falha diante de input ambiental não fixado
w build --emit-recipe               grava a receita sem executar o build
w release prepare --locked          produz payload, manifesto, SBOM e provenance
w release sign                      autoriza metadata; não reescreve o payload
w release envelope --platform ...   aplica code signing/notarização externa
w reproduce <manifest>              reconstrói a receita e compara o payload
w attest <manifest>                 assina o resultado de um builder/reprodutor
w publish <manifest>                publica metadata e objetos por digest
w verify <artifact|package@version> mostra todos os eixos e avalia a policy
w verify --explain                  explica cada evidência, falha e exceção
w registry status <release>         lista tentativas, divergências e revogações
```

`w release prepare` deve poder rodar offline depois que todos os inputs estiverem
no cache. Assinatura, transparência e upload são fases separadas para que secrets
de publicação não entrem no ambiente que produz o payload.

## 12. Modelo de ameaça específico

A arquitetura considera:

- mirror, CDN, DNS ou rede comprometidos;
- registry apresentando rollback, freeze ou combinações inconsistentes;
- credencial de maintainer, publisher ou builder comprometida;
- builder que mente sobre inputs ou output;
- dependência ou toolchain maliciosos porém corretamente assinados;
- nondeterminismo acidental que mascara ou simula divergência;
- substituição de target, ABI, feature set ou payload dentro de um envelope;
- conluio entre reprodutores que compartilham operador ou bootstrap;
- pacote fechado usado para afirmar uma verificação que terceiros não podem
  reproduzir;
- scan ou auditoria antigos sendo exibidos como se cobrissem uma release nova.

Digests detectam alteração de bytes; assinaturas autenticam claims; snapshots e
expiração combatem replay; sandbox reduz inputs e autoridade; provenance registra
o processo alegado; reprodução independente testa o resultado; transparência
torna eventos observáveis; revisão procura problemas semânticos. Nenhuma camada
substitui as demais.

Comprometimento simultâneo de todas as roots aceitas, do cliente local, do kernel
e do hardware fica fora da proteção completa. O sistema deve preservar evidência
suficiente para rotação, revogação, reconstrução de snapshots e investigação.

## 13. Critérios para o primeiro protótipo

Uma implementação exploratória só valida esta arquitetura quando consegue:

1. produzir o mesmo payload em dois diretórios e horários diferentes;
2. mostrar que data, commit, path e seed alteram o digest da receita quando
   declarados, mas não vazam do host quando ausentes;
3. reproduzir o payload em dois builders e registrar `2-of-2 matched`;
4. detectar e preservar uma divergência;
5. assinar metadata sem alterar o digest do payload;
6. envelopar o payload e verificar separadamente envelope e bytes internos;
7. baixar o mesmo objeto de dois mirrors e rejeitar bytes divergentes;
8. exibir todos os eixos de evidência sem uma nota agregada.

## 14. Questões em aberto

- Qual serialização canônica será usada para receita, manifesto e attestations?
- Qual algoritmo de digest inicia o ecossistema e como ocorre sua migração?
- A nota W será uma section nativa por formato (ELF/PE/Mach-O/Wasm), um trailer
  comum ou ambos?
- Qual parte da nota permanece após strip, LTO, empacotamento e code signing?
- Como extrair e verificar o payload interno de envelopes de cada plataforma?
- Qual policy N-de-M é viável para `core`, `official` e `community`?
- Como medir independência de builders sem centralizar identidade e governança?
- Como tratar SDKs de plataforma que não têm redistribuição ou snapshots
  content-addressed?
- Quais divergências podem receber diff estrutural seguro sem expor source ou
  secrets?
- Qual autoridade pode marcar `diverged`, `revoked`, `yanked` e resolver uma
  investigação contestada?
- Quais garantias mínimas um builder confidencial precisa oferecer para ser
  aceito por uma policy organizacional?

## 15. Referências de arquitetura

- [Reproducible Builds — definição](https://reproducible-builds.org/docs/definition/)
- [SLSA — Provenance](https://slsa.dev/spec/v1.2/provenance)
- [The Update Framework — Specification](https://theupdateframework.github.io/specification/latest/)
- [Sigstore — documentação](https://docs.sigstore.dev/)
