#!/usr/bin/env python3
"""Build the dependency-free CP++ documentation website."""

from __future__ import annotations

import argparse
import html
import json
import os
import posixpath
import re
import shutil
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from urllib.parse import urlsplit, urlunsplit


ROOT = Path(__file__).resolve().parents[1]
WEBSITE = ROOT / "website"
GITHUB = "https://github.com/madhav-gullapalli/CPPP"


@dataclass
class Page:
    route: str
    title: str
    description: str
    markdown: str
    source: str
    group: str
    eyebrow: str = ""


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def clean_title(value: str) -> str:
    value = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", value)
    value = re.sub(r"`([^`]*)`", r"\1", value)
    value = re.sub(r"[*_~]", "", value)
    return html.unescape(value).strip()


def slugify(value: str) -> str:
    value = clean_title(value).lower()
    value = re.sub(r"[^a-z0-9]+", "-", value).strip("-")
    return value or "section"


def split_h2(markdown: str) -> tuple[str, dict[str, str]]:
    intro: list[str] = []
    sections: dict[str, list[str]] = {}
    current: str | None = None
    for line in markdown.splitlines():
        match = re.match(r"^## (.+)$", line)
        if match:
            current = clean_title(match.group(1))
            sections[current] = [line]
        elif current is None:
            intro.append(line)
        else:
            sections[current].append(line)
    return "\n".join(intro).strip(), {k: "\n".join(v).strip() for k, v in sections.items()}


def split_h3(section: str) -> tuple[list[str], dict[str, str]]:
    intro: list[str] = []
    sections: dict[str, list[str]] = {}
    current: str | None = None
    for line in section.splitlines():
        match = re.match(r"^### (.+)$", line)
        if match:
            current = clean_title(match.group(1))
            sections[current] = [line]
        elif current is None:
            intro.append(line)
        else:
            sections[current].append(line)
    return intro, {k: "\n".join(v).strip() for k, v in sections.items()}


def without_h3(section: str, excluded: set[str]) -> str:
    intro, parts = split_h3(section)
    kept = ["\n".join(intro).strip()]
    kept.extend(body for title, body in parts.items() if title not in excluded)
    return "\n\n".join(part for part in kept if part)


def only_h3(section: str, names: list[str], heading: str | None = None) -> str:
    _, parts = split_h3(section)
    chosen = [parts[name] for name in names if name in parts]
    if heading:
        chosen.insert(0, f"## {heading}")
    return "\n\n".join(chosen)


def first_table(section: str, heading: str = "At a glance") -> str:
    lines = section.splitlines()
    for index in range(len(lines) - 1):
        if "|" not in lines[index]:
            continue
        if not re.match(r"^\s*\|?\s*:?-{3,}", lines[index + 1]):
            continue
        table = [lines[index], lines[index + 1]]
        cursor = index + 2
        while cursor < len(lines) and "|" in lines[cursor] and lines[cursor].strip():
            table.append(lines[cursor])
            cursor += 1
        return f"## {heading}\n\n" + "\n".join(table)
    return ""


LANGUAGE = read("cppp_language.md")
_, LANG = split_h2(LANGUAGE)
INVENTORY = read("docs/temporary_complete_language_feature_inventory.md") if (ROOT / "docs/temporary_complete_language_feature_inventory.md").exists() else ""
_, INVENTORY_SECTIONS = split_h2(INVENTORY)


LEARN = [
    ("basics", "1. Language basics"),
    ("io", "2. Competitive-programming I/O"),
    ("control-flow", "3. Control flow"),
    ("lists-strings", "4. Lists and strings"),
    ("functions", "5. Functions"),
    ("containers", "6. Core containers"),
    ("data-semantics", "7. Conversions and data semantics"),
    ("data-modeling", "8. Advanced data modeling"),
    ("comparators", "9. Comparators and ordering"),
    ("diagnostics", "10. Runtime diagnostics"),
    ("compilation", "11. Compilation and submission"),
]


def chapter(title: str, intro: str, pieces: list[str]) -> str:
    return f"# {title}\n\n{intro}\n\n" + "\n\n".join(piece for piece in pieces if piece)


def learn_pages() -> list[Page]:
    functions_order = [
        "Top-level function definition", "Function calls", "Recursive functions",
        "void returns", "Container parameters by reference", "copy parameters",
        "Function values and function types", "Calling and partially applying function values",
    ]
    heap = only_h3(LANG["Strings"], ["Heap"], "Heap")
    string_without_heap = without_h3(LANG["Strings"], {"Heap"})
    data_semantics = "\n\n".join([
        LANG["Conversions and Casts"],
        only_h3(LANG["Expressions and Operators"], ["Aliasing and copy()"], "Assignment, aliasing, and copy"),
        only_h3(LANG["Truthiness and Conditions"], ["Bool-convertible conditions"]),
        only_h3(LANG["Classes and structs"], ["Class equality", "Class aliasing and reassignment", "Default values"], "Equality and defaults"),
    ])
    pages = {
        "basics": chapter("Language basics", "Learn the syntax needed to read and write a small CP++ program. Statements and blocks come first, followed by values, declarations, and operators.", [first_table(INVENTORY_SECTIONS.get("3. Types, defaults, storage model, and C++ correspondence", "")), LANG["General Syntax"], LANG["Types"], LANG["Declarations and Assignment"], LANG["Literals"], without_h3(LANG["Expressions and Operators"], {"Aliasing and copy()"}), LANG["Truthiness and Conditions"]]),
        "io": chapter("Competitive-programming I/O", "I/O comes early in CP++ because a useful contest program needs to read a case and print an answer. Input is target-typed: the destination tells the compiler what shape to read.", [first_table(INVENTORY_SECTIONS.get("8. Input", "")), LANG["Input"], LANG["Output and Introspection"]]),
        "control-flow": chapter("Control flow", "CP++ keeps familiar braced control flow, then adds contest conveniences such as foreach, `range`, `rep`, and loop `nobreak`.", [first_table(INVENTORY_SECTIONS.get("10. Control flow", "")), LANG["Control Flow"], LANG["Ranges"]]),
        "lists-strings": chapter("Lists and strings", "Lists and strings are the center of CP++'s day-to-day ergonomics. Start with the operation table, then learn construction, indexing, mutation, searching, and splitting through examples.", [first_table(INVENTORY_SECTIONS.get("14. List and string", "")), LANG["Lists"], string_without_heap]),
        "functions": chapter("Functions", "Begin with ordinary typed functions, calls, returns, and recursion. Function values and partial application are covered last.", [first_table(INVENTORY_SECTIONS.get("11. Functions and callable values", "")), only_h3(LANG["Functions"], functions_order, "Functions")]),
        "containers": chapter("Core containers", "Every container follows the same questions: how to create it, access it, modify it, query it, iterate it, convert it, and account for its complexity.", [first_table(INVENTORY_SECTIONS.get("15. Stack, Queue, and Deque", "")), LANG["Stacks, Queues, and Deques"], heap, LANG["Ranges"], LANG["Sets, Maps, and Pairs"]]),
        "data-semantics": chapter("Conversions and data semantics", "This chapter collects the rules that matter when a value crosses an assignment, cast, function boundary, or comparison.", [first_table(INVENTORY_SECTIONS.get("6. Conversion rules", "")), data_semantics]),
        "data-modeling": chapter("Advanced data modeling", "Use structs for inline values and classes for nullable, aliasing object graphs. The exact copy and recursion rules are important in contest data structures.", [first_table(INVENTORY_SECTIONS.get("12. Classes, structs, constructors, and members", "")), LANG["Classes and structs"]]),
        "comparators": chapter("Comparators and ordering", "One comparator model controls List sorting, Heap priority, Set order, and Map key order.", [INVENTORY_SECTIONS.get("21. Comparators", "")]),
        "diagnostics": chapter("Runtime diagnostics", "Checked run mode reports common contest failures at the originating CP++ source location. Submit output intentionally favors judge-ready C++.", [LANG["Runtime-Checked Behavior in --run"], INVENTORY_SECTIONS.get("23. Run-mode checks versus submit behavior", "")]),
        "compilation": chapter("Compilation and submission", "The same parsed and checked program feeds development and submission workflows. Only the later emission path differs.", [first_table(INVENTORY_SECTIONS.get("1. Compiler workflows and observable modes", "")), LANG["Using the compiler"], read("docs/compiler_tooling.md").replace("[compiler_pipeline.md](compiler_pipeline.md)", "[compiler pipeline](/advanced/compiler-pipeline/)")]),
    }
    return [Page(f"learn/{slug}/", title.split(". ", 1)[1], "Guided CP++ documentation for competitive programmers.", pages[slug], "cppp_language.md", "learn", title) for slug, title in LEARN]


def reference_features() -> list[tuple[str, list[tuple[str, str]]]]:
    result = []
    for family, section in LANG.items():
        _, features = split_h3(section)
        result.append((family, [(name, slugify(name)) for name in features]))
    return result


def quick_reference() -> str:
    out = [
        "# CP++ Quick Reference",
        "Use this index when you remember the feature but not its exact spelling. Every link opens the canonical language reference; search can also jump directly to an entry.",
        "## Compiler commands",
        "| Task | Command |",
        "| --- | --- |",
        "| Transpile | `make transpile INPUT=solution.cppp` |",
        "| Checked run | `make run INPUT=solution.cppp` |",
        "| Submit C++ | `make submit INPUT=solution.cppp` |",
        "| Readable submit C++ | `make submit INPUT=solution.cppp READABLE=1` |",
        "## Feature index",
    ]
    for family, features in reference_features():
        if not features:
            continue
        links = [f"[{name}](/reference/#{anchor})" for name, anchor in features]
        out.append(f"### {family}\n\n" + " · ".join(links))
    return "\n\n".join(out)


ADVANCED = [
    ("feature-inventory", "Complete feature inventory", "docs/temporary_complete_language_feature_inventory.md", "Every user-visible feature, handwritten C++ correspondence, complexity, and implementation algorithm."),
    ("compiler-pipeline", "Compiler pipeline", "docs/compiler_pipeline.md", "The end-to-end compiler stages and ownership boundaries."),
    ("compiler-tooling", "Compiler tooling", "docs/compiler_tooling.md", "Token, syntax-tree, semantic, regression, and snapshot inspection."),
    ("submit-mode", "Submit-mode pruning", "docs/submit_mode_dependency_fix.md", "The analyzed-to-pruned AST stage and separated submit code generation."),
    ("semantic-surface", "Semantic surface inventory", "docs/semantic_surface_inventory.md", "An exhaustive semantic-rule and AST audit."),
    ("diagnostics-design", "Compile-time diagnostics design", "docs/compile_time_diagnostics_design.md", "Structured spans, suggestions, and diagnostic-cascade design."),
    ("source-overview", "Source overview", "docs/src/README.md", "A fast map from compiler responsibilities to source files."),
]


def advanced_pages() -> list[Page]:
    pages = []
    for slug, title, source, description in ADVANCED:
        path = ROOT / source
        if path.exists():
            pages.append(Page(f"advanced/{slug}/", title, description, path.read_text(encoding="utf-8"), source, "advanced", "Advanced reference"))
    return pages


def advanced_index() -> str:
    cards = ["# Advanced Reference", "The beginner path stops before compiler internals. This shelf keeps exact audits, compiler workflows, generated-code behavior, and implementation detail discoverable when you want the rabbit hole."]
    for slug, title, source, description in ADVANCED:
        if (ROOT / source).exists():
            cards.append(f"## [{title}](/advanced/{slug}/)\n\n{description}\n\nSource: `{source}`")
    return "\n\n".join(cards)


SOURCE_ROUTES = {
    "README.md": "/",
    "cppp_language.md": "/reference/",
    **{source: f"/advanced/{slug}/" for slug, _, source, _ in ADVANCED},
}


class MarkdownRenderer:
    def __init__(self, base_url: str, source: str):
        self.base = base_url.rstrip("/")
        self.source = source
        self.ids: dict[str, int] = {}
        self.headings: list[dict[str, str | int]] = []

    def url(self, href: str) -> str:
        if href.startswith(("https://", "http://", "mailto:", "#")):
            return href
        parts = urlsplit(href)
        raw_path = parts.path
        if raw_path.startswith("/"):
            route = raw_path
        else:
            source_parent = PurePosixPath(self.source).parent
            resolved = posixpath.normpath(str(PurePosixPath(source_parent, raw_path)))
            if resolved in SOURCE_ROUTES:
                route = SOURCE_ROUTES[resolved]
            elif resolved.endswith(".md") and (ROOT / resolved).exists():
                route = GITHUB + "/blob/main/" + resolved
            elif (ROOT / resolved).exists():
                route = GITHUB + "/blob/main/" + resolved
            else:
                route = raw_path
        if route.startswith("/"):
            route = self.base + route
        return urlunsplit((parts.scheme, parts.netloc, route, parts.query, parts.fragment))

    def inline(self, text: str) -> str:
        code: list[str] = []
        def save_code(match: re.Match[str]) -> str:
            code.append(f"<code>{html.escape(match.group(1))}</code>")
            return f"\x00{len(code)-1}\x00"
        text = re.sub(r"`([^`]+)`", save_code, text)
        text = html.escape(text, quote=False)
        text = re.sub(r"\[([^\]]+)\]\(([^ )]+)(?: \"[^\"]*\")?\)", lambda m: f'<a href="{html.escape(self.url(html.unescape(m.group(2))), quote=True)}">{m.group(1)}</a>', text)
        text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
        text = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<em>\1</em>", text)
        text = re.sub(r"\x00(\d+)\x00", lambda m: code[int(m.group(1))], text)
        return text

    def heading(self, level: int, raw: str) -> str:
        base = slugify(raw)
        count = self.ids.get(base, 0)
        self.ids[base] = count + 1
        anchor = base if count == 0 else f"{base}-{count + 1}"
        title = clean_title(raw)
        self.headings.append({"level": level, "title": title, "id": anchor})
        return f'<h{level} id="{anchor}"><a class="heading-anchor" href="#{anchor}" aria-label="Link to {html.escape(title)}">#</a>{self.inline(raw)}</h{level}>'

    def render(self, markdown: str) -> str:
        lines = markdown.splitlines()
        output: list[str] = []
        i = 0
        while i < len(lines):
            line = lines[i]
            if not line.strip():
                i += 1
                continue
            fence = re.match(r"^```([^`]*)$", line)
            if fence:
                language = fence.group(1).strip()
                block: list[str] = []
                i += 1
                while i < len(lines) and not lines[i].startswith("```"):
                    block.append(lines[i])
                    i += 1
                i += 1
                label = f'<span class="code-label">{html.escape(language)}</span>' if language else ""
                output.append(f'<div class="code-block">{label}<button class="copy-code" type="button">Copy</button><pre><code class="language-{html.escape(language)}">{html.escape(chr(10).join(block))}</code></pre></div>')
                continue
            heading = re.match(r"^(#{1,6})\s+(.+)$", line)
            if heading:
                output.append(self.heading(len(heading.group(1)), heading.group(2).strip()))
                i += 1
                continue
            if i + 1 < len(lines) and "|" in line and re.match(r"^\s*\|?\s*:?-{3,}", lines[i + 1]):
                rows = []
                while i < len(lines) and "|" in lines[i] and lines[i].strip():
                    rows.append(self.table_cells(lines[i]))
                    i += 1
                if len(rows) >= 2:
                    output.append('<div class="table-wrap"><table><thead><tr>' + "".join(f"<th>{self.inline(c)}</th>" for c in rows[0]) + "</tr></thead><tbody>")
                    for row in rows[2:]:
                        output.append("<tr>" + "".join(f"<td>{self.inline(c)}</td>" for c in row) + "</tr>")
                    output.append("</tbody></table></div>")
                continue
            if re.match(r"^\s*([-*_])(?:\s*\1){2,}\s*$", line):
                output.append("<hr>")
                i += 1
                continue
            if line.startswith(">"):
                quote = []
                while i < len(lines) and lines[i].startswith(">"):
                    quote.append(lines[i].lstrip("> "))
                    i += 1
                output.append(f"<blockquote>{self.inline(' '.join(quote))}</blockquote>")
                continue
            list_match = re.match(r"^\s*([-*+] |\d+\. )(.*)$", line)
            if list_match:
                ordered = bool(re.match(r"\d", list_match.group(1)))
                tag = "ol" if ordered else "ul"
                items = []
                while i < len(lines):
                    item = re.match(r"^\s*([-*+] |\d+\. )(.*)$", lines[i])
                    if not item or bool(re.match(r"\d", item.group(1))) != ordered:
                        break
                    value = item.group(2)
                    i += 1
                    while i < len(lines) and lines[i].strip() and not re.match(r"^(#{1,6})\s|^\s*([-*+] |\d+\. )|^```", lines[i]):
                        value += " " + lines[i].strip()
                        i += 1
                    items.append(value)
                output.append(f"<{tag}>" + "".join(f"<li>{self.inline(item)}</li>" for item in items) + f"</{tag}>")
                continue
            paragraph = [line.strip()]
            i += 1
            while i < len(lines) and lines[i].strip() and not re.match(r"^(#{1,6})\s|^```|^\s*([-*+] |\d+\. )|^>|^\s*([-*_])(?:\s*\2){2,}\s*$", lines[i]):
                if i + 1 < len(lines) and "|" in lines[i] and re.match(r"^\s*\|?\s*:?-{3,}", lines[i + 1]):
                    break
                paragraph.append(lines[i].strip())
                i += 1
            output.append(f"<p>{self.inline(' '.join(paragraph))}</p>")
        return "\n".join(output)

    @staticmethod
    def table_cells(line: str) -> list[str]:
        line = line.strip()
        if line.startswith("|"):
            line = line[1:]
        if line.endswith("|") and not line.endswith("\\|"):
            line = line[:-1]
        cells: list[str] = []
        current: list[str] = []
        in_code = False
        escaped = False
        for char in line:
            if escaped:
                current.append(char)
                escaped = False
            elif char == "\\":
                escaped = True
                current.append(char)
            elif char == "`":
                in_code = not in_code
                current.append(char)
            elif char == "|" and not in_code:
                cells.append("".join(current).strip())
                current = []
            else:
                current.append(char)
        cells.append("".join(current).strip())
        return cells


def asset(base: str, path: str) -> str:
    return base.rstrip("/") + "/assets/" + path


def route_url(base: str, route: str) -> str:
    return base.rstrip("/") + "/" + route.lstrip("/")


def search_snippets(markdown: str) -> list[str]:
    """Return plain text following each Markdown heading, in heading order."""
    lines = markdown.splitlines()
    heading_positions: list[tuple[int, int]] = []
    fenced = False
    for index, line in enumerate(lines):
        if line.startswith("```"):
            fenced = not fenced
            continue
        if fenced:
            continue
        match = re.match(r"^(#{1,6})\s+(.+)$", line)
        if match:
            heading_positions.append((index, len(match.group(1))))
    snippets = []
    for position, (start, level) in enumerate(heading_positions):
        end = len(lines)
        for candidate, candidate_level in heading_positions[position + 1:]:
            if candidate_level <= level:
                end = candidate
                break
        raw = " ".join(lines[start + 1:end])
        raw = re.sub(r"```[a-zA-Z0-9_+-]*", " ", raw)
        raw = re.sub(r"[`*_#>|\[\]]", " ", raw)
        raw = re.sub(r"\s+", " ", raw).strip()
        snippets.append(raw[:600])
    return snippets


def top_nav(base: str, active: str) -> str:
    links = [("overview", "Overview", ""), ("examples", "Examples", "examples/"), ("getting-started", "Get started", "getting-started/"), ("learn", "Learn", "learn/basics/"), ("reference", "Reference", "reference/quick/"), ("advanced", "Advanced", "advanced/")]
    return "".join(f'<a href="{route_url(base, route)}" class="{"active" if active == key else ""}">{label}</a>' for key, label, route in links)


def sidebar(base: str, page: Page) -> str:
    if page.group == "learn":
        links = [(f"learn/{slug}/", title) for slug, title in LEARN]
        heading = "Learn CP++"
    elif page.group == "advanced":
        links = [("advanced/", "Advanced overview")] + [(f"advanced/{slug}/", title) for slug, title, source, _ in ADVANCED if (ROOT / source).exists()]
        heading = "Advanced"
    elif page.group == "reference":
        links = [("reference/quick/", "Quick reference"), ("reference/", "Complete reference")]
        heading = "Reference"
    else:
        return ""
    body = "".join(f'<a href="{route_url(base, route)}" class="{"active" if page.route == route else ""}">{html.escape(label)}</a>' for route, label in links)
    return f'<aside class="docs-sidebar" id="docs-sidebar"><div class="sidebar-title">{heading}</div>{body}</aside>'


def toc(headings: list[dict[str, str | int]]) -> str:
    chosen = [h for h in headings if h["level"] in (2, 3)]
    if len(chosen) < 2:
        return ""
    links = "".join(f'<a class="toc-level-{h["level"]}" href="#{h["id"]}">{html.escape(str(h["title"]))}</a>' for h in chosen[:80])
    return f'<aside class="page-toc"><div class="toc-title">On this page</div>{links}</aside>'


def page_template(base: str, page: Page, content: str, headings: list[dict[str, str | int]], home: bool = False) -> str:
    active = "overview" if home else page.group
    side = sidebar(base, page)
    layout = "docs-layout" if side else "solo-layout"
    hero = ""
    if home:
        hero = f'''<section class="hero">
          <div class="hero-copy"><div class="eyebrow">Competitive programming language</div><h1>CP<span>++</span></h1><p>Write the algorithm. Shed the contest boilerplate. Compile to ordinary C++17 and submit anywhere C++ goes.</p>
          <div class="hero-actions"><a class="button primary" href="{route_url(base, 'getting-started/')}">Get started</a><a class="button" href="{route_url(base, 'learn/basics/')}">Learn CP++</a><a class="button quiet" href="{route_url(base, 'reference/')}">Language reference</a></div>
          <div class="proof-line"><span>Real compiler</span><span>Used on contest problems</span><span>Checked development mode</span><span>Judge-ready C++</span></div></div>
          <div class="hero-code"><div class="window-bar"><i></i><i></i><i></i><span>solution.cppp</span></div><pre><code><span class="kw">int</span> n = <span class="fn">input</span>();
<span class="type">List&lt;int&gt;</span> a = <span class="fn">input</span>(n);

<span class="kw">var</span> best = <span class="fn">max</span>(a);
<span class="kw">for</span> (<span class="kw">int</span> x <span class="kw">in</span> a) {{
    <span class="kw">if</span> (x == best) {{
        <span class="fn">print</span>(x);
    }}
}}</code></pre></div>
        </section>'''
    breadcrumb = f'<div class="page-eyebrow">{html.escape(page.eyebrow)}</div>' if page.eyebrow else ""
    canonical = route_url(base, page.route)
    return f'''<!doctype html>
<html lang="en" data-theme="dark">
<head>
  <meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="description" content="{html.escape(page.description, quote=True)}">
  <meta name="theme-color" content="#0b0d10">
  <link rel="canonical" href="{canonical}"><link rel="stylesheet" href="{asset(base, 'site.css')}">
  <title>{html.escape(page.title)} · CP++</title>
</head>
<body>
  <a class="skip-link" href="#main">Skip to content</a>
  <header class="site-header"><a class="brand" href="{route_url(base, '')}" aria-label="CP++ home"><span>CP</span><b>++</b></a><nav class="top-nav" aria-label="Primary">{top_nav(base, active)}</nav><div class="header-tools"><button class="search-trigger" type="button" aria-label="Search documentation"><kbd>/</kbd><span>Search</span></button><button class="theme-toggle" type="button" aria-label="Toggle color theme">◐</button><a class="github-link" href="{GITHUB}">GitHub ↗</a><button class="nav-toggle" type="button" aria-label="Open navigation">Menu</button></div></header>
  {hero}
  <div class="{layout}">{side}<main id="main" class="content">{breadcrumb}<article class="prose">{content}</article><footer class="page-footer"><span>CP++ documentation</span><a href="{GITHUB}/edit/main/{page.source}">Edit source ↗</a></footer></main>{toc(headings)}</div>
  <dialog class="search-dialog"><form method="dialog" class="search-box"><div class="search-input-row"><span>⌕</span><input type="search" placeholder="Search syntax, features, and docs…" autocomplete="off" aria-label="Search documentation"><button value="close" aria-label="Close search">Esc</button></div><div class="search-results"><p>Start typing to search the complete documentation.</p></div></form></dialog>
  <script>window.CPPP_BASE={json.dumps(base.rstrip('/'))};</script><script src="{asset(base, 'site.js')}"></script>
</body></html>'''


def all_pages() -> list[Page]:
    pages = [
        Page("", "CP++", "A programming language designed for competitive programming, with checked development and C++17 submission output.", read("website/content/overview.md"), "website/content/overview.md", "overview"),
        Page("examples/", "Examples", "Small, current CP++ examples and the original graph example.", read("website/content/examples.md"), "website/content/examples.md", "examples", "Show the language"),
        Page("getting-started/", "Getting Started", "Build CP++, run a first program, and produce submission-ready C++.", read("website/content/getting-started.md"), "website/content/getting-started.md", "getting-started", "Start here"),
        Page("reference/quick/", "Quick Reference", "A searchable index of CP++ syntax, methods, containers, and compiler commands.", quick_reference(), "cppp_language.md", "reference", "Lookup"),
        Page("reference/", "Language Reference", "The complete canonical CP++ language reference.", LANGUAGE, "cppp_language.md", "reference", "Exact language surface"),
        Page("advanced/", "Advanced Reference", "Compiler internals, exact semantic audits, pruning, diagnostics, and implementation algorithms.", advanced_index(), "website/README.md", "advanced", "The rabbit hole"),
    ]
    pages.extend(learn_pages())
    pages.extend(advanced_pages())
    return pages


def write_page(output: Path, page: Page, content: str) -> None:
    target = output / page.route / "index.html"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default=str(ROOT / "_site"))
    parser.add_argument("--base-url", default=os.environ.get("CPPP_BASE_URL", ""))
    args = parser.parse_args()
    output = Path(args.output).resolve()
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    shutil.copytree(WEBSITE / "static", output / "assets")

    search = []
    pages = all_pages()
    for page in pages:
        renderer = MarkdownRenderer(args.base_url, page.source)
        rendered = renderer.render(page.markdown)
        document = page_template(args.base_url, page, rendered, renderer.headings, home=page.route == "")
        write_page(output, page, document)
        page_url = route_url(args.base_url, page.route)
        search.append({"title": page.title, "section": page.eyebrow or "CP++", "url": page_url, "text": page.description})
        snippets = search_snippets(page.markdown)
        for number, heading in enumerate(renderer.headings):
            if int(heading["level"]) <= 3:
                search.append({"title": heading["title"], "section": page.title, "url": page_url + "#" + str(heading["id"]), "text": snippets[number] if number < len(snippets) else clean_title(str(heading["title"]))})

    (output / "search-index.json").write_text(json.dumps(search, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    (output / ".nojekyll").write_text("", encoding="utf-8")
    print(f"Built {len(pages)} pages and {len(search)} search entries in {output}")


if __name__ == "__main__":
    main()
