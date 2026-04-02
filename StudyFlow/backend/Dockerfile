# Blop Study API – ffmpeg/ffprobe für Podcast-Concat und Lernvideo (MP4)
FROM python:3.10-slim-bookworm

RUN apt-get update \
    && apt-get install -y --no-install-recommends ffmpeg \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

# Render setzt PORT; lokal fallback 10000
ENV PORT=10000
EXPOSE 10000

CMD ["sh", "-c", "uvicorn main:app --host 0.0.0.0 --port ${PORT:-10000}"]
