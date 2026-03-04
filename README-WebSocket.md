# WebSocket Setup (HTTPS + Cloudflare Worker)

## Overview

- **Client (Remote DLL)**: Loads `config.json` at startup (same folder as the DLL), reads `server_url`, connects via WebSocket over HTTPS (WSS). Sends JSON messages (e.g. `{"type":"target_hit","message":"..."}`).
- **Server (Viewer MFC)**: Listens for WebSocket on port **8765**. Shows clients in a table (UID, Last Ping, Status, Events Today, Received Logs, IP). On `target_hit` messages, shows a Windows notification.

## Local testing (no Cloudflare)

1. **Server**: Run Viewer.exe. It starts the WebSocket server on `0.0.0.0:8765`.
2. **Client**: Place `config.json` next to the DLL with:
   ```json
   { "server_url": "ws://127.0.0.1:8765" }
   ```
   Then load the DLL; it will connect and send a test `target_hit` message.

## Production (HTTPS via Cloudflare Worker)

1. **Expose the server**  
   Use [Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-apps/) (e.g. `cloudflared`) so your Viewer machine is reachable at a hostname, e.g. `https://your-host.your-domain.com` (and thus `wss://your-host.your-domain.com` for WebSocket).

2. **Deploy the Worker**  
   - In [Cloudflare Dashboard](https://dash.cloudflare.com/) → Workers & Pages → Create Worker.  
   - Paste the code from `cloudflare-worker/worker.js`.  
   - Add an environment variable: `ORIGIN_WS_URL` = `https://your-host.your-domain.com` (the tunnel URL to your Viewer).  
   - Deploy and note the Worker URL, e.g. `https://your-worker.your-subdomain.workers.dev`.

3. **Client config**  
   In `config.json` next to the DLL set:
   ```json
   { "server_url": "wss://your-worker.your-subdomain.workers.dev/ws" }
   ```

Traffic is then: **Client** → WSS to Worker (port 443, looks like normal Cloudflare) → Worker → **Your server** (via tunnel). All between client and Worker is HTTPS/WSS with Cloudflare’s certificate.
