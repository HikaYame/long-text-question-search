#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON="${PYTHON:-python3}"

echo "[1/4] Creating virtual environment..."
cd "$ROOT/backend"
if [ ! -d ".venv" ]; then "$PYTHON" -m venv .venv; fi
source .venv/bin/activate

echo "[2/4] Installing Python dependencies..."
python -m pip install --upgrade pip
python -m pip install -r requirements.txt

echo "[3/4] Building C++ core..."
cd "$ROOT/core"
PYBIND11_DIR="$(python -m pybind11 --cmakedir)"
cmake -S . -B build -Dpybind11_DIR="$PYBIND11_DIR"
cmake --build build --config Release -j

echo "[4/4] Installing C++ extension into backend..."
shopt -s nullglob
mods=(build/*.so build/*.pyd build/Release/*.pyd)
if [ "${#mods[@]}" -eq 0 ]; then
  echo "ERROR: C++ extension was not produced." >&2
  exit 1
fi
cp "${mods[0]}" "$ROOT/backend/"

cd "$ROOT/backend"
echo "Starting API on http://127.0.0.1:8000"
echo "Open frontend/index.html through a static server, e.g.:"
echo "  cd frontend && python3 -m http.server 5500"
exec python -m uvicorn main:app --host 127.0.0.1 --port 8000
