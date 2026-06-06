# Tor3DS Headless Bridge Server
# Copyright (C) 2026 DisLoPik
#
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

from flask import Flask, request, Response, jsonify, render_template_string
from playwright.sync_api import sync_playwright
import socket
import datetime
import threading

app = Flask(__name__)

# ── Config 
TOR_SOCKS_PORT = 9150 # default for Tor Browser
SERVER_PORT    = 5000
TOR_PROXY      = f"socks5://127.0.0.1:{TOR_SOCKS_PORT}"


request_log = []
log_lock    = threading.Lock()
start_time  = datetime.datetime.now()

# ── Helpers 
def log_request(url: str, status: str, note: str = ""):
    entry = {
        "time":   datetime.datetime.now().strftime("%H:%M:%S"),
        "url":    url[:80],
        "status": status,
        "note":   note,
    }
    with log_lock:
        request_log.insert(0, entry)
        if len(request_log) > 50:
            request_log.pop()

def check_tor() -> bool:
    """Return True if Tor's SOCKS port is reachable."""
    try:
        s = socket.create_connection(("127.0.0.1", TOR_SOCKS_PORT), timeout=3)
        s.close()
        return True
    except OSError:
        return False

def get_local_ip():
    """Smart check to find the actual Wi-Fi adapter IP."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

# ── Routes 
@app.route("/")
def dashboard():
    """Human-readable status dashboard."""
    tor_ok   = check_tor()
    uptime   = str(datetime.datetime.now() - start_time).split(".")[0]
    local_ip = get_local_ip()

    with log_lock:
        log_copy = list(request_log)

    return render_template_string(DASHBOARD_HTML,
        tor_ok=tor_ok, uptime=uptime,
        local_ip=local_ip, log=log_copy,
        port=SERVER_PORT)

@app.route("/status")
def status():
    """3DS calls this on startup to check the server is alive."""
    tor_ok = check_tor()
    return jsonify({
        "server":  "tor3ds-bridge",
        "tor":     "connected" if tor_ok else "not_running",
    })

@app.route("/fetch_image")
def fetch_image():
    """Playwright Headless Browser: Returns a JPEG snapshot."""
    url = request.args.get("url", "").strip()
    if not url: return "Missing URL", 400
    if not url.startswith(("http://", "https://")): url = "https://" + url

    if not check_tor():
        log_request(url, "ERROR", "Tor not running (Image fetch)")
        return "Tor not running", 503

    try:
        with sync_playwright() as p:
            browser = p.chromium.launch(proxy={"server": TOR_PROXY})
            page = browser.new_page(viewport={"width": 300, "height": 1024})
            page.goto(url, timeout=30000)
            
            screenshot_bytes = page.screenshot(
                type="jpeg", 
                quality=60, 
                clip={"x": 0, "y": 0, "width": 300, "height": 1024}
            )
            browser.close()
            
        log_request(url, "OK", "Rendered Image Snapshot")
        return Response(screenshot_bytes, mimetype='image/jpeg')
    except Exception as e:
        log_request(url, "FAIL", f"Image failed: {str(e)}")
        return str(e), 500

@app.route("/fetch_links")
def fetch_links():
    """Playwright Headless Browser: Returns invisible link coordinates."""
    url = request.args.get("url", "").strip()
    if not url: return "Missing URL", 400
    if not url.startswith(("http://", "https://")): url = "https://" + url

    try:
        with sync_playwright() as p:
            browser = p.chromium.launch(proxy={"server": TOR_PROXY})
            page = browser.new_page(viewport={"width": 300, "height": 1024})
            page.goto(url, timeout=30000)
            
            links = page.evaluate("""() => {
                return Array.from(document.querySelectorAll('a')).map(a => {
                    let rect = a.getBoundingClientRect();
                    return `${Math.round(rect.left)}|${Math.round(rect.top)}|${Math.round(rect.width)}|${Math.round(rect.height)}|${a.href}`;
                }).filter(l => l.split('|')[2] > 0 && l.split('|')[3] > 0 && l.split('|')[4].length > 0);
            }""")
            browser.close()
            
        log_request(url, "OK", f"Mapped {len(links)} interactive links")
        return Response("\n".join(links), mimetype='text/plain')
    except Exception as e:
        log_request(url, "FAIL", f"Links failed: {str(e)}")
        return str(e), 500

# ── Dashboard HTML 
DASHBOARD_HTML = """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Tor3DS Bridge</title>
<meta http-equiv="refresh" content="10">
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:#0d0118;color:#e2d9f3;font-family:'Courier New',monospace;
       min-height:100vh;padding:24px}
  h1{color:#c084fc;font-size:22px;letter-spacing:2px;margin-bottom:20px}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;margin-bottom:24px}
  .card{background:#1a0a2e;border:1px solid #3b0764;border-radius:8px;padding:16px}
  .card .label{font-size:10px;color:#7c3aed;letter-spacing:1px;text-transform:uppercase}
  .card .value{font-size:20px;color:#f3e8ff;margin-top:4px}
  .ok{color:#4ade80}.err{color:#f87171}
  .log-table{width:100%;border-collapse:collapse;font-size:12px}
  .log-table th{text-align:left;color:#7c3aed;padding:6px 8px;
                border-bottom:1px solid #3b0764;font-weight:normal;
                text-transform:uppercase;letter-spacing:1px}
  .log-table td{padding:5px 8px;border-bottom:1px solid #1e0535;vertical-align:top}
  .log-table tr:hover td{background:#1a0a2e}
  .status-ok{color:#4ade80}.status-fail{color:#f87171}.status-err{color:#fb923c}
  small{color:#6d28d9;font-size:11px}
</style>
</head>
<body>
<h1>🧅 Tor3DS Bridge Engine</h1>
<div class="grid">
  <div class="card">
    <div class="label">Tor Status</div>
    <div class="value {% if tor_ok %}ok{% else %}err{% endif %}">
      {% if tor_ok %}● Connected{% else %}✗ Not Running{% endif %}
    </div>
  </div>
  <div class="card">
    <div class="label">Server Uptime</div>
    <div class="value">{{ uptime }}</div>
  </div>
  <div class="card">
    <div class="label">3DS Should Connect To</div>
    <div class="value" style="font-size:14px">{{ local_ip }}:{{ port }}</div>
  </div>
  <div class="card">
    <div class="label">Requests Logged</div>
    <div class="value">{{ log|length }}</div>
  </div>
</div>

{% if not tor_ok %}
<div style="background:#450a0a;border:1px solid #7f1d1d;border-radius:8px;
            padding:14px;margin-bottom:20px;color:#fca5a5">
  ⚠ Tor is not running. Start Tor before using the 3DS app.
</div>
{% endif %}

<div style="margin-bottom:8px;color:#7c3aed;font-size:11px;letter-spacing:1px">
  RECENT BRIDGE TRAFFIC <small style="color:#4b1d82">(auto-refreshes every 10s)</small>
</div>
<table class="log-table">
  <tr><th>Time</th><th>URL</th><th>Status</th><th>Note</th></tr>
  {% for entry in log %}
  <tr>
    <td style="color:#6d28d9;white-space:nowrap">{{ entry.time }}</td>
    <td>{{ entry.url }}</td>
    <td class="status-{{ entry.status|lower }}">{{ entry.status }}</td>
    <td style="color:#9d7ac7;max-width:300px;overflow:hidden">{{ entry.note }}</td>
  </tr>
  {% else %}
  <tr><td colspan="4" style="color:#4b1d82;padding:16px 8px">
    No traffic yet — connect your 3DS app to begin headless rendering.
  </td></tr>
  {% endfor %}
</table>
</body>
</html>"""

# ── Entry point 
if __name__ == "__main__":
    local_ip = get_local_ip()
    print("=" * 50)
    print("  🧅  Tor3DS Headless Bridge Server")
    print("=" * 50)
    print(f"  Dashboard Open: http://localhost:{SERVER_PORT}/")
    print(f"  3DS Target IP : {local_ip}")
    print("=" * 50)
    app.run(host="0.0.0.0", port=SERVER_PORT, debug=False)
