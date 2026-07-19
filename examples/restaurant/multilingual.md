# Experimento de funções multilíngues

> **Status:** Pesquisa posterior à FFI C · nenhuma forma `fn<lang>` está na grammar

O restaurante precisa reaproveitar uma rotina C que ainda pertence à aplicação.
Ela não é uma library externa: é uma ilha de implementação no próprio programa,
útil para migrar código gradualmente sem reescrevê-lo como W idiomático no
primeiro dia. A chamada W deve permanecer simples e tipada:

```w
let temperature = try equipment.read(.cavity)
```

O custo e a linguagem da implementação aparecem no hover/cost lens e na
declaração, não como pontuação repetida em todo call site. Conversão, ownership,
blocking, thread safety e error continuam parte da boundary.

## Baseline implementável: `foreign c`

[`interop.w`](interop.w) declara o header/ABI e escreve um wrapper W. O raw
pointer fica privado; status vira `EquipmentInteropError`; `double` só vira
`Temperature` após validar finitude e unidade. Essa forma vem primeiro porque C
é ABI estável do ecossistema, não porque será a única linguagem externa.

## Hipótese principal: body inline com `fn<C>`

```w
fn<C> readProbeRaw(_ handle: c.ptr<restaurant_equipment>, _ probe: c.int): c.double {
  double value = 0.0;
  int status = restaurant_read_probe(handle, probe, &value);
  return status == 0 ? value : NAN;
}
```

É a forma mais próxima da ideia histórica: como `asm` em C, mas o body pertence a
uma linguagem que possui frontend completo. O parser W reconhece assinatura e
delimitação, preserva o body como source opaco e o entrega ao adapter C. O
adapter compila essa compilation unit junto da aplicação e devolve IR/object,
symbol, diagnostics, source map, dependências e metadata de fronteira.

O fato de C, Rust ou outra linguagem usarem LLVM pode permitir um ponto de
convergência e até otimização conjunta. Não é condição suficiente: calling
convention, data layout, runtime, ownership, panic/exception e versões do IR
ainda precisam de contrato explícito. W só habilita uma linguagem cujo adapter
consiga baixar o subset suportado de maneira verificável para o target.

## Alternativa A: source da aplicação em arquivo separado

```w
fn<C> readProbeRaw(
  _ handle: c.ptr<restaurant_equipment>,
  _ probe: c.int,
): c.double from "native/restaurant_equipment.c"
```

O arquivo continua sendo source da aplicação e entra na mesma receita de build;
`from` não significa package ou library externa. Essa organização oferece ao
formatter, debugger e language server nativos um arquivo normal, mas separa o
código do wrapper W e torna migrações pequenas mais cerimoniosas.

## Alternativa C: namespace de linguagem

```w
fn<C::equipment> readProbeRaw(_ handle: c.ptr<restaurant_equipment>, _ probe: c.int): c.double {
  // Funções C no namespace `equipment` compartilham includes e podem chamar-se
  // diretamente sem atravessar W entre cada call.
}
```

Essa forma recupera a ideia original de agrupar ilhas da mesma linguagem como
uma compilation unit. Funções C da unit podem chamar-se diretamente sem entrar e
sair de W a cada call. O risco é transformar `C::equipment` simultaneamente em
namespace, arquivo e unidade de link; lifecycle não deve ser inferido dessa tag.

## Alternativa D: declaration + adapter

```w
@foreign(language: C, source: "native/restaurant_equipment.c")
fn readProbeRaw(_ handle: c.ptr<restaurant_equipment>, _ probe: c.int): c.double
```

Uma annotation evita gramática especial e generaliza plugins, mas é mais longa e
pode esconder a linguagem no meio de metadata. Outra variante é gerar a mesma
declaration a partir de um adapter/package, mantendo o source W igual à baseline.

## O que decide

| Critério | `foreign c` | inline | `fn<C> from` | namespace/plugin |
|---|---|---|---|---|
| papel | dependência/ABI existente | ilha da aplicação | source da aplicação separado | várias ilhas/unidade |
| primeira implementação | melhor baseline | primeiro `fn<lang>` | variante simples | posterior |
| parser/formatter W | declarations | body opaco/injection | declaração pequena | plugin e metadata |
| builds reproduzíveis | header/library fixados | body + toolchain | source/toolchain fixados | graph do adapter fixado |
| diagnostics/debug | wrapper + harness C | source maps exigidos | source C normal | depende do adapter |
| chamadas C → C | biblioteca externa | naturais no mesmo body/TU | naturais na mesma TU | naturais no namespace |
| risco de cópia oculta | wrapper explicita | alto sem regras | adapter deve reportar | adapter deve reportar |

Para qualquer forma, o lock registra plugin/toolchain, target, flags, includes,
source digests e artefatos. Tipos sem mapeamento não atravessam automaticamente;
borrow, owner/deallocator, callback thread, blocking e erro precisam de adapter.
JS, Rust, Zig, GPU e outras linguagens só reutilizam o mecanismo depois de uma
boundary C completa provar esse contrato. Compartilhar LLVM/MLIR é uma vantagem,
não um passe livre; uma linguagem com runtime próprio pode precisar de um adapter
e deployment muito diferentes ou não ser suportada naquele target.
