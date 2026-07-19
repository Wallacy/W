# Alternativas históricas do gerenciador de pacotes

> **Arquivo histórico · 19 de julho de 2026**

Estas direções foram preservadas fora da árvore publicável `W/`. Elas ajudam a
explicar escolhas, mas não são decisões vigentes.

## `package.w` como programa W em estilo Bash

A sintaxe fluente (`package .name ...`, `deps ...`, `on .start ...`) era atraente
por reutilizar a linguagem e permitir extensões. Executar o manifesto, porém,
amplia o bootstrap e transforma resolução em execução de código não confiável.

## Dispensar lockfile e espalhar hashes por `mod.w` ou imports

A abordagem fixa dependências pontuais e pode ser confortável para scripts de um
arquivo, mas não registra grafo transitivo, features, target, metadata snapshot
ou seleção de binário. A direção atual exige que `w resolve` materialize o grafo.

## Biblioteca estática como único formato

Favorece otimização e auditoria do executável final, mas como regra universal
impediria integrações válidas com sistema, plugins e algumas licenças.

## Fallback `bundle -> static -> dynamic -> source`

A sequência serve como heurística, mas não pode trocar silenciosamente target,
ABI, toolchain, policy de segurança ou evidência por disponibilidade.

## Metadata externa e artefatos em hosts doados

GitHub, distribuições e buckets podem hospedar bytes, desde que metadata assinada
ligue nome, release e digests e trate cada host como mirror não confiável.

## Estado único `undefined` / `valid` / `verified` / `invalid`

Um estado único mistura integridade, auditoria e policy. A proposta atual separa
facetas de evidência e estados operacionais como revoked/diverged.

## Tiers `STD / Util / Community / Packages / externo`

A hierarquia pode descrever estabilidade e manutenção, nunca servir sozinha como
selo de segurança.

## Audit encadeado ao hash da release seguinte

O encadeamento cria continuidade, mas também dependência circular. Metadata
append-only/transparente e attestations adicionais preservam releases sem mudar
os bytes reproduzíveis.

## Dependências detectadas pelo loader e lock binário

Inspeção de ELF/PE/Mach-O ajuda `w doctor`, SBOM e verificação do output, mas não
observa todas as dependências e não substitui declarações/lockfile.

## Qualquer compressão com download automático do decoder

Cria ciclo de bootstrap e execução antes da verificação. O bootstrap precisa de
um conjunto pequeno de formatos; decoders extras são tools fixadas no lockfile.

## Um gerenciador para libraries, drivers, apps e suporte

Pode ser um produto futuro sobre CAS, metadata e provenance. Drivers,
self-installers, auto-update, bug report, chat e tickets não pertencem ao MVP de
package/build nem à semântica da linguagem.
