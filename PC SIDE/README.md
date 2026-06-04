# Tor3DS Bridge — PC Server

The PC-side component of the Tor3DS homebrew project.
Runs a local server that fetches web pages through Tor and
serves simplified HTML back to the 3DS homebrew app.

## How it works

```
[3DS App] <──WiFi──> [This Server :5000] <──Tor──> [Internet]
```

## Requirements

- Python 3.8+
- Tor installed and running on your PC
- Python packages: `pip install flask requests[socks] beautifulsoup4`

## Starting Tor

**Windows:**
- Download and run the [Tor Browser](https://www.torproject.org/)
- Keep it open in the background (this exposes port 9050)
- OR install the Expert Bundle and run `tor.exe` directly

**Linux:**
```bash
sudo apt install tor
sudo service tor start
```

**macOS:**
```bash
brew install tor
brew services start tor
```

## Running the server

```bash
python server.py
```

Open `http://localhost:5000` in your browser to see the dashboard.

## 3DS App endpoints

| Endpoint | Description |
|---|---|
| `GET /status` | Health check — returns JSON |
| `GET /fetch?url=https://example.com` | Fetch a page through Tor |

## Notes

- The server listens on all interfaces (`0.0.0.0:5000`) so your 3DS can reach it over WiFi
- Your PC and 3DS must be on the same local network
- Traffic between the 3DS and PC is unencrypted local WiFi — Tor anonymity starts at the PC
