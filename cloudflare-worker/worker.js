/**
 * Cloudflare Worker: WebSocket proxy (HTTPS/WSS on 443).
 * Client connects to wss://your-subdomain.workers.dev/ws
 * Worker forwards to ORIGIN_WS_URL (your server, e.g. via Cloudflare Tunnel).
 *
 * Set env ORIGIN_WS_URL in Worker settings, e.g.:
 *   https://your-tunnel-host.trycloudflare.com  (or your Cloudflare Tunnel hostname)
 * Worker will use this for WebSocket upgrade to your Viewer server.
 */

export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);
    if (url.pathname !== "/ws" && url.pathname !== "/") {
      return new Response("Not Found", { status: 404 });
    }

    const upgrade = request.headers.get("Upgrade");
    if (!upgrade || upgrade.toLowerCase() !== "websocket") {
      return new Response("Expected WebSocket upgrade", { status: 426 });
    }

    const originUrl = env.ORIGIN_WS_URL || "http://127.0.0.1:8765";
    const wsOrigin = originUrl.replace(/^http/, "ws");

    const [client, server] = Object.values(new WebSocketPair());
    server.accept();

    let upstream = null;
    try {
      const upstreamResp = await fetch(wsOrigin + (url.pathname === "/" ? "/" : url.pathname), {
        headers: {
          Upgrade: "websocket",
          "Connection": "Upgrade",
          "Sec-WebSocket-Version": request.headers.get("Sec-WebSocket-Version") || "13",
          "Sec-WebSocket-Key": request.headers.get("Sec-WebSocket-Key") || "",
        },
      });
      if (!upstreamResp.webSocket) {
        server.close(1011, "Origin did not accept WebSocket");
        return new Response(null, { status: 101, webSocket: client });
      }
      upstream = upstreamResp.webSocket;
      upstream.accept();

      server.addEventListener("message", (ev) => {
        if (upstream && upstream.readyState === WebSocket.OPEN) {
          upstream.send(ev.data);
        }
      });
      upstream.addEventListener("message", (ev) => {
        if (server.readyState === WebSocket.OPEN) {
          server.send(ev.data);
        }
      });
      server.addEventListener("close", () => {
        if (upstream) upstream.close();
      });
      upstream.addEventListener("close", () => {
        server.close();
      });
    } catch (e) {
      server.close(1011, "Proxy error: " + (e.message || "unknown"));
    }

    return new Response(null, { status: 101, webSocket: client });
  },
};
