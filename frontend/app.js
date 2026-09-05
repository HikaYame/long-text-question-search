const API = window.BACKEND_API || "https://long-text-question-search.onrender.com";
const $ = (id) => document.getElementById(id);
const query = $("query");
const results = $("results");
const status = $("status");
const searchBtn = $("searchBtn");
const pasteBtn = $("pasteBtn");

$("pasteBtn").onclick = async () => {
  try {
    query.value = await navigator.clipboard.readText();
    query.focus();
    status.textContent = "Đã dán nội dung từ clipboard.";
  } catch {
    status.textContent = "Không thể đọc clipboard. Hãy cấp quyền Clipboard cho trình duyệt.";
  }
};

$("clearBtn").onclick = () => {
  query.value = "";
  results.innerHTML = emptyState("Nhập đề bài để bắt đầu tìm kiếm.");
  status.textContent = "";
};

$("searchBtn").onclick = search;
query.addEventListener("keydown", (e) => {
  if ((e.ctrlKey || e.metaKey) && e.key === "Enter") search();
});

function skeleton() {
  return Array.from({length: 5}, () => `
    <div class="rounded-2xl border border-slate-800 bg-slate-900 p-5 animate-pulse">
      <div class="h-3 w-24 rounded bg-slate-800"></div>
      <div class="mt-3 h-5 w-3/5 rounded bg-slate-800"></div>
      <div class="mt-5 h-20 rounded-xl bg-slate-950"></div>
      <div class="mt-4 h-9 w-44 rounded bg-slate-800"></div>
    </div>`).join("");
}

async function search() {
  const text = query.value.trim();
  if (!text) {
    results.innerHTML = emptyState("Hãy dán một đoạn đề bài trước.");
    return;
  }

  searchBtn.disabled = true;
  status.textContent = "C++ BM25 đang xếp hạng...";
  results.innerHTML = skeleton();

  try {
    const response = await fetch(`${API}/api/v1/search`, {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify({query: text, top_k: 5})
    });
    if (!response.ok) {
      const body = await response.json().catch(() => ({}));
      throw new Error(body.detail || `API HTTP ${response.status}`);
    }

    const data = await response.json();
    status.textContent = `${data.results.length} kết quả · ${data.engine}`;
    results.innerHTML = data.results.length
      ? data.results.map(renderCard).join("")
      : emptyState("Không tìm thấy tài liệu phù hợp.");
  } catch (error) {
    status.textContent = "Không thể kết nối API";
    results.innerHTML = `
      <div class="rounded-2xl border border-red-900/60 bg-red-950/30 p-6">
        <h2 class="font-semibold text-red-200">Lỗi kết nối</h2>
        <p class="mt-2 text-sm text-red-300">${escapeHtml(error.message)}</p>
        <p class="mt-3 text-xs text-red-400">Kiểm tra backend tại http://127.0.0.1:8000.</p>
      </div>`;
  } finally {
    searchBtn.disabled = false;
  }
}

function renderCard(r, i) {
  return `
    <article class="card rounded-2xl border border-slate-800 bg-slate-900 p-5 shadow-xl" style="animation-delay:${i*45}ms">
      <div class="flex flex-col gap-3 sm:flex-row sm:items-start sm:justify-between">
        <div class="min-w-0">
          <div class="text-xs font-bold uppercase tracking-wider text-cyan-400">${escapeHtml(r.site_name)}</div>
          <h2 class="mt-1 text-lg font-semibold text-slate-100">${escapeHtml(r.title)}</h2>
        </div>
        <div class="shrink-0 rounded-full border border-emerald-400/20 bg-emerald-400/10 px-3 py-1 text-sm font-bold text-emerald-300">
          ${r.match_score}% match
        </div>
      </div>
      <div class="mt-4 rounded-xl bg-slate-950 p-4 text-sm leading-6 text-slate-300">${r.snippet}</div>
      <div class="mt-4 flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
        <span class="text-xs text-slate-500">BM25: ${r.bm25_score}</span>
        <a href="${escapeAttr(r.direct_url)}" target="_blank" rel="noopener noreferrer"
           class="inline-flex justify-center rounded-xl bg-slate-100 px-4 py-2.5 text-sm font-bold text-slate-950 hover:bg-white">
          Đến ngay vị trí bài viết ↗
        </a>
      </div>
    </article>`;
}

function emptyState(message) {
  return `<div class="rounded-2xl border border-dashed border-slate-700 bg-slate-900/50 p-10 text-center text-slate-400">${escapeHtml(message)}</div>`;
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#039;'}[c]));
}
function escapeAttr(value) { return escapeHtml(value); }

results.innerHTML = emptyState("Nhập đề bài để bắt đầu tìm kiếm.");
