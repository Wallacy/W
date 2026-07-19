const root = document.documentElement;
const themeButton = document.querySelector("[data-theme-toggle]");
const themeLabel = document.querySelector("[data-theme-label]");
const searchInput = document.querySelector("[data-search]");
const filterButtons = [...document.querySelectorAll("[data-filter]")];
const resultCount = document.querySelector("[data-result-count]");
const emptyState = document.querySelector("[data-empty-state]");
const clearSearchButton = document.querySelector("[data-clear-search]");
const copyStatus = document.querySelector("[data-copy-status]");
const navLinks = [...document.querySelectorAll("[data-section-nav] a")];
const trackedSections = [...document.querySelectorAll(".tracked-section[id]")];
const filterItems = [
  ...document.querySelectorAll(
    ".cheat-card[data-topics], .runtime-card[data-topics], .open-grid > article[data-topics], .pipeline-map[data-topics]",
  ),
];
const contentSections = [...document.querySelectorAll("[data-section]")];

let activeFilter = "all";

function readThemePreference() {
  try {
    const saved = localStorage.getItem("w-portal-theme");
    if (saved === "light" || saved === "dark") return saved;
  } catch {
    // Storage can be unavailable in hardened/private contexts.
  }

  return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}

function applyTheme(theme, persist = false) {
  const isDark = theme === "dark";
  root.dataset.theme = isDark ? "dark" : "light";

  if (themeButton) {
    themeButton.setAttribute("aria-label", isDark ? "Usar tema claro" : "Usar tema escuro");
    themeButton.querySelector(".theme-icon").textContent = isDark ? "☀" : "◐";
  }

  if (themeLabel) themeLabel.textContent = isDark ? "Claro" : "Escuro";

  if (persist) {
    try {
      localStorage.setItem("w-portal-theme", isDark ? "dark" : "light");
    } catch {
      // The visible theme still works when storage is unavailable.
    }
  }
}

applyTheme(readThemePreference());

themeButton?.addEventListener("click", () => {
  applyTheme(root.dataset.theme === "dark" ? "light" : "dark", true);
});

function normalize(value) {
  return value
    .normalize("NFD")
    .replace(/\p{Diacritic}/gu, "")
    .toLocaleLowerCase("pt-BR")
    .trim();
}

function itemMatches(item, query) {
  const topics = normalize(item.dataset.topics ?? "");
  const searchable = normalize(`${topics} ${item.textContent ?? ""}`);
  const matchesFilter = activeFilter === "all" || topics.split(/\s+/).includes(activeFilter);
  const matchesQuery = query.length === 0 || searchable.includes(query);
  return matchesFilter && matchesQuery;
}

function updateNavVisibility(section, visible) {
  const link = navLinks.find((candidate) => candidate.hash === `#${section.id}`);
  link?.classList.toggle("is-filtered", !visible);
}

function applyFilters() {
  const query = normalize(searchInput?.value ?? "");
  let visibleItems = 0;

  filterItems.forEach((item) => {
    const visible = itemMatches(item, query);
    item.classList.toggle("is-filtered", !visible);
    if (visible) visibleItems += 1;
  });

  contentSections.forEach((section) => {
    const items = filterItems.filter((item) => section.contains(item));
    const sectionVisible = items.some((item) => !item.classList.contains("is-filtered"));
    section.classList.toggle("is-filtered", !sectionVisible);
    updateNavVisibility(section, sectionVisible);
  });

  if (resultCount) {
    if (visibleItems === filterItems.length && query.length === 0 && activeFilter === "all") {
      resultCount.textContent = `Todos os ${filterItems.length} exemplos visíveis`;
    } else {
      const noun = visibleItems === 1 ? "exemplo visível" : "exemplos visíveis";
      resultCount.textContent = `${visibleItems} ${noun}`;
    }
  }

  if (emptyState) emptyState.hidden = visibleItems !== 0;
}

filterButtons.forEach((button) => {
  button.addEventListener("click", () => {
    activeFilter = button.dataset.filter ?? "all";
    filterButtons.forEach((candidate) => {
      const selected = candidate === button;
      candidate.classList.toggle("is-active", selected);
      candidate.setAttribute("aria-pressed", String(selected));
    });
    applyFilters();
  });
});

searchInput?.addEventListener("input", applyFilters);

clearSearchButton?.addEventListener("click", () => {
  if (searchInput) searchInput.value = "";
  activeFilter = "all";
  filterButtons.forEach((button) => {
    const selected = button.dataset.filter === "all";
    button.classList.toggle("is-active", selected);
    button.setAttribute("aria-pressed", String(selected));
  });
  applyFilters();
  searchInput?.focus();
});

document.addEventListener("keydown", (event) => {
  const target = event.target;
  const isEditing =
    target instanceof HTMLInputElement ||
    target instanceof HTMLTextAreaElement ||
    target instanceof HTMLSelectElement ||
    target?.isContentEditable;

  if (event.key === "/" && !isEditing) {
    event.preventDefault();
    searchInput?.focus();
  }

  if (event.key === "Escape" && document.activeElement === searchInput && searchInput?.value) {
    searchInput.value = "";
    applyFilters();
  }
});

async function writeClipboard(text) {
  if (navigator.clipboard && window.isSecureContext) {
    await navigator.clipboard.writeText(text);
    return;
  }

  const textarea = document.createElement("textarea");
  textarea.value = text;
  textarea.setAttribute("readonly", "");
  textarea.className = "clipboard-fallback";
  textarea.setAttribute("aria-hidden", "true");
  document.body.append(textarea);
  textarea.select();

  const copied = document.execCommand("copy");
  textarea.remove();
  if (!copied) throw new Error("copy command failed");
}

function createCopyButton() {
  const button = document.createElement("button");
  button.type = "button";
  button.className = "copy-button";
  button.setAttribute("aria-label", "Copiar exemplo de código");
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 16 16");
  svg.setAttribute("aria-hidden", "true");
  const rect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
  rect.setAttribute("x", "5");
  rect.setAttribute("y", "5");
  rect.setAttribute("width", "8");
  rect.setAttribute("height", "8");
  rect.setAttribute("rx", "1");
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute("d", "M3 10H2.5A1.5 1.5 0 0 1 1 8.5v-6A1.5 1.5 0 0 1 2.5 1h6A1.5 1.5 0 0 1 10 2.5V3");
  const label = document.createElement("span");
  label.textContent = "Copiar";
  svg.append(rect, path);
  button.append(svg, label);
  return button;
}

document.querySelectorAll(".code-shell").forEach((shell) => {
  const code = shell.querySelector("code");
  if (!code) return;

  const button = createCopyButton();
  const label = button.querySelector("span");
  shell.append(button);

  button.addEventListener("click", async () => {
    try {
      await writeClipboard(code.textContent ?? "");
      button.classList.add("is-copied");
      if (label) label.textContent = "Copiado";
      if (copyStatus) copyStatus.textContent = "Exemplo copiado para a área de transferência.";

      window.setTimeout(() => {
        button.classList.remove("is-copied");
        if (label) label.textContent = "Copiar";
      }, 1_600);
    } catch {
      if (label) label.textContent = "Falhou";
      if (copyStatus) copyStatus.textContent = "Não foi possível copiar o exemplo.";

      window.setTimeout(() => {
        if (label) label.textContent = "Copiar";
      }, 1_600);
    }
  });
});

window.WSyntax?.highlightAll();

function setCurrentSection(id) {
  navLinks.forEach((link) => {
    const current = link.hash === `#${id}`;
    link.classList.toggle("is-current", current);
    if (current) link.setAttribute("aria-current", "location");
    else link.removeAttribute("aria-current");
  });
}

if ("IntersectionObserver" in window) {
  const observer = new IntersectionObserver(
    (entries) => {
      const visible = entries
        .filter((entry) => entry.isIntersecting && !entry.target.classList.contains("is-filtered"))
        .sort((a, b) => Math.abs(a.boundingClientRect.top) - Math.abs(b.boundingClientRect.top));

      if (visible[0]) setCurrentSection(visible[0].target.id);
    },
    { rootMargin: "-20% 0px -66% 0px", threshold: [0, 0.05, 0.2] },
  );

  trackedSections.forEach((section) => observer.observe(section));
}

navLinks.forEach((link) => {
  link.addEventListener("click", () => setCurrentSection(link.hash.slice(1)));
});

setCurrentSection("inicio");
applyFilters();
