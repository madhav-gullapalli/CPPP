#!/usr/bin/env python3
"""Validate generated CP++ Pages routes, assets, anchors, and search coverage."""

from __future__ import annotations

import argparse
import json
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit


class DocumentParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.ids: list[str] = []
        self.links: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        if values.get("id"):
            self.ids.append(values["id"] or "")
        if tag in {"a", "link"} and values.get("href"):
            self.links.append(values["href"] or "")
        if tag == "script" and values.get("src"):
            self.links.append(values["src"] or "")


def target_for(site: Path, path: str) -> Path:
    if path.endswith("/"):
        return site / path.lstrip("/") / "index.html"
    target = site / path.lstrip("/")
    if target.suffix:
        return target
    return target / "index.html"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("site", nargs="?", default="_site")
    parser.add_argument("--base-url", default="")
    args = parser.parse_args()
    site = Path(args.site).resolve()
    base = args.base_url.rstrip("/")
    failures: list[str] = []
    documents: dict[Path, DocumentParser] = {}

    required = ["index.html", "getting-started/index.html", "examples/index.html", "learn/basics/index.html", "learn/io/index.html", "reference/index.html", "reference/quick/index.html", "advanced/index.html"]
    for route in required:
        if not (site / route).is_file():
            failures.append(f"missing required route: {route}")

    for page in site.rglob("*.html"):
        document = DocumentParser()
        document.feed(page.read_text(encoding="utf-8"))
        documents[page] = document
        if len(document.ids) != len(set(document.ids)):
            failures.append(f"duplicate id in {page.relative_to(site)}")

    for page, document in documents.items():
        for link in document.links:
            parsed = urlsplit(link)
            if parsed.scheme or parsed.netloc or link.startswith(("mailto:", "#")):
                if link.startswith("#") and unquote(link[1:]) not in document.ids:
                    failures.append(f"missing local anchor {link} in {page.relative_to(site)}")
                continue
            path = unquote(parsed.path)
            if base and path.startswith(base + "/"):
                path = path[len(base):]
            elif base and path == base:
                path = "/"
            elif base and path.startswith("/"):
                failures.append(f"URL escapes base path in {page.relative_to(site)}: {link}")
                continue
            if path.startswith("/"):
                target = target_for(site, path)
            else:
                target = (page.parent / path).resolve()
                if path.endswith("/"):
                    target /= "index.html"
            if not target.exists():
                failures.append(f"broken link in {page.relative_to(site)}: {link}")
                continue
            if parsed.fragment and target.suffix == ".html":
                target_document = documents.get(target.resolve())
                if target_document and unquote(parsed.fragment) not in target_document.ids:
                    failures.append(f"missing target anchor in {page.relative_to(site)}: {link}")

    index_path = site / "search-index.json"
    if not index_path.is_file():
        failures.append("missing search-index.json")
    else:
        search = json.loads(index_path.read_text(encoding="utf-8"))
        haystack = " ".join(f'{entry["title"]} {entry["text"]}' for entry in search).lower()
        for term in ["input(n)", "map.prev", "copy()", "find", "split", "max-heap", "nobreak", "assignment aliases"]:
            if term not in haystack:
                failures.append(f"search coverage missing: {term}")

    if failures:
        print("Website validation failed:")
        for failure in failures[:80]:
            print(f"- {failure}")
        raise SystemExit(1)
    print(f"Validated {len(documents)} pages, internal links, anchors, assets, and search coverage")


if __name__ == "__main__":
    main()
