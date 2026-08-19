# CP++ website

The GitHub Pages site is generated without third-party packages.

## Build locally

From the repository root:

```sh
python3 website/build.py
python3 -m http.server 8000 --directory _site
```

Open <http://localhost:8000>. To test repository-subpath URLs locally, build with:

```sh
python3 website/build.py --base-url /CPPP
```

## Documentation sources

The generator treats `cppp_language.md` as the canonical contest-user reference. It renders the full reference directly and extracts/reorders its sections into the learning path. It also renders the relevant Markdown under `docs/` as advanced material. Site-only introductory prose lives in `website/content/`.

After changing the language reference or contributor documentation, rebuild the site. Navigation, the feature index, and client-side search are regenerated automatically; do not edit `_site`.

## Deployment

`.github/workflows/pages.yml` builds with Python, passes the repository Pages path as `--base-url`, validates the output, and deploys `_site` as the Pages artifact. No branch contains committed generated HTML.
