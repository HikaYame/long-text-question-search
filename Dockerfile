FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

WORKDIR /app

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

COPY backend/requirements.txt /app/backend/requirements.txt
RUN pip install --no-cache-dir -r /app/backend/requirements.txt

COPY core /app/core
RUN cmake -S /app/core -B /app/core/build -Dpybind11_DIR=$(python -m pybind11 --cmakedir) \
    && cmake --build /app/core/build --config Release \
    && cp /app/core/build/search_core*.so /app/backend/

COPY backend /app/backend

WORKDIR /app/backend

CMD sh -c 'uvicorn main:app --host 0.0.0.0 --port ${PORT:-8000}'
