# Governança do projeto W

> **Status:** governança inicial, válida antes do W 1.0

Esta governança protege a visão do W sem fechar o projeto à comunidade.
Ela separa contribuição aberta, autoridade de decisão e responsabilidade
operacional.

## Princípios

1. Qualquer pessoa pode propor, implementar, testar ou revisar uma mudança.
2. O projeto avalia resultado e evidência, não a ferramenta usada.
3. Uma pessoa identificável responde por cada contribuição aceita.
4. Somente pessoas podem manter permissões, aprovar merges e assinar releases.
5. `DESIGN.md` continua sendo a autoridade técnica da linguagem e do sistema.
6. Decisões importantes registram motivo, alternativa e evidência ausente.
7. Permissões seguem escopo, necessidade e menor privilégio.
8. O projeto não cria um conselho ou quorum fictício antes de ter comunidade.

## Modelo atual

W usa liderança do fundador antes do 1.0. O Project Lead guarda a coerência da
linguagem, delega áreas e toma a decisão final quando não existe consenso.

| Papel | Responsabilidade | Autoridade |
|---|---|---|
| Project Lead | visão, design integrado, releases, segurança e delegação | decisão final antes do 1.0 |
| Maintainer | saúde de uma área, revisão, triagem e merge | somente no escopo delegado |
| Reviewer | revisão consistente e orientação de contributors | recomendação, sem merge por padrão |
| Contributor | issue, design, código, testes, documentação ou pesquisa | nenhuma permissão especial necessária |
| Automação ou IA | análise, proposta, edição, teste e revisão auxiliar | nenhuma autoridade de governança |

### Composição atual

- Project Lead e maintainer geral: Wallacy Freitas
  ([@Wallacy](https://github.com/Wallacy)).
- Não existem ainda maintainers delegados.

Esta composição explicita um risco de pessoa única. O projeto não afirma ter
revisão independente enquanto esse risco existir.

## Classes de mudança

| Classe | Exemplos | Decisão mínima |
|---|---|---|
| A — editorial | typo, link, texto sem mudança normativa | maintainer da área |
| B — implementação | tooling, corpus, oracle, CI e produto de referência | maintainer da área e checks verdes |
| C — design | syntax, ABI, runtime, SDK e package | proposta completa e decisão do Project Lead |
| D — confiança | governança, segurança, release e licença | Project Lead e revisão independente quando disponível |

Quando o repositório for público, uma proposta C recebe pelo menos sete dias
de revisão após ficar completa. Uma proposta D recebe pelo menos 14 dias.
Correções de segurança podem usar o processo de emergência.

Durante a fase privada, o Project Lead pode reduzir o período. O pull request
deve registrar o motivo. O período não substitui a qualidade da revisão.

## Processo de decisão

1. O autor define o problema e a classe da mudança.
2. A discussão separa fato, inferência, alternativa e preferência.
3. O autor atualiza a proposta com impactos e evidência.
4. Reviewers registram objeções técnicas e condições de aceitação.
5. O maintainer resume consenso e dissenso.
6. A autoridade aplicável aceita, pede revisão ou rejeita.
7. O merge registra a decisão no artefato canônico.

W busca consenso aproximado. W não decide design por contagem de reações.
Uma maioria não substitui segurança, coerência ou prova executável.

Antes do 1.0, o Project Lead pode escolher entre alternativas viáveis. A decisão
deve registrar o motivo. Uma objeção técnica relevante deve permanecer junto
da decisão ou no histórico ligado.

## Autoridade técnica

[`DESIGN.md`](DESIGN.md) possui a decisão técnica vigente. Pull requests e
issues preservam discussão, mas não criam uma segunda especificação.

Uma alteração de design deve:

- atualizar o ID W aplicável;
- usar um estado oficial;
- registrar alternativas plausíveis;
- incluir um exemplo ou oracle;
- informar impacto em grammar, formatter, diagnostics e produto de referência.

O Project Lead pode delegar áreas como linguagem, compiler, runtime, packages,
tooling, documentação ou infraestrutura. A delegação deve aparecer neste
arquivo antes da concessão de merge.

## Nomeação e saída de maintainers

Um candidato a maintainer deve demonstrar:

- contribuições sustentadas durante pelo menos três meses;
- decisões técnicas consistentes no escopo;
- revisões úteis e respeitosas;
- domínio dos checks e fontes canônicas;
- cumprimento do Código de Conduta;
- disponibilidade para responder por merges.

O período não concede o papel automaticamente. O Project Lead publica a
nomeação, o escopo e a evidência. Depois da criação de um steering group, esse
grupo assume a nomeação.

Um maintainer pode renunciar a qualquer momento. Se não houver atividade por
seis meses, o projeto tenta contato e pode mover a pessoa para emeritus.
O retorno exige confirmação de disponibilidade e revisão das políticas atuais.

Uma violação grave de segurança, conduta ou confiança pode remover permissões
imediatamente. O projeto protege dados pessoais e publica somente a justificativa
que for segura e necessária.

## Conflitos de interesse e recusa

Uma pessoa deve informar interesse financeiro, empregatício ou pessoal que
possa afetar uma decisão. Essa pessoa não deve ser a única aprovadora.

Quando o Project Lead tiver conflito, outro maintainer ativo conduz a revisão.
Se não existir outro maintainer, o projeto registra a limitação e adia uma
decisão não urgente que seja irreversível.

## Processo de emergência

Um maintainer pode agir sem o período normal para:

- conter uma vulnerabilidade;
- revogar uma credencial;
- interromper uma release incorreta;
- corrigir corrupção de dados ou falha do branch principal.

O maintainer registra o incidente assim que a divulgação for segura.
Uma revisão retrospectiva deve ocorrer em até sete dias após a contenção.

## Recurso

O autor pode pedir nova revisão quando identifica erro factual, evidência nova
ou aplicação incorreta desta governança. O recurso deve apontar o ponto exato.

O Project Lead decide o recurso antes da criação de um steering group.
Uma diferença de preferência sem evidência nova não exige reabertura.

## Transição de governança

O projeto deve revisar este modelo quando ocorrer primeiro uma destas condições:

- três maintainers ficarem ativos durante seis meses, incluindo dois que não
  sejam o fundador;
- W preparar seu primeiro release candidate 1.0;
- uma organização jurídica assumir marca, domínio ou infraestrutura crítica.

A revisão deve avaliar um Language Steering Group e maintainers por área.
Ela também deve definir eleição ou nomeação, mandatos, sucessão e posse dos
ativos do projeto.

Se o Project Lead ficar indisponível por 90 dias, maintainers ativos escolhem
um líder interino por maioria simples. O Project Lead deve manter um plano de
recuperação de acesso antes de delegar o segundo papel de maintainer.

## Alteração desta governança

Uma mudança neste arquivo é classe D. Ela exige motivo, impacto, período de
revisão e plano de transição. Uma mudança não pode reduzir silenciosamente o
direito de recurso ou ampliar permissões existentes.

## Referências de processo

Este modelo usa ideias de processos maduros sem copiar sua escala:

- [Swift Evolution](https://www.swift.org/swift-evolution/) para propostas e
  revisão pública;
- [Python PEP 13](https://peps.python.org/pep-0013/) para autoridade ampla usada
  com moderação;
- [governança do LLVM](https://www.llvm.org/docs/ProjectGovernance.html) para
  responsabilidade por área;
- [teams do Rust](https://rust-lang.org/governance/teams/) para delegação
  explícita e registro de membros.
