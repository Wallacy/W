const examples = {
  hello: {
    filename: "hello.w",
    source: `entry {
  print("Hello, W")
}`,
  },
  options: {
    filename: "options.w",
    source: `enum LoadError: Error {
  notFound(UserId)
  transport(HttpError)
}

fn label(id: UserId): String throws LoadError {
  guard let user = find(id) else throw .notFound(id)
  return user.name
}`,
  },
  tasks: {
    filename: "tasks.w",
    source: `async fn dashboard(id: UserId): Dashboard throws LoadError {
  let user = async loadUser(id)
  let activity = async loadActivity(id)
  let (user, activity) = try await (user, activity)

  let score = spawn<.compute> rank(take activity)
  return Dashboard(user: user, score: await score)
}`,
  },
  units: {
    filename: "restaurant/units.w",
    source: `import std.si
import std.iec

dimension Applause
unit clap: Applause
unit ovation = 1_000<clap>
unit smoot = 1.7018<si.m>

let gravity = 9.80665<si.m/si.s^2>
let distance = 2<smoot>
let memory = 64<KiB>`,
  },
  tensor: {
    filename: "restaurant/oracle.w",
    source: `let observations: Tensor<f32, shape: [2, 3]> = [
  [1.0, 0.0, 0.5],
  [0.2, 0.8, 0.0],
]

let weights: Tensor<f32, shape: [3, 4]> = [
  [0.9, 0.1, 0.2, 0.4],
  [0.1, 0.8, 0.3, 0.2],
  [0.4, 0.2, 0.7, 0.1],
]

let demand = (observations @ weights).softmax(axis: 1)`,
  },
  service: {
    filename: "restaurant/restaurant.w",
    source: `export service LastLightRestaurant as RestaurantApi {
  let pantry: ServiceRef<PantryApi>
  let ovens: ServiceRef<OvenApi>
  var Lazy priceTable = loadPriceTable()
  var atomic completedOrders: u64 = 0

  mut async fn place(order: take Order): Receipt throws RestaurantError {
    let stock = async pantry.reserve(order.course)
    let plan = spawn<.compute> optimize(order)
    let (stock, plan) = try await (stock, plan)
    defer async { await stock.release() }
    return try await cook(take order, stock: stock, plan: plan)
  }
}`,
  },
  entries: {
    filename: "restaurant/app.w",
    source: `async fn fetch(request: http.Request, ctx: http.Context): http.Response throws AppError {
  let restaurant = try await ctx.services.get<RestaurantApi>(key: "last-light")
  let order = try request.json.decode<Order>()
  let receipt = try await restaurant.place(take order)
  return try http.Response.json(receipt)
}

entry LastLight {
  process.main = run
  process.stdinLine = readCommand
  http.fetch = fetch
}`,
  },
  ffi: {
    filename: "restaurant/hardware.w",
    source: `foreign c from "last_light_probe.h" {
  type ll_probe

  struct ll_sample {
    aroma: c.double
    kelvin: c.double
    status: c.int
  }

  fn ll_probe_read(probe: c.ptr<ll_probe>, sample: c.ptr<ll_sample>): c.int
  fn ll_probe_close(probe: c.ptr<ll_probe>)
}

let status = unsafe { ll_probe_read(handle, inout raw) }`,
  },
};

const editor = document.querySelector("[data-editor]");
const select = document.querySelector("[data-example-select]");
const filename = document.querySelector("[data-filename]");
const stream = document.querySelector("[data-token-stream]");
const summary = document.querySelector("[data-token-summary]");
const annotations = document.querySelector("[data-annotations]");
const runMessage = document.querySelector("[data-run-message]");
const syntaxPreview = document.querySelector("[data-syntax-preview]");
let selectedExample = "hello";

function scanWithNotes(source) {
  const lexical = window.WSyntax?.scan(source) ?? {
    tokens: [],
    notes: [{ tone: "warning", text: "O realçador lexical local não foi carregado; nenhuma análise foi produzida." }],
  };
  const notes = [...lexical.notes];
  const patterns = [
    [/=\s*async\b/u, "O initializer async sugere um filho concorrente estruturado."],
    [/=\s*spawn\s*</u, "O initializer spawn seleciona um domínio e sugere intenção de paralelismo."],
    [/\bforeign\s+c\b/u, "foreign c marca uma fronteira de ABI C."],
    [/\bentry\b/u, "entry liga funções comuns a slots tipados do host."],
    [/\bunit\b/u, "unit declara uma unidade verificada estaticamente."],
    [/\b(?:take|inout|copy)\b/u, "Há uma operação explícita de ownership ou mutabilidade."],
    [/\bthrows\s+[\p{L}_]/u, "A assinatura declara um conjunto de erro tipado candidato."],
    [/\bServiceRef\s*</u, "ServiceRef é um handle tipado; a análise lexical não prova localidade ou contrato."],
    [/\bdefer\b/u, "defer sugere cleanup lexical; o lexer não verifica se o recurso é liberado corretamente."],
  ];

  patterns.forEach(([pattern, text]) => {
    if (pattern.test(source)) notes.push({ tone: "info", text });
  });
  if (notes.length === 0) {
    notes.push({ tone: "info", text: "Nenhum padrão especial foi anotado. Isso não significa que o programa esteja correto." });
  }
  return { tokens: lexical.tokens, notes };
}

function renderSyntaxPreview(source) {
  if (!syntaxPreview) return;
  if (window.WSyntax) window.WSyntax.appendHighlightedCode(syntaxPreview, source);
  else syntaxPreview.textContent = source;
}

function renderAnalysis() {
  if (!editor || !stream || !summary || !annotations) return;
  const { tokens, notes } = scanWithNotes(editor.value);
  renderSyntaxPreview(editor.value);
  const counts = new Map();
  tokens.forEach((token) => counts.set(token.kind, (counts.get(token.kind) ?? 0) + 1));

  summary.replaceChildren();
  [["linhas", editor.value.split("\n").length], ["tokens", tokens.length], ["keywords", counts.get("keyword") ?? 0]].forEach(
    ([label, value]) => {
      const wrapper = document.createElement("div");
      const term = document.createElement("dt");
      const detail = document.createElement("dd");
      term.textContent = label;
      detail.textContent = String(value);
      wrapper.append(term, detail);
      summary.append(wrapper);
    },
  );

  stream.replaceChildren();
  tokens.slice(0, 80).forEach((token) => {
    const item = document.createElement("li");
    const kind = document.createElement("span");
    const value = document.createElement("code");
    kind.textContent = `${token.kind} · L${token.line}`;
    value.textContent = token.value.length > 36 ? `${token.value.slice(0, 35)}…` : token.value;
    item.append(kind, value);
    stream.append(item);
  });
  if (tokens.length > 80) {
    const item = document.createElement("li");
    item.textContent = `${tokens.length - 80} tokens adicionais omitidos desta visualização.`;
    stream.append(item);
  }

  annotations.replaceChildren();
  notes.forEach((note) => {
    const item = document.createElement("li");
    item.className = note.tone;
    item.textContent = note.text;
    annotations.append(item);
  });
}

function loadExample(name) {
  const example = examples[name] ?? examples.hello;
  selectedExample = examples[name] ? name : "hello";
  if (editor) editor.value = example.source;
  if (select) select.value = selectedExample;
  if (filename) filename.textContent = example.filename;
  if (runMessage) runMessage.hidden = true;
  renderAnalysis();
}

async function copySource() {
  if (!editor) return;
  try {
    await navigator.clipboard.writeText(editor.value);
  } catch {
    editor.select();
    document.execCommand("copy");
  }
}

async function explainRun() {
  if (!editor || !runMessage) return;
  runMessage.hidden = false;
  runMessage.textContent = "Consultando o contrato local…";
  try {
    const response = await fetch("/api/playground/compile", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ source: editor.value, target: "native" }),
    });
    const payload = await response.json();
    runMessage.textContent = `${payload.error?.message ?? "Compilador indisponível."} Um adapter futuro poderá usar WASM ou um serviço remoto isolado; nenhum está configurado.`;
  } catch {
    runMessage.textContent = "Não há compilador W. Um adapter futuro poderá usar WASM ou um serviço remoto isolado; nenhum está configurado.";
  }
  runMessage.focus();
}

select?.addEventListener("change", () => loadExample(select.value));
editor?.addEventListener("input", renderAnalysis);
document.querySelector("[data-reset]")?.addEventListener("click", () => loadExample(selectedExample));
document.querySelector("[data-copy-editor]")?.addEventListener("click", copySource);
document.querySelector("[data-run]")?.addEventListener("click", explainRun);
editor?.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && (event.ctrlKey || event.metaKey)) {
    event.preventDefault();
    explainRun();
  }
});

const requested = new URLSearchParams(window.location.search).get("example");
loadExample(requested && examples[requested] ? requested : "hello");
