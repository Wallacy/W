# Formatação canônica

> **Status:** Candidato · Working Draft

W deve ter uma representação formatada previsível desde o primeiro parser. O
formatter não é decoração: reduz diffs, estabiliza exemplos, melhora cache e dá
a humanos e ferramentas uma única forma provável de produzir o mesmo código.

## Candidato inicial

- indentação de dois espaços;
- largura **preferida**, não limite léxico, de 120 colunas;
- assinatura, call, import, literal ou generic inteiro permanece em uma linha se
  couber com seus comentários;
- quando a construção não cabe, um item por linha, vírgula final e delimitador de
  fechamento alinhado com o início;
- o formatter decide pela AST e pelos comentários, não pela presença manual de
  uma quebra ou vírgula; formatar duas vezes produz os mesmos bytes;
- até duas linhas em branco deliberadas podem separar conceitos; whitespace no
  fim da linha e linhas vazias no início/fim são removidos;
- fórmulas quebram antes do operador e preservam grupos algébricos; o formatter
  não transforma cada operando em uma linha isolada apenas para obedecer a 120.

Forma horizontal preferida:

```w
fn heatLoss(area: Area, uValue: ThermalTransmittance, delta: TemperatureDelta): Power
let lease = try await ovens.reserve(temperature, tolerance: 2_Celsius)
```

Forma vertical quando o conjunto inteiro não cabe:

```w
export fn scheduleBakes(
  jobs: ref List<BakeJob>,
  laneCount: Int,
  opening: Instant,
): List<ScheduledBake> throws PlanningError {
  // ...
}
```

Imports obedecem à mesma regra. Um import curto fica em uma linha; um grupo
quebrado tem um nome por linha e vírgula final. O formatter pode ordenar nomes
dentro de um único grupo, mas nunca reordena imports através de comentários nem
funde módulos distintos sem provar equivalência.

## Por que 120 primeiro

Não há um número universal. Prettier usa 80 como largura default e a trata como
alvo aproximado, não como `max-len`; rustfmt usa 100; o próprio repositório do
Swift Format trabalha com 120. A preferência do projeto é 120: dá espaço para
labels, generics e fórmulas sem verticalização prematura. Métricas do corpus
podem reabrir a escolha antes de existir compatibilidade publicada.

O princípio mais importante é estrutural: uma call que cabe não paga a forma
vertical. Prettier também usa uma árvore intermediária de documentos para o
printer escolher layouts a partir da AST. O formatter W deve seguir esse modelo
de decisão e consumir a AST/HIR própria, não editar texto com regex.

## Checks do protótipo

1. golden `source → formatted source` para cada construct;
2. idempotência `format(format(source)) == format(source)`;
3. parse antes e depois com AST semanticamente equivalente;
4. comentários preservados e ligados ao mesmo node;
5. medição, no restaurante, de linhas, diffs e tokens e quantidade de constructs
   que ainda excedem 120;
6. diagnóstico quando uma fórmula excede a largura porque quebrá-la reduziria a
   legibilidade.

## Referências

- [Prettier: Options](https://prettier.io/docs/options.html)
- [Prettier: Rationale](https://prettier.io/docs/rationale.html)
- [Prettier: Plugins e printers](https://prettier.io/docs/plugins)
- [rustfmt: configuração estável](https://rust-lang.github.io/rustfmt/)
- [configuração usada pelo Swift Format](https://github.com/swiftlang/swift-format/blob/main/.swift-format)
