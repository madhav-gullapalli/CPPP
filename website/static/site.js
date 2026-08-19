(() => {
  const base = window.CPPP_BASE || "";
  const root = document.documentElement;
  const storedTheme = localStorage.getItem("cppp-theme");
  if (storedTheme) root.dataset.theme = storedTheme;
  else if (matchMedia("(prefers-color-scheme: light)").matches) root.dataset.theme = "light";

  document.querySelector(".theme-toggle")?.addEventListener("click", () => {
    root.dataset.theme = root.dataset.theme === "light" ? "dark" : "light";
    localStorage.setItem("cppp-theme", root.dataset.theme);
  });
  document.querySelector(".nav-toggle")?.addEventListener("click", () => document.body.classList.toggle("nav-open"));
  document.querySelector(".docs-sidebar")?.addEventListener("click", event => {
    if (innerWidth <= 900 && (event.target === event.currentTarget || event.target.classList.contains("active"))) {
      event.preventDefault();
      event.currentTarget.classList.toggle("expanded");
    }
  });

  document.querySelectorAll(".copy-code").forEach(button => button.addEventListener("click", async () => {
    const code = button.parentElement.querySelector("code")?.textContent || "";
    await navigator.clipboard.writeText(code);
    button.textContent = "Copied";
    setTimeout(() => button.textContent = "Copy", 1200);
  }));

  const tocLinks = [...document.querySelectorAll(".page-toc a")];
  const headings = tocLinks.map(link => document.getElementById(decodeURIComponent(link.hash.slice(1)))).filter(Boolean);
  if (headings.length) {
    const observer = new IntersectionObserver(entries => {
      const visible = entries.filter(entry => entry.isIntersecting).sort((a, b) => a.boundingClientRect.top - b.boundingClientRect.top)[0];
      if (!visible) return;
      tocLinks.forEach(link => link.classList.toggle("current", link.hash === `#${visible.target.id}`));
    }, { rootMargin: "-80px 0px -72% 0px" });
    headings.forEach(heading => observer.observe(heading));
  }

  const dialog = document.querySelector(".search-dialog");
  const input = dialog?.querySelector("input");
  const results = dialog?.querySelector(".search-results");
  let index = [];
  let loaded = false;
  async function openSearch() {
    dialog.showModal();
    input.focus();
    if (!loaded) {
      loaded = true;
      try { index = await fetch(`${base}/search-index.json`).then(response => response.json()); }
      catch { results.innerHTML = "<p>Search index could not be loaded.</p>"; }
    }
  }
  document.querySelector(".search-trigger")?.addEventListener("click", openSearch);
  addEventListener("keydown", event => {
    if (event.key === "/" && !/INPUT|TEXTAREA/.test(document.activeElement.tagName)) { event.preventDefault(); openSearch(); }
    if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "k") { event.preventDefault(); openSearch(); }
  });
  input?.addEventListener("input", () => {
    const query = input.value.trim().toLowerCase();
    if (!query) { results.innerHTML = "<p>Start typing to search the complete documentation.</p>"; return; }
    const terms = query.split(/\s+/);
    const found = index.map(item => {
      const haystack = `${item.title} ${item.section} ${item.text}`.toLowerCase();
      const score = terms.reduce((total, term) => total + (item.title.toLowerCase() === term ? 8 : item.title.toLowerCase().includes(term) ? 4 : haystack.includes(term) ? 1 : -20), 0);
      return { item, score };
    }).filter(match => match.score >= terms.length).sort((a, b) => b.score - a.score).slice(0, 14);
    results.innerHTML = found.length ? found.map(({ item }) => `<a class="search-result" href="${item.url}"><strong>${escapeHtml(item.title)}</strong><span>${escapeHtml(item.section)}</span></a>`).join("") : "<p>No matching documentation.</p>";
  });
  function escapeHtml(value) { const node = document.createElement("span"); node.textContent = value; return node.innerHTML; }
})();
