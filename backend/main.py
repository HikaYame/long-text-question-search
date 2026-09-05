from __future__ import annotations
import html
import json
import re
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

try:
    import search_core
except ImportError as exc:
    raise RuntimeError(
        "C++ search_core is unavailable. Build it and place the .pyd/.so next to main.py."
    ) from exc

ROOT = Path(__file__).resolve().parent
with (ROOT / "database_mock.json").open("r", encoding="utf-8") as f:
    RAW_DOCS = json.load(f)

app = FastAPI(title="Long-text Question Search API", version="1.0.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["GET", "POST"],
    allow_headers=["Content-Type"],
)

class SearchRequest(BaseModel):
    query: str = Field(min_length=1, max_length=100_000)
    top_k: int = Field(default=5, ge=1, le=20)

class SearchResponse(BaseModel):
    query: str
    engine: str
    results: list[dict[str, Any]]

CORE_DOCS = []
for d in RAW_DOCS:
    x = search_core.Document()
    x.id, x.site_name, x.title, x.url, x.text = d["id"], d["site_name"], d["title"], d["url"], d["text"]
    CORE_DOCS.append(x)

BY_ID = {d["id"]: d for d in RAW_DOCS}

def terms_for_highlight(query: str) -> list[str]:
    return [x for x in search_core.tokenize(query) if len(x) >= 2][:32]

def make_snippet(text: str, query: str, window: int = 260) -> str:
    normalized = search_core.normalize_text(text)
    qterms = terms_for_highlight(query)
    if not qterms:
        return html.escape(text[:window])

    lower = normalized.lower()
    positions = [lower.find(t.lower()) for t in qterms if lower.find(t.lower()) >= 0]
    center = min(positions) if positions else 0
    start = max(0, center - window // 2)
    snippet = normalized[start:start + window]

    # Escape first, then mark matched terms. This avoids injecting source HTML.
    safe = html.escape(snippet)
    for term in sorted(set(qterms), key=len, reverse=True):
        pattern = re.compile(re.escape(html.escape(term)), re.IGNORECASE)
        safe = pattern.sub(lambda m: f"<mark>{m.group(0)}</mark>", safe)
    prefix = "…" if start else ""
    suffix = "…" if start + window < len(normalized) else ""
    return prefix + safe + suffix

@app.get("/api/v1/health")
def health():
    return {"ok": True, "engine": "BM25/C++", "documents": len(CORE_DOCS)}

@app.post("/api/v1/search", response_model=SearchResponse)
def search(req: SearchRequest):
    if not req.query.strip():
        raise HTTPException(status_code=422, detail="query must not be empty")

    hits = search_core.bm25_search(CORE_DOCS, req.query, req.top_k)
    max_score = hits[0].score if hits else 0.0
    results = []

    for hit in hits:
        d = BY_ID[hit.id]
        fragment = search_core.build_text_fragment(d["text"], req.query)
        direct_url = d["url"] + fragment
        relative = (hit.score / max_score * 100.0) if max_score else 0.0

        results.append({
            "site_name": d["site_name"],
            "title": d["title"],
            "match_score": round(relative, 1),
            "bm25_score": round(hit.score, 6),
            "snippet": make_snippet(d["text"], req.query),
            "direct_url": direct_url,
        })

    return {"query": req.query, "engine": "BM25/C++", "results": results}

@app.get("/api/v1/documents")
def documents():
    return {"count": len(RAW_DOCS), "documents": [
        {"id": d["id"], "site_name": d["site_name"], "title": d["title"], "url": d["url"]}
        for d in RAW_DOCS
    ]}
