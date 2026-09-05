# Long-text Question Search — Production Baseline

## Stack

- C++17 search core
- pybind11 Python binding
- FastAPI REST API
- JSON seed corpus
- HTML5 + Tailwind CDN + vanilla JS SPA

## Run

Linux/macOS:

```bash
chmod +x run.sh
./run.sh
```

In another terminal:

```bash
cd frontend
python3 -m http.server 5500
```

Open `http://127.0.0.1:5500`.

Windows PowerShell:

```powershell
cd backend
py -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
cd ..\core
cmake -S . -B build -Dpybind11_DIR="$(python -m pybind11 --cmakedir)"
cmake --build build --config Release
Copy-Item build\Release\*.pyd ..\backend\
cd ..\backend
python -m uvicorn main:app --host 127.0.0.1 --port 8000
```

## API

`POST /api/v1/search`

```json
{"query":"đoạn đề bài dài...","top_k":5}
```

Response contains:

- `site_name`
- `title`
- `match_score`
- `snippet`
- `direct_url`
- `bm25_score`

## Important production notes

The included 18 records are a **seed dataset**, not scraped copies of third-party sites. Their URLs are source-style placeholders and should only be replaced with URLs that your crawler/indexer is legally permitted to process.

The C++ core performs BM25 at query time over the loaded corpus. For a genuinely large production corpus, replace this in-memory scan with an offline inverted-index builder and memory-mapped postings lists. The BM25 scoring function can remain unchanged.

For Vietnamese Unicode quality, a production indexer should add a Unicode-aware Vietnamese word segmenter before indexing. The current core deliberately avoids unsafe byte-level Unicode case conversion.

Chrome Text Fragment is generated from normalized token boundaries. For exact highlighting, the fragment should be generated from the exact extracted text stored for that URL; if a site changes its content, a previously generated fragment may stop matching.

Before internet deployment:

- restrict CORS to the deployed frontend origin
- add authentication/rate limiting if publicly exposed
- enforce reverse-proxy body/time limits
- validate and canonicalize crawler URLs
- honor robots.txt and site terms
- avoid SSRF by isolating crawler fetches and blocking private network ranges
- add structured logging and metrics
- persist the index/database rather than loading JSON on every process start


## Upload-and-run deployment
This package includes a `Dockerfile` and `render.yaml` so Render can build the C++ extension automatically. See `UPLOAD_AND_RUN.md`.
