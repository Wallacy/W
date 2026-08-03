# Política de segurança

W ainda não possui compiler, runtime, registry ou release executável.
O repositório contém tooling que executa localmente e no CI. Esse tooling já
possui uma superfície de segurança.

## Escopo atual

Relate de forma privada:

- execução de código não prevista no tooling ou no CI;
- dependency confusion ou comprometimento de supply chain;
- parsing de input hostil que cause impacto de segurança no host;
- exposição de credencial, chave ou dado privado;
- falha no modelo de assinatura, provenance ou autorização de release;
- instrução que leve um maintainer a publicar artefato incorreto como confiável.

Uma preferência de syntax ou uma lacuna comum de design não é vulnerabilidade.
Use uma proposta de design quando não existir exploração ou quebra de confiança.

## Versões suportadas

W não possui versões suportadas. Somente o branch `main` recebe correções.
O projeto publicará uma matriz de suporte antes do primeiro release executável.

## Como relatar

Quando o repositório for público e o recurso estiver habilitado, use
[GitHub Private Vulnerability Reporting](https://github.com/Wallacy/W/security/advisories/new).

Enquanto o repositório for privado, collaborators podem abrir um repository
security advisory. Outros reporters devem usar o contato privado publicado no
perfil [@Wallacy](https://github.com/Wallacy).

Não abra uma issue pública. Informe:

- componente e commit afetado;
- impacto esperado;
- passos mínimos para reproduzir;
- código ou input de prova, quando seguro;
- mitigação conhecida;
- restrições de divulgação.

## Resposta

O projeto busca confirmar o relato em até sete dias. Esse prazo é um objetivo,
não uma garantia de correção. A prioridade depende do impacto e da capacidade
de reprodução.

O maintainer coordena correção, crédito e divulgação com o reporter.
O projeto pode manter embargo enquanto uma correção estiver em preparação.

W não oferece atualmente um bug bounty.
