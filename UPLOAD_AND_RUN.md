# Upload-and-run deployment

## Render backend
1. Upload this whole folder to a GitHub repository.
2. In Render, create a Web Service from that repository.
3. Render detects `Dockerfile` automatically. Select Docker runtime if prompted.
4. Do NOT enter custom Build or Start commands. The Dockerfile builds the C++17 BM25 module and starts FastAPI.
5. After deployment, open `/api/v1/health` on the Render URL. It should return `ok: true`.

## Cloudflare frontend
Upload the `frontend` folder to Cloudflare Pages (Direct Upload/static site).

Before deploying the frontend, open `frontend/app.js` and replace:
`https://YOUR-RENDER-SERVICE.onrender.com`
with the actual Render service URL.

Backend CORS is already configured for public frontend requests, so no CORS edit is needed.

## Files added for deployment
- `Dockerfile`: builds the C++ search engine automatically on Render.
- `render.yaml`: Render service definition.
- `UPLOAD_AND_RUN.md`: these instructions.
