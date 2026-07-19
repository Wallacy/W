const examples = {
  hello: {
    filename: "hello.w",
    source: `fn main() {
  print("Olá, W!")
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
    source: `fn dashboard(id: UserId): Dashboard async throws LoadError {
  async let user = loadUser(id)
  async let activity = loadActivity(id)
  let (user, activity) = try await (user, activity)

  spawn let score = rank(take activity)
  return Dashboard(user: user, score: await score)
}`,
  },
  restaurantMenu: {
    filename: "restaurant/menu.w",
    source: `// Working Draft: pseudocódigo, não executável.
fn place(item: MenuItem, orders: inout ServiceHost, kitchen: ServiceRef<KitchenApi>): Receipt async throws MenuError {
  switch item {
    case .cake(let request):
      let order = try await openMenuOrder(request.orderId, on: inout orders)
      return try await prepareMenuCake(request, for: order, in: kitchen)
    case .soup(let request):
      let order = try await openMenuOrder(request.orderId, on: inout orders)
      return try await prepareMenuSoup(request, for: order, in: kitchen)
    case .salad(let request):
      let order = try await openMenuOrder(request.orderId, on: inout orders)
      return try await prepareMenuSalad(request, for: order, in: kitchen)
  }
}`,
  },
  restaurantOrder: {
    filename: "restaurant/order_service.w",
    source: `// Working Draft: ServiceRef e configuração são candidatos.
fn openOrder(id: OrderId, on host: inout ServiceHost): ServiceRef<OrderApi> async throws OrderError {
  do {
    return try await host.startService(
      OrderState(id: id, stage: .accepted),
      as: OrderApi,
      scope: .key(id),
      policy: .serial,
      mailbox: .bounded(items: 32),
    )
  } catch let error {
    throw .start(error)
  }
}`,
  },
  restaurantKitchen: {
    filename: "restaurant/kitchen.w",
    source: `// Working Draft: lexer local, não compilador.
let slot = try await reserveOven(plan.temperature, in: ovens)
defer { slot.release() }

async let ovenTask = preheatOven(slot)
async let ingredientsTask = fetchCakeIngredients(plan, from: pantry)
let (oven, ingredients) = try await (ovenTask, ingredientsTask)

let batter = mix(take ingredients.dry, with: ingredients.wet)
let icing = prepareIcing(take ingredients.icing)
let (left, right) = splitBatter(take batter)
spawn let cookA = bakeLayer(take left, in: oven.leftKitchen)
spawn let cookB = bakeLayer(take right, in: oven.rightKitchen)
let (leftLayer, rightLayer) = try await (cookA, cookB)
Task.checkCancellation()
return decorate(take leftLayer, take rightLayer, with: take icing, using: plan)`,
  },
  restaurantTable: {
    filename: "restaurant/dining_room.w",
    source: `// O mesmo host inout abre pedidos sequencialmente.
let cakeOrder = try await openMenuOrder(request.cake.orderId, on: inout orders)
let soupOrder = try await openMenuOrder(request.soup.orderId, on: inout orders)
let saladOrder = try await openMenuOrder(request.salad.orderId, on: inout orders)

// Handles independentes permitem concorrência estruturada entre pratos.
async let cake = prepareMenuCake(request.cake, for: cakeOrder, in: kitchen)
async let soup = prepareMenuSoup(request.soup, for: soupOrder, in: kitchen)
async let salad = prepareMenuSalad(request.salad, for: saladOrder, in: kitchen)
let (cake, soup, salad) = try await (cake, soup, salad)

return TableReceipt(tableId: request.tableId, cake: cake, soup: soup, salad: salad)`,
  },
  restaurantOven: {
    filename: "restaurant/oven.w",
    source: `// Função pura: dimensions devem ser verificadas em compile time.
fn predictStep(
  model: ref ThermalModel,
  state: ThermalState,
  ambient: Temperature,
  duty: Ratio,
  elapsed: Duration,
): ThermalState {
  let wallLoss = model.surface * model.transmittance * (state.cavity - ambient)
  let foodTransfer = model.coupling * (state.cavity - state.food) / 1.0_KelvinDelta
  let cavityEnergy = (model.heaterPower * duty - wallLoss - foodTransfer) * elapsed
  let foodEnergy = foodTransfer * elapsed
  let nextCavity = state.cavity + cavityEnergy / model.cavityCapacity
  let nextFood = state.food + foodEnergy / model.foodCapacity
  return ThermalState(cavity: nextCavity, food: nextFood, cavityRate: (nextCavity - state.cavity) / elapsed)
}`,
  },
  restaurantPlanning: {
    filename: "restaurant/planning.w",
    source: `// Earliest-deadline-first + lane menos carregada.
var ordered = copy jobs
ordered.sort(by: (left, right) => left.deadline < right.deadline)
var loads = List.filled(count: laneCount, with: Duration.zero)
var entries: List<ScheduledBake> = []

for job in ordered {
  let lane = earliestLane(loads)
  let startsAt = opening + loads[lane]
  let finishesAt = startsAt + job.duration
  entries.append(ScheduledBake(
    orderId: job.orderId,
    lane: lane,
    startsAt: startsAt,
    finishesAt: finishesAt,
    temperature: job.temperature,
  ))
  loads[lane] += job.duration
}`,
  },
  restaurantInterfaces: {
    filename: "restaurant/app.w",
    source: `// Nomes std curtos não escondem effects/capabilities.
print("Restaurante W")

// TUI e HTTP são children long-lived da mesma árvore.
async let terminal = runTerminalInterface(restaurant)
async let web = runWebInterface(address, restaurant)
let (_, _) = try await (terminal, web)`,
  },
  restaurantInterop: {
    filename: "restaurant/multilingual-experiment.w",
    source: `// Baseline aceita pelo parser: fronteira C + wrapper W seguro.
foreign c from "restaurant_equipment.h" {
  type restaurant_equipment
  fn restaurant_read_probe(
    _ handle: c.ptr<restaurant_equipment>,
    _ probe: c.int,
    _ outCelsius: c.ptr<c.double>,
  ): c.int
}

let temperature = try equipment.read(.cavity)

// Pesquisa visual: ilha da aplicação, ainda fora da grammar.
fn<C> readProbeRaw(_ handle: c.ptr<restaurant_equipment>, _ probe: c.int): c.double {
  double value = 0.0;
  int status = restaurant_read_probe(handle, probe, &value);
  return status == 0 ? value : NAN;
}

let celsius = readProbeRaw(device, Probe.cavity.rawValue)`,
  },
  ffi: {
    filename: "sqlite.w",
    source: `foreign c from "sqlite3.h" {
  type sqlite3
  fn sqlite3_close(handle: c.ptr<sqlite3>): c.int
}`,
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
    [/\basync\s+let\b/u, "async let sugere um filho concorrente estruturado."],
    [/\bspawn\s+let\b/u, "spawn let sugere intenção de paralelismo."],
    [/\bforeign\s+c\b/u, "foreign c marca uma fronteira de ABI C."],
    [/\b(?:take|inout|copy)\b/u, "Há uma operação explícita de ownership ou mutabilidade."],
    [/\bthrows\s+[\p{L}_]/u, "A assinatura declara um conjunto de erro tipado candidato."],
    [/\bServiceRef\s*</u, "ServiceRef é tratado aqui como handle conceitual candidato; a análise não prova localidade ou contrato."],
    [/\bdefer\b/u, "defer sugere cleanup lexical; o lexer não verifica se o recurso é liberado corretamente."],
  ];
  patterns.forEach(([pattern, text]) => { if (pattern.test(source)) notes.push({ tone: "info", text }); });
  if (notes.length === 0) notes.push({ tone: "info", text: "Nenhum padrão especial foi anotado. Isso não significa que o programa esteja correto." });
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
  [["linhas", editor.value.split("\n").length], ["tokens", tokens.length], ["keywords", counts.get("keyword") ?? 0]].forEach(([label, value]) => {
    const wrapper = document.createElement("div");
    const term = document.createElement("dt");
    const detail = document.createElement("dd");
    term.textContent = label;
    detail.textContent = String(value);
    wrapper.append(term, detail);
    summary.append(wrapper);
  });

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
  if (event.key === "Enter" && (event.ctrlKey || event.metaKey)) { event.preventDefault(); explainRun(); }
});

const requested = new URLSearchParams(window.location.search).get("example");
loadExample(requested && examples[requested] ? requested : "hello");
