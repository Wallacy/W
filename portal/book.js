const bookLinks = [...document.querySelectorAll("[data-book-nav] a")];
const bookSections = [...document.querySelectorAll("[data-book-section]")];
const progressBar = document.querySelector("[data-reading-progress]");

function markChapter(id) {
  bookLinks.forEach((link) => {
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
        .filter((entry) => entry.isIntersecting)
        .sort((a, b) => Math.abs(a.boundingClientRect.top) - Math.abs(b.boundingClientRect.top));
      if (visible[0]) markChapter(visible[0].target.id);
    },
    { rootMargin: "-18% 0px -68% 0px", threshold: [0, 0.1] },
  );
  bookSections.forEach((section) => observer.observe(section));
}

function updateProgress() {
  if (!progressBar) return;
  const body = document.documentElement;
  const range = body.scrollHeight - body.clientHeight;
  const progress = range <= 0 ? 100 : Math.min(100, Math.max(0, (body.scrollTop / range) * 100));
  progressBar.value = progress;
}

bookLinks.forEach((link) => link.addEventListener("click", () => markChapter(link.hash.slice(1))));
window.addEventListener("scroll", updateProgress, { passive: true });
markChapter(bookSections[0]?.id ?? "");
updateProgress();
