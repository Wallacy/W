# WVUI0 — provedores first-party de UI web

`WVUI0` registra uma pesquisa candidata humano+machine sobre provedores
first-party de UI web. O estudo compara hosts embedded, browser e terminal.
Ele não implementa provider, runtime, compiler ou protocolo W.

O estado é `design-oracle-input`. Isso registra uma entrada de pesquisa para
revisão. O estudo não promove API, syntax, semântica, especificação ou
comportamento implementado. `DESIGN.md` permanece a autoridade normativa.

## Pergunta e forma do estudo

A pergunta é: qual composição expõe a mesma aplicação web do Last Light em
targets diferentes, com comandos tipados, lifecycle fechado e defaults seguros?

O estudo separa a aplicação web dos providers. A aplicação possui assets e um
protocolo de comandos tipados. Cada provider declara sua engine, versão e
features. A disponibilidade compõe [AVF0](../avf0-availability-feature/).

O estudo registra evidência primária e perguntas de execução. Ele não apresenta
uma proposta como `Forma vigente`. As rotas permanecem `Pesquisa` ou
`Alternativa` até a revisão humana e a evidência exigida.

## Matriz de estratégias

| Estratégia | Candidatos first-party | Observação da pesquisa | Estado |
|---|---|---|---|
| System embedded WebView | WebView2, WKWebView, WebKitGTK, Android WebView | Usa a engine entregue pelo sistema ou um runtime instalado. Distribuição, versão, permissões e lifecycle variam por target. | `Pesquisa` |
| Wrappers e frameworks | Tauri/Wry, Wails, webview/webview, WebViewJS | Fornece host, janela, bindings ou integração de linguagem. Não é uma engine HTML independente. | `Pesquisa` |
| External browser ou loopback | WebUI, Neutralino browser/cloud | Usa browser externo e pode servir assets por HTTP local. A sessão precisa de origem, token e lifecycle explícitos. | `Pesquisa` |
| Bundled ou controlled engine | Electron, CEF, Qt WebEngine | Controla uma engine empacotada ou integrada. A versão fica mais previsível, mas o pacote, update e security surface aumentam. | `Alternativa` |
| Embeddable engine candidate | Servo | A intenção de embedding existe. A documentação de embedding ainda é sparse e work in progress. Existe um gap experimental explícito. | `Pesquisa` |
| Terminal HTML semantic/light | Provider que extrai headings, text, lists, tables, forms, focus, commands e live state | Oferece uma visão semântica baseada em HTML e accessibility semantics. Não promete pixels CSS. | `Pesquisa` |
| Terminal browser-backed text | Browsh com headless Firefox | Usa um browser real para extrair uma representação textual. O browser é uma dependência explícita do provider. | `Pesquisa` |
| Terminal full/raster | Carbonyl/Chromium | Renderiza uma página visual no terminal. É uma opção explícita para visuais complexos. | `Pesquisa` |
| Terminal conservative baseline | Lynx | Serve como baseline de text web browser com capacidades conservadoras. | `Alternativa` |

Textual e Bubble Tea podem inspirar uma UI terminal. Eles não são HTML engines
e não entram como providers desta matriz.

Nenhuma linha seleciona uma implementação. A matriz identifica dependências,
gaps e perguntas para uma futura prova real.

## Camadas candidatas

As camadas devem permanecer separadas:

1. A aplicação web host-agnostic define UI, assets e estado.
2. O protocolo de comandos usa declarações tipadas e payloads bounded.
3. O embedded provider hospeda a aplicação em uma WebView do target.
4. O external browser provider abre a aplicação em um browser selecionado.
5. O terminal semantic provider projeta HTML e accessibility semantics.
6. O full/raster terminal provider é opcional e explícito.

O protocolo reutiliza o modelo de services do W. Ele não inventa `Channel` ou
`Stream` para cada provider. A mensagem inclui somente comandos declarados,
respostas bounded, `origin`, `view generation` e `session generation`.

O binding não usa reflect-auto-binding. O host não expõe um generic native
proxy. Cada comando tem owner, direção, versão, limite de payload e resultado
de erro explícito. Comandos desconhecidos ou fora de limite falham fechados.

## Lifecycle e concorrência

- O domínio UI/main-thread é owner da janela, da view e das chamadas da engine.
- Structured child tasks tratam assets, bridge, browser externo e renderer terminal.
- `close` cancela os filhos, drena eventos bounded e invalida a generation.
- Eventos de uma view ou sessão anterior são descartados pela generation.
- O receipt do provider registra engine, versão, features, target e profile.
- Falha de inicialização retorna erro observável. O host não escolhe outro provider.

O lifecycle do provider não concede authority adicional ao conteúdo web. A
conclusão de um child task não significa que a view continua válida.

## Security com defaults fechados

- O provider exige CSP fechada para assets e comandos da aplicação.
- O origin custom é immutable. O provider não usa `file://` como identidade.
- Conteúdo remoto não recebe comandos native. Comandos native ficam denied by default.
- Navigation, new windows, devtools e permissions ficam denied by default.
- Uma capability scoped pode abrir uma ação específica com owner e expiry.
- Persistent profile é opt-in e declara local, retenção e política de limpeza.
- Loopback escuta somente em loopback e valida `Host` e `Origin`.
- O handshake usa one-time capability token fora da query string e fora dos logs.
- O token não substitui a verificação de origin, session generation ou payload.
- O terminal renderer sanitiza control bytes, ANSI, OSC e links antes da saída.
- O provider não faz fallback silencioso entre embedded, browser e terminal.

O modelo é deny-by-default. Uma permissão aceita somente o scope declarado. A
presença de uma WebView não prova isolamento. A presença de um browser externo
não prova autenticação da sessão.

## Availability e fallback

`WVUI0` compõe [AVF0](../avf0-availability-feature/) para separar package
feature, target facts e runtime policy.

- Quando embedded é required, a ausência da WebView retorna startup error.
- Browser e terminal são escolhas explícitas do target ou da configuração.
- Um target sem WebView continua válido com browser ou terminal selecionado.
- Um provider indisponível não ativa outro provider por inferência.
- A mensagem de indisponibilidade preserva engine, target e feature ausentes.

Essa composição não adiciona keyword nem runtime authority. Ela somente
registra a pergunta de disponibilidade e a condição fail-closed.

## Fronteira do terminal semantic

O semantic provider promete somente a informação que HTML e accessibility
semantics representam:

- headings e text;
- lists e tables;
- forms e fields;
- focus e comandos declarados;
- live state exposto pela aplicação.

Ele não promete fidelidade de pixels CSS. Ele não promete executar qualquer
modern JavaScript. Uma extração baseada em browser pode usar a accessibility
tree da engine, mas deve declarar essa dependência no receipt.

Visuais complexos exigem uma escolha explícita de full/raster provider. Carbonyl
é um candidato desse tipo. Browsh é um candidato browser-backed text. Lynx é
um baseline conservador. Screenshot, PDF, accessibility-tree e automation
headless são perguntas opcionais de provider. Elas não são promessas do core.

## Crosspoints com W

| Crosspoint | Pergunta fechada para o estudo |
|---|---|
| `std.http`, `URL`, `Body` e assets | Como servir e validar assets immutable em embedded e loopback? |
| Services | Como mapear comandos, respostas, eventos e close sem criar `Channel` ou `Stream` novos? |
| AVF0 | Como representar target/provider facts e startup error sem fallback implícito? |
| Signed registry asset digests e CSP | Como ligar identidade de bytes, origin custom e policy de conteúdo? |
| Terminal UI | Como preservar semantics, focus, live state e terminal diff bounded? |
| Sandbox e security | Como provar scopes, profile, navigation, permissions e ausência de authority remota? |

Esses crosspoints são entradas de pesquisa. Eles não alteram o contrato de
`std.http`, services, AVF0, terminal UI ou sandbox/security nesta rodada.

## Product witness

O witness usa a mesma UI do restaurante [Last Light](../../../reference/last-light/).
O estudo deve exercitar a mesma navegação, menu, formulário, foco, comando
tipado e estado live nos seguintes modos:

| Modo | Witness | Smoke exigido |
|---|---|---|
| Embedded | WebView2, WKWebView, WebKitGTK ou Android WebView conforme o target | Provider real por plataforma, receipt e lifecycle de close |
| Browser | Browser externo com WebUI ou uma rota Neutralino-like de loopback | Origin, Host, token one-use, comandos e encerramento reais |
| Terminal semantic | Provider semantic, com extração própria ou browser-backed | Headings, text, lists, tables, forms, focus, commands e live state |
| Terminal full/raster | Carbonyl/Chromium quando explicitamente selecionado | Renderização visual real, sem alterar a promessa semantic |

O smoke deve usar providers reais por plataforma. A matriz não aceita claim
mock-only. Cada provider recebe adversariais próprios:

- embedded testa conteúdo remoto contra native bridge, navigation, new windows,
  devtools, permissions, CSP e profile persistence;
- browser testa bind fora de loopback, `Host` ou `Origin` inválido, replay do
  token, token em query ou log, stale session e encerramento do processo;
- terminal testa ANSI, OSC, control bytes, links, payload bounded, foco e
  estado live malformado;
- todos testam close, cancel, drain, stale generation e provider unavailable.

Nenhuma medição ou resultado é afirmado neste bundle.

## Performance planejada

Os eixos abaixo são somente um plano de observação:

- cold start;
- first contentful paint;
- memory;
- idle CPU;
- package/cache size;
- bridge latency e throughput;
- input jank;
- terminal diff bytes e CPU;
- update cost.

O estudo não contém timing, ranking, baseline numérico ou claim de performance.
Qualquer resultado futuro deve registrar provider, engine, versão, target,
profile, assets, workload, warmup, ordem, ambiente e regra de parada.

## Fronteira de evidência

As referências oficiais abaixo sustentam somente descrições de capabilities e
arquiteturas publicadas. A leitura foi registrada em `2026-08-31`.

O bundle ainda não possui provider receipt, smoke real, compile, run, security
adversarial execution, human study, model study ou performance result. O
registry e este README não provam conformance, isolamento, compatibilidade,
accessibility completa ou disponibilidade cross-platform.

## Perguntas abertas

1. Quais campos de engine, versão, features, target e profile são estáveis no receipt?
2. Como a identidade de assets assinados e o CSP chegam ao origin custom immutable?
3. Quais comandos e eventos compõem o modelo de services sem generic proxy?
4. Como browser externo prova origin, Host, token one-use, replay e close?
5. Quais semantics e estados live entram na promessa terminal mínima?
6. Quais targets fornecem WebView, browser, renderer semantic e update path reais?
7. Como o estudo humano e o estudo de modelo avaliam a mesma witness UI?
8. Como medir os eixos planejados sem transformar uma observação em contrato?

## Stop conditions

Pare a pesquisa quando ocorrer qualquer condição abaixo:

- uma referência, claim, receipt ou digest ficar stale;
- um provider não entregar engine, versão ou features;
- o smoke real de uma plataforma falhar ou virar mock-only;
- um adversarial revelar bypass de CSP, origin, scope, token ou sanitização;
- o terminal exigir pixel fidelity ou JavaScript arbitrário para cumprir a promessa;
- a disponibilidade deixar de ser fail-closed ou ativar fallback silencioso;
- um resultado de performance aparecer sem identidade, workload e disclosure;
- uma revisão tentar promover API, syntax, semântica ou specification.

Depois de um stop, o bundle aguarda a revisão do Sol e humana. Nenhum stop
recascateia `DESIGN.md` ou `RATIONALE.md`. Este estudo não promove API, syntax,
specification ou implementação W.

## Referências primárias oficiais

Claims são resumos curtos das páginas indicadas. Todas foram acessadas em
`2026-08-31`.

| Fonte | Claim registrado |
|---|---|
| [Tauri architecture](https://v2.tauri.app/concept/architecture/) | Tauri combina host Rust, HTML em WebView do sistema e message passing. |
| [Tauri capabilities](https://v2.tauri.app/security/capabilities/) | Capabilities concedem ou negam permissions para windows ou WebViews selecionados. |
| [Tauri Wry](https://github.com/tauri-apps/wry) | Wry é uma biblioteca Rust cross-platform de WebView e exige event loop e window handle. |
| [webview/webview](https://github.com/webview/webview) | A biblioteca C/C++ abstrai HTML5 com WebKit em Linux/macOS e WebView2 em Windows. |
| [webviewjs/webview](https://github.com/webviewjs/webview) | WebViewJS fornece biblioteca cross-platform para Node, Deno e Bun. |
| [Wails introduction](https://wails.io/docs/introduction/) | Wails cria desktop apps com Go e web technologies. |
| [Neutralino architecture](https://neutralino.js.org/docs/contributing/architecture/) | Neutralino separa core layered, recursos HTTP e mensagens native API por WebSocket. |
| [Electron process model](https://www.electronjs.org/docs/latest/tutorial/process-model) | Electron separa main e renderer processes e permite utility work isolado. |
| [Electron security](https://www.electronjs.org/docs/latest/tutorial/security) | A segurança inclui Chromium, Node.js, Electron, dependencies e application code. |
| [WebView2 distribution](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution) | Production WebView2 usa o WebView2 Runtime, não o Edge Stable instalado. |
| [WebView2 security](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/security) | Conteúdo hosted pode alcançar recursos native sem isolamento correto. |
| [Apple WKWebView](https://developer.apple.com/documentation/webkit/wkwebview) | WKWebView é a view WebKit da Apple para web content dentro de apps. |
| [WebKitGTK WebView](https://webkitgtk.org/reference/webkit2gtk/stable/class.WebView.html) | WebKitGTK WebView gerencia drawing, events, URI ou data loading e settings. |
| [Android WebView](https://developer.android.com/develop/ui/views/layout/webapps/webview) | Android WebView embutida carrega web content e oferece navegação por APIs Android. |
| [WebUI](https://github.com/webui-dev/webui) | WebUI usa browser instalado ou WebView opcional e fornece bridge WebSocket. |
| [CEF](https://chromiumembedded.github.io/cef/) | CEF embute browsers baseados em Chromium em outras aplicações. |
| [Qt WebEngine](https://doc.qt.io/qt-6/qtwebengine-overview.html) | Qt WebEngine usa Chromium e profiles podem isolar pages, settings, scripts e cookies. |
| [Servo embedding](https://book.servo.org/embedding/overview.html) | Servo pretende ser embeddable, mas a documentação de embedding é sparse e work in progress. |
| [Carbonyl](https://github.com/fathyb/carbonyl) | Carbonyl é um browser baseado em Chromium que roda no terminal. |
| [Browsh repository](https://github.com/browsh-org/browsh) | Browsh renderiza páginas modernas interativas em TTY usando headless Firefox. |
| [Browsh introduction](https://www.brow.sh/docs/introduction/) | O CLI inicia browser headless compatível e conecta por remote debugging. |
| [Lynx](https://lynx.invisible-island.net/) | Lynx é um text web browser e fornece baseline terminal conservador. |
| [WAI-ARIA](https://www.w3.org/TR/wai-aria/) | WAI-ARIA define accessibility semantics e o modelo de accessibility tree. |

## Não promoção

`WVUI0` é um registro de pesquisa candidata. Ele não adiciona API, syntax,
keyword, service, provider, channel, stream, semantics ou specification ao W.
Ele também não afirma que Last Light já executa em qualquer provider.
